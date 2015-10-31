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

/* Synthetic client for benchmarking: a tool for directly generating NFS
 * requests and reading back the responses, so that we can exercise the server
 * differently than the Linux kernel NFS client does.
 *
 * Much of this is copied from rpc.c and other BlueSky server code but is
 * designed to run independently of BlueSky. */

#include "mount_prot.h"
#include "nfs3_prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <glib.h>

/* TCP port number to use for NFS protocol.  (Would be 2049 in standard NFS.) */
#define NFS_SERVICE_PORT 2051

/* Maximum size of a single RPC message that we will accept (8 MB). */
#define MAX_RPC_MSGSIZE (8 << 20)

FILE *logfile = NULL;

int warmup_mode = 0;
int threads;
int completed = 0;
int read_size = 32768;

struct bench_file {
    uint64_t inum;
    uint64_t size;
};

struct data_point {
    uint32_t timestamp;     // Unix timestamp of completion
    uint32_t latency;       // Latency, in microseconds
} __attribute__((packed));

GArray *bench_files;

struct rpc_reply {
    uint32_t xid;
    uint32_t type;
    uint32_t stat;
    uint32_t verf_flavor;
    uint32_t verf_len;
    uint32_t accept_stat;
};

struct rpc_fail_reply {
    uint32_t xid;
    uint32_t type;
    uint32_t stat;
    uint32_t verf_flavor;
    uint32_t verf_len;
    uint32_t accept_stat;
};

struct rpc_call_header {
    uint32_t xid;
    uint32_t mtype;
    uint32_t rpcvers;
    uint32_t prog;
    uint32_t vers;
    uint32_t proc;
};

struct rpc_auth {
    uint32_t flavor;
    uint32_t len;
};

typedef struct {
    GIOChannel *channel;

    /* The reassembled message, thus far. */
    GString *msgbuf;

    /* Remaining number of bytes in this message fragment; 0 if we next expect
     * another fragment header. */
    uint32_t frag_len;

    /* If frag_len is zero: the number of bytes of the fragment header that
     * have been read so far. */
    int frag_hdr_bytes;

    /* Mapping of XID values to outstanding RPC calls. */
    GHashTable *xid_table;
} NFSConnection;


typedef void (*NFSFunc)(NFSConnection *nfs,
                        gpointer user_data, const char *reply, size_t len);

typedef struct {
    NFSFunc callback;
    gpointer user_data;
    int64_t start, end;
} CallInfo;

static GMainLoop *main_loop;

int64_t now_hires()
{
    struct timespec time;

    if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
        perror("clock_gettime");
        return 0;
    }

    return (int64_t)(time.tv_sec) * 1000000000 + time.tv_nsec;
}

static void do_write(NFSConnection *conn, const char *buf, size_t len)
{
    while (len > 0) {
        gsize written = 0;
        switch (g_io_channel_write_chars(conn->channel, buf, len,
                                         &written, NULL)) {
        case G_IO_STATUS_ERROR:
        case G_IO_STATUS_EOF:
        case G_IO_STATUS_AGAIN:
            fprintf(stderr, "Error writing to socket!\n");
            return;
        case G_IO_STATUS_NORMAL:
            len -= written;
            buf += written;
            break;
        }
    }
}

static void send_rpc(NFSConnection *nfs, int proc, GString *msg,
                     NFSFunc completion_handler, gpointer user_data)
{
    static int xid_count = 0;
    struct rpc_call_header header;
    struct rpc_auth auth;

    header.xid = GUINT32_TO_BE(xid_count++);
    header.mtype = 0;
    header.rpcvers = GUINT32_TO_BE(2);
    header.prog = GUINT32_TO_BE(NFS_PROGRAM);
    header.vers = GUINT32_TO_BE(NFS_V3);
    header.proc = GUINT32_TO_BE(proc);

    auth.flavor = GUINT32_TO_BE(AUTH_NULL);
    auth.len = 0;

    CallInfo *info = g_new0(CallInfo, 1);

    uint32_t fragment = htonl(0x80000000
                              | (sizeof(header) + 2*sizeof(auth) + msg->len));
    do_write(nfs, (const char *)&fragment, sizeof(fragment));
    do_write(nfs, (const char *)&header, sizeof(header));
    do_write(nfs, (const char *)&auth, sizeof(auth));
    do_write(nfs, (const char *)&auth, sizeof(auth));
    do_write(nfs, msg->str, msg->len);
    g_io_channel_flush(nfs->channel, NULL);

    info->start = now_hires();
    info->callback = completion_handler;
    info->user_data = user_data;
    g_hash_table_insert(nfs->xid_table,
                        GINT_TO_POINTER(GUINT32_FROM_BE(header.xid)), info);
}

