/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2009  The Regents of the University of California
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

/* Interface to the simple BlueSky test storage server. */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "bluesky-private.h"

#define MAX_IDLE_CONNECTIONS 8

typedef struct {
    GThreadPool *thread_pool;
    struct sockaddr_in server_addr;

    /* A pool of open file connections to the server which are not currently in
     * use. */
    GQueue *fd_pool;
    GMutex *fd_pool_lock;
} SimpleStore;

static int get_connection(SimpleStore *store)
{
    int fd = -1;

    g_mutex_lock(store->fd_pool_lock);
    if (!g_queue_is_empty(store->fd_pool)) {
        fd = GPOINTER_TO_INT(g_queue_pop_head(store->fd_pool));
    }
    g_mutex_unlock(store->fd_pool_lock);
    if (fd != -1)
        return fd;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        g_warning("Error creating simplestore socket: %m");
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&store->server_addr,
                sizeof(store->server_addr)) < 0) {
        g_warning("Error connecting to simplestore server: %m");
        return -1;
    }

    return fd;
}

static void put_connection(SimpleStore *store, int fd)
{
    g_mutex_lock(store->fd_pool_lock);
    g_queue_push_head(store->fd_pool, GINT_TO_POINTER(fd));
    while (g_queue_get_length(store->fd_pool) > MAX_IDLE_CONNECTIONS) {
        fd = GPOINTER_TO_INT(g_queue_pop_tail(store->fd_pool));
        close(fd);
    }
    g_mutex_unlock(store->fd_pool_lock);
}

static gboolean write_data(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t bytes = write(fd, buf, len);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            return FALSE;
        }
        buf += bytes;
        len -= bytes;
    }
    return TRUE;
}

static ssize_t read_line(int fd, char *buf, size_t maxlen)
{
    size_t buflen = 0;

    /* Leave room for a null byte at the end */
    maxlen--;

    while (buflen == 0 || memchr(buf, '\n', buflen) == NULL) {
        ssize_t bytes;

        if (buflen == maxlen) {
            return -1;
        }

        bytes = read(fd, buf + buflen, maxlen - buflen);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            else
                return -1;
        }
        if (bytes == 0)
            break;

        g_assert(bytes <= maxlen - buflen);
        buflen += bytes;
    }

    buf[buflen] = '\0';
    return buflen;
}

static gboolean read_bytes(int fd, char *buf, size_t len)
{
    while (len > 0) {
        ssize_t bytes;

        bytes = read(fd, buf, len);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            else
                return FALSE;
        }
        if (bytes == 0)
            return FALSE;

        g_assert(bytes <= len);
        len -= bytes;
        buf += bytes;
    }

    return TRUE;
}

/* TODO: Re-use a connection for multiple requests, and support pipelining. */
static void simplestore_task(gpointer a, gpointer b)
{
    BlueSkyStoreAsync *async = (BlueSkyStoreAsync *)a;
    SimpleStore *server = (SimpleStore *)bluesky_store_async_get_handle(async);

    async->status = ASYNC_RUNNING;

    int fd = get_connection(server);
    if (fd < 0) {
        bluesky_store_async_mark_complete(async);
        bluesky_store_async_unref(async);
        return;
    }

    switch (async->op) {
    case STORE_OP_GET:
    {
        async->result = -1;
        char *cmd = g_strdup_printf("GET %s %zd %zd\n",
                                    async->key, async->start, async->len);
        char result_buf[256];

        if (!write_data(fd, cmd, strlen(cmd))) {
            g_free(cmd);
            break;
        }

        g_free(cmd);

        int bytes = read_line(fd, result_buf, sizeof(result_buf));
        if (bytes < 0)
            break;
        int result = atoi(result_buf);
        if (result < 0)
            break;

        char *data = g_malloc(result);
        if (strchr(result_buf, '\n') != NULL) {
            int header_size = strchr(result_buf, '\n') - result_buf + 1;
            int data_bytes = bytes - header_size;
            if (data_bytes > result)
                data_bytes = result;
            memcpy(data, result_buf + header_size, data_bytes);
            if (!read_bytes(fd, data + data_bytes, result - data_bytes))
                break;
        } else {
            if (!read_bytes(fd, data, result))
                break;
        }

        async->data = bluesky_string_new(data, result);
        async->result = 0;
        async->range_done = TRUE;
        break;
    }

    case STORE_OP_PUT:
    {
        async->result = -1;
        char *cmd = g_strdup_printf("PUT %s %zd\n",
                                    async->key, async->data->len);
        char result_buf[256];

        if (!write_data(fd, cmd, strlen(cmd))) {
            g_free(cmd);
            break;
        }

        g_free(cmd);
        if (!write_data(fd, async->data->data, async->data->len)) {
            break;
        }

        if (read_line(fd, result_buf, sizeof(result_buf)) < 0)
            break;
        if (atoi(result_buf) != 0)
            break;

        async->result = 0;
        break;
    }

    default:
        break;
    }

    int success = (async->result == 0);
    bluesky_store_async_mark_complete(async);
    bluesky_store_async_unref(async);

    if (success) {
        put_connection(server, fd);
    } else {
        close(fd);
    }
}

