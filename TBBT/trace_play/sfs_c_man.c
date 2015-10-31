#ifndef lint
static char sccsid_hp[] = "@(#)sfs_c_man.c	2.1	97/10/23";
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

/*
 * Generates an artifical NFS client load based on a given mix of operations,
 * and block transfer distribution.
 *
 * Usage: sfs [-l load] [-p procs] [-w warmup] [-t time]
 *               [-m mix_file] [-B block_size] [-b blocksz_file]
 *		 [-f file_set_delta] [-a access_pnct]
 *		 [-A append_pcnt] [-D dir_cnt] [-F file_cnt] [-S symlink_cnt]
 *		 [-d debug_level] [-i] [-P] [-T op_num]
 *		 [-V validation_level] [-z] [-Q]
 *		 [-R biod_reads] [-W biod_writes]
 *		 [-M prime_client_hostname] [-N client_cnt]
 *
 * NOTE: REFER TO SFS MAN PAGE (sfs.1) FOR A DESCRIPTION OF ALL
 * 	 SFS OPTIONS
 *
 *
 * Single Client Options
 *
 * option	   description				 	    default
 * --------------- ------------------------------------------------ -----------
 * -a access_pcnt  % of file set to access 			    20% access
 * -A append_pcnt  % of writes that append rather than overwrite    70% append
 * -b blocksz_file file specifying distribution of block xfer sizes (see below)
 * -B block_size   # of KB in block, up to 8 KB			    8 KB
 * -Q              Do TCP connection for NFS rather than UDP        off
 * -d debug_level  debug level (higher number gives more output)    off
 * -D dir_cnt	   # directories used for directory operations      20 dirs
 * -f fileset_delta % change in file set size allowed               10%
 * -F file_cnt	   # files used for read and write operations	    100 files
 * -i 		   interactive; wait for input before starting test off
 * -l load	   # NFS calls/second to generate from each client  60 calls/sec
 * -m mix_file	   file specifying NFS call distribution	    (see below)
 * -p procs	   # processes used to generate load on each client 7 procs
 * -P		   populate test directories, but don't run a test  off
 * -R biod_reads   max # of outstanding read requests at one time   2 reqs
 * -S symlink_cnt  # symbolic links used for symlink operations     20 symlinks
 * -t time	   # seconds to generate load for the timed test    600 secs
 * -T op_num	   test the NFS operation specified one time        off
 * -V		   validate correctness of server's NFS             off
 * -W biod_writes   max # of outstanding writes req at one time     2 reqs
 * -w warmup	   # secs to generate load before starting test     60 secs
 * -z		   If specified, collect and dump raw data.         off
 *
 *
 * Multi Client Options
 *
 * option	   		description		 	    default
 * ------------------------     ----------------------------------- -----------
 * -M prime_client_hostname	hostname of prime client	    no default
 * -N client_cnt		# client machines in test	    no default
 *
 *
 *
 * Block Transfer Size Distribution
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
 * A second parameter controlled by the block transfer size distribution
 * table is ethernet packet size.  The distribution tables define the
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
 *
 *
 *
 * NFS Operation Mix
 *
 * The operation mix is described assigning a percentage to each type
 * of NFS operation.  The default mix of operations is:
 *
 * operation	percent
 * ---------	-------
 * null		 0
 * getattr	13
 * setattr	 1
 * root		 0
 * lookup	34
 * readlink	 8
 * read		22
 * wrcache	 0
 * write	15
 * create	 2
 * remove	 1
 * rename	 0
 * link		 0
 * symlink	 0
 * mkdir	 0
 * rmdir	 0
 * readdir	 3
 * fsstat	 1
 *
 * The user may specify a a different operation mix by using the '-m' option.
 * The format for the mix file consists of the output from an nfsstat(1)
 * command, which lists an operation count and percentage for each NFS
 * operation.
 *
 *
 * File Set Size
 *
 * !!! needs to be re-written - mew 8/24
 * This still needs to be rewritten
 *   - The default number of i/o files is based on load level.
 *   - For non I/O files, the number of initialized versus empty
 *     slots is hardcoded
 *  - dr 2/8/94
 *
 * The file set used by SFS is determined by 2 factors. First,
 * SFS creates a base set of files.  By default this consists of
 * 100 files for i/o operations, 100 files for non-i/o operations, 20
 * directories, and 20 symbolic links.  These default values can be
 * changed from the command line by using the "-F", "-D", and "-S" options.
 * This file set is divided evenly among the set of processes used to
 * generate load.
 *
 * Then, enough resources are allocated to allow for the eventual creation
 * of new files, directories and symlinks by the creat, link, mkdir,
 * and symlink operations.  The number of extra slots allocated depends
 * on the mix percentages assigned to each of the create and deletion
 * operations, multiplied by an estimate of the total number of operations
 * to be performed.  For instance, the default number of extra files names
 * allocated for non-i/o operations is computed as follows:
 *	300 secs * 60 ops/sec = 18000 total operations
 *	2% create * 18000 ops = 360 create ops
 *	1% remove * 18000 ops = 180 remove ops
 *	260 creates ops - 180 remove ops = 180 extra files to be created.
 * These 90 files are distributed evenly among the processes used to
 * generate load.  With the default settings, no extra directories are
 * created or removed, so no extra allocation is done for directories
 * beyond the base file set.  The same is true for symbolic links.
 *
 * Thus, the total default file set size is:
 *	100 files for i/o operations
 *	280 files for non-i/o operations
 *	 20 directories
 *	 20 symlinks
 *
 * By allocating all required space before the test begins, any space
 * allocation problems encountered by SFS are discovered before the
 * test is started.
 *
 *
 * Program Control
 *
 * Strategy: loop for some number of NFS calls doing a random sleep
 * followed by a call to one of the op generator routines. The routines
 * are called based on a weighting factor determined by the set of
 * default percentages or a mix supplied by the user.
 *
 * The generator routines are able to keep an accurate count of the
 * NFS operations they are generating by using the NFS protocol
 * directly and not going through the kernel.  This eliminates the
 * effects of kernel name caches and retry mechanisms that
 * complicate control of what actually hits the wire.  The calling
 * routine benefits by avoiding having to get the NFS statistics
 * from the kernel because they KNOW what calls they've made.
 *
 * By using the NFS protocol directly :
 *	"lookup" operations sidestep the client kernel name cache,
 *	"getattr" operations avoid the client kernel attribute cache,
 *	"read" operations avoid the client kernel buffer cache,
 *	and so on.
 *
 * A consequence of not going thru the client kernel is that the sfs
 * program must maintain a table of file handles rather than open
 * file descriptors.
 *
 * The parent process starts children to do the real work of generating load.
 * The parent coordinates them so that they all start at the same time, and
 * collects statistics from them when they are done. To coordinate the
 * start up, the parent waits for each child to write one byte into
 * a common log file (opened in append mode to avoid overwriting).
 * After they write a byte the children pause, and the parent send SIGUSR1
 * when it has heard from all of the kids. The children write their statistics
 * into the same common log file and the parent reads and accumulates the
 * statistics and prints them out.
 *
 *
 *.Exported_Routines
 *	int main(int, char*)
 *
 *.Local_Routines
 *	void init_logfile(void)
 *	void usage(void)
 *	int setmix(char *)
 *	int setiodist(FILE *)
 *	int parseiodist(FILE *, int)
 *	void init_iodist(sfs_io_dist_type *)
 *	void init_fss(void)
 *	void init_filedist(void)
 *	int lad_substr(char *, char *)
 *
 *.Revision_History
 *	21-Aug-92	0.1.11	 Wittle File set access code.
 *      14-Jul-92	0.1.9    Teelucksingh
 *				 	Implemented Mark Wittle's proposal to
 *					base File Set Size on peak load value,
 *					added "-L peak_load" option.
 *	10-Jan-92	0.0.0.19 Teelucksingh
 *					Reworked setpgrp() usage to
 *					better handle BSD vs SYSV variations.
 *      04-Jan-92       0.0.0.18 Pawlowski
 *                                      Added raw data dump code.
 *      04-Dec-91	0.0.0.15 Keith
 *					Include string.h if SVR4.
 *	28-Nov-91	0.0.0.13 Teelucksingh
 *					Modified code to use unique sfs /tmp
 *					logfiles; sfs can now be used on
 *					clients that have a shared /tmp area.
 *					Added ANSI C features. Fixed 'multiple
 *					signals from the Prime-Client' problem.
 *					Added code to allow clients to
 *					check for and create client specific
 *					directories under each mount point -
 *					clients share partitions. (code from
 *					M.Molloy).
 *	22-Nov-91		 Wittle Updated program description comment.
 *					Added new op generation code.
 *					Added block_dist_table and block_size
 *					options, removed 8KB packet assumptions.
 *	04-Oct-91	0.0.0.12 Teelucksingh
 *					Changed SFS sources and executables
 *					to use the "prelad" prefix.
 *	23-Sep-91	0.0.0.11 Teelucksingh
 *					Changed format of sfs output.
 *	01-Aug-91	0.0.9 Wiryaman  Use the SPEC random number generator.
 *					Since this RNG cannot take seed = 0,
 *					use child_num+1 instead.
 *	17-Jul-91	0.0.8 Teelucksingh
 *					Enhance multi-client code and
 *					documentation.
 *			      Keith	Map "nhfsstone" to "laddis" in
 *					README, nhfsstone_mgr.c. Create
 *					initial DESCR.SFS for SPEC
 *					submission.
 *	15-Jul-91	0.0.8 Wiryaman	Add getmnt() for ULTRIX
 *	25-Jun-91	0.0.7 Wiryaman	Added validation option to test all
 *					of the NFS operations.
 *	17-Jun-91	0.0.7 Teelucksingh
 *					Added multi-client synchronization
 *					support. By designating a client as
 *					"Prime client" you can synchronize
 *					multi-client SFS execution.
 *	12-May-91	0.0.6 Wittle	Fix standard deviation.
 *	02-May-91	0.0.5 Wittle	Fix SunOS signal bug; use default
 *					warmuptime; add local time routine;
 *					check for calls that underflow elapsed
 *					time measurement; add std deviation
 *					statistics; rework verbose output.
 *					fix init invalid protocol rmdir calls;
 *	15-Apr-91	0.0.4 Wittle	Test can be repeated without removing
 *					test directories - initialization
 *					restores base file set count and sizes.
 *					Fix lack of call rate & mix accuracy -
 *					set op_check before artificially
 *					increasing call_targets.
 *					Don't pre-create files/dirs that are
 *					meant to be created during the test.
 *	10-Mar-91	0.0.3 Wittle	Longer RPC timeout while populating
 *					testdir; strstr() bug fix;
 *					'-i' and '-e' options
 *	06-Mar-91	0.0.2 Wittle	Loop forever pre-filling files.
 *	22-Feb-91	0.0.1 Wittle	Use signal(2) instead of sigset(2).
 *	18-Feb-91	0.0.0 Wittle	Change algorythm for determining i/o
 *					sizes, preserve i/o file working set
 *					by using separate files; bugs fixes.
 *
 *	nhfsstone renamed to laddis
 *
 *	31-Oct-90	2.0.4 Wittle	Many bug fixes.
 *	24-Aug-90	2.0.3 Wittle	Output compatible w/graphing tools.
 *	24-July-90	2.0.2 Wittle	Handle mounting below symlinks.
 *	24-June-90	2.0.1 Wittle	Prefill files with data.
 *	17-May-90	2.0.0 Bean	Rewrote the guts to use NFS
 *					protocol directly.
 *					Cleaned up self-pacing mechanism.
 *	08-Nov-89	Guillermo Roa	Ported original version to DG/UX.
 *	07-Jul-89	Legato Systems	Created.
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
#include <ctype.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <sys/signal.h>

#include <sys/file.h>
#include <fcntl.h>

#include <unistd.h>

extern getmyhostname(char *, int);

#include "sfs_c_def.h"
#include "sfs_m_def.h"

/*
 * -------------------------  External Definitions  -------------------------
 */

