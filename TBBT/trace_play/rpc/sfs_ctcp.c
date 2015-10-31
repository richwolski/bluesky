#ifndef lint
static char sfs_ctcp_id[] = "@(#)sfs_ctcp.c     2.1     97/10/23";
#endif
/* @(#)clnt_tcp.c	2.2 88/08/01 4.0 RPCSRC */
/*
 *   Copyright (c) 1992-1997,2001 by Standard Performance Evaluation Corporation
 *	All rights reserved.
 *		Standard Performance Evaluation Corporation (SPEC)
 *		6585 Merchant Place, Suite 100
 *		Warrenton, VA 20187
 *
 *	This product contains benchmarks acquired from several sources who
 *	understand and agree with SPEC's goal of creating fair and objective
 *	benchmarks to measure computer performance.
 *
 *	This copyright notice is placed here only to protect SPEC in the
 *	event the source is misused in any manner that is contrary to the
 *	spirit, the goals and the intent of SPEC.
 *
 *	The source code is provided to the user or company under the license
 *	agreement for the SPEC Benchmark Suite for this product.
 */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_tcp.c 1.37 87/10/05 Copyr 1984 Sun Micro";
#endif
 
/*
 * clnt_tcp.c, Implements a TCP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * TCP based RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer.  The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent.  The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that is,
 * the server side should be aware that a call is batched and not produce any
 * return message.  Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 *
 * Now go hang yourself.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "rpc/rpc.h"
#include "rpc/osdep.h"
#include <netdb.h>
#include <errno.h>
#include "rpc/pmap_clnt.h"

#define MCALL_MSG_SIZE 24

struct ct_data {
	int		ct_sock;
	bool_t		ct_closeit;
	struct timeval	ct_wait;
	struct sockaddr_in ct_addr; 
	struct sockaddr_in ct_oaddr; 
	struct rpc_err	ct_error;
	char		ct_mcall[MCALL_MSG_SIZE];	/* marshalled callmsg */
	uint_t		ct_mpos;			/* pos after marshal */
	XDR		ct_xdrs;
	int		ct_first;
	uint32_t	ct_prog;
	uint32_t	ct_vers;
	uint_t		ct_sendsz;
	uint_t		ct_recvsz;
};

static int	readtcp(struct ct_data *, char *, int);
static int	writetcp(struct ct_data *ct, char * buf, int len);

static enum clnt_stat	sfs_ctcp_call(CLIENT *, uint32_t, xdrproc_t, void *,
					xdrproc_t, void *, struct timeval);
static void		sfs_ctcp_abort(CLIENT *);
static void		sfs_ctcp_geterr(CLIENT *, struct rpc_err *);
static bool_t		sfs_ctcp_freeres(CLIENT *, xdrproc_t, void *);
static bool_t		sfs_ctcp_control(CLIENT *, uint_t, void *);
static void		sfs_ctcp_destroy(CLIENT *);
static bool_t		sfs_ctcp_getreply(CLIENT *, xdrproc_t, void *,
				int, uint32_t *, uint32_t *, struct timeval *);
static int		sfs_ctcp_poll(CLIENT *, uint32_t);

static struct clnt_ops tcp_ops = {
	sfs_ctcp_call,
	sfs_ctcp_abort,
	sfs_ctcp_geterr,
	sfs_ctcp_freeres,
	sfs_ctcp_destroy,
	sfs_ctcp_control,
	sfs_ctcp_getreply,
	sfs_ctcp_poll
};

