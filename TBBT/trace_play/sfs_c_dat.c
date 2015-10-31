#ifndef lint
static char sfs_c_datSid[] = "@(#)sfs_c_dat.c	2.1	97/10/23";
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
 *
 * ------------------------- sfs_c_dat.c --------------------------
 *
 *      Space declarations for sfs.
 *
 *.Revision_History
 *      11-Jul-94	ChakChung Ng	Add codes for NFS/v3
 *      24-Aug-92	Wittle		New file set access.
 *      05-Jan-92	Pawlowski	Added hooks for raw data dump.
 *      04-Dec-91	Bruce Keith	Include string.h for SYSV/SVR4.
 *	17-May-90	Richard Bean	Created.
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

#include <unistd.h>

#include "sfs_c_def.h"
#include "sfs_m_def.h"


/*
 * -------------------------  NFS Operations  -------------------------
 */

/*
 * RPC timeout values by call type.
 * Index'ed by #defines in sfs_def.h.
 */
struct timeval *Nfs_timers;

struct timeval Nfs_udp_timers[] = {

	/* secs  usecs */
	{  10,	  0 },		/* All commands during initialization phase */
	{   1,	  0 },		/* Lookup during warmup and test run phases */
	{   2,	  0 },		/* Read during warmup and test run phases */
	{   3,	  0 },		/* Write during warmup and test run phases */
};

/*
 * TCP is a "reliable" protocol so timeouts should never occur.  Set
 * the values to be large enough to ensure all servers will respond
 * but not too large so a broken implementation will still complete.
 */
struct timeval Nfs_tcp_timers[] = {

	/* secs  usecs */
	{  60,	  0 },		/* All commands during initialization phase */
	{  60,	  0 },		/* Lookup during warmup and test run phases */
	{  60,	  0 },		/* Read during warmup and test run phases */
	{  60,	  0 },		/* Write during warmup and test run phases */
};

/*
 * -----------------------  Transfer Size Distribution  -----------------------
 *
 * Over-the-wire transfer sizes are divided into 2 cases: read and write.
 * For each case, a percentile rank determines the basic size unit of the
 * transfer which is multiplied by a count to give the total size for the
 * percentile.
 *
 * The block transfer size distribution is specified by a table of values.
 * The first column gives the percent of operations that will be a
 * specific block transfer size.  The second column gives the number of
 * blocks units that will be transferred.  Normally the block unit size
 * is 8KB.  The third column is a boolean specifying whether a trailing
 * fragment block should be transferred.  The fragment size for each transfer
 * is a random multiple of 1 KB, up to the block size - 1 KB.  Two tables
 * are needed, one for read operation and one for write operations.  The
 * following table gives the default distributions.
 *
 * 	Read  - Default Block Transfer Size Distribution Table
 * percent   block count   fragment	resulting transfer (8KB block size)
 * -------   -----------   --------	-----------------------------------
 *    0        0             0 		 0%    1 -   7 KB
 *   85        1             0 		85%    9 -  15 KB
 *    8        2             1 		 8%   17 -  23 KB
 *    4        4             1 		 4%   33 -  39 KB
 *    2        8             1 		 2%   65 -  71 KB
 *    1       16             1 		 1%  129 - 135 KB
 *
 * 	Write  - Default Block Transfer Size Distribution Table
 * percent   block count   fragment	resulting transfer (8KB block size)
 * -------   -----------   --------	-----------------------------------
 *   49        0             1		49%    1 -   7 KB
 *   36        1             1		36%    9 -  15 KB
 *    8        2             1           8%   17 -  23 KB
 *    4        4             1		 4%   33 -  39 KB
 *    2        8             1		 2%   65 -  71 KB
 *    1       16             1		 1%  129 - 135 KB
 *
 * The user may specify a a different distribution by using the '-b' option.
 * The format for the block size distribution file consists of the first
 * three columns given above: percent, block count, and fragment.  Read
 * and write distribution tables are identified by the keywords "Read" and
 * "Write".  An example input file, using the default values, is given below:
 *
 * 		Read
 *		 0  0  0
 *		85  1  0
 *		 8  2  1
 *		 4  4  1
 *		 2  8  1
 *		 1 16  1
 *		Write
 *		49  0  1
 *		36  1  1
 *		 8  2  1
 *	         4  4  1
 *               2  8  1
 *               1 16  1
 *
 * A second parameter controlled by the block transfer size distribution table
 * is the network transport packet size.  The distribution tables define the
 * relative proportion of full blocks packets to fragment packets.  For
 * instance, the default tables have been constructed to produce a specific
 * distribution of ethernet packet sizes for i/o operations by controlling
 * the amount of data in each packet.  The write packets produced consist
 * of 50% 8-KB packets, and 50% 1-7 KB packets.  The read packets consist
 * of 85% 8-KB packets, and 15% 1-7 KB packets.  These figures are
 * determined by multiplying the percentage for the type of transfer by
 * the number of blocks and fragments generated, and adding the totals.
 * These conmputations are performed below for the default block size
 * distribution tables:
 *
 * 		Read		blocks		fragments
 *		 0  0  0	  0		  0
 *		85  1  0	 85	  	  0
 *		 8  2  1	 16       	  8
 *		 4  4  1	 16	  	  4
 *		 2  8  1	 16	  	  2
 *		 1 16  1	 16	  	  1
 *				---		---
 *				149 (90%)	 15 (10%)
 *
 *		Write
 *		49  0  1	  0		 49
 *		36  1  1	 36		 36
 *		 8  2  1	 16		  8
 *	         4  4  1	 16		  4
 *               2  8  1	 16		  2
 *               1 16  1	 16		  1
 *				---		---
 *				100 (50%)	100 (50%)
 *
 *
 */