/* external routines from RPC and system libraries */
#if defined(SETPGRP_BSD)
extern int setpgrp(int, int);
#endif /* SETPGRP_BSD */
#if defined(SETPGRP_SYSV)
extern pid_t setpgrp(void);
#endif /* SETPGRP_SYSV */

/* forward definitions for local routines */
static void init_logfile(void);
static void usage(void);
static int setmix(char *);
static int setiodist(FILE *);
static int parseiodist(FILE *, int);
static void init_iodist(sfs_io_dist_type *);
static void init_fss(void);
static void init_filedist(void);
static int lad_substr(char *, char *);
static double time_so_far1(void);
static double get_resolution(void);
static void check_clock(void);

int	Tot_client_num_io_files = 0;    /* # of files used for i/o per client */
int	Tot_client_num_non_io_files =   /* # of files used for i/o per client */
				DEFAULT_NFILES;
int	Files_per_dir =		        /* # of pre-created dirs */
				DEFAULT_FILES_PER_DIR;
int	Tot_client_num_symlinks =       /* # of pre-created symlinks/client */
				DEFAULT_NSYMLINKS;
int	Child_num;
char *  Prime_client = NULL;            /* Prime client hostname */
int     Client_num = 1;                 /* My client number */
int     Tcp = 0;                        /* Flag set on command line */
char    *sfs_Myname;                 /* name program invoked under */
int     Log_fd;                         /* log fd */
char    Logname[NFS_MAXNAMLEN];         /* child processes sync logfile */
uid_t   Real_uid;                       /* real uid */
uid_t   Cur_uid;                        /* my uid */
gid_t   Cur_gid;                        /* my gid list */

static char Client_logname[SFS_MAXNAMLEN];

/*
 * -----------------  SFS Main and Initialization Code  -----------------
 */

/*
 * Read the command line arguments, fork off child processes to
 * generate NFS load, and perform the local (ie, on this client)
 * book-keeping for the test and the results.
 */
