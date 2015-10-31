#ifndef lint
static char sfs_c_clntSid[] = "@(#)sfs_c_clnt.c	2.1	97/10/23";
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
 * -------------------------  Include Files  -------------------------
 */

/*
 * ANSI C headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <unistd.h>

#include "sfs_c_def.h"
#include "sfs_m_def.h"

#if !defined(_XOPEN_SOURCE)
#include <sys/socket.h>
#else
#define	AF_INET		2
#endif

CLIENT *
lad_clnt_create(int prot, struct hostent *hostent, uint32_t program,
		uint32_t version, int sock, struct timeval *wait)
{
	struct sockaddr_in	sin;
	CLIENT			*client_ptr;
	uint_t			sendsz = 0;
	uint_t			recvsz = 0;

	/* set up the socket address for the remote call */
	(void) memset((char *) &sin, '\0',  sizeof(sin));
	(void) memmove((char *) &sin.sin_addr,
			hostent->h_addr,
			hostent->h_length);
        sin.sin_family = AF_INET;

	switch (prot) {
	case 0:			/* UDP */
		if (sendsz == 0)
			client_ptr = sfs_cudp_create(&sin, program, version,
						*wait, &sock);
		else
			client_ptr = sfs_cudp_bufcreate(&sin, program, version,
						*wait, &sock,
						sendsz, recvsz);
		break;
	case 1:			/* TCP */
		sendsz = NFS_MAXDATA + 1024;
		recvsz = NFS_MAXDATA + 1024;

		client_ptr = sfs_ctcp_create(&sin, program, version, &sock,
						sendsz, recvsz);
		break;
	}

	if (client_ptr == ((CLIENT *) NULL)) {
		char	buf[128];

		(void) sprintf(buf, "%s: server not responding",
                            sfs_Myname);
		clnt_pcreateerror(buf);
		return((CLIENT *) NULL);
	}

	return (client_ptr);
}
