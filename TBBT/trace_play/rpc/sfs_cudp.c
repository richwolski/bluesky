#ifndef lint
static char sfs_cudp_id[] = "@(#)sfs_cudp.c	2.1	97/10/23";
#endif

/* @(#)clnt_udp.c	2.2 88/08/01 4.0 RPCSRC */
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

/*
 * clnt_udp.c, Implements a UDP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#ifndef FreeBSD
#include <stropts.h>
#endif  /* ndef FreeBSD */
#include <string.h>
#include "rpc/rpc.h"
#include "rpc/osdep.h"
#include <netdb.h>
#include "rpc/pmap_clnt.h"
#include <errno.h>
#include "rfs_c_def.h"	/* Just for the define of RFS */
/*
 * UDP bases client side rpc operations
 */
static enum clnt_stat sfs_cudp_call(CLIENT *, uint32_t, xdrproc_t,
					void *, xdrproc_t, void *,
					struct timeval);
static void		sfs_cudp_abort(CLIENT *h);
static void		sfs_cudp_geterr(CLIENT *, struct rpc_err *);
static bool_t		sfs_cudp_freeres(CLIENT *, xdrproc_t, void *);
static bool_t		sfs_cudp_control(CLIENT *, uint_t, void *);
static void		sfs_cudp_destroy(CLIENT *);
static bool_t           sfs_cudp_getreply(CLIENT *, xdrproc_t, void *,
				int, uint32_t *, uint32_t *, struct timeval *);
static int              sfs_cudp_poll(CLIENT *, uint32_t);
static enum clnt_stat
get_areply(
        CLIENT *cl,
        uint32_t *xid,
	struct timeval *timeout);

static struct clnt_ops sfs_cudp_ops = {
	sfs_cudp_call,
	sfs_cudp_abort,
	sfs_cudp_geterr,
	sfs_cudp_freeres,
	sfs_cudp_destroy,
	sfs_cudp_control,
	sfs_cudp_getreply,
	sfs_cudp_poll
};

/* 
 * Private data kept per client handle
 */
struct cu_data {
	int		   cu_sock;
	bool_t		   cu_closeit;
	struct sockaddr_in cu_raddr;
	int		   cu_rlen;
	struct rpc_err cu_error;
	XDR		   cu_outxdrs;
	uint_t		   cu_xdrpos;
	uint_t		   cu_sendsz;
	char		   *cu_outbuf;
	uint_t		   cu_recvsz;
	char		   cu_inbuf[1];
};

/*
 * Create a UDP based client handle.
 * If *sockp<0, *sockp is set to a newly created UPD socket.
 * If raddr->sin_port is 0 a binder on the remote machine
 * is consulted for the correct port number.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is initialized to null authentication.
 *     Caller may wish to set this something more useful.
 *
 * wait is the amount of time used between retransmitting a call if
 * no response has been heard;  retransmition occurs until the actual
 * rpc call times out.
 *
 * sendsz and recvsz are the maximum allowable packet sizes that can be
 * sent and received.
 */