static int
sfs_ctcp_make_conn(
	struct ct_data *ct,
	struct sockaddr_in *raddr,
	int *sockp)
{
	int i;
        int min_buf_sz;
        int new_buf_sz;
        int type;
        int error;
	int setopt = 1;
#if defined(UNIXWARE) || defined(AIX)
        size_t optlen;
#else
        int optlen;
#endif

	if (ct->ct_first == 0)
		ct->ct_first++;

#ifdef DEBUG
	if (ct->ct_first)
		(void) fprintf(stderr, "Re-establishing connection.\n");
#endif

	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr->sin_port == 0) {
		uint16_t port;
		if ((port = pmap_getport(raddr, ct->ct_prog, ct->ct_vers,
							IPPROTO_TCP)) == 0) {
			return (-1);
		}
		raddr->sin_port = htons(port);
	}

	/*
	 * If no socket given, open one
	 */
	if (*sockp >= 0) {
		ct->ct_closeit = FALSE;
		return (*sockp);
	}

	ct->ct_closeit = TRUE;

	*sockp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	(void)bindresvport(*sockp, (struct sockaddr_in *)0);
	if ((*sockp < 0)
	    || (connect(*sockp, (struct sockaddr *)raddr,
	    sizeof(struct sockaddr_in)) < 0)) {
		return (-1);
	}

        /*
         * Need to try to size the socket buffers based on the number of
         * outstanding requests desired.  NFS reads and writes can do as
         * much as 8K per request which can quickly run us out of space
         * on the socket buffer queue.  Use the maximum number of bio style
         * requests * NFS_MAXDATA plus a pad as a starting point for desired
         * socket buffer size and then back off by NFS_MAXDATA until the buffer
         * sizes are successfully set.  Note, the algorithm never sets the
         * buffer size to less than the OS default.
         */
        type = SO_SNDBUF;
        for (i = 0; i < 2; i++) {
                optlen = sizeof(min_buf_sz);
#ifdef UNIXWARE
                if (getsockopt(*sockp, SOL_SOCKET, type,
                                (void *)&min_buf_sz, &optlen) < 0) {
                        /* guess the default */
                        min_buf_sz = 18 * 1024;
                }
#else
                if (getsockopt(*sockp, SOL_SOCKET, type,
                                (char *)&min_buf_sz, &optlen) < 0) {
                        /* guess the default */
                        min_buf_sz = 18 * 1024;
                }
#endif

                new_buf_sz = 512 * 1024;
                if (new_buf_sz > min_buf_sz) {
                        do {
                                error = setsockopt(*sockp, SOL_SOCKET,
                                                type, (char *)&new_buf_sz,
                                                sizeof(int));
                                new_buf_sz -= (8 * 1024);
                        } while (error != 0 && new_buf_sz > min_buf_sz);
                }

                type = SO_RCVBUF;
        }

#ifdef TCP_NODELAY
	setsockopt(*sockp, IPPROTO_TCP, TCP_NODELAY, (char *) &setopt,
		sizeof(setopt));
#endif /* TCP_NODELAY */

	return (*sockp);
}

/*
 * Create a client handle for a tcp/ip connection.
 * If *sockp<0, *sockp is set to a newly created TCP socket and it is
 * connected to raddr.  If *sockp non-negative then
 * raddr is ignored.  The rpc/tcp package does buffering
 * similar to stdio, so the client must pick send and receive buffer sizes,];
 * 0 => use the default.
 * If raddr->sin_port is 0, then a binder on the remote machine is
 * consulted for the right port number.
 * NB: *sockp is copied into a private area.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is set null authentication.  Caller may wish to set this
 * something more useful.
 */