int
main(
    int		argc,
    char	*argv[])
{

    char	*mix_file;		/* name of mix file */
    char	*iodist_file;		/* name of block i/o dist table file */
    int		children;		/* number of children */
    int		child_num;		/* child index */
    int		total_load;		/* total client load factor */
    float	child_load;		/* per child load factor */
    int		pid;			/* process id */
    FILE	*pid_fp;
    FILE	*iodist_fp;		/* block io dist table file */
    int		i;
    int		c;
    int		Saveerrno;
    int		ret;
    int		nsigs = 32;		/* reasonable default */
    extern char *optarg;
    extern int optind;

    /*
     * Place pid in pid log file
     */
    if ((pid_fp = fopen(SFS_PNT_PID, "a+")) == NULL) {
	perror(SFS_PNT_PID);
	exit(1);
    }

    (void) fprintf(pid_fp, "%d\n", getpid());

    /* Get program name for stderr printing */
    sfs_Myname = argv[0];

    check_clock();
    getmyhostname(lad_hostname, HOSTNAME_LEN);

    init_ops();

/*
 * Get the uid and gid information.
 */
    Real_uid = getuid();
    Cur_gid = getgid();

/*
 * Form a new process group so our syncrhonization signals don't
 * cause our parent shell to exit.  Clear the umask.
 * Default is to use the standard setsid
 */
#ifdef SETPGRP3
    ret = setpgrp3(); /* Work around HP-UX bug */
#else
#ifdef SETPGRP_SYSV
    ret = setpgrp();
#else
#ifdef SETPGRP_BSD
    ret = setpgrp(0, getpid());
#else
    ret = setsid();
#endif /* SETPGRP_BSD */
#endif /* SETPGRP_SYSV */
#endif /* SETPGRP3 */

    if (ret == -1) {
	(void) fprintf(stderr, "%s: failed on setsid/setpgrp\n",
		sfs_Myname);
	exit(95);
    }

    (void) umask(0);

/* Set up default parameters */
    Validate = 0;

    children = DEFAULT_NPROCS;
    total_load = DEFAULT_LOAD;
    mix_file = 0;
    iodist_file = 0;
    Nfs_timers = Nfs_udp_timers;

    /* Parse command line arguments */
    while ((c = getopt(argc, argv, "a:A:b:B:cd:D:f:F:il:m:M:N:p:PQR:S:T:t:V:W:w:z")) != EOF)
	switch (c) {

	case 'a': /* Percent of files to access */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal access value %s\n",
					sfs_Myname, optarg);
		exit(96);
	    }
	    Access_percent = atoi(optarg);
	    if (Access_percent < 0 || Access_percent > 100) {
		(void) fprintf(stderr,
		       "%s: %% access must be between 0 and 100\n",
			sfs_Myname);
		exit(97);
	    }
	    break;

	case 'A': /* Percent of writes that append */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal append value %s\n",
				sfs_Myname, optarg);
		exit(98);
	    }
	    Append_percent = atoi(optarg);
	    if (Append_percent < 0 || Append_percent > 100) {
		(void) fprintf(stderr,
		       "%s: %% append must be between 0 and 100\n",
			sfs_Myname);
		exit(99);
	    }
	    break;

	case 'b': /* Set block size distribution table from file */
	    if ((iodist_fp = fopen(optarg, "r")) == NULL) {
		Saveerrno = errno;
		(void) fprintf(stderr, "%s: bad block size file",
				sfs_Myname);
		errno = Saveerrno;
		perror(optarg);
		exit(100);
	    }
	    if (setiodist(iodist_fp) < 0) {
		exit(101);
	    }
	    iodist_file = optarg;
	    (void) fclose(iodist_fp);
	    break;

	case 'B': /* Set the per packet maximum block size */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr,
				"%s: illegal block size value %s\n",
				sfs_Myname, optarg);
		exit(102);
	    }
	    Kb_per_block = atoi(optarg);
	    if ((Kb_per_block < 1) ||
		(Kb_per_block > (DEFAULT_MAX_BUFSIZE/1024))) {
		(void) fprintf(stderr,
				"%s: illegal block size value %s\n",
				sfs_Myname, optarg);
		exit(103);
	    }
	    Bytes_per_block = Kb_per_block * 1024;
	    break;


	case 'c': /* Set number of calls */
	    (void) fprintf(stderr, "%s: '-c option no longer supported\n",
				    sfs_Myname);
	    exit(104);
	    break;

	case 'd': /* Set debugging level */
	    Debug_level = set_debug_level(optarg);
	    break;

	case 'D': /* Set number of directories */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal dirs value %s\n",
				sfs_Myname, optarg);
		exit(105);
	    }
	    Files_per_dir = atoi(optarg);
	    break;

	case 'f': /* Percent change in file set size */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal file set delta value %s\n",
				sfs_Myname, optarg);
		exit(106);
	    }
	    Fss_delta_percent = atoi(optarg);
	    if (Fss_delta_percent < 0 || Fss_delta_percent > 100) {
		(void) fprintf(stderr,
		   "%s: %% file set delta must be between 0 and 100\n",
		    sfs_Myname);
		exit(107);
	    }
	    break;

	case 'F': /* Set number of io files */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal files value %s\n",
				sfs_Myname, optarg);
		exit(108);
	    }
	    Tot_client_num_io_files = atoi(optarg);
	    break;

	case 'i': /* Set interactive mode */
	    if (Prime_client != NULL) {
		(void) fprintf(stderr,
			    "%s: -i and -M options are incompatible\n",
			    sfs_Myname);
		exit(109);
	    }
	    Interactive++;
	    break;

	case 'l': /* Set load */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal load value %s\n",
				sfs_Myname, optarg);
		exit(110);
	    }
	    total_load = atoi(optarg);
	    if (total_load < 0) {
		(void) fprintf(stderr, "%s: load must be > 0\n",
				sfs_Myname);
		exit(111);
	    }
	    break;

	case 'm': /* Set mix from a file */
	    mix_file = optarg;
	    if (setmix(mix_file) < 0) {
		exit(112);
	    }
	    break;

	case 'M': /* Set prime_client host name for multi-client sync */
	    if (Interactive) {
		(void) fprintf(stderr,
			    "%s: -M and -i options are incompatible\n",
			    sfs_Myname);
		exit(113);
	    }
	    Prime_client = optarg;
	    break;

	case 'N': /* Set client number in multi-client run */
	    Client_num = atoi(optarg);
	    if (Client_num <= 0) {
		(void) fprintf(stderr,
				"%s: client number must be > 0\n",
				sfs_Myname);
		exit(114);
	    }
	    break;

	case 'p': /* Set number of child processes */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal procs value %s\n",
				sfs_Myname, optarg);
		exit(115);
	    }
	    children = atoi(optarg);
	    if (children < 0) {
		(void) fprintf(stderr, "%s: number of children must be > 0\n",
				sfs_Myname);
		exit(116);
	    }
	    break;

	case 'P': /* Populate only */
	    Populate_only++;
	    break;

	case 'Q': /* Set NFS/TCP behaviour */
	    Tcp = 1;
	    Nfs_timers = Nfs_tcp_timers;
	    break;

	case 'R': /* set maximum async read concurrency level */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal read count value %s\n",
				sfs_Myname, optarg);
		exit(117);
	    }
	    Biod_max_outstanding_reads = atoi(optarg);
	    if (Biod_max_outstanding_reads < 0 ||
			Biod_max_outstanding_reads > MAX_BIODS) {
		(void) fprintf(stderr,
				"%s: read count must be >= 0 and <= %d\n",
				sfs_Myname, MAX_BIODS);
		exit(118);
	    }
	    break;

	case 'S': /* Set number of symlinks */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr,
				"%s: illegal symlinks value %s\n",
				sfs_Myname, optarg);
		exit(119);
	    }
	    Tot_client_num_symlinks = atoi(optarg);
	    break;

	case 'T': /* Set test mode, number following is opnum */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal test value %s\n",
				sfs_Myname, optarg);
		exit(120);
	    }
	    Testop = atoi(optarg);
	    if (Testop >= NOPS) {
		(void) fprintf(stderr, "%s: illegal test value %d\n",
				sfs_Myname, Testop);
		exit(121);
	    }
	    break;

	case 't': /* Set run time */
	    if (Ops[TOTAL].target_calls > 0) {
		(void) fprintf(stderr,
			    "%s: -t and -c options are incompatible\n",
			    sfs_Myname);
		exit(122);
	    }
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal time value %s\n",
				sfs_Myname, optarg);
		exit(123);
	    }
	    Runtime = atoi(optarg);
	    if (Runtime < 0) {
		(void) fprintf(stderr, "%s: run time must be >= 0\n",
				sfs_Myname);
		exit(124);
	    }
	    break;

	case 'V': /* Set Validate Level */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal validate value %s\n",
					sfs_Myname, optarg);
		exit(125);
	    }
	    Validate = atoi(optarg);
	    if (Validate < 1 || Validate > 3) {
		(void) fprintf(stderr, "%s: validate must be between 1 and 3\n",
				    sfs_Myname);
		exit(126);
	    }
	    break;

	case 'W': /* set maximum async write concurrency level */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal write count value %s\n",
					sfs_Myname, optarg);
		exit(127);
	    }
	    Biod_max_outstanding_writes = atoi(optarg);
	    if (Biod_max_outstanding_writes < 0 ||
			Biod_max_outstanding_writes > MAX_BIODS) {
		(void) fprintf(stderr,
				"%s: write count must be >= 0 and <= %d\n",
				sfs_Myname, MAX_BIODS);
		exit(128);
	    }
	    break;

	case 'w': /* Set warmup time */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal warmup value %s\n",
				sfs_Myname, optarg);
		exit(129);
	    }
	    Warmuptime = atoi(optarg);
	    if (Warmuptime < 0) {
		(void) fprintf(stderr, "%s: warmup time must be >= 0\n",
				sfs_Myname);
		exit(130);
	    }
	    break;

	case 'z': /* Do raw data dumps */
	    Dump_data++;
	    break;

	case '?':
	default:
	    usage();
	    exit(131);

	} /* end switch on arg */


   /* compute ops/request for i/o operations */
   init_iodist(Io_dist_ptr);

   /* compute bytes/file and number of files */
   init_filedist();

    /* validate all the NFS operations that sfs will use */
    if (Validate > 0) {
	/*
	 * -F <number of files > or else
	 * DEFAULT_NFILES
	 */
	if (Tot_client_num_io_files == 0) {
		Tot_client_num_io_files = DEFAULT_NFILES;
	}
	Num_io_files = Tot_client_num_io_files/children + 1;
	/* number of non-io files, dir and symlinks base on constants */
	Num_non_io_files = Tot_client_num_non_io_files/children + 1;
	Num_dirs = Num_io_files/Files_per_dir + 1;
	Num_symlinks = Tot_client_num_symlinks/children + 1;

	/* io operations access a subset of the files */
	Num_working_io_files = ((Num_io_files * Access_percent) / 100) + 1;
	/* non-io and other operations access all of the files */
	Num_working_non_io_files = Num_io_files;
	Num_working_dirs = Num_dirs;
	Num_working_symlinks = Num_symlinks;

	Validate_ops(argc - optind, &argv[optind]);
	exit(133);
    }

    /*
     * Initial check on the mount arguments, must be at least an
     * even multiple of the number of procs.
     */
    if ((argc - optind) % children) {
        (void) fprintf(stderr,
"%s: Invalid mount point list: Not a multiple of number of procs\n",
                        sfs_Myname);
        exit(182);
    }

    /*
     * -F <number of io files > or else
     * base files set on load ; this in NON-SPEC though
     */
    if (Tot_client_num_io_files == 0) {
       Tot_client_num_io_files = ((DEFAULT_BYTES_PER_OP / 1024  * total_load)
			     / (1024)) * files_per_megabyte;
    }
    Num_io_files = Tot_client_num_io_files/children + 1;

    /*   
     * Number of non-io files scales with load and is set at 2% of all files,
     * but at least DEFAULT_NFILES worth.
     */  
    Tot_client_num_non_io_files = Tot_client_num_io_files * 0.02;
    if (Tot_client_num_non_io_files < DEFAULT_NFILES)
        Tot_client_num_non_io_files = DEFAULT_NFILES;
    Num_non_io_files = Tot_client_num_non_io_files/children + 1;
 
    /* number of dir and symlinks base on constants */
    Num_dirs = Num_io_files/Files_per_dir + 1;
    Num_symlinks = Tot_client_num_symlinks/children + 1;

    /* io operations access a subset of the files */
    Num_working_io_files = ((Num_io_files * Access_percent) / 100) + 1;

    /* non-io and other operations access all of the files */
    Num_working_non_io_files = Num_io_files;
    Num_working_dirs = Num_dirs;
    Num_working_symlinks = Num_symlinks;

    /*
     * If we are doing a timed test, we still need an
     * estimate of how many calls are needed in order to
     * judge our progress.
     * If we are doing a test for a number of calls, we still need an
     * estimate of how long the test will take in order to
     * establish the time interval between progress checks.
     */
    if (Timed_run) {
	/*
	 * the total number of calls will be divided between the children
	 * when they are forked off.
	 */
	Ops[TOTAL].target_calls = Runtime * total_load;
    } else {
	Runtime = (int) ((float) Ops[TOTAL].target_calls / (float) total_load);
    }

    /*
     * multi-client sync support
     * offset the Runtime value by MULTICLIENT_OFFSET seconds.
     * This offset prevents the client from finishing before
     * the Prime Client tells it to 'STOP'. The MULTICLIENT_OFFSET is larger
     * than the time_out value on the Prime-Client; so in case the client
     * does not stop when it's told to, the Prime-client should time_out.
     */
    if (Prime_client && Timed_run)
	Runtime += MULTICLIENT_OFFSET;

    /* compute file set sizes */
    init_fss();

    /* Set up synchronization and results log file */
    init_logfile();

    /*
     * setup value of nsigs
     */