static char *simplestore_lookup_last(gpointer s, const char *prefix)
{
    return NULL;
}

static gpointer simplestore_new(const gchar *path)
{
    SimpleStore *store = g_new0(SimpleStore, 1);

    /* TODO: Right now we leak this memory.  We should probably clean up in
     * simplestore_destroy, but it's not a big deal. */
    const gchar *host = "127.0.0.1", *port = "9541";
    if (path != NULL) {
        gchar **target = g_strsplit(path, ":", 0);
        if (target[0] != NULL) {
            host = target[0];
            if (target[1] != NULL) {
                port = target[1];
            }
        }
    }

    g_print("simplestore: %s port %s\n", host, port);

    struct addrinfo hints;
    struct addrinfo *lookup_result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int res = getaddrinfo(host, port, &hints, &lookup_result);
    if (res != 0) {
        fprintf(stderr, "simplestore: cannot resolve target name: %s\n",
                gai_strerror(res));
        return NULL;
    }
    for (struct addrinfo *ai = lookup_result; ai != NULL; ai = ai->ai_next) {
        printf("flags=%d family=%d socktype=%d proto=%d\n",
               ai->ai_flags,
               ai->ai_family,
               ai->ai_socktype,
               ai->ai_protocol);
        if (ai->ai_addrlen == sizeof(struct sockaddr_in)) {
            memcpy(&store->server_addr, ai->ai_addr,
                   sizeof(struct sockaddr_in));
        } else {
            fprintf(stderr, "Warning: Bad address record size!\n");
        }
    }
    freeaddrinfo(lookup_result);

    store->fd_pool = g_queue_new();
    store->fd_pool_lock = g_mutex_new();

    store->thread_pool = g_thread_pool_new(simplestore_task, NULL,
                                           bluesky_max_threads, FALSE, NULL);

    return store;
}

static void simplestore_destroy(gpointer store)
{
}

static void simplestore_submit(gpointer store, BlueSkyStoreAsync *async)
{
    SimpleStore *server = (SimpleStore *)store;

    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
    case STORE_OP_PUT:
        async->status = ASYNC_PENDING;
        bluesky_store_async_ref(async);
        g_thread_pool_push(server->thread_pool, async, NULL);
        break;

    default:
        g_warning("Uknown operation type for simplestore: %d\n", async->op);
        bluesky_store_async_mark_complete(async);
        break;
    }
}

static void simplestore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static BlueSkyStoreImplementation store_impl = {
    .create = simplestore_new,
    .destroy = simplestore_destroy,
    .submit = simplestore_submit,
    .cleanup = simplestore_cleanup,
    .lookup_last = simplestore_lookup_last,
};

void bluesky_store_init_simple(void)
{
    bluesky_store_register(&store_impl, "simple");
}