/* ARGSUSED */
CLIENT *
sfs_cudp_bufcreate(
	struct sockaddr_in *raddr,
	uint32_t program,
	uint32_t version,
	struct timeval wait,
	int *sockp,
	uint_t sendsz,
	uint_t recvsz)
{
	CLIENT *cl;
	struct cu_data *cu;
	struct timeval now;
	struct rpc_msg call_msg;
	int min_buf_sz;
	int new_buf_sz;
	int type;
	int i;
	int error;
#if defined(UNIXWARE) || defined(AIX)
	size_t optlen;
#else
	int optlen;
#endif

	cl = (CLIENT *)mem_alloc(sizeof(CLIENT));
	if (cl == NULL) {
		(void) fprintf(stderr, "sfs_cudp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	sendsz = ((sendsz + 3) / 4) * 4;
	recvsz = ((recvsz + 3) / 4) * 4;
	cu = (struct cu_data *)mem_alloc(sizeof(struct cu_data) +
							sendsz + recvsz);
	if (cu == NULL) {
		(void) fprintf(stderr, "sfs_cudp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	cu->cu_outbuf = &cu->cu_inbuf[recvsz];

	(void)gettimeofday(&now, (struct timezone *)0);
	if (raddr->sin_port == 0) {
		uint16_t port;
		if ((port =
		    pmap_getport(raddr, program, version, IPPROTO_UDP)) == 0) {
			goto fooy;
		}
		raddr->sin_port = htons(port);
	}
	cl->cl_ops = &sfs_cudp_ops;
	cl->cl_private = (void *)cu;
	cu->cu_raddr = *raddr;
	cu->cu_rlen = sizeof (cu->cu_raddr);
	cu->cu_sendsz = sendsz;
	cu->cu_recvsz = recvsz;
	call_msg.rm_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = program;
	call_msg.rm_call.cb_vers = version;
	xdrmem_create(&(cu->cu_outxdrs), cu->cu_outbuf,
	    sendsz, XDR_ENCODE);
	if (! xdr_callhdr(&(cu->cu_outxdrs), &call_msg)) {
		goto fooy;
	}
	cu->cu_xdrpos = XDR_GETPOS(&(cu->cu_outxdrs));
	if (*sockp < 0) {
#if defined(O_NONBLOCK)
		int flags;
#elif defined(FIONBIO)
		int dontblock = 1;
#endif

		*sockp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (*sockp < 0) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			goto fooy;
		}
		/* attempt to bind to prov port */
		(void)bindresvport(*sockp, (struct sockaddr_in *)0);
		/* the sockets rpc controls are non-blocking */
#if defined(O_NONBLOCK)
		flags = fcntl(*sockp, F_GETFL, 0) | O_NONBLOCK;
		(void)fcntl(*sockp, F_SETFL, flags);
#elif defined(FIONBIO)
		(void)ioctl(*sockp, FIONBIO, (char *) &dontblock);
#endif
		cu->cu_closeit = TRUE;
	} else {
		cu->cu_closeit = FALSE;
	}
	cu->cu_sock = *sockp;
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
#if defined(UNIXWARE)
		if (getsockopt(cu->cu_sock, SOL_SOCKET, type,
				(void *)&min_buf_sz, &optlen) < 0) {
			/* guess the default */
			min_buf_sz = 18 * 1024;
		}
#else
		if (getsockopt(cu->cu_sock, SOL_SOCKET, type,
				(char *)&min_buf_sz, &optlen) < 0) {
			/* guess the default */
			min_buf_sz = 18 * 1024;
		}
#endif

		new_buf_sz = 512 * 1024;
		if (new_buf_sz > min_buf_sz) {
			do {
				error = setsockopt(cu->cu_sock, SOL_SOCKET,
						type, (char *)&new_buf_sz,
						sizeof(int));
				new_buf_sz -= (8 * 1024);
			} while (error != 0 && new_buf_sz > min_buf_sz);
		}

		type = SO_RCVBUF;
	}

	cl->cl_auth = authnone_create();
	return (cl);
fooy:
	if (cu)
		mem_free((void *)cu, sizeof(struct cu_data) + sendsz + recvsz);
	if (cl)
		mem_free((void *)cl, sizeof(CLIENT));
	return ((CLIENT *)NULL);
}

CLIENT *
sfs_cudp_create(
	struct sockaddr_in *raddr,
	uint32_t program,
	uint32_t version,
	struct timeval wait,
	int *sockp)
{

	return(sfs_cudp_bufcreate(raddr, program, version, wait, sockp,
	    UDPMSGSIZE, UDPMSGSIZE));
}

#ifdef RFS
enum clnt_stat get_areply_udp (
		CLIENT * cl,
		uint32_t *xid,
		struct timeval *timeout) 
{
	return get_areply (cl, xid, timeout);
}
#endif

static enum clnt_stat
get_areply(
        CLIENT *cl,
        uint32_t *xid,
	struct timeval *timeout)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	int inlen;
#if defined(AIX)
	size_t fromlen;
#else
	int fromlen;
#endif
	struct sockaddr_in from;
#ifdef FD_SETSIZE
	fd_set readfds;
	fd_set mask;
#else
	int readfds;
	int mask;
#endif /* def FD_SETSIZE */

#ifdef FD_SETSIZE
	FD_ZERO(&mask);
	FD_SET(cu->cu_sock, &mask);
#else
	mask = 1 << cu->cu_sock;
#endif /* def FD_SETSIZE */

	for (;;) {
		readfds = mask;
		switch (select(_rpc_dtablesize(), &readfds, NULL, 
			       NULL, timeout)) {

		case 0:
			return (cu->cu_error.re_status = RPC_TIMEDOUT);

		case -1:
			if (errno == EINTR)
				continue;	
			cu->cu_error.re_errno = errno;
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}
		do {
			fromlen = sizeof(struct sockaddr);
			inlen = recvfrom(cu->cu_sock, cu->cu_inbuf, 
				(int) cu->cu_recvsz, 0,
				(struct sockaddr *)&from, &fromlen);
		} while (inlen < 0 && errno == EINTR);
		if (inlen < 0) {
			if (errno == EWOULDBLOCK)
				continue;	
			cu->cu_error.re_errno = errno;
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}

		if (inlen < sizeof(uint32_t))
			continue;	

		*xid = ntohl(*((uint32_t *)(cu->cu_inbuf)));
		return (RPC_SUCCESS);
	}
}

enum clnt_stat
proc_header(
	CLIENT *cl,
	xdrproc_t xdr_results,
	void *results_ptr)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	XDR *xdrs = &(cu->cu_outxdrs);
	struct rpc_msg reply_msg;
	XDR reply_xdrs;
	bool_t ok;

	/*
	 * now decode and validate the response
	 */
	xdrmem_create(&reply_xdrs, cu->cu_inbuf, cu->cu_recvsz, XDR_DECODE);

	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = results_ptr;
	reply_msg.acpted_rply.ar_results.proc = xdr_results;

	ok = xdr_replymsg(&reply_xdrs, &reply_msg);
	/* XDR_DESTROY(&reply_xdrs);  save a few cycles on noop destroy */
	if (!ok) {
		return (cu->cu_error.re_status = RPC_CANTDECODERES);
	}

	_seterr_reply(&reply_msg, &(cu->cu_error));

	if (cu->cu_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(cl->cl_auth,
				&reply_msg.acpted_rply.ar_verf)) {
			cu->cu_error.re_status = RPC_AUTHERROR;
			cu->cu_error.re_why = AUTH_INVALIDRESP;
		}
		if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs,
				&(reply_msg.acpted_rply.ar_verf));
		}
	}

	return (cu->cu_error.re_status);
}

