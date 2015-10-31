#ifndef lint
static char sfs_m_msgSid[] = "@(#)sfs_m_msg.c	2.1	97/10/23";
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
 *	int * signal_sfs_1(sync_string *, CLIENT *);
 *
 *.Local_routines
 *	None.
 *
 *.Revision_history
 *	04-Dec-91	Keith		Include sfs_def.h for SYSV/SVR4
 *					mem* routines.
 *	17-Jun-91	Teelucksingh	Create multi-client synchronization
 *					rpc definition
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
 
#include <sys/types.h>
#include <sys/stat.h> 

#include "sfs_c_def.h"
#include "sfs_m_def.h"

/*
 * -----------------------  Static Definitions  -----------------------
 */

/*
 * Almost all rpcs are sent over TCP so we have a reliable transport,
 * unfortunately the initialization phase may highly overload the
 * server and the previous default of 5 seconds might timeout before
 * it gets acknowledged.  The risk of a large timeout is that there is
 * an added period after a stop message is sent.  This is usually not
 * a problem.
 */
static struct timeval TIMEOUT = { 60, 0 };


/*
 * -------------------  Multi-client Message Handling  -------------------
 */

int *
signal_sfs_1(
    sync_string *	argp,
    CLIENT *		clnt)
{
    static int		res;

    (void) memset((char *) &res, '\0', sizeof(res));
    if (clnt_call(clnt, SIGNAL_SFS, xdr_sync_string, (caddr_t)argp, xdr_int,
		  (caddr_t)&res, TIMEOUT) != RPC_SUCCESS) {
	return (NULL);
    }
    return (&res);
} /* signal_sfs_1 */


/* sfs_m_msg.c */
