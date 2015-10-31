/*
 * profile.c
 *
 * $Id: profile.c,v 1.1 2002/08/21 22:08:01 ningning Exp $
 * Changes:
 *	$Log: profile.c,v $
 *	Revision 1.1  2002/08/21 22:08:01  ningning
 *	*** empty log message ***
 *	
 */

#include <sys/time.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "profile.h"
#include "rfs_assert.h"
//#include "rfs_c_def.h"
extern FILE * profile_fp;

static struct timezone tz;
inline void calculate_interval 
	(struct timeval * ts, struct timeval * te, struct timeval * interval)
{
	if (te->tv_usec < ts->tv_usec) {
		if (te->tv_sec <= ts->tv_sec) {
			printf ("te->tv_sec %d ts->tv_sec %d\n", te->tv_sec, ts->tv_sec);
			printf ("te->tv_usec %d ts->tv_usec %d\n", te->tv_usec, ts->tv_usec);
		}
		RFS_ASSERT (te->tv_sec > ts->tv_sec);
		te->tv_usec += 1000000;
		te->tv_sec -= 1;
	}

	interval->tv_sec  = te->tv_sec - ts->tv_sec;
	interval->tv_usec = te->tv_usec - ts->tv_usec;
	if (interval->tv_usec > 1000000) {
		if (interval->tv_usec > 2000000) {
			printf ("interval->tv_sec %d interval->tv_usec %d \n", interval->tv_sec, interval->tv_usec);
			printf ("ts->tv_sec %d ts->tv_usec %d \n", ts->tv_sec, ts->tv_usec);
			printf ("te->tv_sec %d te->tv_usec %d \n", te->tv_sec, te->tv_usec);
		}
		/* Sometimes it can happend that te->tv_usec > 1000000 */
		interval->tv_sec += 1;
		interval->tv_usec -= 1000000;
		RFS_ASSERT (interval->tv_usec < 1000000);
	}
}

inline void normalize_profile (int pos, struct timeval * time)
{
	if (!(time->tv_sec >=0 && time->tv_usec >=0 && time->tv_usec < 2000000)) {
		printf ("pos %d tv_sec %d tv_usec %d\n", pos, time->tv_sec, time->tv_usec);
	};
	RFS_ASSERT (time->tv_sec >=0 && time->tv_usec >=0 && time->tv_usec < 2000000);
	while (time->tv_usec >= 1000000) {
		time->tv_usec -= 1000000;
		time->tv_sec += 1;
	}
}

inline void start_real_profile (profile_t * profile)
{
	start_profile(profile);
}

inline void end_real_profile (profile_t * profile)
{
	end_profile(profile);
}

inline void start_profile (profile_t * profile)
{
/*
	if (strlen(profile->about) < 3) {
		printf ("total_profile address: %x %x\n", &total_profile, profile);
	}
*/

	gettimeofday(&(profile->ts), &tz);
	normalize_profile (1, &(profile->ts));
}

inline void end_profile (profile_t * profile)
{
	struct timeval te, teorg;
	struct timeval * ts; 
	struct timeval * in;
	struct timeval oldin;
	
/*
	//printf ("end_profile %s\n", profile->about);

	if (strlen(profile->about) < 3) {
		printf ("total_profile address: %x %x\n", &total_profile, profile);
	}
*/

	oldin = profile->in;
	in = &(profile->in);
	ts = &(profile->ts);

	gettimeofday(&te, &tz);
	normalize_profile (2, &te);
	teorg = te;

	RFS_ASSERT (te.tv_sec >= ts->tv_sec);
	RFS_ASSERT (te.tv_usec >=0 && ts->tv_usec >=0);
	while (te.tv_usec < ts->tv_usec) {
		if (te.tv_sec <= ts->tv_sec) {
		 	printf ("%s ts.tv_sec %d ts.tv_usec %d\n", profile->about, ts->tv_sec, ts->tv_usec);
			printf ("teorg.tv_sec %d teorg.tv_usec %d\n", teorg.tv_sec, teorg.tv_usec);
		}
		RFS_ASSERT (te.tv_sec > ts->tv_sec);
		te.tv_usec += 1000000;
		te.tv_sec -= 1;
	}

	if (!(in->tv_sec >=0 && in->tv_usec >=0)) {
		printf ("in->tv_sec %d, in->tv_usec %d\n", in->tv_sec, in->tv_usec);
	};
	RFS_ASSERT (in->tv_sec >=0 && in->tv_usec >=0);
	in->tv_sec  += te.tv_sec - ts->tv_sec;
	in->tv_usec += te.tv_usec - ts->tv_usec;
	normalize_profile (3, in);

	if (!(in->tv_sec >=0 && in->tv_sec <864000)) {
		 printf (" ts.tv_sec %d ts.tv_usec %d\n", ts->tv_sec, ts->tv_usec);
		 printf (" te.tv_sec %d te.tv_usec %d\n", te.tv_sec, te.tv_usec);
		 printf (" in.tv_sec %d in.tv_usec %d\n", in->tv_sec, in->tv_usec);
		 printf (" oldin.tv_sec %d oldin.tv_usec %d\n", oldin.tv_sec, oldin.tv_usec);
	}

	profile->num ++;
	profile->ts = teorg;
}

