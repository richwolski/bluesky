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

/* RPC handling: registration, marshalling and unmarshalling of messages.  For
 * now this uses the standard Sun RPC mechanisms in the standard C library.
 * Later, it might be changed to use something better.  Much of this code was
 * generated with rpcgen from the XDR specifications, but has been hand-edited
 * slightly. */

#include "mount_prot.h"
#include "nfs3_prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "bluesky.h"
extern BlueSkyFS *fs;

static int outstanding_rpcs = 0;
static struct bluesky_stats *rpc_recv_stats, *rpc_send_stats;

/* TCP port number to use for NFS protocol.  (Should be 2049.) */
/* #define NFS_SERVICE_PORT 2051 */
#define NFS_SERVICE_PORT 2051

/* Maximum size of a single RPC message that we will accept (8 MB). */
#define MAX_RPC_MSGSIZE (8 << 20)

static void
mount_program_3(struct svc_req *rqstp, register SVCXPRT *transp)
{
    union {
        dirpath mountproc3_mnt_3_arg;
        dirpath mountproc3_umnt_3_arg;
    } argument;
    char *result;
    xdrproc_t _xdr_argument, _xdr_result;
    char *(*local)(char *, struct svc_req *);

    switch (rqstp->rq_proc) {
    case MOUNTPROC3_NULL:
        _xdr_argument = (xdrproc_t) xdr_void;
        _xdr_result = (xdrproc_t) xdr_void;
        local = (char *(*)(char *, struct svc_req *)) mountproc3_null_3_svc;
        break;

    case MOUNTPROC3_MNT:
        _xdr_argument = (xdrproc_t) xdr_dirpath;
        _xdr_result = (xdrproc_t) xdr_mountres3;
        local = (char *(*)(char *, struct svc_req *)) mountproc3_mnt_3_svc;
        break;

    case MOUNTPROC3_DUMP:
        _xdr_argument = (xdrproc_t) xdr_void;
        _xdr_result = (xdrproc_t) xdr_mountlist;
        local = (char *(*)(char *, struct svc_req *)) mountproc3_dump_3_svc;
        break;

    case MOUNTPROC3_UMNT:
        _xdr_argument = (xdrproc_t) xdr_dirpath;
        _xdr_result = (xdrproc_t) xdr_void;
        local = (char *(*)(char *, struct svc_req *)) mountproc3_umnt_3_svc;
        break;

    case MOUNTPROC3_UMNTALL:
        _xdr_argument = (xdrproc_t) xdr_void;
        _xdr_result = (xdrproc_t) xdr_void;
        local = (char *(*)(char *, struct svc_req *)) mountproc3_umntall_3_svc;
        break;

    case MOUNTPROC3_EXPORT:
        _xdr_argument = (xdrproc_t) xdr_void;
        _xdr_result = (xdrproc_t) xdr_exports;
        local = (char *(*)(char *, struct svc_req *)) mountproc3_export_3_svc;
        break;

    default:
        svcerr_noproc (transp);
        return;
    }
    memset ((char *)&argument, 0, sizeof (argument));
    if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
        svcerr_decode (transp);
        return;
    }
    result = (*local)((char *)&argument, rqstp);
    if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
        svcerr_systemerr (transp);
    }
    if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
        fprintf (stderr, "%s", "unable to free arguments");
        exit (1);
    }
    return;
}

struct rpc_reply {
    uint32_t xid;
    uint32_t type;
    uint32_t stat;
    uint32_t verf_flavor;
    uint32_t verf_len;
    uint32_t accept_stat;
};

static void async_rpc_write(RPCConnection *rpc,
                            const char *buf, gsize len);
static void async_rpc_flush(RPCConnection *rpc);

struct rpc_fail_reply {
    uint32_t xid;
    uint32_t type;
    uint32_t stat;
    uint32_t verf_flavor;
    uint32_t verf_len;
    uint32_t accept_stat;
};