#ifdef __NSIG
    nsigs = __NSIG;
#endif
#ifdef _NSIG
    nsigs = _NSIG;
#endif
#ifdef NSIG
    nsigs = NSIG;
#endif
#if defined(SOLARIS2) && !defined(_sys_nsig)
    nsigs = _sys_siglistn;
#endif

    /* Set up the signal handlers for all signals */

#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    {
	struct sigaction sig_act, old_sig_act;

	/* use XOPEN signal handling */

	sig_act.sa_handler = generic_catcher;
	(void)sigemptyset(&sig_act.sa_mask);
	sig_act.sa_flags = 0;

	if (DEBUG_PARENT_GENERAL) {
	    if (nsigs == 0) {
		(void) fprintf (stderr,
		    "WARNING: nsigs not defined, no extra signals caught\n");
	    }
	    for (i = 1; i < nsigs; i++) {
/* attempt to set up signal handler for these signals give an error !! K.T. */
	        if (i!=SIGCHLD && i!=SIGKILL && i!=SIGSTOP && i!=SIGCONT) {
		    if (sigaction(i,&sig_act,&old_sig_act) == -1) {
			if (errno == EINVAL) {
			    (void) fprintf (stderr,
					"Skipping invalid signal %d\n", i);
			} else {
			    perror("sigaction failed");
			    exit(134);
			}
		    }
		}
	    }
        }

	/* signals handlers for signals used by sfs */
	sig_act.sa_handler = sfs_cleanup;
	if (sigaction(SIGINT,&sig_act,&old_sig_act) == -1) {
	    perror("sigaction failed: SIGINT");
	    exit(135);
	}

	sig_act.sa_handler = sfs_alarm;
	if (sigaction(SIGALRM,&sig_act,&old_sig_act) != 0) {
	    perror("sigaction failed: SIGALRM");
	    exit(136);
	}

	sig_act.sa_handler = sfs_cleanup;
	if (sigaction(SIGTERM,&sig_act,&old_sig_act) != 0)  {
	    perror("sigaction failed: SIGTERM");
	    exit(137);
	}

	sig_act.sa_handler = sfs_startup;
	if (sigaction(SIGUSR1,&sig_act,&old_sig_act) != 0)  {
	    perror("sigaction failed: SIGUSR1");
	    exit(138);
	}

	sig_act.sa_handler = sfs_stop;
	if (sigaction(SIGUSR2,&sig_act,&old_sig_act) != 0)  {
	    perror("sigaction failed: SIGUSR2");
	    exit(139);
	}
    }
#else
    if (DEBUG_PARENT_GENERAL) {
	if (nsigs == 0) {
	    (void) fprintf (stderr,
		    "WARNING: nsigs not defined, no extra signals caught\n");
	}
	for (i = 1; i < nsigs; i++) {
	    if (i!=SIGCHLD)
		(void) signal(i, generic_catcher);
	}
    }
    /* signals handlers for signals used by sfs */
    (void) signal(SIGINT, sfs_cleanup);
    (void) signal(SIGALRM, sfs_alarm);
    (void) signal(SIGTERM, sfs_cleanup);
    (void) signal(SIGUSR1, sfs_startup);
    (void) signal(SIGUSR2, sfs_stop);
