#ifndef lint
static char sccsid[] = "@(#)sfs_m_snc.c	2.1	97/10/23";
#endif

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

/*****************************************************************
 *                                                               *
 *	Copyright 1991,1992  Legato Systems, Inc.                *
 *	Copyright 1991,1992  Auspex Systems, Inc.                *
 *	Copyright 1991,1992  Data General Corporation            *
 *	Copyright 1991,1992  Digital Equipment Corporation       *
 *	Copyright 1991,1992  Interphase Corporation              *
 *	Copyright 1991,1992  Sun Microsystems, Inc.              *
 *                                                               *
 *****************************************************************/

/*
 *.Exported_routines
 *     None.
 *
 *.Local_routines
 *     void sfs_syncprog_1(struct svc_req *, SVCXPRT *)
 *     int * signal_sync_sfs_1(struct sync_string *)
 *     void lad_syncd_cleanup(int)
 *
 *.Revision_history
 *      04-Dec-91	Keith	Include sfs_def.h for SYSV/SVR4 mem*
 *				functions. Include string.h for SYSV/SVR4.
 *	28-Nov-91	Teeluckingh	Fixed 'multiple signals to sfs'
 *				problem.  Uses a 'transaction id' field in
 *				the sync rpc xdr structure to compare
 *				previous rpc, if the current transaction id
 *				matches the previous one then sfs_syncd
 *				just return 'success' to the client. If the
 *				transaction ids do not match, the actions
 *				are performed and the transaction id value
 *				is saved.
 *	17-Jun-91 	Teelucksingh	Creation - multi-client
 *				synchronization server sfs_syncd.
 *				Processes the sfs sync rpcs between systems.
 */


/*
 * -------------------------  Include Files  -------------------------
 */

/*
 * ANSI C headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
 
#include <sys/types.h>
#include <sys/stat.h> 

#include <sys/signal.h>
#include <sys/file.h>

#include "sfs_c_def.h"
#include "sfs_m_def.h"


/*
 * -----------------------  External Definitions  -----------------------
 */

/* forward definitions for local routines */
static void sfs_syncprog_1(struct svc_req *, SVCXPRT *);
static int * signal_sync_sfs_1(struct sync_string *);
static void lad_syncd_cleanup(int);

/*
 * -----------------------  Static Definitions  -----------------------
 */

int Debug_level = 0;            /* flag indicates prime client debug mode */
char *sfs_Myname;                   /* program name */

static char previous_transaction[MAX_STR1_LEN];	/* to hold transaction id */


/*
 * -------------------  Multi-client Synchronization  -------------------
 */


/*ARGSUSED*/
int
main(
    int		argc,
    char *	argv[])
{
    char 	*nameptr;
    SVCXPRT *	transp;
    FILE	*pid_fp;
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    struct sigaction sig_act, old_sig_act;
#endif /* USE_POSIX_SIGNALS */

    /* 
     * Place pid in pid log file 
     */   
    if ((pid_fp = fopen(SFS_SYNCD_PID, "a+")) == NULL) { 
        perror(SFS_SYNCD_PID);  
        (void) unlink(SFS_SYNCD_PID); 
        exit(1); 
    } 
    (void) fprintf(pid_fp, "%d\n", getpid()); 
    (void) fclose(pid_fp);

    sfs_Myname = argv[0];

    if ((nameptr = strrchr(argv[0], '/')) != NULL)
        sfs_Myname = ++nameptr;

#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    /* use XOPEN signal handling */
    sig_act.sa_handler = generic_catcher;
    (void)sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;

    /* signals handlers for signals used by sfs_prime */
    sig_act.sa_handler = lad_syncd_cleanup;
    if (sigaction(SIGINT,&sig_act,&old_sig_act) != 0) {
		perror("sigaction failed: SIGINT");
        	(void) unlink(SFS_SYNCD_PID); 
		exit(4);
    }
#else
    /* set up SIGINT signal handler */
    (void) signal(SIGINT, lad_syncd_cleanup);
#endif /* USE_POSIX_SIGNALS */

    (void) fprintf(stderr,"--------------------\n");
    (void) fprintf(stderr,"Start of sfs run.\n");

    (void) pmap_unset(SFS_SYNCPROG, SFS_SYNCVERS);

    transp = svcudp_create(RPC_ANYSOCK);
    if (transp == ((SVCXPRT *) NULL)) {
	(void) fprintf(stderr, "%s: cannot create udp service.\n", sfs_Myname);
       	(void) unlink(SFS_SYNCD_PID); 
	exit(5);
    }
    if (!svc_register(transp, SFS_SYNCPROG, SFS_SYNCVERS,
		     sfs_syncprog_1, IPPROTO_UDP)) {
	(void) fprintf(stderr,
	    "%s: unable to register (SFS_SYNCPROG,SFS_SYNCVERS, udp).\n",
	    sfs_Myname);
       	(void) unlink(SFS_SYNCD_PID); 
	exit(6);
    }

    transp = svctcp_create(RPC_ANYSOCK, 0, 0);
    if (transp == ((SVCXPRT *) NULL)) {
	(void) fprintf(stderr, "%s: cannot create tcp service.\n", sfs_Myname);
       	(void) unlink(SFS_SYNCD_PID); 
	exit(6);
    }
    if (!svc_register(transp, SFS_SYNCPROG, SFS_SYNCVERS,
		      sfs_syncprog_1, IPPROTO_TCP)) {
	(void) fprintf(stderr,
	    "%s: unable to register (SFS_SYNCPROG, SFS_SYNCVERS, tcp).\n",
	    sfs_Myname);
       	(void) unlink(SFS_SYNCD_PID); 
	exit(7);
    }

    svc_run();
    (void) fprintf(stderr, "%s: svc_run returned\n", sfs_Myname);
    return(1);

} /* main */