static void
async_rpc_send_failure(RPCRequest *req, enum accept_stat stat)
{
    struct rpc_fail_reply header;

    g_atomic_int_add(&outstanding_rpcs, -1);

    header.xid = htonl(req->xid);
    header.type = htonl(1);     /* REPLY */
    header.stat = htonl(MSG_ACCEPTED);
    header.verf_flavor = 0;
    header.verf_len = 0;
    header.accept_stat = htonl(stat);

    g_mutex_lock(req->connection->send_lock);
    uint32_t fragment = htonl(sizeof(header) | 0x80000000);
    if (!req->connection->udp_transport)
        async_rpc_write(req->connection, (const char *)&fragment,
                        sizeof(fragment));
    async_rpc_write(req->connection, (const char *)&header, sizeof(header));
    async_rpc_flush(req->connection);
    g_mutex_unlock(req->connection->send_lock);

    bluesky_profile_free(req->profile);

    if (req->args != NULL) {
        char buf[4];
        XDR xdr;
        xdrmem_create(&xdr, buf, sizeof(buf), XDR_FREE);
        if (!req->xdr_args_free(&xdr, req->args)) {
            fprintf(stderr, "unable to free arguments");
        }
        g_free(req->args);
    }

    if (req->raw_args != NULL)
        g_string_free(req->raw_args, TRUE);

    while (req->cleanup != NULL) {
        struct cleanup_list *c = req->cleanup;
        req->cleanup = c->next;
        c->func(c->arg);
        g_free(c);
    }

    if (req->connection->udp_transport) {
        /* For UDP, a connection only exists for the duration of a single
         * message. */
        g_mutex_free(req->connection->send_lock);
        g_string_free(req->connection->msgbuf, TRUE);
        g_string_free(req->connection->sendbuf, TRUE);
        g_free(req->connection);
    }

    g_free(req);
}

void
async_rpc_send_reply(RPCRequest *req, void *result)
{
    bluesky_time_hires time_end;

    bluesky_profile_add_event(req->profile,
                              g_strdup("Start encoding NFS response"));

    GString *str = g_string_new("");
    XDR xdr_out;
    xdr_string_create(&xdr_out, str, XDR_ENCODE);
    if (!req->xdr_result(&xdr_out, result)) {
        async_rpc_send_failure(req, SYSTEM_ERR);
        g_string_free(str, TRUE);
        return;
    }

    g_atomic_int_add(&outstanding_rpcs, -1);
    bluesky_stats_add(rpc_send_stats, str->len);

    struct rpc_reply header;
    header.xid = htonl(req->xid);
    header.type = htonl(1);     /* REPLY */
    header.stat = htonl(MSG_ACCEPTED);
    header.verf_flavor = 0;
    header.verf_len = 0;
    header.accept_stat = 0;

    g_mutex_lock(req->connection->send_lock);
    gsize msg_size = str->len;
    uint32_t fragment = htonl((msg_size + sizeof(header)) | 0x80000000);
    if (!req->connection->udp_transport)
        async_rpc_write(req->connection, (const char *)&fragment,
                        sizeof(fragment));
    async_rpc_write(req->connection, (const char *)&header, sizeof(header));
    async_rpc_write(req->connection, str->str, str->len);
    async_rpc_flush(req->connection);
    g_mutex_unlock(req->connection->send_lock);

    time_end = bluesky_now_hires();

#if 0
    printf("RPC[%"PRIx32"]: time = %"PRId64" ns\n",
           req->xid, time_end - req->time_start);
#endif

    bluesky_profile_add_event(req->profile,
                              g_strdup("NFS reply sent"));
    bluesky_profile_print(req->profile);

    /* Clean up. */
    bluesky_profile_free(req->profile);
    g_string_free(str, TRUE);

    if (req->args != NULL) {
        char buf[4];
        XDR xdr;
        xdrmem_create(&xdr, buf, sizeof(buf), XDR_FREE);
        if (!req->xdr_args_free(&xdr, req->args)) {
            fprintf(stderr, "unable to free arguments");
        }
        g_free(req->args);
    }

    if (req->raw_args != NULL)
        g_string_free(req->raw_args, TRUE);

    while (req->cleanup != NULL) {
        struct cleanup_list *c = req->cleanup;
        req->cleanup = c->next;
        c->func(c->arg);
        g_free(c);
    }

    if (req->connection->udp_transport) {
        /* For UDP, a connection only exists for the duration of a single
         * message. */
        g_mutex_free(req->connection->send_lock);
        g_string_free(req->connection->msgbuf, TRUE);
        g_string_free(req->connection->sendbuf, TRUE);
        g_free(req->connection);
    }

    g_free(req);
}