static sfs_io_op_dist_type Default_read_size_dist[] = {
	/* percentile	8KB xfers	fragments 			*/
	{	 0,	 0,		0 },	/*   0%    1 -   7 KB	*/
	{	85,	 1,		0 },	/*  85%    8       KB	*/
	{	93,	 2,		1 },	/*   8%   17 -  23 KB	*/
	{	97,	 4,		1 },	/*   4%   33 -  39 KB	*/
	{	99,	 8,		1 },	/*   2%   65 -  71 KB	*/
	{      100,	16,		1 },	/*   1%  129 - 135 KB	*/
};

static sfs_io_op_dist_type Default_write_size_dist[] = {
	/* percentile	8KB xfers	fragments 			*/
	{	49,	 0,		1 },	/*  49%    1 -   7 KB	*/
	{	85,	 1,		1 },	/*  36%    9 -  15 KB	*/
	{	93,	 2,		1 },	/*   8%   17 -  23 KB	*/
	{	97,	 4,		1 },	/*   4%   33 -  39 KB	*/
	{	99,	 8,		1 },	/*   2%   65 -  71 KB	*/
	{      100,	16,		1 },	/*   1%  129 - 135 KB	*/
};

static sfs_io_dist_type Default_io_dist = {
	Default_read_size_dist,		/* read size distribution */
	Default_write_size_dist,	/* write size distribution */
	17,				/* max file size in Block_size units */
	1.64,				/* average read ops / request */
	2				/* average write ops / request */
};

sfs_io_file_size_dist Default_file_size_dist[] = {
	/* percentage	KB size */
	{	 33,	  1},			/* 33% */
	{	 54,	  2},			/* 21% */
	{	 67,	  4},			/* 13% */
	{	 77,	  8},			/* 10% */
	{	 85,	 16},			/*  8% */
	{	 90,	 32},			/*  5% */
	{	 94,	 64},			/*  4% */
	{	 97,	128},			/*  3% */
	{	 99,	256},			/*  2% */
	{	100,   1024},			/*  1% */
	{	  0,	  0}
};

/*
 * -------------------------  Remote File Information  -------------------------
 */
int  Num_io_file_sizes;			/* # of different size of files */
int  Num_io_files;			/* # of files used for i/o */
int  Num_non_io_files;			/* # of non-i/o regular files */
int  Num_dirs;				/* # of pre-created directories */
int  Num_dir_files;			/* # of directories */
int  Num_symlinks;			/* # of pre-created symlinks */
int  Num_symlink_files;			/* # of symlinks  */