CLIENT *
sfs_ctcp_create(
	struct sockaddr_in *raddr,
	uint32_t prog,
	uint32_t vers,
	int *sockp,
	uint_t sendsz,
	uint_t recvsz)
{
	CLIENT *h;
	struct ct_data *ct;
	struct timeval now;
	struct rpc_msg call_msg;

	h  = (CLIENT *)mem_alloc(sizeof(CLIENT));
	if (h == NULL) {
		(void)fprintf(stderr, "clnttcp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	ct = (struct ct_data *)mem_alloc(sizeof(struct ct_data));
	if (ct == NULL) {
		(void)fprintf(stderr, "clnttcp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	(void) memset(ct, '\0', sizeof (struct ct_data));

	ct->ct_oaddr = *raddr;
	ct->ct_prog = prog;
	ct->ct_vers = vers;

	if (sfs_ctcp_make_conn(ct, raddr, sockp) < 0) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		(void)close(*sockp);
		goto fooy;
	}

	/*
	 * Set up private data struct
	 */
	ct->ct_sock = *sockp;
	ct->ct_wait.tv_sec = 0;
	ct->ct_wait.tv_usec = 0;
	ct->ct_addr = *raddr;

	/*
	 * Initialize call message
	 */
	(void)gettimeofday(&now, (struct timezone *)0);
	call_msg.rm_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;

	/*
	 * pre-serialize the static part of the call msg and stash it away
	 */
	xdrmem_create(&(ct->ct_xdrs), ct->ct_mcall, MCALL_MSG_SIZE,
	    XDR_ENCODE);
	if (! xdr_callhdr(&(ct->ct_xdrs), &call_msg)) {
		if (ct->ct_closeit) {
			(void)close(*sockp);
		}
		goto fooy;
	}
	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	/*
	 * Create a client handle which uses xdrrec for serialization
	 * and authnone for authentication.
	 */
	xdrrec_create(&(ct->ct_xdrs), sendsz, recvsz,
	    (void *)ct, readtcp, writetcp);
	ct->ct_sendsz = sendsz;
	ct->ct_recvsz = recvsz;
	h->cl_ops = &tcp_ops;
	h->cl_private = (void *) ct;
	h->cl_auth = authnone_create();
	return (h);

fooy:
	/*
	 * Something goofed, free stuff and barf
	 */
	mem_free((void *)ct, sizeof(struct ct_data));
	mem_free((void *)h, sizeof(CLIENT));
	return ((CLIENT *)NULL);
}

static enum clnt_stat
get_areply(
	CLIENT *h,
	struct rpc_msg *reply_msg)
{
	struct ct_data *ct = (struct ct_data *) h->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);

	/* CONSTCOND */
	while (TRUE) {
		reply_msg->acpted_rply.ar_verf = _null_auth;
		reply_msg->acpted_rply.ar_results.where = NULL;
		reply_msg->acpted_rply.ar_results.proc = xdr_void;
		if (! xdrrec_skiprecord(xdrs)) {
			return (ct->ct_error.re_status);
		}
		/* now decode and validate the response header */
		if (! xdr_replymsg(xdrs, reply_msg)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				continue;
                }
		return (ct->ct_error.re_status);
	}
}

static enum clnt_stat
proc_header(
	CLIENT *h,
	struct rpc_msg *reply_msg,
	xdrproc_t xdr_results,
	void * results_ptr)
{
	struct ct_data *ct = (struct ct_data *) h->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);

	/*
	 * process header
	 */
	_seterr_reply(reply_msg, &(ct->ct_error));
	if (ct->ct_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth,
					&reply_msg->acpted_rply.ar_verf)) {
			ct->ct_error.re_status = RPC_AUTHERROR;
			ct->ct_error.re_why = AUTH_INVALIDRESP;
		} else if (! (*xdr_results)(xdrs, results_ptr)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				ct->ct_error.re_status = RPC_CANTDECODERES;
		}
		/* free verifier ... */
		if (reply_msg->acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs,
					&(reply_msg->acpted_rply.ar_verf));
		}
	}  /* end successful completion */
	return (ct->ct_error.re_status);
}

static enum clnt_stat
sfs_ctcp_call(
	CLIENT *h,
	uint32_t proc,
	xdrproc_t xdr_args,
	void * args_ptr,
	xdrproc_t xdr_results,
	void * results_ptr,
	struct timeval timeout)
{
	struct ct_data *ct = (struct ct_data *) h->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;
	uint32_t x_id;
	uint32_t *msg_x_id = (uint32_t *)(ct->ct_mcall);	/* yuk */
	bool_t shipnow;

	ct->ct_wait = timeout;

	shipnow =
	    (xdr_results == (xdrproc_t)0 && timeout.tv_sec == 0
	    && timeout.tv_usec == 0) ? FALSE : TRUE;