#endif

    /* Fork children */
    for (child_num = 0; child_num < children; child_num++) {
	pid = fork();
	if (pid == -1) {
	    Saveerrno = errno;
	    (void) fprintf(stderr, "%s: can't fork children.", sfs_Myname);
	    errno = Saveerrno;
	    perror("fork");
	    (void) generic_kill(0, SIGINT);
	    exit(140);
	} else if (pid == 0) {
	    break;	/* get out of child creation */
	}
	(void) fprintf(pid_fp, "%d\n", pid);
    } /* end for forking kids */
    (void) fclose(pid_fp);

    /*
     * Parent: wait for kids to get ready, start them, wait for them to
     * finish, read and accumulate results.
     */
    if (pid != 0) {
	if (setuid(Real_uid) != 0) {
	   (void) fprintf(stderr,"%s: %s%s\n",
		   sfs_Myname, "cannot perform setuid operation.\n",
		   "Do `make install` as root.\n");
	}

	/* I'm the parent - let the common code signal handlers know it */
	Child_num = -1;

	parent(children, total_load, mix_file, iodist_file);

	/* Clean up and exit. */
	(void) close(Log_fd);
	(void) unlink(Logname);
	exit(0);

    } /* parent */

    /*
     * Children : initialize, then notify parent through log file,
     * wait to get signal, beat the snot out of the server, write
     * stats to the log file, and exit.
     */
    if (pid == 0) {

	/* I'm a child - let the common code signal handlers know it */
	Child_num = child_num;

	/*
	 * Determine my share of the calls and load (including any left over)
	 * The call target for each child differs by at most 1 call.
	 * The load rate for each child differs by at most 1 call/sec.
	 */
	Ops[TOTAL].target_calls = Ops[TOTAL].target_calls / children;
	if (child_num <= Ops[TOTAL].target_calls % children) {
	    Ops[TOTAL].target_calls++;
	}
	child_load = (float) total_load / (float) children;

	/*
	 * Sleep a bit so the parent can catch up after procreating all us
	 * children.
	 */
	(void) sleep(10);

	child(child_num, children, child_load, argc - optind, &argv[optind]);
	exit(0);

    } /* child */

    (void) unlink(SFS_PNT_PID);

    return(0);

} /* main */


/*
 * -----------------  Initalization of Parent/Child  ---------------------
 */

/*
 * Open the multi-client synchronization file with append mode.
 */
static void
init_logfile(void)
{
    FILE	*cl_log_fd;
    int		Saveerrno;

    (void) sprintf(Logname, "%s%d", CHILD_SYNC_LOG, Client_num);
    Log_fd = open(Logname, (O_RDWR | O_CREAT | O_TRUNC | O_APPEND), 0666);
    if (Log_fd == -1) {
	Saveerrno = errno;
	(void) fprintf(stderr, "%s: can't open log file %s ", sfs_Myname, Logname);
	errno = Saveerrno;
	perror(Logname);
	exit(141);
    }
    if (chown(Logname, Real_uid, Cur_gid) ==-1) {
	perror("chown");
	(void) fprintf(stderr, "%s: chown failed\n", sfs_Myname);
    }

    /* if multi-client execution then init client sync log */
    if (Prime_client != NULL) {
	/* init logfile and write process id */
	(void) sprintf(Client_logname, "%s%d",
			SFS_CLIENT_SYNC_LOG, Client_num);
	cl_log_fd = fopen(Client_logname, "w+");
	if (chown(Client_logname, Real_uid, Cur_gid) ==-1) {
		perror("chown");
		(void) fprintf(stderr, "%s: chown failed\n", sfs_Myname);
	}
	if (cl_log_fd == NULL) {
	    Saveerrno = errno;
	    (void) fprintf(stderr,
		"%s: can't open Client synchronization file %s ",
			    sfs_Myname, Client_logname);
	    errno = Saveerrno;
	    perror(Client_logname);
	    exit(142);
	} else {
	    /* store parent pid */
	    (void) fprintf(cl_log_fd, "%d", (int)getpid());
	    (void) fclose(cl_log_fd);
	}
    } /* init multi-client sync log */

} /* init_logfile */

/*
 * ------------------------  Utility Routines  --------------------------
 */


/*
 * Print the program's usage message.
 * Usage: sfs [-l load] [-p procs] [-w warmup] [-t time]
 *               [-m mix_file] [-B block_size] [-b blocksz_file]
 *		 [-f file_set_delta] [-a access_pnct]
 *		 [-A append_pcnt] [-D dir_cnt] [-F file_cnt] [-S symlink_cnt]
 *		 [-d debug_level] [-i] [-P] [-T op_num]
 *		 [-V validation_level] [-z] [-Q]
 *		 [-R biod_reads] [-W biod_writes]
 *		 [-M prime_client_hostname] [-N client_cnt]
 */
static void
usage(void)
{
    (void) fprintf(stderr,
      "Usage: %s [-l load] [-p procs] [-w warmup] [-t time]\n", sfs_Myname);
    (void) fprintf(stderr,
      "              [-m mix_file] [-B block_size] [-b blocksz_file]\n");
    (void) fprintf(stderr,
      "              [-f file_set_delta] [-a access_pnct]\n");
    (void) fprintf(stderr,
      "              [-A append_pcnt] [-D dir_cnt] [-F file_cnt] [-S symlink_cnt]\n");
    (void) fprintf(stderr,
      "              [-d debug_level] [-i] [-P] [-T op_num]\n");
    (void) fprintf(stderr,
      "              [-V validation_level] [-z] [-Q]\n");
    (void) fprintf(stderr,
      "              [-R biod_reads] [-W biod_writes]\n");
    (void) fprintf(stderr,
      "              [-M prime_client_hostname] [-N client_cnt]\n");
} /* usage */



/*
 * --------------  Command Line File Parsing  -------------------
 */

/*
 * Constants for mix file
 */
#define LINELEN         128             /* max bytes/line in mix file */
#define MIX_START       0
#define MIX_DATALINE    1
#define MIX_DONE        2
#define MIX_FIRSTLINE   3

/*
 * Parse the operation mix file 'mix_file'.
 *
 * ORIGINAL PRE-SFS1.2 format:
 *	Assumes that the input file is in the same format as
 *	the output of the nfsstat(8) command.
 *
 *	Uses a simple state transition to keep track of what to expect.
 *	Parsing is done a line at a time.
 *
 *	State	   	Input			action		New state
 *	MIX_START	".*nfs:.*"		skip one line	MIX_FIRSTLINE
 *	MIX_FIRSTLINE   ".*[0-9]*.*"		get calls	MIX_DATALINE
 *	MIX_DATALINE    "[0-9]* [0-9]*%"X6	get op counts	MIX_DATALINE
 *	MIX_DATALINE    "[0-9]* [0-9]*%"X4	get op counts	MIX_DONE
 *	MIX_DONE	EOF			return
 *
 *	We read operation counts from the mix file
 *	and compute our own mix percentages,
 *	rather than using those in the mix file.
 *
 * NEW SFS1.2 format version #2:
 *	SFS MIXFILE VERSION 2		Version header (must come first line)
 *	"^#.*"				Comment (any line except first)
 *	"%s [0-9]*%"			Op name Op percentage
 */