static const char *nfs_proc_names[] = {
    [NFSPROC3_NULL] = "NULL",
    [NFSPROC3_GETATTR] = "GETATTR",
    [NFSPROC3_SETATTR] = "SETATTR",
    [NFSPROC3_LOOKUP] = "LOOKUP",
    [NFSPROC3_ACCESS] = "ACCESS",
    [NFSPROC3_READLINK] = "READLINK",
    [NFSPROC3_READ] = "READ",
    [NFSPROC3_WRITE] = "WRITE",
    [NFSPROC3_CREATE] = "CREATE",
    [NFSPROC3_MKDIR] = "MKDIR",
    [NFSPROC3_SYMLINK] = "SYMLINK",
    [NFSPROC3_MKNOD] = "MKNOD",
    [NFSPROC3_REMOVE] = "REMOVE",
    [NFSPROC3_RMDIR] = "RMDIR",
    [NFSPROC3_RENAME] = "RENAME",
    [NFSPROC3_LINK] = "LINK",
    [NFSPROC3_READDIR] = "READDIR",
    [NFSPROC3_READDIRPLUS] = "READDIRPLUS",
    [NFSPROC3_FSSTAT] = "FSSTAT",
    [NFSPROC3_FSINFO] = "FSINFO",
    [NFSPROC3_PATHCONF] = "PATHCONF",
    [NFSPROC3_COMMIT] = "COMMIT",
};