	xdrs->x_op = XDR_ENCODE;
	ct->ct_error.re_status = RPC_SUCCESS;
	x_id = ntohl(--(*msg_x_id));
	if ((! XDR_PUTBYTES(xdrs, ct->ct_mcall, ct->ct_mpos)) ||
	    (! XDR_PUTLONG(xdrs, (int32_t *)&proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*xdr_args)(xdrs, args_ptr))) {
		if (ct->ct_error.re_status == RPC_SUCCESS)
			ct->ct_error.re_status = RPC_CANTENCODEARGS;
		(void)xdrrec_endofrecord(xdrs, TRUE);
		return (ct->ct_error.re_status);
	}
	if (! xdrrec_endofrecord(xdrs, TRUE))
		return (ct->ct_error.re_status = RPC_CANTSEND);
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		if (! shipnow && results_ptr == NULL)
			return (RPC_SUCCESS);

		/*
		 * Double hack, send back xid in results_prt if non-NULL
		 */
		*(uint32_t *)results_ptr = x_id;
		return(ct->ct_error.re_status = RPC_TIMEDOUT);
	}


	/*
	 * Keep receiving until we get a valid transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	/* CONSTCOND */
	while (TRUE) {
		enum clnt_stat res;

		if ((res = get_areply(h, &reply_msg)) != RPC_SUCCESS)
			return (res);

		if (reply_msg.rm_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
	return (proc_header(h, &reply_msg, xdr_results, results_ptr));
}

static void
sfs_ctcp_geterr(
	CLIENT *h,
	struct rpc_err *errp)
{
	struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	*errp = ct->ct_error;
}

static bool_t
sfs_ctcp_freeres(
	CLIENT *cl,
	xdrproc_t xdr_res,
	void * res_ptr)
{
	struct ct_data *ct = (struct ct_data *)cl->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

/* ARGSUSED */
static void
sfs_ctcp_abort(CLIENT *h)
{
}

static bool_t
sfs_ctcp_control(
	CLIENT *cl,
	uint_t request,
	void *info)
{
	struct ct_data *ct = (struct ct_data *)cl->cl_private;

	switch (request) {
	case CLGET_SERVER_ADDR:
		*(struct sockaddr_in *)info = ct->ct_addr;
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}


static void
sfs_ctcp_destroy(
	CLIENT *h)
{
	struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	if (ct->ct_closeit) {
		(void)close(ct->ct_sock);
	}
	XDR_DESTROY(&(ct->ct_xdrs));
	mem_free((void *)ct, sizeof(struct ct_data));
	mem_free((void *)h, sizeof(CLIENT));
}

/*
 * Interface between xdr serializer and tcp connection.
 * Behaves like the system calls, read & write, but keeps some error state
 * around for the rpc level.
 */
static int
readtcp(struct ct_data *ct,
	char * buf,
	int len)
{
#ifdef FD_SETSIZE
	fd_set mask;
	fd_set readfds;

	FD_ZERO(&mask);
	FD_SET(ct->ct_sock, &mask);
#else
	int mask = 1 << (ct->ct_sock);
	int readfds;
#endif /* def FD_SETSIZE */

	if (len == 0)
		return (0);

	/* CONSTCOND */
	while (TRUE) {
		readfds = mask;
		switch (select(_rpc_dtablesize(), &readfds, NULL, NULL,
			       &(ct->ct_wait))) {
		case 0:
			ct->ct_error.re_status = RPC_TIMEDOUT;
			return (-1);

		case -1:
			if (errno == EINTR)
				continue;
			ct->ct_error.re_status = RPC_CANTRECV;
			ct->ct_error.re_errno = errno;
			goto lost;
		}
		break;
	}
	switch (len = read(ct->ct_sock, buf, len)) {

	case 0:
		/* premature eof */
		ct->ct_error.re_errno = ECONNRESET;
		ct->ct_error.re_status = RPC_CANTRECV;
		len = -1;  /* it's really an error */
		goto lost;

	case -1:
		ct->ct_error.re_errno = errno;
		ct->ct_error.re_status = RPC_CANTRECV;
		goto lost;
	}
	return (len);
lost:
	/*
	 * We have lost our connection to the server.  Try and
	 * reestablish it.
	 */
	(void) close(ct->ct_sock);
	ct->ct_addr = ct->ct_oaddr;
	ct->ct_sock = -1;
	XDR_DESTROY(&(ct->ct_xdrs));

	(void) sfs_ctcp_make_conn(ct, &ct->ct_addr, &ct->ct_sock);
	/*
	 * Create a client handle which uses xdrrec for
	 * serialization and authnone for authentication.
	 */
	xdrrec_create(&(ct->ct_xdrs), ct->ct_sendsz, ct->ct_recvsz,
						(void *)ct, readtcp, writetcp);
	return (-1);
}

static int
writetcp(
	struct ct_data *ct,
	char * buf,
	int len)
{
	int i, cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = write(ct->ct_sock, buf, cnt)) == -1) {
			/*
			 * We have lost our connection to the server.  Try and
			 * reestablish it.
			 */
			(void) close(ct->ct_sock);
			ct->ct_addr = ct->ct_oaddr;
			ct->ct_sock = -1;
			XDR_DESTROY(&(ct->ct_xdrs));

			(void) sfs_ctcp_make_conn(ct, &ct->ct_addr,
								&ct->ct_sock);
			/*
			 * Create a client handle which uses xdrrec for
			 * serialization and authnone for authentication.
			 */
			xdrrec_create(&(ct->ct_xdrs), ct->ct_sendsz,
						ct->ct_recvsz,
						(void *)ct, readtcp, writetcp);
			ct->ct_error.re_errno = errno;
			ct->ct_error.re_status = RPC_CANTSEND;
			return (-1);
		}
	}
	return (len);
}

