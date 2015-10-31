#ifndef lint
static char sfs_c_rndSid[] = "@(#)sfs_c_rnd.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_rnd.c ---------------------
 *
 *      Random number generator.
 *
 *.Exported_routines
 *	int32_t sfs_random(void)
 *	void sfs_srandom(int)
 *
 *.Local_routines
 *	double ran(void)
 *	int32_t spec_rand(void)
 *	void spec_srand(int)
 *
 *.Revision_History
 *	28-Nov-91	Teelucksingh	ANSI C
 *	01-Aug-91	Wiryaman	sfs_srandom() and sfs_random()
 *					now use spec_srand() and spec_rand()
 *					instead of srandom() and random().
 *      17-Apr-91       Wittle		Created.
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

/*
 * Here's the source for the random number generator that SPEC uses.
 * The function to be called is "spec_rand" which returns an integer
 * between 1 and MAX_INT-1.
 *
 * One question we may wanna think about is the seeding of the random
 * number generator. Do we start with the same seed everytime (for
 * repeatability) or use a array of possible seeds (some seeds are better
 * than others and SPEC prople mention that they have a list of 15
 * "good" seeds).
 */


/*
 * -------------------------  Static Declarations  -------------------------
 */

static int32_t seedi = 2231;


/*
 * -------------------------  External Definitions  -------------------------
 */

static double ran(void);
static int32_t spec_rand(void);
static void spec_srand(int);

/*
 * -----------------------  Random Number Routines  -----------------------
 */


/*
 * Seed the random number generator.
 */
static void
spec_srand(
    int seed)
{
    seedi = seed;
}


/*
 * Returns a random number.
 */
static int32_t
spec_rand(void)
{
    (void) ran();
    return(seedi);
}


/*
 * Compute the next random number.
 */
static double
ran(void)

/* See "Random Number Generators: Good Ones Are Hard To Find", */
/*     Park & Miller, CACM 31#10 October 1988 pages 1192-1201. */
/***********************************************************/
/* THIS IMPLEMENTATION REQUIRES AT LEAST 32 BIT INTEGERS ! */
/***********************************************************/

#define _A_MULTIPLIER  16807L
#define _M_MODULUS     2147483647L /* (2**31)-1 */
#define _Q_QUOTIENT    127773L     /* 2147483647 / 16807 */
#define _R_REMAINDER   2836L       /* 2147483647 % 16807 */
{
    int32_t	lo;
    int32_t	hi;
    int32_t	test;

    hi = seedi / _Q_QUOTIENT;
    lo = seedi % _Q_QUOTIENT;
    test = _A_MULTIPLIER * lo - _R_REMAINDER * hi;
    if (test > 0) {
	seedi = test;
    } else {
	seedi = test + _M_MODULUS;
    }
    return((float) seedi / _M_MODULUS);
}

/*
 * Local interface to seed random number generator.
 */
void
sfs_srandom(
    int		seed)
{
    spec_srand(seed);
}


/*
 * Local interface to obtain a random number.
 */
int32_t
sfs_random(void)
{
    return(spec_rand());
}

static struct r_array {
	int n1;
	int n2;
} *r_array;

static int r_length = 0;

static int
r_array_compare(const void *i, const void *j)
{
	if (((struct r_array *)i)->n2 > ((struct r_array *)j)->n2)
		return (1);
	if (((struct r_array *)i)->n2 < ((struct r_array *)j)->n2)
		return (-1);
	return (0);
}

int
init_rand_range(int length)
{
	int i;

	/*
	 * If array already exists free it
	 */
	if (r_length != 0) {
		(void)free(r_array);
		r_length = 0;
	}

	/*
	 * If length is zero just free memory and return
	 */
	if (length == 0)
		return (0);

	/*
	 * Allocate array of sequential numbers and random numbers
	 */
	if ((r_array = malloc(length * sizeof(struct r_array))) == NULL)
		return (1);

	r_length = length;

	/*
	 * Initialize array of sequential values and random values
	 */
	for (i = 0; i < length; i++) {
		r_array[i].n1 = i;
		r_array[i].n2 = sfs_random();
	}

	/*
	 * Sort random array values to put sequential values in random order
	 */
	qsort(r_array, length, sizeof(struct r_array), r_array_compare);

	return (0);
}

int
rand_range(int index)
{
	return (r_array[index].n1);
}
/* sfs_c_rnd.c */