static void
nfs_program_3(RPCRequest *req)
{
    RPCConnection *connection = req->connection;
    uint32_t xid = req->xid;
    const char *msg_buf = req->raw_args->str + req->raw_args_header_bytes;
    size_t msg_len = req->raw_args->len - req->raw_args_header_bytes;

    union argtype {
        nfs_fh3 nfsproc3_getattr_3_arg;
        setattr3args nfsproc3_setattr_3_arg;
        diropargs3 nfsproc3_lookup_3_arg;
        access3args nfsproc3_access_3_arg;
        nfs_fh3 nfsproc3_readlink_3_arg;
        read3args nfsproc3_read_3_arg;
        write3args nfsproc3_write_3_arg;
        create3args nfsproc3_create_3_arg;
        mkdir3args nfsproc3_mkdir_3_arg;
        symlink3args nfsproc3_symlink_3_arg;
        mknod3args nfsproc3_mknod_3_arg;
        diropargs3 nfsproc3_remove_3_arg;
        diropargs3 nfsproc3_rmdir_3_arg;
        rename3args nfsproc3_rename_3_arg;
        link3args nfsproc3_link_3_arg;
        readdir3args nfsproc3_readdir_3_arg;
        readdirplus3args nfsproc3_readdirplus_3_arg;
        nfs_fh3 nfsproc3_fsstat_3_arg;
        nfs_fh3 nfsproc3_fsinfo_3_arg;
        nfs_fh3 nfsproc3_pathconf_3_arg;
        commit3args nfsproc3_commit_3_arg;
    };
    char *result;
    xdrproc_t _xdr_argument, _xdr_result;
    char *(*local)(char *, RPCRequest *);

    bluesky_profile_set(req->profile);

    if (req->req_proc < sizeof(nfs_proc_names) / sizeof(const char *)) {
        bluesky_profile_add_event(
            req->profile,
            g_strdup_printf("Dispatching NFS %s request",
                            nfs_proc_names[req->req_proc])
        );
    }

    switch (req->req_proc) {
    case NFSPROC3_NULL:
        _xdr_argument = (xdrproc_t) xdr_void;
        _xdr_result = (xdrproc_t) xdr_void;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_null_3_svc;
        break;

    case NFSPROC3_GETATTR:
        _xdr_argument = (xdrproc_t) xdr_nfs_fh3;
        _xdr_result = (xdrproc_t) xdr_getattr3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_getattr_3_svc;
        break;

    case NFSPROC3_SETATTR:
        _xdr_argument = (xdrproc_t) xdr_setattr3args;
        _xdr_result = (xdrproc_t) xdr_wccstat3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_setattr_3_svc;
        break;

    case NFSPROC3_LOOKUP:
        _xdr_argument = (xdrproc_t) xdr_diropargs3;
        _xdr_result = (xdrproc_t) xdr_lookup3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_lookup_3_svc;
        break;

    case NFSPROC3_ACCESS:
        _xdr_argument = (xdrproc_t) xdr_access3args;
        _xdr_result = (xdrproc_t) xdr_access3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_access_3_svc;
        break;

    case NFSPROC3_READLINK:
        _xdr_argument = (xdrproc_t) xdr_nfs_fh3;
        _xdr_result = (xdrproc_t) xdr_readlink3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_readlink_3_svc;
        break;

    case NFSPROC3_READ:
        _xdr_argument = (xdrproc_t) xdr_read3args;
        _xdr_result = (xdrproc_t) xdr_read3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_read_3_svc;
        break;

    case NFSPROC3_WRITE:
        _xdr_argument = (xdrproc_t) xdr_write3args;
        _xdr_result = (xdrproc_t) xdr_write3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_write_3_svc;
        break;

    case NFSPROC3_CREATE:
        _xdr_argument = (xdrproc_t) xdr_create3args;
        _xdr_result = (xdrproc_t) xdr_diropres3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_create_3_svc;
        break;

    case NFSPROC3_MKDIR:
        _xdr_argument = (xdrproc_t) xdr_mkdir3args;
        _xdr_result = (xdrproc_t) xdr_diropres3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_mkdir_3_svc;
        break;

    case NFSPROC3_SYMLINK:
        _xdr_argument = (xdrproc_t) xdr_symlink3args;
        _xdr_result = (xdrproc_t) xdr_diropres3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_symlink_3_svc;
        break;

    case NFSPROC3_MKNOD:
        _xdr_argument = (xdrproc_t) xdr_mknod3args;
        _xdr_result = (xdrproc_t) xdr_diropres3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_mknod_3_svc;
        break;

    case NFSPROC3_REMOVE:
        _xdr_argument = (xdrproc_t) xdr_diropargs3;
        _xdr_result = (xdrproc_t) xdr_wccstat3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_remove_3_svc;
        break;

    case NFSPROC3_RMDIR:
        _xdr_argument = (xdrproc_t) xdr_diropargs3;
        _xdr_result = (xdrproc_t) xdr_wccstat3;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_rmdir_3_svc;
        break;

    case NFSPROC3_RENAME:
        _xdr_argument = (xdrproc_t) xdr_rename3args;
        _xdr_result = (xdrproc_t) xdr_rename3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_rename_3_svc;
        break;

    case NFSPROC3_LINK:
        _xdr_argument = (xdrproc_t) xdr_link3args;
        _xdr_result = (xdrproc_t) xdr_link3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_link_3_svc;
        break;

    case NFSPROC3_READDIR:
        _xdr_argument = (xdrproc_t) xdr_readdir3args;
        _xdr_result = (xdrproc_t) xdr_readdir3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_readdir_3_svc;
        break;

    case NFSPROC3_READDIRPLUS:
        _xdr_argument = (xdrproc_t) xdr_readdirplus3args;
        _xdr_result = (xdrproc_t) xdr_readdirplus3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_readdirplus_3_svc;
        break;

    case NFSPROC3_FSSTAT:
        _xdr_argument = (xdrproc_t) xdr_nfs_fh3;
        _xdr_result = (xdrproc_t) xdr_fsstat3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_fsstat_3_svc;
        break;

    case NFSPROC3_FSINFO:
        _xdr_argument = (xdrproc_t) xdr_nfs_fh3;
        _xdr_result = (xdrproc_t) xdr_fsinfo3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_fsinfo_3_svc;
        break;

    case NFSPROC3_PATHCONF:
        _xdr_argument = (xdrproc_t) xdr_nfs_fh3;
        _xdr_result = (xdrproc_t) xdr_pathconf3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_pathconf_3_svc;
        break;

    case NFSPROC3_COMMIT:
        _xdr_argument = (xdrproc_t) xdr_commit3args;
        _xdr_result = (xdrproc_t) xdr_commit3res;
        local = (char *(*)(char *, RPCRequest *)) nfsproc3_commit_3_svc;
        break;

    default:
        async_rpc_send_failure(req, PROC_UNAVAIL);
        return;
    }

    /* Decode incoming message */
    req->xdr_args_free = _xdr_argument;
    req->args = g_new0(union argtype, 1);
    XDR xdr_in;
    xdrmem_create(&xdr_in, (char *)msg_buf, msg_len, XDR_DECODE);
    if (!_xdr_argument(&xdr_in, req->args)) {
        async_rpc_send_failure(req, GARBAGE_ARGS);
        fprintf(stderr, "RPC decode error!\n");
        return;
    }

    /* Perform the call. */
    req->xdr_result = _xdr_result;
    result = (*local)((char *)req->args, req);

    return;
}