/* ARGSUSED */
static bool_t
sfs_ctcp_getreply(
        CLIENT *cl,
        xdrproc_t xdr_results,
        void *results_ptr,
	int cnt,
	uint32_t *xids,
        uint32_t *xid,
	struct timeval *tv)
{
	struct ct_data *ct = (struct ct_data *) cl->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;
	enum clnt_stat res;
	int i;

	/*
	 * Receive just one returning transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	ct->ct_error.re_status = RPC_SUCCESS;
	ct->ct_wait.tv_sec = tv->tv_sec;
	ct->ct_wait.tv_usec = tv->tv_usec;

	if ((res = get_areply(cl, &reply_msg)) != RPC_SUCCESS)
		return (res);

	*xid = reply_msg.rm_xid;

	/*
	 * Check to make sure xid matchs one that we are interested in
	 */
	for (i = 0; i < cnt; i++) {
		if (xids[i] == *xid)
			break;
	}

	if (i == cnt)
		return (RPC_CANTDECODERES);

	/*
	 * process header
	 */
	return (proc_header(cl, &reply_msg, xdr_results, results_ptr));
}
 
/* ARGSUSED */
static int
sfs_ctcp_poll(
        CLIENT *cl,
        uint32_t usecs)
{
	struct ct_data *ct = (struct ct_data *)cl->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);
	struct timeval tv;
#ifdef FD_SETSIZE
	fd_set mask;
	fd_set readfds;

	FD_ZERO(&mask);
	FD_SET(ct->ct_sock, &mask);
#else
	int mask = 1 << (ct->ct_sock);
	int readfds;
#endif /* def FD_SETSIZE */

	if (xdrrec_eof(xdrs) == FALSE)
		return (1);

	tv.tv_sec = 0;
	if (usecs > 1000000)
		tv.tv_sec = usecs / 1000000;
	tv.tv_usec = usecs % 1000000;

	readfds = mask;
	return (select(_rpc_dtablesize(), &readfds, NULL, NULL, &tv));
}
