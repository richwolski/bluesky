#ifndef lint
static char sfs_c_clkSid[] = "@(#)sfs_c_clk.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_clk.c ---------------------
 *
 *      Clock handling.  Routines to read the clock, measure elapsed
 *	time and sleep for a timed interval.
 *
 *.Exported_Routines
 *	void sfs_gettime(struct ladtime	*)
 *	void sfs_elapsedtime(sfs_op_type *, struct ladtime *,
 *				struct ladtime *)
 *	int msec_sleep(int)
 *
 *.Local_Routines
 *	None.
 *
 *.Revision_History
 *	05-Jan-92	Pawlowski	Added raw data dump hooks.
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
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/time.h>
#include <sys/times.h>
 
#include <sys/signal.h>


#include <unistd.h>

#include "sfs_c_def.h"

#if !(defined(USE_GETTIMEOFDAY) || defined(USE_TIMES))
#define USE_GETTIMEOFDAY
#endif

#define CLK_RESOLUTION		10	/* clock resolution in msec */

/*
 * -------------------------  Clock Handling  -------------------------
 */

#if defined(USE_GETTIMEOFDAY)
/*
 * The time that the test first starts, all times are relative to this
 * value to prevent integer overflows.
 */
extern struct ladtime	test_start = {0, 0, 0};

/*
 * Get the time of day, offset by 'tz_ptr', and return it in 'time_ptr'.
 * This is a local time of day interface to allow support for system
 * dependent clock resolution.
 */
void
sfs_gettime(
    struct ladtime	*time_ptr)
{
	time_t		t;
	struct timeval	tv;

	(void) gettimeofday(&tv, NULL);
	/*
	 * Use standard time function to get epoch time since 1970 for
	 * setattr/create.
	 */
	t = time((time_t *)NULL);

	if (test_start.sec == 0 && test_start.usec == 0) {
		test_start.sec = tv.tv_sec;
		test_start.usec = 0;
	}
	time_ptr->sec = tv.tv_sec - test_start.sec;
	time_ptr->usec = tv.tv_usec;

	time_ptr->esec = (int32_t)t;
} /* sfs_gettime */
#endif /* USE_GETTIMEOFDAY */

#if defined(USE_TIMES)
static uint32_t	test_start = 0;

void
sfs_gettime(
	struct ladtime	*time_ptr)
{
	time_t		t;
	uint32_t	clock_ticks;
	struct tms	tms;
	int32_t		ticks_per_sec = 100;

	/*
	 * Try use possible conversions from least accurate to most accurate
	 */
#if defined(HZ)
	ticks_per_sec = HZ;
#endif /* HZ */
#if defined(CLK_TCK)
	ticks_per_sec = CLK_TCK;
#endif /* CLK_TCK */
#if defined(_SC_CLK_TCK)
	ticks_per_sec = sysconf(_SC_CLK_TCK);
#endif /* _SC_CLK_TCK */

	if ((clock_ticks = (uint32_t)times(&tms)) == -1) {
		(void) fprintf(stderr, "%s: can't get time of day from system: %s\n",
			sfs_Myname, strerror(errno));
		(void) generic_kill(0, SIGINT);
		exit(175);
	}

	/*
	 * Use standard time function to get epoch time since 1970 for
	 * setattr/create.
	 */
	t = time((time_t *)NULL);

	if (test_start == 0) {
		test_start = clock_ticks;
	}

	time_ptr->sec = (clock_ticks - test_start) / ticks_per_sec;
	time_ptr->usec = ((clock_ticks - test_start) -
					(time_ptr->sec * ticks_per_sec)) *
					(1000000/ticks_per_sec);
	time_ptr->esec = (int32_t)t;
}
#endif /* USE_TIMES */

/*
 * Compute the elapsed time between 'stop_ptr' and 'start_ptr', and
 * add the result to the time acculated for operation 'op_ptr'.
 */
