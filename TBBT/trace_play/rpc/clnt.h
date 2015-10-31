/*
 * @(#)clnt.h     2.1     97/10/23
 */

/* @(#)clnt.h	2.1 88/07/29 4.0 RPCSRC; from 1.31 88/02/08 SMI*/
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
 * clnt.h - Client side remote procedure call interface.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */


#ifndef _CLNT_
#define _CLNT_

/*
 * Rpc calls return an enum clnt_stat.  This should be looked at more,
 * since each implementation is required to live with this (implementation
 * independent) list of errors.
 */
enum clnt_stat {
	RPC_SUCCESS=0,			/* call succeeded */
	/*
	 * local errors
	 */
	RPC_CANTENCODEARGS=1,		/* can't encode arguments */
	RPC_CANTDECODERES=2,		/* can't decode results */
	RPC_CANTSEND=3,			/* failure in sending call */
	RPC_CANTRECV=4,			/* failure in receiving result */
	RPC_TIMEDOUT=5,			/* call timed out */
	/*
	 * remote errors
	 */
	RPC_VERSMISMATCH=6,		/* rpc versions not compatible */
	RPC_AUTHERROR=7,		/* authentication error */
	RPC_PROGUNAVAIL=8,		/* program not available */
	RPC_PROGVERSMISMATCH=9,		/* program version mismatched */
	RPC_PROCUNAVAIL=10,		/* procedure unavailable */
	RPC_CANTDECODEARGS=11,		/* decode arguments error */
	RPC_SYSTEMERROR=12,		/* generic "other problem" */

	/*
	 * callrpc & clnt_create errors
	 */
	RPC_UNKNOWNHOST=13,		/* unknown host name */
	RPC_UNKNOWNPROTO=17,		/* unkown protocol */

	/*
	 * _ create errors
	 */
	RPC_PMAPFAILURE=14,		/* the pmapper failed in its call */
	RPC_PROGNOTREGISTERED=15,	/* remote program is not registered */
	/*
	 * unspecified error
	 */
	RPC_FAILED=16
};


/*
 * Error info.
 */
struct rpc_err {
	enum clnt_stat re_status;
	union {
		int RE_errno;		/* realated system error */
		enum auth_stat RE_why;	/* why the auth error occurred */
		struct {
			uint32_t low;	/* lowest verion supported */
			uint32_t high;	/* highest verion supported */
		} RE_vers;
		struct {		/* maybe meaningful if RPC_FAILED */
			int32_t s1;
			int32_t s2;
		} RE_lb;		/* life boot & debugging only */
	} ru;
#define	re_errno	ru.RE_errno
#define	re_why		ru.RE_why
#define	re_vers		ru.RE_vers
#define	re_lb		ru.RE_lb
};


/*
 * Client rpc handle.
 * Created by individual implementations, see e.g. rpc_udp.c.
 * Client is responsible for initializing auth, see e.g. auth_none.c.
 */
typedef struct {
	AUTH	*cl_auth;			/* authenticator */
	struct clnt_ops	*cl_ops;
	void	*cl_private;			/* private stuff */
} CLIENT;

struct clnt_ops {
	enum clnt_stat	(*cl_call)(CLIENT *, uint32_t, xdrproc_t, void *, xdrproc_t, void *, struct timeval);
	void		(*cl_abort)(CLIENT *);
	void		(*cl_geterr)(CLIENT *, struct rpc_err *);
	bool_t		(*cl_freeres)(CLIENT *, xdrproc_t, void *);
	void		(*cl_destroy)(CLIENT *);
	bool_t          (*cl_control)(CLIENT *, uint_t, void *);
	bool_t		(*cl_getreply)(CLIENT *, xdrproc_t, void *, int,
				uint32_t *, uint32_t *, struct timeval *);
	int		(*cl_poll)(CLIENT *, uint32_t);
};

/*
 * client side rpc interface ops
 *
 * Parameter types are:
 *
 */