static int
setmix(
    char *	mix_file)
{
    int		state;		/* current state of state machine */
    int		got;		/* number of items read from input line */
    int		opnum;		/* operation number index */
    int		calls;		/* total number of calls in mix */
    char	line[LINELEN];	/* input line buffer */
    char	op_name[LINELEN];	/* name buffer */
    int		mix_pcnt;
    unsigned int	len;	/* length of input line */
    FILE	*mix_fp;	/* mix file */
    int		vers;		/* mix file version number */
    sfs_op_type *op_ptr;

    if ((mix_fp = fopen(mix_file, "r")) == NULL) {
	(void) fprintf(stderr, "%s: bad mix file", sfs_Myname);
	perror(mix_file);
	return(-1);
    }

    if (fgets(line, LINELEN, mix_fp) == NULL) {
	(void) fprintf(stderr, "%s: bad mix format - unexpected empty file\n",
					sfs_Myname);
    	(void) fclose(mix_fp);
	return (-1);
    }

    opnum = 0;

    /*
     * Look for initial version string
     */
    got = sscanf(line, "SFS MIXFILE VERSION %d", &vers);
    if (got != 1) {
	/*
	 * Check to see if this is old mixfile
	 */
	len = strlen(line);
	if (len < 4 || lad_substr(line, "nfs:") == 0) {
	    (void) fprintf(stderr, "%s: bad mix format - initial line '%s'\n",
					sfs_Myname, line);
	    (void) fclose(mix_fp);
	    return (-1);
	}
	vers = 1;
    }

    if (vers == 1) {
	/*
	 * Old style mix file
	 */
	state = MIX_START;
	while (state != MIX_DONE && fgets(line, LINELEN, mix_fp)) {

	    switch (state) {
	        case MIX_START:
		    /*
		     * Ate first line after nfs:
		     */
		    state = MIX_FIRSTLINE;
		    break;

	        case MIX_FIRSTLINE:
		    got = sscanf(line, "%d", &calls);
		    if (got != 1) {
		        (void) fprintf(stderr,
			"%s: bad mix format - can't find 'calls' value %d\n",
				    sfs_Myname,got);
	    			(void) fclose(mix_fp);
				return (-1);
		    }
		    if (fgets(line, LINELEN, mix_fp) == NULL) {
		        (void) fprintf(stderr,
			"%s: bad mix format - unexpected EOF after 'calls'\n",
				    sfs_Myname);
	    	        (void) fclose(mix_fp);
		        return (-1);
		    }
		    state = MIX_DATALINE;
		    break;

	        case MIX_DATALINE:
		    got = sscanf(line,
	       "%d %*d%% %d %*d%% %d %*d%% %d %*d%% %d %*d%% %d %*d%% %d %*d%%",
			    &Ops[opnum].mix_pcnt,
			    &Ops[opnum + 1].mix_pcnt,
			    &Ops[opnum + 2].mix_pcnt,
			    &Ops[opnum + 3].mix_pcnt,
			    &Ops[opnum + 4].mix_pcnt,
			    &Ops[opnum + 5].mix_pcnt,
			    &Ops[opnum + 6].mix_pcnt);

		    if (got == 4 && opnum == 14) {
		        /* looks like the last line */
		        state = MIX_DONE;
		    } else if (got == 7) {
		        opnum += 7;
		        if (fgets(line, LINELEN, mix_fp) == NULL) {
			    (void) fprintf(stderr,
			"%s: bad mix format - unexpected EOF after 'calls'\n",
					sfs_Myname);
		            (void) fclose(mix_fp);
			    return (-1);
		        }
		    } else {
		        (void) fprintf(stderr,
			    "%s: bad mix format - can't find %d op values\n",
				sfs_Myname, got);
	                (void) fclose(mix_fp);
		        return (-1);
		    }
		    break;

	        default:
		    (void) fprintf(stderr,
				"%s: error parsing mix file - bad state %d\n",
				sfs_Myname, state);
	            (void) fclose(mix_fp);
		    return (-1);
	    } /* end switch on state */
        } /* end while there are lines to read */

        if (state != MIX_DONE) {
	    (void) fprintf(stderr, "%s: bad mix format - unexpected EOF\n",
		sfs_Myname);
	    (void) fclose(mix_fp);
	    return (-1);
        }
        for (opnum = 0; opnum < NOPS; opnum++) {
	    Ops[opnum].mix_pcnt = Ops[opnum].mix_pcnt * 100 / calls
			     + ((Ops[opnum].mix_pcnt * 1000 / calls % 10) >= 5);
        }
        (void) fclose(mix_fp);
        return (0);
    }
    if (vers == 2) {
	/*
	 * New style mix file
	 */
	while (fgets(line, LINELEN, mix_fp) != NULL) {
	    if (line[0] == '#')			/* Comment line */
		continue;
	    got = sscanf(line, "%s %d", op_name, &mix_pcnt);
	    if (got != 2) {
	        (void) fprintf(stderr,
			    "%s: bad mix format - can't find op values: %s\n",
				sfs_Myname, line);
	        (void) fclose(mix_fp);
		return (-1);
	    }
	    op_ptr = Ops;
	    while (strcmp(op_ptr->name, "TOTAL") != 0) {
	        if (strcmp(op_ptr->name, op_name) == 0) {
		    op_ptr->mix_pcnt = mix_pcnt;
		    break;
	        }
	        op_ptr++;
	    }
	    if (strcmp(op_ptr->name, "TOTAL") == 0) {
	        (void) fprintf(stderr,
			    "%s: unknown op name: %s\n",
				sfs_Myname, op_name);
	        (void) fclose(mix_fp);
		return (-1);
	    }
        }
	/*
	 * Make sure that the total mix percentages == 100
	 */
        op_ptr = Ops;
	mix_pcnt = 0;
        while (strcmp(op_ptr->name, "TOTAL") != 0) {
	    mix_pcnt += op_ptr->mix_pcnt;
	    op_ptr++;
        }
	if (mix_pcnt != 100) {
	    (void) fprintf(stderr,
			    "%s: WARNING total mix percentage %d != 100\n",
				sfs_Myname, mix_pcnt);
	}
        (void) fclose(mix_fp);
	return (0);
    }

    (void) fprintf(stderr, "%s: Unknown mix file version number %d\n",
				sfs_Myname, vers);
    (void) fclose(mix_fp);
    return (-1);
} /* setmix */


/*
 * Parse the block I/O distribution file 'fp'.
 */
static int
setiodist(
    FILE *	fp)
{
    int		i;

    /* first, read and parse the i/o distribution file for syntax and size */
    if (parseiodist(fp, 1) == -1) {
	exit(143);
    }
    (void) fseek(fp, 0, SEEK_SET);

    /* read the i/o distribution file into the i/o dist table */
    if (parseiodist(fp, 2) == -1) {
	exit(144);
    }

    if (DEBUG_PARENT_GENERAL) {
	(void) fprintf(stdout, "I/o Distribution Table\n");
	(void) fprintf(stdout, "Read:\n");
	(void) fprintf(stdout, "\tpcnt bufs frags\n");
	for (i = 0 ; ; i++) {
	    (void) fprintf(stdout, "\t%4d %4d %5d\n", Io_dist_ptr->read[i].pcnt,
			Io_dist_ptr->read[i].bufs, Io_dist_ptr->read[i].frags);
	    if (Io_dist_ptr->read[i].pcnt == 100)
		break;
	}

	(void) fprintf(stdout, "Write:\n");
	(void) fprintf(stdout, "\tpcnt bufs frags\n");
	for (i = 0; ; i++) {
	    (void) fprintf(stdout, "\t%4d %4d %5d\n",
			    Io_dist_ptr->write[i].pcnt,
			    Io_dist_ptr->write[i].bufs,
			    Io_dist_ptr->write[i].frags);
	    if (Io_dist_ptr->write[i].pcnt == 100)
		break;
	}
	(void) fprintf(stdout, "Maximum file size: %d KB (%d * %d KB)\n",
			Io_dist_ptr->max_bufs * Kb_per_block,
			Io_dist_ptr->max_bufs, Kb_per_block);
    }
    return(0);
} /* setiodist */


