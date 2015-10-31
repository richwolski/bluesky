/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2011  The Regents of the University of California
 * Written by Michael Vrable <mvrable@cs.ucsd.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* A very simple AWS-like server, without any authentication.  This is meant
 * performance testing in a local environment.  The server offers weak
 * guarantees on data durability--data is stored directly to the file system
 * without synchronization, so data might be lost in a crash.  This should
 * offer good performance though for benchmarking.
 *
 * Protocol: Each request is a whitespace-separated (typically, a single space)
 * list of parameters terminated by a newline character.  The response is a
 * line containing a response code and a body length (again white-separated and
 * newline-terminated) followed by the response body.  Requests:
 *   GET filename offset length
 *   PUT filename length
 *   DELETE filename
 *   LIST prefix
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SIMPLESTORE_PORT 9541

/* Maximum number of connections the server will queue waiting for us to call
 * accept(). */
#define TCP_BACKLOG 32

#define WHITESPACE " \t\r\n"
#define MAX_ARGS 4

/* Maximum length of a command that we will accept. */
#define MAX_CMD_LEN 4096

#define MAX_OBJECT_SIZE (256 << 20)

enum command { GET, PUT, LIST, DELETE };

void write_data(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t bytes = write(fd, buf, len);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            exit(1);
        }
        buf += bytes;
        len -= bytes;
    }
}

/* SIGCHLD handler, used to clean up processes that handle the connections. */
static void sigchld_handler(int signal)
{
    int pid;
    int reaped = 0;

    while ((pid = waitpid(WAIT_ANY, NULL, WNOHANG)) > 0) {
        reaped++;
    }

    if (pid < 0) {
        if (errno == ECHILD && reaped) {
            /* Don't print an error for the caes that we successfully cleaned
             * up after all children. */
        } else {
            perror("waitpid");
        }
    }
}

/* Return a text representation of a socket address.  Returns a pointer to a
 * static buffer so it is non-reentrant. */
const char *sockname(struct sockaddr_in *addr, socklen_t len)
{
    static char buf[128];
    if (len < sizeof(struct sockaddr_in))
        return NULL;
    if (addr->sin_family != AF_INET)
        return NULL;

    uint32_t ip = ntohl(addr->sin_addr.s_addr);
    sprintf(buf, "%d.%d.%d.%d:%d",
            (int)((ip >> 24) & 0xff),
            (int)((ip >> 16) & 0xff),
            (int)((ip >> 8) & 0xff),
            (int)(ip & 0xff),
            ntohs(addr->sin_port));

    return buf;
}

/* Convert a path from a client to the actual filename used.  Returns a string
 * that must be freed. */
char *normalize_path(const char *in)
{
    char *out = malloc(2 * strlen(in) + 1);
    int i, j;
    for (i = 0, j = 0; in[i] != '\0'; i++) {
        if (in[i] == '/') {
            out[j++] = '_';
            out[j++] = 's';
        } else if (in[i] == '_') {
            out[j++] = '_';
            out[j++] = 'u';
        } else {
            out[j++] = in[i];
        }
    }
    out[j++] = '\0';
    return out;
}

void cmd_get(int fd, char *path, size_t start, ssize_t len)
{
    char buf[65536];

    char *response = "-1\n";
    int file = open(path, O_RDONLY);
    if (file < 0) {
        write_data(fd, response, strlen(response));
        return;
    }

    struct stat statbuf;
    if (fstat(file, &statbuf) < 0) {
        write_data(fd, response, strlen(response));
        return;
    }

    size_t datalen = statbuf.st_size;
    if (start > 0) {
        if (start >= datalen) {
            datalen = 0;
        } else {
            lseek(file, start, SEEK_SET);
            datalen -= start;
        }
    }
    if (len > 0 && len < datalen) {
        datalen = len;
    }

    sprintf(buf, "%zd\n", datalen);
    write_data(fd, buf, strlen(buf));

    while (datalen > 0) {
        size_t needed = datalen > sizeof(buf) ? sizeof(buf) : datalen;
        ssize_t bytes = read(file, buf, needed);
        if (bytes < 0 && errno == EINTR)
            continue;
        if (bytes <= 0) {
            /* Error reading necessary data, but we already told the client the
             * file size so pad the data to the client with null bytes. */
            memset(buf, 0, needed);
            bytes = needed;
        }
        write_data(fd, buf, bytes);
        datalen -= bytes;
    }

    close(file);
}

void cmd_put(int fd, char *path, char *buf,
             size_t object_size, size_t buf_used)
{
    while (buf_used < object_size) {
        ssize_t bytes;

        bytes = read(fd, buf + buf_used, object_size - buf_used);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            else
                exit(1);
        }

        if (bytes == 0)
            exit(1);

        assert(bytes <= object_size - buf_used);
        buf_used += bytes;

        continue;
    }

    /* printf("Got %zd bytes for object '%s'\n", buf_used, path); */

    char *response = "-1\n";
    int file = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (file >= 0) {
        write_data(file, buf, object_size);
        response = "0\n";
        close(file);
    }

    write_data(fd, response, strlen(response));
}

/* The core handler for processing requests from the client.  This can be
 * single-threaded since each connection is handled in a separate process. */