static void
sfs_syncprog_1(
    struct svc_req *	rqstp,
    SVCXPRT *		transp)
{
    union {
	sync_string	signal_sync_sfs_1_arg;
    } argument;
    char *		result;
    bool_t 		(*xdr_argument)(), (*xdr_result)();
    char *		(*local)();

    switch (rqstp->rq_proc) {
	case SIGNAL_NULLPROC:
	    (void) svc_sendreply(transp, xdr_void, (char *)NULL);
	    return;

	case SIGNAL_SFS:
	    xdr_argument = xdr_sync_string;
	    xdr_result = xdr_int;
	    local = (char * (*)()) signal_sync_sfs_1;
	    break;

	default:
	    svcerr_noproc(transp);
	    return;
    }
    (void) memset((char *) &argument, '\0', sizeof(argument));
    if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
	svcerr_decode(transp);
	return;
    }
    result = (*local)(&argument);
    if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
	svcerr_systemerr(transp);
    }
    if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
	(void) fprintf(stderr, "%s: unable to free arguments\n", sfs_Myname);
       	(void) unlink(SFS_SYNCD_PID); 
	exit(8);
    }

} /* sfs_syncprog_1 */


/*
 * signal_sync_sfs_1 - multi-client synch RPC
 * Provides interface between sfs program running
 * on multiple clients and the controlling sfs_prime program.
 */
static int *
signal_sync_sfs_1(
    struct sync_string *	sfs_signal)
{
    static int			result = 0 ;	/* return status - failure */
    FILE *			fp;
    int				sfs_pid;	/* sfs parent process pid */
    char			datafile[SFS_MAXPATHLEN]; /* results file */
    char			CL_Logname[SFS_MAXPATHLEN];

    result = 0;
    /* if a duplicate transactions then just return success to calling client */
    if (strcmp(sfs_signal->clnt_transaction, previous_transaction) == 0) {
	(void) fprintf(stderr,"%s: Got a duplicate signal - %s\n",
			sfs_Myname, sfs_signal->clnt_transaction);
	result = 1;
	return(&result);
    }

