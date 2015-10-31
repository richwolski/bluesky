/*
 * @(#)sfs_m_def.h	2.1	97/10/23
 */

/*
 *	Copyright (c) 1992-1997,2001 by Standard Performance Evaluation Corporation
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

#define MAX_LINE_LEN 160 		/* max per line results string */
#define MAX_STR1_LEN 31 		/* max msg, number, and xid str len */
#define MAX_STR2_LEN 2560		/* total results data string length */
#define MULTICLIENT_OFFSET 500  	/* Runtime offset */

/* multi-client sync logfiles and prefixes */
#define SFS_CLIENT_SYNC_LOG "/tmp/sfs_CL" /* client logfile prefix */
#define SFS_PRIME_SYNC_LOG "/tmp/sfs_PC_sync" /* prime client logfile */
#define PRIME_RESULTS_LOG "/tmp/sfs_res"  /* prime results logfile prefix */

struct sync_string {
	int clnt_id;				/* client number */
	char *clnt_type;			/* message type, hard coded */
	char *clnt_transaction;			/* transaction id */
	char *clnt_data;			/* results strings */
};
typedef struct sync_string sync_string;

#define SFS_SYNCPROG ((uint32_t) 100500)
#define SFS_SYNCVERS ((uint32_t) 1)
#define SIGNAL_NULLPROC ((uint32_t) 0)
#define SIGNAL_SFS ((uint32_t) 1)

extern bool_t	xdr_sync_string(XDR *, sync_string *);
extern int *    signal_sfs_1(sync_string *, CLIENT *);