static void process_reply(NFSConnection *nfs, GString *msg)
{
    struct rpc_reply *reply = (struct rpc_reply *)msg->str;

    uint32_t xid = GUINT32_FROM_BE(reply->xid);

    gpointer key = GINT_TO_POINTER(GUINT32_FROM_BE(reply->xid));
    CallInfo *info = g_hash_table_lookup(nfs->xid_table, key);
    if (info == NULL) {
        g_print("Could not match reply XID %d with a call!\n", xid);
        return;
    }

    struct data_point d;
    info->end = now_hires();
    d.timestamp = info->end / 1000000000;
    d.latency = (info->end - info->start + 500) / 1000; /* Round off */
    //printf("XID %d: Time = %"PRIi64"\n", xid, info->end - info->start);
    if (info->callback != NULL)
        info->callback(nfs, info->user_data,
                       msg->str + sizeof(*reply), msg->len - sizeof(*reply));

    g_hash_table_remove(nfs->xid_table, key);
    g_free(info);
    if (logfile != NULL) {
        fwrite(&d, sizeof(d), 1, logfile);
        fflush(logfile);
    }

    completed++;
    if (warmup_mode) {
        printf("Done warming up %d\n", completed);
        int scale = 1;
        if (read_size > (1 << 20))
            scale = read_size / (1 << 20);
        if (completed == bench_files->len * scale)
            g_main_loop_quit(main_loop);
    }
}

static gboolean read_handler(GIOChannel *channel,
                             GIOCondition condition,
                             gpointer data)
{
    NFSConnection *nfs = (NFSConnection *)data;

    gsize bytes_to_read = 0;    /* Number of bytes to attempt to read. */

    /* If we have not yet read in the fragment header, do that first.  This is
     * 4 bytes that indicates the number of bytes in the message to follow
     * (with the high bit set if this is the last fragment making up the
     * message). */
    if (nfs->frag_len == 0) {
        bytes_to_read = 4 - nfs->frag_hdr_bytes;
    } else {
        bytes_to_read = nfs->frag_len & 0x7fffffff;
    }

    if (bytes_to_read > MAX_RPC_MSGSIZE
        || nfs->msgbuf->len + bytes_to_read > MAX_RPC_MSGSIZE)
    {
        fprintf(stderr, "Excessive fragment size for RPC: %zd bytes\n",
                bytes_to_read);
        g_io_channel_shutdown(nfs->channel, TRUE, NULL);
        return FALSE;
    }

    gsize bytes_read = 0;
    g_string_set_size(nfs->msgbuf, nfs->msgbuf->len + bytes_to_read);
    char *buf = &nfs->msgbuf->str[nfs->msgbuf->len - bytes_to_read];
    switch (g_io_channel_read_chars(nfs->channel, buf,
                                    bytes_to_read, &bytes_read, NULL)) {
    case G_IO_STATUS_NORMAL:
        break;
    case G_IO_STATUS_AGAIN:
        return TRUE;
    case G_IO_STATUS_EOF:
        if (bytes_read == bytes_to_read)
            break;
        /* else fall through */
    case G_IO_STATUS_ERROR:
        fprintf(stderr, "Unexpected error or end of file on RPC stream %d!\n",
                g_io_channel_unix_get_fd(nfs->channel));
        g_io_channel_shutdown(nfs->channel, TRUE, NULL);
        /* TODO: Clean up connection object. */
        return FALSE;
    }

    g_assert(bytes_read >= 0 && bytes_read <= bytes_to_read);

    g_string_set_size(nfs->msgbuf,
                      nfs->msgbuf->len - (bytes_to_read - bytes_read));

    if (nfs->frag_len == 0) {
        /* Handle reading in the fragment header.  If we've read the complete
         * header, store the fragment size. */
        nfs->frag_hdr_bytes += bytes_read;
        if (nfs->frag_hdr_bytes == 4) {
            memcpy((char *)&nfs->frag_len,
                   &nfs->msgbuf->str[nfs->msgbuf->len - 4], 4);
            nfs->frag_len = ntohl(nfs->frag_len);
            g_string_set_size(nfs->msgbuf, nfs->msgbuf->len - 4);
            nfs->frag_hdr_bytes = 0;
        }
    } else {
        /* We were reading in the fragment body. */
        nfs->frag_len -= bytes_read;

        if (nfs->frag_len == 0x80000000) {
            process_reply(nfs, nfs->msgbuf);
            nfs->frag_len = 0;
            g_string_set_size(nfs->msgbuf, 0);
        }
    }

    return TRUE;
}

static void send_read_request(NFSConnection *nfs, uint64_t inum,
                               uint64_t offset, uint64_t len);