inline void init_profile (char * string, profile_t * profile)
{
	RFS_ASSERT (strlen(string) < sizeof (profile->about));
	memset (profile, 0, sizeof(profile_t));
	strcpy (profile->about, string);
}

inline int calculate_avg_timeval (struct timeval * in, int num)
{
	unsigned long long i;
	int ret;

	if (in->tv_sec < 2000) {
		return ((in->tv_sec*1000000+in->tv_usec)/num );
	} else {
		i = ((unsigned long long)in->tv_sec)*1000000 + in->tv_usec;
		i/= num;
		RFS_ASSERT (i<2000000000);
		ret = i;
		return ret;
	}
}

inline void print_profile (char * string, profile_t * profile)
{
	struct timeval * ts = &(profile->ts);
	struct timeval * in = &(profile->in);

/*
	if (strcmp (string, profile->about)) {
		printf ("print_profile string %s about %s\n", string, profile->about);
	}
*/

	//RFS_ASSERT (!strcmp (string, profile->about));
	if (in->tv_usec<0 || in->tv_usec>1000000) {
		printf ("%s in->tv_usec %d, in->tv_sec %d num %d\n", profile->about, in->tv_usec, in->tv_sec, profile->num);
	}

	RFS_ASSERT (in->tv_usec>=0 && in->tv_usec<1000000);
	
	if (!(in->tv_sec >=0 && in->tv_sec <864000)) {
		 printf ("%s ts.tv_sec %d ts.tv_usec %d\n", profile->about, ts->tv_sec, ts->tv_usec);
		 printf ("%s in.tv_sec %d in.tv_usec %d\n", profile->about, in->tv_sec, in->tv_usec);
	}
	RFS_ASSERT (in->tv_sec >=0 && in->tv_sec <864000);	/* it's about 10 days */

	if (profile->num == 0) {
		printf("... %40s %3d.%06d num %d \n", profile->about, in->tv_sec, in->tv_usec, 
			profile->num);

		if (profile_fp) {
			fprintf(profile_fp, "... %40s %3d.%06d num %d \n", profile->about, in->tv_sec,
				in->tv_usec, profile->num );
			//perror("print_profile_1");
		}
	} else {

		int avg = calculate_avg_timeval (in, profile->num);
		printf("... %40s %3d.%06d num %d avg %d \n", profile->about, in->tv_sec, in->tv_usec, 
			profile->num, avg);

		if (profile_fp) {
			fprintf(profile_fp, "... %40s %3d.%06d num %d avg %d \n", profile->about, in->tv_sec,
				in->tv_usec, profile->num, avg);
		}

/*
		printf("... %40s %3d.%06d num %d avg %d \n", string, in->tv_sec, in->tv_usec, 
			profile->num, (in->tv_sec*1000000+in->tv_usec)/profile->num );

		if (profile_fp) {
			fprintf(profile_fp, "... %40s %3d.%06d num %d avg %d \n", string, in->tv_sec,
				in->tv_usec, profile->num, (in->tv_sec*1000000+in->tv_usec)/profile->num );
		}
*/
	}
}