void handle_connection(int fd)
{
    char cmdbuf[MAX_CMD_LEN];
    size_t buflen = 0;

    while (1) {
        /* Keep reading data until reaching a newline, so that a complete
         * command can be parsed. */
        if (buflen == 0 || memchr(cmdbuf, '\n', buflen) == NULL) {
            ssize_t bytes;

            if (buflen == MAX_CMD_LEN) {
                /* Command is too long and thus malformed; close the
                 * connection. */
                return;
            }

            bytes = read(fd, cmdbuf + buflen, MAX_CMD_LEN - buflen);
            if (bytes < 0) {
                if (errno == EINTR)
                    continue;
                else
                    return;
            }
            if (bytes == 0)
                return;

            assert(bytes <= MAX_CMD_LEN - buflen);
            buflen += bytes;

            continue;
        }

        size_t cmdlen = (char *)memchr(cmdbuf, '\n', buflen) - cmdbuf + 1;
        cmdbuf[cmdlen - 1] = '\0';
        char *token;
        int arg_count;
        char *args[MAX_ARGS];
        int arg_int[MAX_ARGS];
        for (token = strtok(cmdbuf, WHITESPACE), arg_count = 0;
             token != NULL;
             token = strtok(NULL, WHITESPACE), arg_count++)
        {
            args[arg_count] = token;
            arg_int[arg_count] = atoi(token);
        }

        if (arg_count < 2) {
            return;
        }
        char *path = normalize_path(args[1]);
        enum command cmd;
        if (strcmp(args[0], "GET") == 0 && arg_count == 4) {
            cmd = GET;
        } else if (strcmp(args[0], "PUT") == 0 && arg_count == 3) {
            cmd = PUT;
        } else if (strcmp(args[0], "DELETE") == 0 && arg_count == 2) {
            cmd = DELETE;
        } else {
            return;
        }

        if (cmdlen < buflen)
            memmove(cmdbuf, cmdbuf + cmdlen, buflen - cmdlen);
        buflen -= cmdlen;

        switch (cmd) {
        case GET:
            cmd_get(fd, path, arg_int[2], arg_int[3]);
            break;
        case PUT: {
            size_t object_size = arg_int[2];
            if (object_size > MAX_OBJECT_SIZE)
                return;
            char *data_buf = malloc(object_size);
            if (data_buf == NULL)
                return;
            size_t data_buflen = buflen > object_size ? object_size : buflen;
            if (data_buflen > 0)
                memcpy(data_buf, cmdbuf, data_buflen);
            if (data_buflen < buflen) {
                memmove(cmdbuf, cmdbuf + data_buflen, buflen - data_buflen);
                buflen -= cmdlen;
            } else {
                buflen = 0;
            }
            cmd_put(fd, path, data_buf, object_size, data_buflen);
            break;
        }
        case DELETE:
            //cmd_delete(fd, path);
            break;
        default:
            return;
        }

        free(path);
    }
}

/* Create a listening TCP socket on a new address (we do not use a fixed port).
 * Return the file descriptor of the listening socket. */
int server_init()
{
    int fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SIMPLESTORE_PORT);
    if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(fd, TCP_BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    if (getsockname(fd, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("getsockname");
        exit(1);
    }

    printf("Server listening on %s ...\n", sockname(&server_addr, addr_len));
    fflush(stdout);

    return fd;
}

/* Process-based server main loop.  Wait for a connection, accept it, fork a
 * child process to handle the connection, and repeat.  Child processes are
 * reaped in the SIGCHLD handler. */
void server_main(int listen_fd)
{
    struct sigaction handler;

    /* Install signal handler for SIGCHLD. */
    handler.sa_handler = sigchld_handler;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &handler, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr,
                               &addr_len);
        int pid;

        /* Very simple error handling.  Exit if something goes wrong.  Later,
         * might want to look into not killing off current connections abruptly
         * if we encounter an error in the accept(). */
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;

            perror("accept");
            exit(1);
        }

        printf("Accepted connection from %s ...\n",
               sockname(&client_addr, addr_len));
        fflush(stdout);

        pid = fork();
        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) {
            handle_connection(client_fd);
            printf("Closing connection %s ...\n",
                   sockname(&client_addr, addr_len));
            close(client_fd);
            exit(0);
        }

        close(client_fd);
    }
}

/* Print a help message describing command-line options to stdout. */
static void usage(char *argv0)
{
    printf("Usage: %s [options...]\n"
           "A simple key-value storage server.\n", argv0);
}

int main(int argc, char *argv[])
{
    int fd;
    //int i;
    int display_help = 0, cmdline_error = 0;

#if 0
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0) {
            display_help = 1;
        } else if (strcmp(argv[i], "-document_root") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr,
                        "Error: Argument to -document_root expected.\n");
                cmdline_error = 1;
            } else {
                document_root = argv[i];
            }
        } else if (strcmp(argv[i], "-port") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr,
                        "Error: Expected port number after -port.\n");
                cmdline_error = 1;
            } else {
                server_port = atoi(argv[i]);
                if (server_port < 1 || server_port > 65535) {
                    fprintf(stderr,
                            "Error: Port must be between 1 and 65535.\n");
                    cmdline_error = 1;
                }
            }
        } else if (strcmp(argv[i], "-mime_types") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr,
                        "Error: Argument to -mime_types expected.\n");
                cmdline_error = 1;
            } else {
                mime_types_file = argv[i];
            }
        } else if (strcmp(argv[i], "-log") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr,
                        "Error: Argument to -log expected.\n");
                cmdline_error = 1;
            } else {
                log_fname = argv[i];
            }
        } else {
            fprintf(stderr, "Error: Unrecognized option '%s'\n", argv[i]);
            cmdline_error = 1;
        }
    }
#endif

    /* Display a help message if requested, or let the user know to look at the
     * help message if there was an error parsing the command line.  In either
     * case, the server never starts. */
    if (display_help) {
        usage(argv[0]);
        exit(0);
    } else if (cmdline_error) {
        fprintf(stderr, "Run '%s -help' for a summary of options.\n", argv[0]);
        exit(2);
    }

    fd = server_init();
    if (fd >= 0) {
        server_main(fd);
    }

    return 0;
}