    if (strcmp(sfs_signal->clnt_type,"CLIENT_SIGNAL") == 0) {

	/*
	 * message from parent sfs process on client to Prime-client
	 * (sfs_prime).
	 *
	 * Append client id to Prime client sync logfile
	 */
	fp = fopen(SFS_PRIME_SYNC_LOG, "a");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, SFS_PRIME_SYNC_LOG);
	    return (&result);
	}
	(void) fwrite((char *)&sfs_signal->clnt_id,
			sizeof(sfs_signal->clnt_id), 1, fp);
	(void) fclose(fp);
	result = 1;
	(void) sprintf(previous_transaction, sfs_signal->clnt_transaction);
	(void) fprintf(stderr,"%s: Got Client_SIGNAL - %s\n",
			sfs_Myname, sfs_signal->clnt_transaction);
	return (&result); /* success */

    } else if (strcmp(sfs_signal->clnt_type,"CLIENT_DATA") == 0) {

	/*
	 * message from parent sfs process on client to Prime-client
	 * completed run, here are my results. Write it to file and let
	 * Prime client know about it.
	 */
	(void) sprintf(datafile,"%s%d",
			PRIME_RESULTS_LOG, sfs_signal->clnt_id);
	fp = fopen(datafile, "w");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, datafile);
	    return (&result);
	}
	(void) fprintf(fp,"%s",sfs_signal->clnt_data);
	(void) fclose(fp);

	/* after writing data write client id to sync log */
	fp = fopen(SFS_PRIME_SYNC_LOG, "a");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, SFS_PRIME_SYNC_LOG);
	    return (&result);
	}
	(void) fwrite((char *)&sfs_signal->clnt_id,
				sizeof(sfs_signal->clnt_id), 1, fp);
	(void) fclose(fp);

	/* let the remote process know success */
	result = 1;
	(void) sprintf(previous_transaction, sfs_signal->clnt_transaction);
	(void) fprintf(stderr,"%s: Got Client_DATA - %s\n",
			sfs_Myname, sfs_signal->clnt_transaction);
	return (&result);

    } else if (strcmp(sfs_signal->clnt_type,"CLIENT_STOP") == 0) {

	/*
	 * message from parent sfs process on client to Prime-client
	 * (sfs_prime) to stop due to error.
	 */
	fp = fopen(SFS_PRIME_SYNC_LOG, "a");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, SFS_PRIME_SYNC_LOG);
	    return (&result);
	}
	/*
	 * Write out client id 1000 times to fool prime into thinking
	 * all clients have responded and will get an error when it
	 * tries to communicate to it.
	 */
	for (result = 0; result < 1000; result++)
		(void) fwrite((char *)&sfs_signal->clnt_id,
			sizeof(sfs_signal->clnt_id), 1, fp);
	(void) fclose(fp);
	result = 1;
	(void) sprintf(previous_transaction, sfs_signal->clnt_transaction);
	(void) fprintf(stderr,"%s: Got Client_STOP - %s\n",
			sfs_Myname, sfs_signal->clnt_transaction);
	return (&result); /* success */

    } else if (strcmp(sfs_signal->clnt_type,"PRIME_SIGNAL") == 0) {

	/*
	 * message from the Prime client (sfs_prime)
	 * send SIGUSR1 signal to parent sfs process on
	 * client - signals it to proceed
	 */
	(void) sprintf(CL_Logname,"%s%d",
			SFS_CLIENT_SYNC_LOG, sfs_signal->clnt_id);
	fp = fopen(CL_Logname, "r");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, CL_Logname);
	    return(&result);
	}
	if (fscanf(fp,"%d",&sfs_pid) != 1)
	    return (&result);
	if ((int) generic_kill(sfs_pid, SIGUSR1) == 0) {
	    result = 1;
	    (void) sprintf(previous_transaction,
			   sfs_signal->clnt_transaction);
	    (void) fprintf(stderr,"%s: Got PRIME_SIGNAL(SIGUSR1) - %s\n",
			    sfs_Myname, sfs_signal->clnt_transaction);
	    (void) fprintf(stderr,"   Sent SIGUSR1\n");
	    return (&result); /* success */
	} else
	    return (&result);

    } else if (strcmp(sfs_signal->clnt_type,"PRIME_ALARM") == 0) {

	/*
	 * message from the Prime client (sfs_prime)
	 * send SIGALRM signal to parent sfs process on
	 * client - tell it to wake up and finish execution at this time
	 */

	(void) sprintf(CL_Logname,"%s%d",
			SFS_CLIENT_SYNC_LOG, sfs_signal->clnt_id);
	fp = fopen(CL_Logname, "r");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, CL_Logname);
	    return (&result);
	}
	if (fscanf(fp,"%d",&sfs_pid) != 1)
	    return (&result);
	if ((int) generic_kill(sfs_pid, SIGALRM) == 0) {
	    result = 1;
	    (void) sprintf(previous_transaction,
			   sfs_signal->clnt_transaction);
	    (void) fprintf(stderr,"%s: Got PRIME_ALARM(SIGALRM) - %s\n",
				sfs_Myname, sfs_signal->clnt_transaction);
	    (void) fprintf(stderr,"   Sent SIGALRM\n");
	    return (&result); /* success */
	} else
	    return (&result);

    } else if (strcmp(sfs_signal->clnt_type,"PRIME_STOP") == 0) {

	/*
	 * message from Prime-client
	 * sent SIGINT signal to sfs parent process
	 * to tell it to terminate experiment now
	 */
	(void) sprintf(CL_Logname,"%s%d",
			SFS_CLIENT_SYNC_LOG, sfs_signal->clnt_id);
	fp = fopen(CL_Logname, "r");
	if (fp == NULL) {
	    (void) fprintf(stderr,"%s: Cannot open %s\n",
		sfs_Myname, CL_Logname);
	    return (&result);
	}
	if (fscanf(fp,"%d",&sfs_pid) != 1)
	    return (&result);
	if ((int) generic_kill(sfs_pid, SIGINT) == 0) {
	    result = 1;
	    (void) sprintf(previous_transaction,
			   sfs_signal->clnt_transaction);
	    (void) fprintf(stderr,"%s: Got PRIME_STOP(SIGSTOP) - %s\n",
				sfs_Myname, sfs_signal->clnt_transaction);
	    (void) fprintf(stderr,"   Sent SIGINT\n");
	    return (&result); /* success */
	} else
	    return (&result);

    } else
	return (&result); /* failure */

} /* signal_sync_sfs_1 */

/* ARGSUSED */
static void
lad_syncd_cleanup(
    int         sig_id)
{
    (void) pmap_unset(SFS_SYNCPROG, SFS_SYNCVERS);
    (void) fprintf(stderr, "Unregistered sfs_syncd.\n");
    (void) unlink(SFS_SYNCD_PID); 
    exit(0);

} /* lad_syncd_cleanup */

/* sfs_m_snc.c */