int  Num_working_io_files;		/* # of i/o files in working set */
int  Num_working_non_io_files;		/* # of non i/o files in working set */
int  Num_working_dirs;			/* # of dirs in working set */
int  Num_working_symlinks;		/* # of symlinks in working set */
int  files_per_megabyte;		/* # of files created of each MB */


sfs_io_dist_type *Io_dist_ptr=	 /* block transfer distribution info */
			&Default_io_dist;

sfs_fh_type		*Io_files;	/* list of i/o files  */
sfs_fh_type		*Non_io_files;	/* list of non-i/o files */
sfs_fh_type		*Dirs;		/* list of directories */
sfs_fh_type		*Symlinks;	/* list of symlinks */

sfs_fh_type	*Cur_file_ptr;		/* current file */
char	Cur_filename[SFS_MAXNAMLEN];	/* current dir entry name */
char	Filespec[SFS_MAXNAMLEN]	/* sprintf spec for file names */
		     = "file_en.%05d";
char	Dirspec[SFS_MAXNAMLEN]	/* sprintf spec for directory names */
		     = "dir_ent.%05d";
char	Symspec[SFS_MAXNAMLEN]	/* sprintf spec for symlink names */
		     = "symlink.%05d";

/*
 * -------------------------  Run Parameters -------------------------
 */

int	nfs_version;
sfs_phase_type	Current_test_phase;	/* current phase of the test */

sfs_fh_type 	Export_dir;			/* filehandle for exported fs */
CLIENT *	NFS_client;			/* RPC client handle */
CLIENT * 	NFS_client_recv;	/* RPC client handle used for recv_thread */

bool_t	Timed_run = TRUE;	       		/* Timed run or call target ? */
int	Runtime = DEFAULT_RUNTIME;     		/* seconds in benchmark run */
int	Warmuptime = DEFAULT_WARMUP;   		/* seconds to warmup */
int	Access_percent = DEFAULT_ACCESS;        /* % of file set to access */
int	Append_percent = DEFAULT_APPEND;        /* % of writes that append */
int	Fss_delta_percent = DEFAULT_DELTA_FSS;	/* allowed change to file set */
int	Kb_per_block = DEFAULT_KB_PER_BLOCK;	/* i/o pkt block sz in KB */
int	Bytes_per_block = DEFAULT_KB_PER_BLOCK * 1024;/* i/o pkt sz in bytes */
int	Num_failed_lookup = DEFAULT_FAILED_LOOKUP; /* percent failed lookups */
int	Testop = -1;				/* test mode operation number */
int	Interactive = 0;			/* test synchronized by user*/
int	Populate_only = 0;			/* populate test dirs & quit */
int	Debug_level = 0xFFFF;			/* debugging switch */


/*
 * ---------------------  Biod Simulation Variables ---------------------
 */

int	Biod_max_outstanding_writes = DEFAULT_BIOD_MAX_WRITE;
int	Biod_max_outstanding_reads = DEFAULT_BIOD_MAX_READ;


/*
 * -------------------  File Set Size Control -------------------------
 */

int	avg_bytes_per_file = 136*1024;	/* calculated average file size */
int 	Base_fss_bytes = 0;		/* base file set size in bytes */
int	Most_fss_bytes = 0;		/* most bytes ever in file set */
int	Least_fss_bytes = 0;		/* least bytes ever in file set */
int	Limit_fss_bytes = 0;		/* target upper limit on fileset size */
int	Cur_fss_bytes = 0;		/* bytes currently in file set */
int	Total_fss_bytes = 0;		/* Total bytes created */


/*
 * -------------  Per Child Load Generation Rate Variables  -----------
 */

int	Msec_per_period;	/* total msec during the current run period */


/*
 * -------------------------  Miscellaneous  -------------------------
 */

struct ladtime	Cur_time;		/* current time of day */
struct ladtime	Starttime;		/* start of test */
int		start_run_phase = 0; 

char	lad_hostname[HOSTNAME_LEN];
/* sfs_c_dat.c */