/*
 * enum clnt_stat
 * CLNT_CALL(rh, proc, xargs, argsp, xres, resp, timeout)
 * 	CLIENT *rh;
 *	uint32_t proc;
 *	xdrproc_t xargs;
 *	void * argsp;
 *	xdrproc_t xres;
 *	void * resp;
 *	struct timeval timeout;
 */
#define	CLNT_CALL(rh, proc, xargs, argsp, xres, resp, secs)	\
	((*(rh)->cl_ops->cl_call)(rh, proc, xargs, argsp, xres, resp, secs))
#define	clnt_call(rh, proc, xargs, argsp, xres, resp, secs)	\
	((*(rh)->cl_ops->cl_call)(rh, proc, xargs, argsp, xres, resp, secs))

/*
 * void
 * CLNT_ABORT(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_ABORT(rh)	((*(rh)->cl_ops->cl_abort)(rh))
#define	clnt_abort(rh)	((*(rh)->cl_ops->cl_abort)(rh))

/*
 * struct rpc_err
 * CLNT_GETERR(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_GETERR(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))
#define	clnt_geterr(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))


/*
 * bool_t
 * CLNT_FREERES(rh, xres, resp);
 * 	CLIENT *rh;
 *	xdrproc_t xres;
 *	void * resp;
 */
#define	CLNT_FREERES(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))
#define	clnt_freeres(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))

/*
 * bool_t
 * CLNT_CONTROL(cl, request, info)
 *      CLIENT *cl;
 *      uint_t request;
 *      void *info;
 */
#define	CLNT_CONTROL(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))
#define	clnt_control(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))

/*
 * control operations that apply to both udp and tcp transports
 */
#define CLSET_TIMEOUT       1   /* set timeout (timeval) */
#define CLGET_TIMEOUT       2   /* get timeout (timeval) */
#define CLGET_SERVER_ADDR   3   /* get server's address (sockaddr) */
/*
 * udp only control operations
 */
#define CLSET_RETRY_TIMEOUT 4   /* set retry timeout (timeval) */
#define CLGET_RETRY_TIMEOUT 5   /* get retry timeout (timeval) */

/*
 * void
 * CLNT_DESTROY(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_DESTROY(rh)	((*(rh)->cl_ops->cl_destroy)(rh))
#define	clnt_destroy(rh)	((*(rh)->cl_ops->cl_destroy)(rh))

/*
 * bool_t
 * CLNT_GETREPLY(rh,xres,xresp,xid,tv)
 *	CLIENT *rh;
 *	xdrproc_t xres;
 *	void * resp;
 *	int cnt;
 *	uint32_t *xids;
 *	uint32_t *xid;
 *	struct timeval *tv;
 */
#define	CLNT_GETREPLY(rh,xres,xresp,cnt,xids,xid,tv) ((*(rh)->cl_ops->cl_getreply)(rh,xres,xresp,cnt,xids,xid,tv))
#define	clnt_getreply(rh,xres,xresp,cnt,xids,xid,tv) ((*(rh)->cl_ops->cl_getreply)(rh,xres,xresp,cnt,xids,xid,tv))

/*
 * bool_t
 * CLNT_POLL(rh,xusec)
 *	CLIENT *rh;
 *	uint32_t xusec;
 */
#define	CLNT_POLL(rh,xid) ((*(rh)->cl_ops->cl_poll)(rh,xid))
#define	clnt_poll(rh,xid) ((*(rh)->cl_ops->cl_poll)(rh,xid))

/*
 * RPCTEST is a test program which is accessable on every rpc
 * transport/port.  It is used for testing, performance evaluation,
 * and network administration.
 */

#define RPCTEST_PROGRAM		((uint32_t)1)
#define RPCTEST_VERSION		((uint32_t)1)
#define RPCTEST_NULL_PROC	((uint32_t)2)
#define RPCTEST_NULL_BATCH_PROC	((uint32_t)3)