/* Enhanced, asynchronous-friendly RPC layer.  This is a replacement for the
 * built-in sunrpc parsing and dispatch that will allow for processing multiple
 * requests at the same time. */
static GMainContext *main_context;
static GMainLoop *main_loop;

static GThreadPool *rpc_thread_pool;

static volatile int fs_dump_requested = 0;

static void sig_handler(int sig)
{
    if (sig == SIGUSR1) {
        fs_dump_requested = 1;
    }
}

static gboolean async_flushd(gpointer data)
{
#if 0
    int rpc_count = g_atomic_int_get(&outstanding_rpcs);
    if (rpc_count != 0) {
        g_print("Currently outstanding RPC requests: %d\n", rpc_count);
    }
#endif

    if (fs_dump_requested) {
        bluesky_debug_dump(fs);
        bluesky_stats_dump_all();
        fs_dump_requested = 0;
    }

    bluesky_flushd_invoke(fs);
    return TRUE;
}

static void async_rpc_task(gpointer data, gpointer user_data)
{
    nfs_program_3((RPCRequest *)data);
}

static async_rpc_init()
{
    main_context = g_main_context_new();
    main_loop = g_main_loop_new(main_context, FALSE);

    rpc_thread_pool = g_thread_pool_new(async_rpc_task, NULL,
                                        bluesky_max_threads, FALSE, NULL);

    /* Arrange to have the cache writeback code run every five seconds. */
    GSource *source = g_timeout_source_new_seconds(5);
    g_source_set_callback(source, async_flushd, NULL, NULL);
    g_source_attach(source, main_context);
    g_source_unref(source);

    /* Signal USR1 is used to request a debugging dump of filesyste info */
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
    }
}

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

/* Decode an RPC message and process it.  Returns a boolean indicating whether
 * the message could be processed; if false, an unrecoverable error occurred
 * and the transport should be closed. */
