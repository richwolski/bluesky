#ifndef lint
static char sfs_c_dmpSid[] = "@(#)sfs_c_dmp.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_dmp.c ---------------------
 *
 *      Raw data dump routines for SFS.
 *
 *	The routines contained here capture and dump the raw data
 *	points (operation, response time) to allow the researcher
 *	to perform detailed analysis on the response time characteristics
 *	of an NFS server.
 *
 *.Exported Routines
 *	void log_dump(ladtime *, ladtime *, int)
 *	void print_dump(int, int)
 *
 *.Local Routines
 *	None.
 *
 *.Revision_History
 *	11-Jul-94	ChakChung Ng	Add codes for NFS/v3
 *	03-Feb-92	0.0.20 Pawlowski
 *					Use of "Current_test_phase"
 *					obviates need for dump init
 *					routine, so it has been deleted.
 *	10-Jan-92	0.0.19 Teelucksingh
 *					Changed dump file names to be
 *					< 14 chars. Added header to
 *					output.
 *      04-Jan-92	0.0.18 Pawlowski
 *					Added raw data dump code.
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

struct savedata {
    struct savedata	*next;
    int32_t	rtv_sec;	/* The response time */
    int32_t	rtv_usec;
    int32_t	stv_sec;	/* The start time */
    int32_t	stv_usec;
    uint32_t	count;		/* Used only for read and write */
    uint32_t	offset;		/* Used only for read and write */
    uint32_t	length;		/* Used only for read and write */
    uint16_t	unique_num;	/* unique id for file */
    unsigned int flags;		/* Things like whether truncating, etc. */
    int16_t	op;
};

uint32_t	Dump_count;	/* place to put read and write count */
uint32_t	Dump_offset;	/* place to put r/w offset */
uint32_t	Dump_length;	/* place to put r/w offset */
int		Dump_data;	/* Flag set by command line option */
int		dump_create_existing_file = FALSE;
int		dump_truncate_op = FALSE;

/*
 * ----------------------  Static Declarations  ----------------------
 */

static struct savedata *saveary = 0;

/*
 * --------------------------  Constants  --------------------------
 */

/* flags bit values */
#define CREATE_OF_EXISTING_FILE	0x0001
#define TRUNCATE_OPERATION	0x0002

/*
 * ----------------------  Dump Routines  ----------------------
 */

/*
 * log_dump()
 *
 * This function is called on the completion of every operation
 * to log the data point. We log the operation and the elapsed time
 * (storing the microsecond component also).
 *
 * The data is kept in a singly linked list, elements dynamically
 * allocated as needed.
 *
 * Dynamic allocation of elements using malloc and single link list
 * pointer adds overhead to the storage space for the data for each
 * point. Dynamic allocation can result in system calls to get more
 * space for elements, adding to execution overhead. However, if you're
 * doing a raw data dump run, you assume costs are negligible.
 */

void
log_dump(
    struct ladtime *start,
    struct ladtime *elapsed,
    int op)
{
    static struct savedata *saveptr = 0;

    if (!Dump_data || Current_test_phase != Testrun_phase)
	return;

    if (saveary == (struct savedata *)NULL) {
	if ((saveary = (struct savedata *)
			malloc(sizeof(struct savedata)))
				== (struct savedata *)NULL) {
	    (void) fprintf(stderr, "Unable to allocate dump element\n");
	    return;
	}
	saveptr = saveary;
    } else {
	if ((saveptr->next = (struct savedata *)
			malloc(sizeof(struct savedata)))
				== (struct savedata *)NULL) {
	    (void) fprintf(stderr, "Unable to allocate dump element\n");
	    return;
	}
	saveptr = saveptr->next;
    }
    saveptr->next = (struct savedata *)NULL;
    saveptr->flags = 0;
    saveptr->op = op;
    saveptr->rtv_sec = elapsed->sec;
    saveptr->rtv_usec = elapsed->usec;
    saveptr->stv_sec = start->sec;
    saveptr->stv_usec = start->usec;
    saveptr->unique_num = Cur_file_ptr->unique_num;
    if (op == NFSPROC3_READ || op == NFSPROC3_WRITE ||
	op == NFSPROC_READ || op == NFSPROC_WRITE) {
	saveptr->count = (uint16_t) Dump_count;
	saveptr->offset = Dump_offset;
	saveptr->length = Dump_length;
    }

    if (dump_create_existing_file == TRUE) {
	saveptr->flags |= CREATE_OF_EXISTING_FILE;
	dump_create_existing_file = FALSE;
    }

    if (dump_truncate_op == TRUE) {
	saveptr->flags |= TRUNCATE_OPERATION;
	dump_truncate_op = FALSE;
    }
}