static void submit_random_read(NFSConnection *nfs)
{
    static int warmup_counter = 0;
    struct bench_file *bf;

    if (warmup_mode) {
        int scale = 1;
        if (read_size > (1 << 20)) {
            scale = read_size / (1 << 20);
        }
        int filecount = bench_files->len;
        printf("Warming up file %d\n", warmup_counter);
        if (warmup_counter >= filecount * scale)
            return;
        bf = &g_array_index(bench_files, struct bench_file,
                            warmup_counter % filecount);
        send_read_request(nfs, bf->inum, (warmup_counter / filecount) << 20,
                          read_size > (1 << 20) ? (1 << 20) : read_size);
        warmup_counter++;
        return;
    }

    bf = &g_array_index(bench_files, struct bench_file,
                        g_random_int_range(0, bench_files->len));
    int blocks = bf->size / read_size;
    if (blocks == 0)
        blocks = 1;

    int offset = g_random_int_range(0, blocks);
    send_read_request(nfs, bf->inum, offset * read_size, read_size);
}

static void finish_read_request(NFSConnection *nfs, gpointer user_data,
                                const char *reply, size_t len)
{
    submit_random_read(nfs);
}

static void send_read_request(NFSConnection *nfs, uint64_t inum,
                               uint64_t offset, uint64_t len)
{
    struct nfs_fh3 fh;
    uint64_t fhdata = GUINT64_TO_BE(inum);
    fh.data.data_val = (char *)&fhdata;
    fh.data.data_len = 8;
    int i;

    char buf[64];
    struct read3args read;
    memcpy(&read.file, &fh, sizeof(struct nfs_fh3));
    read.offset = offset;
    read.count = len;

    GString *str = g_string_new("");
    XDR xdr;
    xdr_string_create(&xdr, str, XDR_ENCODE);
    xdr_read3args(&xdr, &read);
    send_rpc(nfs, NFSPROC3_READ, str, finish_read_request,
             GINT_TO_POINTER((int)inum));
    g_string_free(str, TRUE);
}

static gboolean idle_handler(gpointer data)
{
    NFSConnection *nfs = (NFSConnection *)data;
    int i;

    for (i = 0; i < threads; i++) {
        submit_random_read(nfs);
    }

#if 0
    g_print("Sending requests...\n");
    for (i = 0; i < threads; i++) {
        char buf[64];
        struct diropargs3 lookup;
        uint64_t rootfh = GUINT64_TO_BE(1);

        sprintf(buf, "file-%d", i + 1);
        lookup.dir.data.data_len = 8;
        lookup.dir.data.data_val = (char *)&rootfh;
        lookup.name = buf;

        GString *str = g_string_new("");
        XDR xdr;
        xdr_string_create(&xdr, str, XDR_ENCODE);
        xdr_diropargs3(&xdr, &lookup);
        send_rpc(nfs, NFSPROC3_LOOKUP, str, store_fh, NULL);
        g_string_free(str, TRUE);
    }
#endif

    return FALSE;
}

NFSConnection *nfs_connect(const char *hostname)
{
    int result;
    struct addrinfo hints;
    struct addrinfo *ai = NULL;

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        fprintf(stderr, "Unable to create NFS TCP socket: %m\n");
        exit(1);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    result = getaddrinfo(hostname, "2051", NULL, &ai);
    if (result < 0 || ai == NULL) {
        fprintf(stderr, "Hostname lookup failure for %s: %s\n",
                hostname, gai_strerror(result));
        exit(1);
    }

    if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
        fprintf(stderr, "Unable to connect to %s: %m\n", hostname);
    }

    freeaddrinfo(ai);

    NFSConnection *conn = g_new0(NFSConnection, 1);
    conn->msgbuf = g_string_new("");
    conn->xid_table = g_hash_table_new(g_direct_hash, g_direct_equal);

    conn->channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(conn->channel, NULL, NULL);
    g_io_channel_set_flags(conn->channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(conn->channel, G_IO_IN, read_handler, conn);

    g_idle_add(idle_handler, conn);

    return conn;
}

int main(int argc, char *argv[])
{
    g_thread_init(NULL);
    g_set_prgname("synclient");

    bench_files = g_array_new(FALSE, TRUE, sizeof(struct bench_file));

    FILE *inodes = fopen(argv[1], "r");
    if (inodes == NULL) {
        perror("fopen");
        return 1;
    }
    while (!feof(inodes)) {
        int i1 = -1, i2 = -1;
        fscanf(inodes, "%d %d", &i1, &i2);
        if (i1 < 0 || i2 < 0)
            continue;

        struct bench_file bf;
        bf.inum = i1;
        bf.size = i2;
        g_array_append_val(bench_files, bf);
    }
    fclose(inodes);

    threads = 8;
    if (argc > 2)
        threads = atoi(argv[2]);
    if (argc > 3)
        read_size = atoi(argv[3]);
    if (argc > 4) {
        if (strcmp(argv[4], "WARMUP") == 0) {
            warmup_mode = 1;
        } else {
            logfile = fopen(argv[4], "wb");
        }
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    nfs_connect("vrable2.sysnet.ucsd.edu");

    g_main_loop_run(main_loop);

    return 0;
}
