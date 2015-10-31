#ifndef lint
static char sfs_c_subSid[] = "@(#)sfs_c_sub.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_sub.c ---------------------
 *
 *      Subroutines common to both sfs and sfs_prime_client.
 *
 *.Exported_Routines
 *	int generic_kill(int, int)
 *	void generic_catcher(int)
 *	char * lad_timestamp(void)
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
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <sys/signal.h>

#include <unistd.h>

#include "sfs_c_def.h"

/*
 * Common data shared between sfs and sfs_prime
 *
 * Values for invalid runs
 */
char *invalid_str[INVALID_MAX] = {
	"No error",
	"Unknown",
	"IO distribution file",
	"Mix file",
	"Runtime",
	"Access percentage",
	"Append percentage",
	"KB per block",
	"Number client directories",
	"Fileset delta",
	"Max biod reads",
	"Number symlinks",
	"Max biod writes",
	"Warmup time",
	"No good calls",
	"Failed RPC calls",
	"Op mix missed",
};

/*
 * -------------------------  Signal Handlers  -------------------------
 */

/*
 * Signal Sender.  Send signal 'sig' to process 'pid'.
 */
int
generic_kill(
    int 	pid,
    int 	signal)
{
    if (DEBUG_PARENT_SIGNAL)
	(void) fprintf(stderr,
			"%s: sending Pid %d Signal %d\n", sfs_Myname, pid ,signal);
    return(kill((pid_t)pid, signal));

} /* generic_kill */


/*
 * Signal Handler.  Catch and reset the handler for signal 'i'.
 */
void
generic_catcher(
    int 	i)
{
#if !(defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    (void) signal(i, generic_catcher);
#endif
    if (DEBUG_CHILD_SIGNAL)
	(void) fprintf(stderr, "%s: caught Signal %d\n", sfs_Myname, i);
    (void) fflush(stderr);

} /* generic_catcher */



/*
 * get the date and time and return a string, remove the END-OF-LINE (\n)
 */
char *
lad_timestamp(void)
{
    static time_t	run_date;
    static char		date_string[26];

    run_date = time((time_t *)NULL);
    (void) strncpy((char *) date_string, (char *) ctime(&run_date), 24);
    return(date_string);

} /* lad_timestamp */

int
set_debug_level(char *s)
{
    unsigned int i;
    unsigned int first;
    unsigned int last;
    unsigned int level = 0;

    if (s == NULL || *s == '\0')
	return(0);

    for (;;) {
	if (*s == ',') {
	    s++;
	    continue;
	}

	/* find first flag to set */
	i = 0;
	while (isdigit(*s))
	    i = i * 10 + (*s++ - '0');
	first = i;

	/* find last flag to set */
	if (*s == '-') {
	    i = 0;
	    while (isdigit(*++s))
		i = i * 10 + (*s - '0');
	}
	last = i;

	if (first != 0 && last != 0 && first < 32 && last < 32) {
	    for (i = first - 1; i < last; i++) {
		level |= (1 << i);
	    }
	}

	/* more arguments? */
	if (*s++ == '\0')
		return (level);
    }
}

/* sfs_c_sub.c */
