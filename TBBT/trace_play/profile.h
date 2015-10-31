#ifndef PROFILE_H
#define PROFILE_H

/* profile.h: header file for profiling routine
 * $Id: profile.h,v 1.2 2002/08/21 22:11:40 ningning Exp $
 * Changes:
 *
 *	$Log: profile.h,v $
 *	Revision 1.2  2002/08/21 22:11:40  ningning
 *	*** empty log message ***
 *	
 */

#include <sys/time.h>
#define MAX_PROFILE_ABOUT_LEN 128
typedef struct {
    struct timeval ts;
    struct timeval in;
    int num;
    char about[MAX_PROFILE_ABOUT_LEN];
} profile_t;
                                                                                                                                   
extern inline void start_profile (profile_t * profile);
extern inline void end_profile (profile_t * profile);
extern inline void print_profile (char * string, profile_t * profile);
extern inline void init_profile (char * string, profile_t * profile);
#endif