/*
 * By convention, procedure 0 takes null arguments and returns them
 */

#define NULLPROC ((uint32_t)0)

/*
 * Below are the client handle creation routines for the various
 * implementations of client side rpc.  They can return NULL if a 
 * creation failure occurs.
 */

/*
 * Memory based rpc (for speed check and testing)
 * CLIENT *
 * clntraw_create(prog, vers)
 *	uint32_t prog;
 *	uint32_t vers;
 */
extern CLIENT *clntraw_create(uint32_t, uint32_t);

/*
 * Generic client creation routine. Supported protocols are "udp" and "tcp"
 */
extern CLIENT *clnt_create(char *, uint32_t, uint32_t, char *);


/*
 * TCP based rpc
 * CLIENT *
 * sfs_ctcp_create(raddr, prog, vers, sockp, sendsz, recvsz)
 *	struct sockaddr_in *raddr;
 *	uint32_t prog;
 *	uint32_t version;
 *	register int *sockp;
 *	uint_t sendsz;
 *	uint_t recvsz;
 */
extern CLIENT *sfs_ctcp_create(struct sockaddr_in *, uint32_t, uint32_t,
					int *, uint_t, uint_t);
extern CLIENT *clnttcp_create(struct sockaddr_in *, uint32_t, uint32_t,
					int *, uint_t, uint_t);

/*
 * UDP based rpc.
 * CLIENT *
 * sfs_cudp_create(raddr, program, version, wait, sockp)
 *	struct sockaddr_in *raddr;
 *	uint32_t program;
 *	uint32_t version;
 *	struct timeval wait;
 *	int *sockp;
 *
 * Same as above, but you specify max packet sizes.
 * CLIENT *
 * sfs_cudp_bufcreate(raddr, program, version, wait, sockp, sendsz, recvsz)
 *	struct sockaddr_in *raddr;
 *	uint32_t program;
 *	uint32_t version;
 *	struct timeval wait;
 *	int *sockp;
 *	uint_t sendsz;
 *	uint_t recvsz;
 */
extern CLIENT *clntudp_create(struct sockaddr_in *, uint32_t, uint32_t,
					struct timeval, int *);
extern CLIENT *clntudp_bufcreate(struct sockaddr_in *, uint32_t, uint32_t,
					struct timeval, int *, uint_t, uint_t);
extern CLIENT *sfs_cudp_create(struct sockaddr_in *, uint32_t, uint32_t,
					struct timeval, int *);
extern CLIENT *sfs_cudp_bufcreate(struct sockaddr_in *, uint32_t, uint32_t,
					struct timeval, int *, uint_t, uint_t);

/*
 * Print why creation failed
 */
extern void clnt_pcreateerror(char *);
extern char *clnt_spcreateerror(char *);

/*
 * Like clnt_perror(), but is more verbose in its output
 */ 
extern void clnt_perrno(enum clnt_stat);

/*
 * Print an English error message, given the client error code
 */
extern void clnt_perror(CLIENT *, char *);
extern char *clnt_sperror(CLIENT *, char *);

/* 
 * If a creation fails, the following allows the user to figure out why.
 */
struct rpc_createerr {
	enum clnt_stat cf_stat;
	struct rpc_err cf_error; /* useful when cf_stat == RPC_PMAPFAILURE */
};

extern struct rpc_createerr rpc_createerr;

/*
 * Copy error message to buffer.
 */
extern char *clnt_sperrno(enum clnt_stat);

extern int callrpc(char *, int, int, int, xdrproc_t, char *, xdrproc_t, char *);

extern int bindresvport(int sd, struct sockaddr_in *sin);

#define UDPMSGSIZE	(63 * 1024) /* rpc imposed limit on udp msg size */
#define RPCSMALLMSGSIZE	400	    /* a more reasonable packet size */

#if !defined(RPC_ANYSOCK)
#define RPC_ANYSOCK     -1
#endif

#endif /*!_CLNT_*/