/*
 * Block/File Distribution file parser.
 * Assumes that the input file is in the following format:
 *
 *
 *	READ_KEY_WORD
 *	percent		block_cnt		fragment_flag
 *	   .		     .			      .
 *	   .		     .			      .
 *	   .		     .			      .
 *	WRITE_KEY_WORD
 *	percent		block_cnt		fragment_flag
 *	   .		     .			      .
 *	   .		     .			      .
 *	   .		     .			      .
 *
 *
 * Notes:
 *	- The READ_KEY_WORD is "Read", the WRITE_KEY_WORD is "Write".
 *	- For each key word, the percent fields must sum to 100.
 *	- Fragment is either true (1) or false (0)
 *	- Maximum file size (and transfer size) is the largest
 *	  eight_k_cnt * 8KB plus 7 Kb for fragments
 *
 *
 * Uses a simple state transition to keep track of what to expect.
 * Parsing is done a line at a time.
 *
 * State	Input			action		New state
 * -----	--------------------	-------------	---------
 * START	"Read"			skip one line	READ
 * START	"Write"			skip one line	WRITE
 * READ		"[0-9]* [0-9]* [01]"	get values	READ
 * READ		"Write"			skip one line	WRITE
 * WRITE	"[0-9]* [0-9]* [01]"	get values	WRITE
 * WRITE	"Read"			skip one line	READ
 * DONE		EOF			return
 *
 * Pass 1 reads the file and allocates table space.
 * Pass 2 reads the file data into the tables.
 */
static int
parseiodist(
    FILE *	fp,
    int		pass)
{
    int		state;		/* current state of state machine */
    int		got;		/* number of items read from input line */
    int		pcnt;		/* percent read from input line */
    int		bufs;		/* eight_kb_buffer_cnt read from input line */
    int		frags;		/* fragment flag read from input line */
    int		rbucket;	/* current read distribution table bucket */
    int		wbucket;	/* current write distribution table bucket */
    int		rpcnt;		/* cumulative percent for read buckets */
    int		wpcnt;		/* cumulative percent for write buckets */
    char	key[5];		/* keyword buffer */
    char	line[LINELEN];	/* input line buffer */
    int		nextline;

    /*
     * Pass 1 reads, sizes, and error checks the input
     * and then allocates space for the distribution tables.
     * Pass 2 reads the input into the allocated tables.
     */

    rbucket = 0;
    wbucket = 0;
    rpcnt = 0;
    wpcnt = 0;
    state = IO_DIST_START;

    while (fgets(line, LINELEN, fp)) {

	nextline = 0;
	while (nextline == 0) {

	    if (state == IO_DIST_READ) {
		got = sscanf(line, "%d %d %d", &pcnt, &bufs, &frags);
		if (got != 3) {
		    state = IO_DIST_START;
		    continue; /* same line, but goto new state */
		}
		if (pass == 1) {
		    rbucket++;
		    rpcnt += pcnt;
		    if (frags != 0 && frags != 1) {
			(void) fprintf(stderr,
			    "%s: bad i/o dist format - bad fragment value\n",
					sfs_Myname);
			return(-1);
		    }
		} else {
		    rpcnt += pcnt;
		    Io_dist_ptr->read[rbucket].pcnt = rpcnt;
		    Io_dist_ptr->read[rbucket].bufs = bufs;
		    Io_dist_ptr->read[rbucket].frags = frags;
		    rbucket++;
		}
		if (DEBUG_CHILD_FILES) {
		    (void) fprintf(stdout, "p=%d b=%d f=%d rpcnt=%d\n",
				    pcnt, bufs, frags, rpcnt);
		    (void) fflush(stdout);
		}

		/* read next line in file */
		nextline++;
		break;
	    }

	    if (state == IO_DIST_WRITE) {
		got = sscanf(line, "%d %d %d", &pcnt, &bufs, &frags);
		if (got != 3) {
		    state = IO_DIST_START;
		    continue; /* same line, but goto new state */
		}
		if (pass == 1) {
		    wbucket++;
		    wpcnt += pcnt;
		    if (frags != 0 && frags != 1) {
			(void) fprintf(stderr,
			    "%s: bad i/o dist format - bad fragment value\n",
					sfs_Myname);
			return(-1);
		    }
		} else {
		    wpcnt += pcnt;
		    Io_dist_ptr->write[wbucket].pcnt = wpcnt;
		    Io_dist_ptr->write[wbucket].bufs = bufs;
		    Io_dist_ptr->write[wbucket].frags = frags;
		    wbucket++;
		}
		if (DEBUG_CHILD_FILES) {
		    (void) fprintf(stdout, "p=%d b=%d f=%d wpcnt=%d\n",
				    pcnt, bufs, frags, wpcnt);
		    (void) fflush(stdout);
		}
		/* read next line in file */
		nextline++;
		break;
	    }

	    if (state == IO_DIST_START) {
		got = sscanf(line, "%s", key);
		if (got != 1 || (strlen(key) != 5)){
		    (void) fprintf(stderr,
			    "%s: bad i/o dist format - invalid keyword %s\n",
				    sfs_Myname, key);
		    return(-1);
		}
		if (!strcmp(key, "Read") || !strcmp(key, "read")) {
		    if (rbucket != 0) {
			(void) fprintf(stderr,
			"%s: bad i/o dist format - too many read keywords\n",
					sfs_Myname);
			return(-1);
		    }
		    rpcnt = 0;
		    state = IO_DIST_READ;

		    /* read next line in file */
		    nextline++;
		    break;
		}
		if (!strcmp(key, "Write") || !strcmp(key, "write")) {
		    if (wbucket != 0) {
			(void) fprintf(stderr,
		       "%s: bad i/o dist format - too many write keywords\n",
					sfs_Myname);
			return(-1);
		    }
		    wpcnt = 0;
		    state = IO_DIST_WRITE;

		    /* read next line in file */
		    nextline++;
		    break;
		}
		(void) fprintf(stderr,
			    "%s: bad i/o dist format - unknown keyword %s\n",
				sfs_Myname, key);
		return(-1);
	    }

	} /* end while processing this line */
    } /* end while more lines */

    if (pass == 1) {

	/* error check the input */
	if (rbucket == 0) {
	    (void) fprintf(stderr,
		    "%s: bad i/o dist format - no read distribution data\n",
			    sfs_Myname);
	    return(-1);
	}
	if (rpcnt != 100) {
	    (void) fprintf(stderr,
			"%s: bad i/o dist format - read total percent != 100\n",
			    sfs_Myname);
	    return(-1);
	}
	if (wbucket == 0) {
	    (void) fprintf(stderr,
		    "%s: bad i/o dist format - no write distribution data\n",
			    sfs_Myname);
	    return(-1);
	}
	if (wpcnt != 100) {
	    (void) fprintf(stderr,
		    "%s: bad i/o dist format - write percent total != 100\n",
			    sfs_Myname);
	    return(-1);
	}

	/* allocate space for the table */
	if ((Io_dist_ptr = (sfs_io_dist_type *)
		malloc(sizeof(sfs_io_dist_type))) == NULL) {
	    (void) fprintf(stderr,
		    "%s: block i/o distribution table allocation failed\n",
			    sfs_Myname);
		return(-1);
	}
	if ((Io_dist_ptr->read = (sfs_io_op_dist_type *)
		malloc(rbucket*sizeof(sfs_io_op_dist_type))) == NULL) {
	    (void) fprintf(stderr,
		    "%s: read distribution table allocation failed\n", sfs_Myname);
	    return(-1);
	}
	if ((Io_dist_ptr->write = (sfs_io_op_dist_type *)
		malloc(wbucket*sizeof(sfs_io_op_dist_type)))==NULL) {
	    (void) fprintf(stderr,
		    "%s: write distribution table allocation failed\n", sfs_Myname);
	    return(-1);
	}

    }
    return(0);

} /* parseiodist */


