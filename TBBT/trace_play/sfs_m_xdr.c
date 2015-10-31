#ifndef lint
static char sfs_m_xdrSid[] = "@(#)sfs_m_xdr.c	2.1	97/10/23";
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
 *	bool_t xdr_sync_string(XDR *, sync_string *)
 *
 *.Revision_history
 *	28-Nov-91	0.0.13	Teelucksingh
 *				added 'transaction id' field to xdr data
 *				structure and ANSI C features.
 *	17-Jun-91       0.0.7	Teelucksingh - Creation
 *				Multi-client synchronization rpc xdr
 *				functions.
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
 * ---------------  Multi-client Message XDR Routines  ---------------
 */

bool_t
xdr_sync_string(
    XDR *		xdrs,
    sync_string *	objp)
{
    if (!xdr_int(xdrs, (int *) &objp->clnt_id)) {
	(void) fprintf(stderr, "%s: can't encode client id %d",
			sfs_Myname, objp->clnt_id);
	return (FALSE);
    }
    if (!xdr_string(xdrs, (char **) &objp->clnt_type,
					(unsigned int) MAX_STR1_LEN)) {
	(void) fprintf(stderr, "%s: can't encode client type %s",
			sfs_Myname, objp->clnt_type);
	return (FALSE);
    }
    if (!xdr_string(xdrs, (char **) &objp->clnt_transaction,
					(unsigned int) MAX_STR1_LEN)) {
	(void) fprintf(stderr, "%s: can't encode client transaction %s",
			sfs_Myname, objp->clnt_transaction);
	return (FALSE);
    }
    if (!xdr_string(xdrs, (char **) &objp->clnt_data,
					(unsigned int) MAX_STR2_LEN)) {
	(void) fprintf(stderr, "%s: can't encode client data %s",
			sfs_Myname, objp->clnt_data);
	return (FALSE);
    }
    return (TRUE);
}


/* sfs_m_xdr.c */