void
sfs_elapsedtime(
	sfs_op_type *	op_ptr,
	struct ladtime *	start_ptr,
	struct ladtime *	stop_ptr)
{
	double	msec2;
	struct ladtime time_ptr1, time_ptr2;

	/* get the elapsed time */
	if (stop_ptr->usec >= 1000000) {
		stop_ptr->sec += (stop_ptr->usec / 1000000);
		stop_ptr->usec %= 1000000;
	}

	if (stop_ptr->usec < start_ptr->usec) {
		stop_ptr->usec += 1000000;
		stop_ptr->sec--;
	}

	if (stop_ptr->sec < start_ptr->sec) {
		stop_ptr->sec = 0;
		stop_ptr->usec = 0;;
	} else {
		stop_ptr->usec -= start_ptr->usec;
		stop_ptr->sec -= start_ptr->sec;
	}

	/* count ops that take zero time */
	if ((stop_ptr->sec == 0) && (stop_ptr->usec == 0))
		op_ptr->results.fast_calls++;

	/* add the elapsed time to the total time for this operation */

	time_ptr1 = op_ptr->results.time;
	time_ptr2 = *stop_ptr;

	ADDTIME(time_ptr1, time_ptr2);

	op_ptr->results.time = time_ptr1;
	stop_ptr = &time_ptr2;

	/* square the elapsed time */
	msec2 = (stop_ptr->sec * 1000.0) + (stop_ptr->usec / 1000.0);
	msec2 *= msec2;

	/* add the squared elapsed time to the total of squared elapsed time */
	op_ptr->results.msec2 += msec2;

	/*
	 * Log this point if logging is on. The (op_ptr - Ops)
	 * calculation is a baroque way of deriving the "NFS Operation
	 * code" we just executed--given available information.
	 *
	 * stop_ptr at this time contains the *elapsed* time, or
	 * response time of the operation.
	 */
	 log_dump(start_ptr, stop_ptr, op_ptr - Ops);

} /* sfs_elapsedtime */

long cumulative_resets;
long cumulative_adjusts;
long msec_calls;
/*
 * Use select to sleep for 'msec' milliseconds.
 * Granularity is CLK_RESOLUTION msec.
 * Return the amount we actually slept.
 */
int
msec_sleep(
	int 		msecs)
{
	int		actual_msecs;
	static long	cumulative_error_msecs = 0;
	int		select_msecs;
	struct timeval	sleeptime;
	int		Saveerrno;
	struct ladtime	start;
	struct ladtime	end;

	sfs_gettime(&start);

	select_msecs = msecs + cumulative_error_msecs;
        if (select_msecs < 0) {
		sleeptime.tv_sec = 0;
		sleeptime.tv_usec = 0;
        } else {
		sleeptime.tv_sec = select_msecs / 1000;
		sleeptime.tv_usec = (select_msecs % 1000) * 1000;
        }

	/* sleep */
	if (select(0, NULL, NULL, NULL, &sleeptime) == -1) {
		if (errno != EINTR) {
			Saveerrno = errno;
			(void) fprintf(stderr, "%s: select failed ",
								sfs_Myname);
			errno = Saveerrno;
			perror("select");
			(void) generic_kill(0, SIGINT);
			exit(176);
		}
	}
	sfs_gettime(&end);

	SUBTIME(end, start);
	actual_msecs = ((end.sec * 1000) + (end.usec / 1000));

	cumulative_error_msecs += (msecs - actual_msecs);
	if(cumulative_error_msecs > 100 ||
		cumulative_error_msecs < -100) /* Clip tops */
	{
			cumulative_error_msecs = 0;
			cumulative_resets++;
	}
	else
	{
		if(cumulative_error_msecs != 0) 
			cumulative_adjusts++;
	}
	msec_calls++;
	return actual_msecs;
} /* msec_sleep */


/* sfs_c_clk.c */