/*
 * Compute the max i/o transfer size and average ops per request
 * for the block transfer distribution table.
 */
static void
init_iodist(
    sfs_io_dist_type	*	io_dist_ptr)
{
    int				max_bufs;
    double			weighted_ops;
    double			previous_pcnt;
    int				i;

    /*
     * compute expected number of ops for multi-op requests.
     * the calculation assumes that if a i/o distribution table
     * entry specifies that a fragment is to be generated, then
     * exactly one OTW operation will result.
     */
    max_bufs = 0;

    weighted_ops = 0.0;
    previous_pcnt = 0.0;
    for (i = 0; ; i++) {
	weighted_ops += (io_dist_ptr->read[i].pcnt - previous_pcnt) *
		      (io_dist_ptr->read[i].bufs + io_dist_ptr->read[i].frags);
	previous_pcnt = io_dist_ptr->read[i].pcnt;
	if (io_dist_ptr->read[i].bufs > max_bufs)
	    max_bufs = io_dist_ptr->read[i].bufs;
	if (io_dist_ptr->read[i].pcnt == 100)
	    break;
    }
    io_dist_ptr->avg_ops_per_read_req = weighted_ops / 100.0;

    weighted_ops = 0.0;
    previous_pcnt = 0.0;
    for (i = 0; ; i++) {
	weighted_ops += (io_dist_ptr->write[i].pcnt - previous_pcnt) *
		    (io_dist_ptr->write[i].bufs + io_dist_ptr->write[i].frags);
	previous_pcnt = io_dist_ptr->write[i].pcnt;
	if (io_dist_ptr->write[i].bufs > max_bufs)
	    max_bufs = io_dist_ptr->write[i].bufs;
	if (io_dist_ptr->write[i].pcnt == 100)
	    break;
    }
    io_dist_ptr->avg_ops_per_write_req = weighted_ops / 100.0;

    io_dist_ptr->max_bufs = max_bufs + 1;

} /* init_iodist */

static void
init_filedist()
{
    int i;
    int cur_pcnt;
    int num_files = 0;
    int prev_pcnt = 0;
    int tot_size = 0;

    /*
     * Calculate the average number of bytes per file
     */
    for (i = 0; Default_file_size_dist[i].size != 0; i++) {
        cur_pcnt = Default_file_size_dist[i].pcnt - prev_pcnt;
        num_files += cur_pcnt;
	tot_size += (Default_file_size_dist[i].size * 1024) * cur_pcnt;
	prev_pcnt = Default_file_size_dist[i].pcnt;
    }

    avg_bytes_per_file = tot_size / num_files;
    files_per_megabyte = (((1024*1024) + avg_bytes_per_file) \
                                        / avg_bytes_per_file);
}

static void
init_fss()
{
    int Delta_fss_bytes;

    Base_fss_bytes = Num_working_io_files * (avg_bytes_per_file / 1024);
    Total_fss_bytes = Num_io_files * (avg_bytes_per_file / 1024);
    Cur_fss_bytes = Base_fss_bytes;
    Delta_fss_bytes = (Base_fss_bytes * Fss_delta_percent) / 100;
    Limit_fss_bytes = Base_fss_bytes + Delta_fss_bytes;
    Most_fss_bytes = Base_fss_bytes;
    Least_fss_bytes = Base_fss_bytes;
}

/*
 * return true if 'sp' contains the substring 'subsp', false otherwise
 */
static int
lad_substr(
    char *		sp,
    char *		subsp)
{
    unsigned int	found;
    int			want;
    char *		s2;

    if (sp == NULL || subsp == NULL) {
	return (0);
    }

    want = strlen(subsp);

    while (*sp != '\0') {
	while (*sp != *subsp && *sp != '\0') {
	    sp++;
	}
	found = 0;
	s2 = subsp;
	while (*sp == *s2 && *sp != '\0') {
	    sp++;
	    s2++;
	    found++;
	}
	if (found == want) {
	    return (1);
	}
    }
    return (0);

} /* lad_substr */

/*
 * Check the gettimeofday() resolution. If the resolution
 * is in chunks bigger than SFS_MIN_RES then the client
 * does not have a usable resolution for running the 
 * benchmark.
 */
static void
check_clock(void)
{
	double time_res;
	char tmp_hostname[HOSTNAME_LEN];

	time_res = get_resolution();
    	getmyhostname(tmp_hostname, HOSTNAME_LEN);
	if( time_res > (double)SFS_MIN_RES )
	{
		(void) fprintf(stderr,
		"\n%s: Clock resolution too poor to obtain valid results.\n",
			tmp_hostname);
		(void) fprintf(stderr,
		"%s: Clock resolution %f Micro seconds.\n", tmp_hostname,
			time_res);
		exit(175);
	}
	else
	{
		(void) fprintf(stderr,
		"\n%s: Good clock resolution [ %f ] Micro seconds.\n", 
			tmp_hostname, time_res);
	}
}

/*
 * Lifted code from Iozone with permission from author. (Don Capps)
 * Returns the resolution of the gettimeofday() function 
 * in microseconds.
 */
static double
get_resolution(void)
{
        double starttime, finishtime, besttime;
        long  j,delay;
	int k;

        finishtime=time_so_far1(); /* Warm up the instruction cache */
        starttime=time_so_far1();  /* Warm up the instruction cache */
        delay=j=0;                 /* Warm up the data cache */
	for(k=0;k<10;k++)
	{
	        while(1)
       	 	{
       	         	starttime=time_so_far1();
       	         	for(j=0;j< delay;j++)
       	                ;
       	         	finishtime=time_so_far1();
       	         	if(starttime==finishtime)
       	                 	delay++;
       	         	else
			{
				if(k==0)
					besttime=(finishtime-starttime);
				if((finishtime-starttime) < besttime)
					besttime=(finishtime-starttime);
                       	 	break;
			}
		}
        }
         return(besttime);
}

/*
 * Lifted code from Iozone with permission from author. (Don Capps)
 * Returns current result of gettimeofday() in microseconds.
 */
/************************************************************************/
/* Time measurement routines.                                           */
/* Return time in microseconds                                          */
/************************************************************************/

static double
time_so_far1(void)
{
        /* For Windows the time_of_day() is useless. It increments in 55 */
	/* milli second increments. By using the Win32api one can get */
	/* access to the high performance measurement interfaces. */
	/* With this one can get back into the 8 to 9 microsecond */
	/* resolution.  */
#ifdef Windows
        LARGE_INTEGER freq,counter;
        double wintime;
        double bigcounter;

        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        bigcounter=(double)counter.HighPart *(double)0xffffffff +
                (double)counter.LowPart;
        wintime = (double)(bigcounter/(double)freq.LowPart);
        return((double)wintime*1000000.0);
#else
#if defined (OSFV4) || defined(OSFV3) || defined(OSFV5)
  struct timespec gp;

  if (getclock(TIMEOFDAY, (struct timespec *) &gp) == -1)
    perror("getclock");
  return (( (double) (gp.tv_sec)*1000000.0) +
    ( ((float)(gp.tv_nsec)) * 0.001 ));
#else
  struct timeval tp;

  if (gettimeofday(&tp, (struct timezone *) NULL) == -1)
    perror("gettimeofday");
  return ((double) (tp.tv_sec)*1000000.0) +
    (((double) tp.tv_usec) );
#endif
#endif
}

/* sfs_c_chd.c */
/* sfs_c_man.c */