static gboolean async_rpc_dispatch(RPCConnection *rpc)
{
    bluesky_time_hires time_start = bluesky_now_hires();
    int i;
    GString *msg = rpc->msgbuf;
    const char *buf = msg->str;

    bluesky_stats_add(rpc_recv_stats, msg->len);

    if (msg->len < sizeof(struct rpc_call_header)) {
        fprintf(stderr, "Short RPC message: only %zd bytes!\n", msg->len);
        return FALSE;
    }

    struct rpc_call_header *header = (struct rpc_call_header *)(msg->str);
    uint32_t xid = ntohl(header->xid);

    if (ntohl(header->mtype) != 0) {
        /* Not an RPC call */
        return FALSE;
    }

    if (ntohl(header->rpcvers) != 2) {
        return FALSE;
    }

    g_atomic_int_add(&outstanding_rpcs, 1);

    RPCRequest *req = g_new0(RPCRequest, 1);
    req->connection = rpc;
    req->profile = bluesky_profile_new();
    bluesky_profile_add_event(req->profile, g_strdup("Receive NFS request"));
    req->xid = xid;

    if (ntohl(header->prog) != NFS_PROGRAM) {
        async_rpc_send_failure(req, PROG_UNAVAIL);
        return TRUE;
    } else if (ntohl(header->vers) != NFS_V3) {
        /* FIXME: Should be PROG_MISMATCH */
        async_rpc_send_failure(req, PROG_UNAVAIL);
        return TRUE;
    }

    uint32_t proc = ntohl(header->proc);

    /* Next, skip over authentication headers. */
    buf += sizeof(struct rpc_call_header);
    for (i = 0; i < 2; i++) {
        struct rpc_auth *auth = (struct rpc_auth *)buf;
        if (buf - msg->str + sizeof(struct rpc_auth) > msg->len) {
            g_atomic_int_add(&outstanding_rpcs, -1);
            return FALSE;
        }

        gsize authsize = ntohl(auth->len) + sizeof(struct rpc_auth);
        if (authsize > MAX_RPC_MSGSIZE) {
            g_atomic_int_add(&outstanding_rpcs, -1);
            return FALSE;
        }

        buf += authsize;
    }

    if (buf - msg->str > msg->len) {
        g_atomic_int_add(&outstanding_rpcs, -1);
        return FALSE;
    }

    req->raw_args = msg;
    req->raw_args_header_bytes = buf - msg->str;
    req->req_proc = ntohl(header->proc);
    rpc->msgbuf = g_string_new("");

    if (bluesky_options.sync_frontends) {
        nfs_program_3(req);
    } else {
        g_thread_pool_push(rpc_thread_pool, req, NULL);
    }

    return TRUE;
}

