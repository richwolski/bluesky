#ifndef lint
static char sfs_c_sigSid[] = "@(#)sfs_c_sig.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_sig.c ---------------------
 *
 *      Signal handling.  Routines to send and catch signals.
 *
 *.Exported_Routines
 *	void sfs_alarm(int)
 *	void sfs_startup(int)
 *	void sfs_stop(int)
 *	void sfs_cleanup(int)
 *
 *.Local_Routines
 *	None.
 *
 *.Revision_History
 *	16-Dec-91	Wittle 		Created.
 */


/*
 * -------------------------  Include Files  -------------------------
 */

/*
 * ANSI C headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
 
#include <sys/types.h>
#include <sys/stat.h> 

#include <sys/signal.h>

#include <unistd.h>

#include "sfs_c_def.h"

#ifndef RFS
/*
 * -------------------------  Signal Handlers  -------------------------
 */

void
sfs_alarm(
    int         sig_id)
{
#if !(defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    (void) signal(sig_id, sfs_alarm);
#endif
    if (DEBUG_CHILD_SIGNAL)
	(void) fprintf(stderr, "%s: caught Signal %d\n", sfs_Myname, sig_id);
    (void) fflush(stderr);

} /* sfs_alarm */


/*
 * Signal Handler
 * Catch the parent's "start" signal.
 */
void
sfs_startup(
    int 	sig_id)
{
#if !(defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    (void) signal(SIGUSR1, sfs_startup);
#endif
    if (DEBUG_CHILD_SIGNAL)
	(void) fprintf(stderr, "%s: caught Signal %d SIGUSR1\n",
		       sfs_Myname, sig_id);

    if (Child_num == -1)
	/* I'm the parent, ignore the signal */
	return;

    /*
     * SIGUSR1 is used to make all phase transitions, but we
     * only want to make the Warmup -> Testrun switch here
     */
    if (Current_test_phase != Warmup_phase)
	return;

    Current_test_phase = Testrun_phase;
    start_run_phase++;
} /* sfs_startup */


/*
 * Signal Handler
 * Catch the parent's "stop" signal.
 */
void
sfs_stop(
    int		sig_id)
{
#if !(defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    (void) signal(SIGUSR2, sfs_stop);
#endif
    if (DEBUG_CHILD_SIGNAL)
	(void) fprintf(stderr, "%s: caught Signal %d SIGUSR2\n",
		       sfs_Myname, sig_id);

    /* end the test */
    Runtime = 0;
    Current_test_phase = Results_phase;

} /* sfs_stop */

#endif


/*
 * SIGINT, SIGTERM handler
 * Clean up and exit due to an error/abort condition.
 * We assume if NFS_client was valid, then MOUNT_client was also valid.
 */
void
sfs_cleanup(
    int		sig_id)
{
    if (DEBUG_CHILD_SIGNAL)
	(void) fprintf(stderr, "%s: caught Signal %d SIGINT\n",
		       sfs_Myname, sig_id);

    (void) unlink(Logname);
    if (NFS_client != ((CLIENT *) NULL))
	clnt_destroy(NFS_client);
    exit(65);

} /* sfs_cleanup */

/* sfs_c_sig.c */