/*
 * print_dump()
 *
 * Dumps the raw data to a file in the format:
 *
 *	opcode	sec.msec  sec.msec file-unique-id <length> <offset> <count>
 *	opcode	sec.msec  sec.msec file-unique-id
 *	     . . .
 *
 * The opcode is the numeric NFS operation code as defined in the
 * NFS protocol specification. The first "sec.msec" field is the
 * response time of the operation. The second "sec.msec" field
 * is the start time of the operation. For read and write calls,
 * the "length", "offset" and "count" from the operation arguments are put out
 * as the fourth, fifth, and sixth field.
 *
 * This simple (x, y) pairing should be suitable input for a wide variety
 * of plotting programs.
 *
 * Note that the raw data is precious! Several points to be observed:
 *
 *	1. The raw data for each individual child is dumped to
 *	   its own data file. So each individual child process
 *	   data can be inspected (possibly useful to debug client
 *	   load generation per child process anomalies).
 *
 *	2. More importantly, each raw data dump file presents
 *	   the operations generated by the child in their exact
 *	   order of generation. This can be used to analyze possible
 *	   time dependent behaviour of the server.
 *
 * Client process output files can be merged for analysis using cat(1).
 *
 * If any other data (additional fields) are added to raw data dump
 * file, please add those fields after primary fields. awk(1) scripts
 * and the like can be used to re-arrange data files, but it would
 * be nice if the primary (x, y) data points are the default format.
 */

void
print_dump(
    int clientnum,
    int childnum)
{
    char buf[256];
    FILE *datap;
    struct savedata *p = saveary;

    buf[0] = 0;

    if (!Dump_data)
	return;

    /*
     * We write raw data files to the /tmp directory, and
     * the manager will retrieve to the prime client.
     *
     * Removed preladraw prefix from file names to fit
     * in 14 chars - K.T.
     */

    (void) sprintf(buf, "/tmp/c%3.3d-p%3.3d", clientnum, childnum);

    if ((datap = fopen(buf, "w")) == NULL) {
	(void) fprintf(stderr, "couldn't open %s for writing\n", buf);
	return;
    }

    (void) fprintf(datap,"%s\n%s\n%s\n%s\n",
"-----------------------------------------------------------------------------",
"         Op Response           Start         Unique  File",
"       Code     Time            Time        File Id  Length  Offset  Size",
"-----------------------------------------------------------------------------");

    p = saveary;
    while(p) {
	(void) fprintf(datap, "%11s %8.3f %19.3f %8d",
		Ops[p->op].name,
		(p->rtv_sec * 1000.0) + (p->rtv_usec / 1000.0),
		(p->stv_sec * 1000.0) + (p->stv_usec / 1000.0),
		p->unique_num);
	if (p->op == NFSPROC3_READ || p->op == NFSPROC3_WRITE ||
		p->op == NFSPROC_READ || p->op == NFSPROC_WRITE) {
	    (void) fprintf(datap,
		" %8d %8d %5d\n", p->length, p->offset, p->count);
	}
	else if (p->op == NFSPROC3_SETATTR || p->op == NFSPROC3_CREATE ||
		p->op == NFSPROC_SETATTR || p->op == NFSPROC_CREATE) {
	    if (p->flags & TRUNCATE_OPERATION) {
		(void) fprintf(datap, "  %s", "TRUNCATE");
	    }
	    if (p->flags & CREATE_OF_EXISTING_FILE) {
		(void) fprintf(datap, "  %s", "CREATE_EXISTING");
	    }
	    (void) fprintf(datap, "\n");
	}
	else {
		(void) fprintf(datap, "\n");
	}
	p = p->next;
    }

    (void) fprintf(datap,
"-----------------------------------------------------------------------------\n\n");
    (void) fclose(datap);
}