/* Write the given data to the RPC socket. */
static void async_rpc_write(RPCConnection *rpc,
                            const char *buf, gsize len)
{
    if (rpc->udp_transport) {
        g_string_append_len(rpc->sendbuf, buf, len);
        return;
    }

    /* Normal TCP path */
    while (len > 0) {
        gsize written = 0;
        switch (g_io_channel_write_chars(rpc->channel, buf, len,
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

    // g_io_channel_flush(rpc->channel, NULL);
}

/* Flush a completed message out to the RPC socket */
static void async_rpc_flush(RPCConnection *rpc)
{
    if (rpc->udp_transport) {
        sendto(g_io_channel_unix_get_fd(rpc->channel),
               rpc->sendbuf->str, rpc->sendbuf->len, 0,
               (struct sockaddr *)&rpc->peer, sizeof(struct sockaddr_in));
        return;
    } else {
        g_io_channel_flush(rpc->channel, NULL);
    }
}

static gboolean async_rpc_do_read(GIOChannel *channel,
                                  GIOCondition condition,
                                  gpointer data)
{
    RPCConnection *rpc = (RPCConnection *)data;

    gsize bytes_to_read = 0;    /* Number of bytes to attempt to read. */

    /* If we have not yet read in the fragment header, do that first.  This is
     * 4 bytes that indicates the number of bytes in the message to follow
     * (with the high bit set if this is the last fragment making up the
     * message). */
    if (rpc->frag_len == 0) {
        bytes_to_read = 4 - rpc->frag_hdr_bytes;
    } else {
        bytes_to_read = rpc->frag_len & 0x7fffffff;
    }

    if (bytes_to_read > MAX_RPC_MSGSIZE
        || rpc->msgbuf->len + bytes_to_read > MAX_RPC_MSGSIZE)
    {
        fprintf(stderr, "Excessive fragment size for RPC: %zd bytes\n",
                bytes_to_read);
        g_io_channel_shutdown(rpc->channel, TRUE, NULL);
        return FALSE;
    }

    gsize bytes_read = 0;
    g_string_set_size(rpc->msgbuf, rpc->msgbuf->len + bytes_to_read);
    char *buf = &rpc->msgbuf->str[rpc->msgbuf->len - bytes_to_read];
    switch (g_io_channel_read_chars(rpc->channel, buf,
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
                g_io_channel_unix_get_fd(rpc->channel));
        g_io_channel_shutdown(rpc->channel, TRUE, NULL);
        /* TODO: Clean up connection object. */
        return FALSE;
    }

    g_assert(bytes_read >= 0 && bytes_read <= bytes_to_read);

    g_string_set_size(rpc->msgbuf,
                      rpc->msgbuf->len - (bytes_to_read - bytes_read));

    if (rpc->frag_len == 0) {
        /* Handle reading in the fragment header.  If we've read the complete
         * header, store the fragment size. */
        rpc->frag_hdr_bytes += bytes_read;
        if (rpc->frag_hdr_bytes == 4) {
            memcpy((char *)&rpc->frag_len,
                   &rpc->msgbuf->str[rpc->msgbuf->len - 4], 4);
            rpc->frag_len = ntohl(rpc->frag_len);
            g_string_set_size(rpc->msgbuf, rpc->msgbuf->len - 4);
            rpc->frag_hdr_bytes = 0;
        }
    } else {
        /* We were reading in the fragment body. */
        rpc->frag_len -= bytes_read;

        if (rpc->frag_len == 0x80000000) {
            /* We have a complete message since this was the last fragment and
             * there are no more bytes in it.  Dispatch the message. */
            if (!async_rpc_dispatch(rpc)) {
                fprintf(stderr, "Invalid RPC message, closing channel\n");
                g_io_channel_shutdown(rpc->channel, TRUE, NULL);
                return FALSE;
            }
            rpc->frag_len = 0;
            g_string_set_size(rpc->msgbuf, 0);
        }
    }

    return TRUE;
}

static gboolean async_rpc_do_accept(GIOChannel *channel,
                                    GIOCondition condition,
                                    gpointer data)
{
    int fd = g_io_channel_unix_get_fd(channel);
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    g_print("Received new connection on fd %d!\n", fd);
    int nfd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (nfd < 0) {
        fprintf(stderr, "Error accepting connection: %m\n");
        return TRUE;
    }

    RPCConnection *rpc = g_new0(RPCConnection, 1);
    rpc->channel = g_io_channel_unix_new(nfd);
    rpc->msgbuf = g_string_new("");
    g_io_channel_set_encoding(rpc->channel, NULL, NULL);
    rpc->send_lock = g_mutex_new();
    GSource *source = g_io_create_watch(rpc->channel, G_IO_IN);
    g_source_set_callback(source, (GSourceFunc)async_rpc_do_read,
                          rpc, NULL);
    g_source_attach(source, main_context);
    g_source_unref(source);

    return TRUE;
}

static async_rpc_register_listening(int fd)
{
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(channel, NULL, NULL);
    GSource *source = g_io_create_watch(channel, G_IO_IN);
    g_source_set_callback(source, (GSourceFunc)async_rpc_do_accept,
                          NULL, NULL);
    g_source_attach(source, main_context);
    g_source_unref(source);
}

static gboolean async_rpc_do_udp(GIOChannel *channel,
                                 GIOCondition condition,
                                 gpointer data)
{
    char buf[65536];

    struct sockaddr_in src;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    ssize_t len = recvfrom(g_io_channel_unix_get_fd(channel),
                           buf, sizeof(buf), 0,
                           (struct sockaddr *)&src, &addrlen);
    if (len < 0) {
        fprintf(stderr, "UDP read error: %m, shutting down UDP\n");
        return FALSE;
    }

    g_assert(len < sizeof(buf));

    RPCConnection *rpc = g_new0(RPCConnection, 1);
    rpc->channel = channel;
    rpc->msgbuf = g_string_new_len(buf, len);
    rpc->send_lock = g_mutex_new();
    rpc->udp_transport = TRUE;
    memcpy(&rpc->peer, &src, sizeof(struct sockaddr_in));
    rpc->sendbuf = g_string_new("");

    /* We have a complete message since this was the last fragment and
     * there are no more bytes in it.  Dispatch the message. */
    async_rpc_dispatch(rpc);

    return TRUE;
}

static async_rpc_register_listening_udp(int fd)
{
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(channel, NULL, NULL);
    GSource *source = g_io_create_watch(channel, G_IO_IN);
    g_source_set_callback(source, (GSourceFunc)async_rpc_do_udp,
                          NULL, NULL);
    g_source_attach(source, main_context);
    g_source_unref(source);
}

static gpointer async_rpc_run(gpointer data)
{
    g_print("Starting NFS main loop...\n");
    g_main_loop_run(main_loop);
}

void register_rpc()
{
    SVCXPRT *transp;

    rpc_recv_stats = bluesky_stats_new("NFS RPC Messages In");
    rpc_send_stats = bluesky_stats_new("NFS RPC Messages Out");

    async_rpc_init();

    /* MOUNT protocol */
    pmap_unset (MOUNT_PROGRAM, MOUNT_V3);

    transp = svcudp_create(RPC_ANYSOCK);
    if (transp == NULL) {
        fprintf(stderr, "%s", "cannot create udp service.");
        exit(1);
    }
    if (!svc_register(transp, MOUNT_PROGRAM, MOUNT_V3, mount_program_3, IPPROTO_UDP)) {
        fprintf(stderr, "%s", "unable to register (MOUNT_PROGRAM, MOUNT_V3, udp).");
        exit(1);
    }

    transp = svctcp_create(RPC_ANYSOCK, 0, 0);
    if (transp == NULL) {
        fprintf(stderr, "%s", "cannot create tcp service.");
        exit(1);
    }
    if (!svc_register(transp, MOUNT_PROGRAM, MOUNT_V3, mount_program_3, IPPROTO_TCP)) {
        fprintf(stderr, "%s", "unable to register (MOUNT_PROGRAM, MOUNT_V3, tcp).");
        exit(1);
    }

    /* NFS protocol (version 3) */
    pmap_unset (NFS_PROGRAM, NFS_V3);

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        fprintf(stderr, "Unable to create NFS TCP socket: %m\n");
        exit(1);
    }

    int n = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&n, sizeof(n));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NFS_SERVICE_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Unable to bind to NFS TCP address: %m\n");
        exit(1);
    }

    if (listen(fd, SOMAXCONN) < 0) {
        fprintf(stderr, "Unable to listen on NFS TCP socket: %m\n");
        exit(1);
    }

    if (!pmap_set(NFS_PROGRAM, NFS_V3, IPPROTO_TCP, NFS_SERVICE_PORT)) {
        fprintf(stderr, "Could not register NFS RPC service!\n");
        exit(1);
    }

    async_rpc_register_listening(fd);

    /* Minimal UDP NFSv3 support */
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        fprintf(stderr, "Unable to create NFS UDP socket: %m\n");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(NFS_SERVICE_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Unable to bind to NFS UDP address: %m\n");
        exit(1);
    }

    if (!pmap_set(NFS_PROGRAM, NFS_V3, IPPROTO_UDP, NFS_SERVICE_PORT)) {
        fprintf(stderr, "Could not register NFS UDP RPC service!\n");
        exit(1);
    }

    async_rpc_register_listening_udp(fd);

    g_thread_create(async_rpc_run, NULL, TRUE, NULL);
}