/*
 * Non-standard changes.  Make a call an at-most-once with a per call
 * timer.  Ignore the timeout set at creation. Never retransmit.
 */
static enum clnt_stat 
sfs_cudp_call(
	CLIENT		*cl,		/* client handle */
	uint32_t	proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void *		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void *		resultsp,	/* pointer to results */
	struct timeval	timeout)	/* seconds to wait before giving up */
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	XDR *xdrs = &(cu->cu_outxdrs);
	int outlen;
	uint32_t x_id, r_xid;

	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, cu->cu_xdrpos);

	/*
	 * the transaction is the first thing in the out buffer
	 */
	(*(uint32_t *)(cu->cu_outbuf))++;
	x_id = ntohl(*(uint32_t *)(cu->cu_outbuf));

	if ((! XDR_PUTLONG(xdrs, (int32_t *)&proc)) ||
	    (! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, argsp)))
		return (cu->cu_error.re_status = RPC_CANTENCODEARGS);

	outlen = (int)XDR_GETPOS(xdrs);

	if (sendto(cu->cu_sock, cu->cu_outbuf, outlen, 0,
	    (struct sockaddr *)&(cu->cu_raddr), cu->cu_rlen) != outlen) {
		cu->cu_error.re_errno = errno;
		return (cu->cu_error.re_status = RPC_CANTSEND);
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
                /* 
                 * Double hack, send back xid in results_prt if non-NULL
                 */ 
                if (resultsp != NULL) 
                        *(uint32_t *)resultsp = x_id; 

		return (cu->cu_error.re_status = RPC_TIMEDOUT);
	}

	/* CONSTCOND */
	while (TRUE) {
		enum clnt_stat res;

		if ((res = get_areply(cl, &r_xid, &timeout)) != RPC_SUCCESS)
			return (res);

		if (r_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
	return (proc_header(cl, xresults, resultsp));
}

static void
sfs_cudp_geterr(CLIENT *cl, struct rpc_err *errp)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;

	*errp = cu->cu_error;
}


static bool_t
sfs_cudp_freeres(
	CLIENT *cl,
	xdrproc_t xdr_res,
	void * res_ptr)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	XDR *xdrs = &(cu->cu_outxdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

/* ARGSUSED */
static void 
sfs_cudp_abort(CLIENT *h)
{
}

static bool_t
sfs_cudp_control(
	CLIENT *cl,
	uint_t request,
	void *info)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;

	switch (request) {
	case CLGET_SERVER_ADDR:
		*(struct sockaddr_in *)info = cu->cu_raddr;
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}
	
static void
sfs_cudp_destroy(CLIENT *cl)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;

	if (cu->cu_closeit) {
		(void)close(cu->cu_sock);
	}
	XDR_DESTROY(&(cu->cu_outxdrs));
	mem_free((void *)cu, (sizeof(struct cu_data) + cu->cu_sendsz + cu->cu_recvsz));
	mem_free((void *)cl, sizeof(CLIENT));
}

/* ARGSUSED */
static bool_t
sfs_cudp_getreply(
        CLIENT *cl,
        xdrproc_t xproc,
        void *xres,
	int cnt,
	uint32_t *xids,
        uint32_t *xid,
	struct timeval *tv)
{
        struct cu_data *cu = (struct cu_data *)cl->cl_private;
	bool_t res;
	int i;

	cu->cu_error.re_status = RPC_SUCCESS;

	if ((res = get_areply(cl, xid, tv)) != RPC_SUCCESS)
		return (res);

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
	return (proc_header(cl, xproc, xres));
}
 
static int
sfs_cudp_poll(
        CLIENT *cl,
        uint32_t usecs)
{
        struct cu_data *cu = (struct cu_data *)cl->cl_private;
        struct timeval tv;
#ifdef FD_SETSIZE
        fd_set mask;
        fd_set readfds;
 
        FD_ZERO(&mask);
        FD_SET(cu->cu_sock, &mask);
#else
        int mask = 1 << (cu->cu_sock);
        int readfds;
#endif /* def FD_SETSIZE */
 
        tv.tv_sec = 0; 
        if (usecs > 1000000) 
                tv.tv_sec = usecs / 1000000; 
        tv.tv_usec = usecs % 1000000; 
 
        readfds = mask;
        return (select(_rpc_dtablesize(), &readfds, NULL, NULL, &tv));
}
