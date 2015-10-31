#ifndef _sfs_c_def_h
#define _sfs_c_def_h

#define RFS
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
 * @(#)sfs_c_def.h	2.1	97/10/23
 *
 * -------------------------- sfs_c_def.h -----------------------
 *
 *      Literals and types for the sfs program.
 *
 *.Revision_History
 *      Wittle          24-Aug-92     - New file set access code.
 *      Bruce Keith     04-Dec-91     - Fix SVR4 definitions for NGROUPS.
 *                                    - Include param.h/limits.h as
 *                                      appropriate (BSD/SVR4).
 *                                    - Use bcopy, bzero, bcmp for BSD;
 *                                      memcpy, memset, memcmp for SYSV.
 *      Richard Bean    17-May-90       Created.
 */

#include <sys/stat.h>

#include <limits.h>

#if !defined(_XOPEN_SOURCE)
/*
 * Some non-XOPEN_compilent systems are missing these definitions 
 */
#if !defined(SVR4)			/* Assume BSD */
#include <sys/param.h>
#endif /* SVR4 */
#endif /* !_XOPEN_SOURCE */

#include <sys/time.h>

#if defined(SVR4)
#include <sys/select.h>
#endif

#include "rpc/rpc.h"

#if (defined (SVR4) || defined (AIX))
#include <netinet/in.h>
#endif

#include <netdb.h>

#define SFS_MIN_RES 		100		/* 100 usec resolution */
#define	SFS_MAXNAMLEN		128		/* max test file name length */
#define	SFS_MAXPATHLEN		1024		/* max test path name length */
#define HOSTNAME_LEN		63		/* length of client's hostname */

#include "sfs_c_nfs.h"

/* --------------------------  Constants  ----------------------- */

#define	SFS_VERSION_DATE	"11 July 2001"
#define	SFS_VERSION_NUM		"3.0"

/*
 * Debugging levels
 */
#define	CHILD_TO_DEBUG		0	/* per child debugging uses child 0 */

#define	DEBUG_NEW_CODE		(Debug_level & 0x00000001)		/* 1 */
#define	DEBUG_PARENT_GENERAL	(Debug_level & 0x00000002)		/* 2 */
#define	DEBUG_PARENT_SIGNAL	(Debug_level & 0x00000004)		/* 3 */
#define	DEBUG_CHILD_ERROR	(Debug_level & 0x00000008)		/* 4 */
#define	DEBUG_CHILD_SIGNAL	(Debug_level & 0x00000010)		/* 5 */
#define	DEBUG_CHILD_XPOINT	(Debug_level & 0x00000020)		/* 6 */
#define	DEBUG_CHILD_GENERAL	((Debug_level & 0x00000040) && \
					(Child_num == CHILD_TO_DEBUG))	/* 7 */
#define	DEBUG_CHILD_OPS		((Debug_level & 0x00000080) && \
					(Child_num == CHILD_TO_DEBUG))	/* 8 */
#define	DEBUG_CHILD_FILES	((Debug_level & 0x00000100) && \
					(Child_num == CHILD_TO_DEBUG))	/* 9 */
#define	DEBUG_CHILD_RPC		((Debug_level & 0x00000200) && \
					(Child_num == CHILD_TO_DEBUG))	/* 10 */
#define	DEBUG_CHILD_TIMING	((Debug_level & 0x00000400) && \
					(Child_num == CHILD_TO_DEBUG))	/* 11 */
#define	DEBUG_CHILD_SETUP	((Debug_level & 0x00000800) && \
					(Child_num == CHILD_TO_DEBUG))	/* 12 */
#define	DEBUG_CHILD_FIT		((Debug_level & 0x00001000) && \
					(Child_num == CHILD_TO_DEBUG))	/* 13 */
#define	DEBUG_CHILD_BIOD	((Debug_level & 0x00002000) && \
					(Child_num == CHILD_TO_DEBUG))	/* 14 */


/*
 * General constants for benchmark
 */
#define	DEFAULT_LOAD		60		/* calls per sec */
#define	DEFAULT_CALLS		5000		/* number of calls */
#define	DEFAULT_NPROCS		4		/* number of children to run */
#define	DEFAULT_RUNTIME		(5 * 60)	/* runtime in seconds */
#define	DEFAULT_WARMUP		(5 * 60)	/* warmup time in seconds */
#define	DEFAULT_ACCESS		10		/* % of file set accessed */
#define	DEFAULT_APPEND		70		/* % of writes that append */
#define	DEFAULT_DELTA_FSS	10		/* % change to file set size */
#define	DEFAULT_KB_PER_BLOCK	8		/* block xfer size in KB */
#define DEFAULT_BYTES_PER_OP    (10*1024*1024)  /* bytes per unit load */
#define	DEFAULT_NFILES		100		/* # of regular NON-IO files */
#define	DEFAULT_FILES_PER_DIR	30		/* # of files per directories */
#define	DEFAULT_NSYMLINKS	20		/* # of symlinks  */
#define	DEFAULT_FAILED_LOOKUP	35		/* # of failed lookups  */

#define	DEFAULT_WARM_RATE_CHECK	2		/* check progress each 2 secs */
#define	DEFAULT_RUN_RATE_CHECK	10		/* check progress each 10 sec */
#define	DEFAULT_MAX_BUFSIZE	NFS_MAXDATA	/* max buffer size for i/o */

#define DEFAULT_BIOD_MAX_WRITE	2		/* max outstanding biod write */
#define DEFAULT_BIOD_MAX_READ	2		/* max outstanding biod reads */
#define MAX_BIODS		32


/*
 * For now we only read a fixed number of  files from a directory.  Ideally
 * we would like to read a random number from 0-MAX but we will need a new
 * workload.
 */
#define SFS_MAXDIRENTS	128

/*
 * If you change the default Chi_Sqr value,
 * then also change the field label in the results print out.
 */
#define DEFAULT_CHI_SQR_CI	CHI_SQR_95	/* chi-sqr value to use */
#define	CHI_SQR_90		2.71		/* chi-sqr value for 90% CI */
#define	CHI_SQR_95		3.84		/* chi-sqr value for 95% CI */
#define	CHI_SQR_975		5.02		/* chi-sqr value for 97.5% CI */
#define	CHI_SQR_99		6.63		/* chi-sqr value for 99% CI */
#define	CHI_SQR_995		7.88		/* chi-sqr value for 99.5% CI */

/*
 * NFS operation numbers
 */
#define	NULLCALL	0
#define	GETATTR		1
#define	SETATTR		2
#define	ROOT		3
#define	LOOKUP		4
#define	READLINK	5
#define	READ		6
#define	WRCACHE		7
#define	WRITE		8
#define	CREATE		9
#define	REMOVE		10
#define	RENAME		11
#define	LINK		12
#define	SYMLINK		13
#define	MKDIR		14
#define	RMDIR		15
#define	READDIR		16
#define	FSSTAT		17
#define ACCESS          18
#define COMMIT          19
#define FSINFO          20
#define MKNOD           21
#define PATHCONF        22
#define READDIRPLUS     23
#define	NOPS		(READDIRPLUS+1)		/* number of NFS ops */
#define	TOTAL		NOPS			/* slot for totals */

/*
 * Constants for i/o distribution table
 */
#define IO_DIST_START   0
#define IO_DIST_READ    1
#define IO_DIST_WRITE   2

/*
 * Ratio of non_io_files that are initialized
 * NOTE: initializing half the non-i/o files works ok with the
 *    default op mix.  If the mix is changed affecting the
 *    ratio of creations to removes, there may not be enough
 *    empty slots for file creation (or there may not be
 *    enough created during initialization to handle a lot of
 *    removes that occur early in the test run), and this would
 *    cause do_op() to fail to find a file appropriate for the
 *    chosen op.  This will result in the global variable
 *    Ops[op].no_calls being incremented (turn on child level
 *    debugging to check this count), and the do_op() local
 *    variable aborted_ops to be incremented and checked during
 *    runtime for too many failures.
 */
#define	RATIO_NON_IO_INIT 0.5

/*
 *---------------------------- TCP stuff ---------------------
 */

#define	TCPBUFSIZE	1024 * 32 + 200

/* --------------------------  Macros  ----------------------- */

struct ladtime {
        uint32_t    sec;         /* seconds */
        uint32_t    usec;        /* and microseconds */
        uint32_t    esec;        /* seconds since standard epoch */
};

#define SUBTIME(t1, t2) { \
	if ((t1.sec < t2.sec) || \
		((t1.sec == t2.sec) && (t1.usec < t2.usec))) { \
		t1.sec = t1.usec = 0 ; \
	} else { \
		if (t1.usec < t2.usec) { \
			t1.usec += 1000000; \
			t1.sec--; \
		} \
		t1.usec -= t2.usec; \
		t1.sec -= t2.sec; \
	} \
}

#define	ADDTIME(t1, t2)	{if ((t1.usec += t2.usec) >= 1000000) {\
				t1.sec += (t1.usec / 1000000); \
				t1.usec %= 1000000; \
			 } \
			 t1.sec += t2.sec; \
			}

#define MINIMUM(a, b) ((a < b) ? (a) : (b))
#define MAXIMUM(a, b) ((a > b) ? (a) : (b))

#define MULTIME(t1, s) { \
	t1.usec *=s; \
	t1.sec *=s; \
	if (t1.usec >= 1000000) {\
		t1.sec += (t1.usec/1000000); \
		t1.usec %= 1000000; \
	}\
	}
#define DIVTIME(t1, s) { \
	t1.usec += (t1.sec % s ) *1000000; \
	t1.sec /= s; \
	t1.usec /= s; \
	}
#define LARGERTIME(t1, t2) \
 ((t1.sec>t2.sec) || ((t1.sec==t2.sec) && (t1.usec>t2.usec))) 

/* --------------------------  Types  ----------------------- */

/*
 * SFS test phases.  Values are well-ordered for use of "<" operations.
 */
#define	Mount_phase      1  /* test directories are being mounted */
#define	Populate_phase   2  /* files being created in the test directories */
#define	Warmup_phase     3  /* reach steady state (load is being generated) */
#define	Testrun_phase    4  /* timed test run (load is being generated) */
#define	Results_phase    5  /* results collection and reporting */
typedef int sfs_phase_type;

/*
 * Index constants for lookups into the RPC timer array.
 */
#define	Init	0
#define	Lookup	1
#define	Read	2
#define	Write	3

/*
 * SFS results information structure
 */
typedef struct {
	int		good_calls;	/* successful calls */
	int		bad_calls;	/* failed (timed out) calls */
	int 	timeout_calls;	/* RFS timeout calls */
	int 		fast_calls;	/* calls that competed in 0 time */
	struct ladtime	time; 		/* cumulative execution time */
	double		msec2;		/* cumulative squared time - msecs**2 */
} sfs_results_type;

/*
 * SFS information reported from child back to parent.
 */
typedef struct {
	int			version;
	sfs_results_type	results_buf[NOPS+1];
	int			total_fss_bytes;
	int			least_fss_bytes;
	int			most_fss_bytes;
	int			base_fss_bytes;
} sfs_results_report_type;


/*
 * SFS operation information structure
 */

typedef struct {
	char *		name;		/* operation name */
	int		mix_pcnt;	/* percentage of call target */
#ifndef RFS
	int		(*funct)();	/* op routine */
#endif
	int		call_class;	/* call class: client handle & timeo */
	int		target_calls;	/* number of calls to make */
	int		no_calls;	/* # of times a call couldn't be made */
	double		req_pcnt;	/* cumulative request percentile */
	int		req_cnt;	/* number of requests made */
	int		target_reqs;	/* number of req to be made */
	sfs_results_type results;	/* test results */
} sfs_op_type;


/*
 * Flags used with randfh
 */
#define RANDFH_TRUNC		0x01	/* pick a file to truncate */
#define RANDFH_APPEND		0x02	/* pick a file to append to */

/*
 * SFS file information structure
 * The particular values assiged are used to perform mod operations.
 */
#define	Sfs_io_file		0	/* read, write ops only (0) */
#define	Sfs_non_io_file		1	/* other regular file ops only (1) */
#define	Sfs_symlink		2	/* symlink ops only (2) */
#define	Sfs_dir			3	/* dir ops only (3) */
#define	Sfs_regular		4	/* any regular file (0,1) */
#define	Sfs_non_dir		5	/* any non-directory file (0,1,2) */
#define	Sfs_any_file		6	/* any file, link, or dir (0,1,2,3) */
typedef int sfs_file_type;

#define	Exists		1	/* op needs an object that already exists */
#define	Nonexistent	2	/* op will create an object */
#define	Empty_dir	3	/* op needs an empty directory */
typedef char sfs_state_type;

/*
 * One file (dir, or symlink) in the file set.
 */
typedef struct sfs_fh_data {
	union {
		fhandle_t	f_fh2;            /* the NFS V2 file handle */
		nfs_fh3		f_fh3;            /* the NFS V3 file handle */
	} sfs_fh_un;
	union {
		fattr		a_attributes2;    /* its V2 attributes */
		fattr3		a_attributes3;    /* its V3 attributes */
	} sfs_fattr_un;
	char                    file_name[SFS_MAXNAMLEN]; /* path component*/
} sfs_fh_data;

typedef struct sfs_fh_type {
	struct sfs_fh_type	*dir;	        /* Parent Directory */
	struct sfs_fh_data	*fh_data;	/* Data area */
	int			size;		/* its size */
	int			unique_num;	/* unique part of filename */
	int			use_cnt;	/* count of op to this file */
	int			xfer_cnt;	/* count of KB xfered */
	sfs_state_type		state;		/* existence state */
	char			working_set;	/* is in the working set */
	char			initialize;	/* should be initialized */
#define	attributes2	fh_data->sfs_fattr_un.a_attributes2
#define	attributes3	fh_data->sfs_fattr_un.a_attributes3
#define	fh2	fh_data->sfs_fh_un.f_fh2
#define	fh3	fh_data->sfs_fh_un.f_fh3
#define	file_name 	fh_data->file_name
} sfs_fh_type;

#define fh_size(fhptr)	(nfs_version == NFS_VERSION ? \
						(fhptr)->attributes2.size : \
						(fhptr)->attributes3.size._p._l)

#define fh_uid(fhptr)	(nfs_version == NFS_VERSION ? \
				(uint32_t)((fhptr)->attributes2.uid) : \
				(uint32_t)((fhptr)->attributes3.uid))

#define fh_gid(fhptr)	(nfs_version == NFS_VERSION ? \
				(uint32_t)((fhptr)->attributes2.gid) : \
				(uint32_t)((fhptr)->attributes3.gid))

#define fh_mode(fhptr)	(nfs_version == NFS_VERSION ? \
				(uint32_t)((fhptr)->attributes2.mode) : \
				(uint32_t)((fhptr)->attributes3.mode))

#define fh_isdir(fhptr)	(nfs_version == NFS_VERSION ? \
				((fhptr)->attributes2.type == NFDIR) : \
				((fhptr)->attributes3.type == NF3DIR))

#define fh_isfile(fhptr) (nfs_version == NFS_VERSION ? \
				((fhptr)->attributes2.type == NFREG) : \
				((fhptr)->attributes3.type == NF3REG))

/*
 * One file (dir, or symlink) in the working file set.
 */
typedef struct {
	int		range;		/* random access range for this file */
	int		index;		/* points into actual file array */
} sfs_work_fh_type;

typedef struct {
	sfs_work_fh_type	*entries;          /* array of working files */
	int 			access_group_size; /* # files in a group */
	int			access_group_cnt;  /* # groups in workset */
	int 			max_range;	   /* access range of workset */
} sfs_work_set_type;



/*
 * SFS file size distribution structures
 */
typedef struct {
	int		pcnt;		/* percentile */
	int		bufs;		/* Block_size KB xfers */
	int		frags; 		/* boolean - is a frag present */
} sfs_io_op_dist_type;

typedef	struct {
	sfs_io_op_dist_type	*read;		/* read size table */
	sfs_io_op_dist_type	*write;		/* write size table */
	int			max_bufs;	/* max # of Block_size xfers */
	double		avg_ops_per_read_req;	/* avg read ops/req */
	double		avg_ops_per_write_req;	/* avg write ops/req */
} sfs_io_dist_type;

typedef struct {
	int		pcnt;		/* percentile */
	int		size;		/* file size in KB */
} sfs_io_file_size_dist;


/* sfs child processes synchronization logfile */
#define	CHILD_SYNC_LOG	"/tmp/sfs_log"
#define	SFS_PNT_PID	"/tmp/sfs_pnt_pid"
#define	SFS_PRM_PID	"/tmp/sfs_prm_pid"
#define	SFS_SYNCD_PID	"/tmp/sfs_syncd_pid"

/*
 * Values for invalid runs
 */
#define INVALID_UNKNOWN		1	/* Old value reserved as unknown */
#define INVALID_IODIST		2
#define INVALID_MIX		3
#define INVALID_RUNTIME		4
#define INVALID_ACCESS		5
#define INVALID_APPEND		6
#define INVALID_KB		7
#define INVALID_NDIRS		8
#define INVALID_FSS		9
#define INVALID_BIODREAD	10
#define INVALID_NSYMLINKS	11
#define INVALID_BIODWRITE	12
#define INVALID_WARMUP		13
#define INVALID_GOODCALLS	14
#define INVALID_FAILEDRPC	15
#define INVALID_NOTMIX		16
#define INVALID_MAX		(INVALID_NOTMIX + 1)

/*
 * External variable declarations
 */
extern int		Access_percent;
extern int		Append_percent;
extern int		Base_fss_bytes;
extern int		Biod_max_outstanding_reads;
extern int		Biod_max_outstanding_writes;
extern int		Bytes_per_block;
extern int		Child_num;
extern int		Client_num;
extern sfs_fh_type	*Cur_file_ptr;
extern char		Cur_filename[];
extern gid_t		Cur_gid;
extern struct ladtime	Cur_time;
extern uid_t		Cur_uid;
extern sfs_phase_type	Current_test_phase;
extern int		Debug_level;
extern sfs_io_file_size_dist Default_file_size_dist[];
extern sfs_fh_type	*Dirs;
extern uint32_t		Dump_count;
extern int		dump_create_existing_file;
extern int		Dump_data;
extern uint32_t		Dump_length;
extern uint32_t		Dump_offset;
extern int		dump_truncate_op;
extern sfs_fh_type	Export_dir;
extern char		Filespec[];
extern char		Dirspec[];
extern char		Symspec[];
extern int		avg_bytes_per_file;
extern int		files_per_megabyte;
extern int		Fss_delta_percent;
extern int		Interactive;
extern sfs_io_dist_type * Io_dist_ptr;
extern sfs_fh_type	*Io_files;
extern int		Kb_per_block;
extern char *		sfs_Myname;
extern int		Least_fss_bytes;
extern int		Limit_fss_bytes;
extern int		Cur_fss_bytes;
extern int		Total_fss_bytes;
extern int		Log_fd;
extern char		Logname[];
extern int		Most_fss_bytes;
extern int		Msec_per_period;
extern CLIENT *		NFS_client;
extern CLIENT *		NFS_client_recv;
extern struct timeval	*Nfs_timers;
extern struct timeval	Nfs_udp_timers[];
extern struct timeval	Nfs_tcp_timers[];
extern int		nfs_version;
extern sfs_fh_type	*Non_io_files;
extern int		Num_dirs;
extern int		Num_dir_files;
extern int		Num_failed_lookup;
extern int		Num_io_files;
extern int		Num_non_io_files;
extern int		Num_symlinks;
extern int		Num_symlink_files;
extern int		Num_working_dirs;
extern int		Num_working_io_files;
extern int		Num_working_non_io_files;
extern int		Num_working_symlinks;
extern sfs_op_type	*Ops;
extern int		Populate_only;
extern char *		Prime_client;
extern uid_t		Real_uid;
extern int		Runtime;
extern struct ladtime	Starttime;
extern int		start_run_phase;
extern sfs_fh_type	*Symlinks;
extern int		Tcp;
extern int		Testop;
extern int		Files_per_dir;
extern int		Tot_client_num_io_files;
extern int		Tot_client_num_non_io_files;
extern int		Tot_client_num_symlinks;
extern int		Timed_run;
extern int		Validate;
extern int		Warmuptime;
extern char		*invalid_str[];
extern char		lad_hostname[];


/*
 * External function declarations
 */
extern int	biod_init(int, int);
extern void	biod_term(void);
extern void	biod_turn_on(void);
extern int	check_access(struct stat *);
extern int	check_fh_access(sfs_fh_type *);
extern void	child(int, int, float, int, char **);
extern void	generic_catcher(int);
extern int	generic_kill(int, int);
extern void	init_counters(void);
extern void	init_fileinfo(void);
extern void	init_mount_point(int, char *, CLIENT *);
extern void	init_ops(void);
extern char *	init_write_buffer(void);
extern CLIENT * lad_getmnt_hand(char *);
extern CLIENT *	lad_clnt_create(int, struct hostent *, uint32_t,
			uint32_t, int, struct timeval *);
extern char *	lad_timestamp(void);
extern int	set_debug_level(char *s);
extern void	sfs_alarm(int);
extern void	sfs_cleanup(int);
extern void	sfs_elapsedtime(sfs_op_type *, struct ladtime *,
                                struct ladtime *);
extern void	sfs_gettime(struct ladtime  *);
extern int32_t	sfs_random(void);
extern void	sfs_srandom(int);
extern int	init_rand_range(int);
extern int	rand_range(int);
extern void	sfs_startup(int);
extern void	sfs_stop(int);
extern void	log_dump(struct ladtime *, struct ladtime *, int);
extern void	parent(int, int, char *, char *);
extern void	print_dump(int, int);
extern sfs_fh_type *
		randfh(int, int, uint_t, sfs_state_type, sfs_file_type);
extern int	msec_sleep(int);
extern void	Validate_ops(int, char **);

/* Reliable RPC functions for initialization */
extern int	lad_lookup(sfs_fh_type *, char *);
extern int	lad_remove(sfs_fh_type *, char *);
extern int	lad_rmdir(sfs_fh_type *, char *);
extern int	lad_symlink(sfs_fh_type *, char *, char *);
extern int	lad_mkdir(sfs_fh_type *, char *);
extern int	lad_write(sfs_fh_type *, int32_t, int32_t);
extern int	lad_create(sfs_fh_type *, char *);
extern int	lad_truncate(sfs_fh_type *, int32_t);

/* RFS: moved the definition from sfs_c_bio.c to here because it is used in 
 * sfs_c_chd.c in the new code, information associated with outstanding requests
 */
struct biod_req {
    uint32_t		xid;		/* RPC transmission ID	*/
    bool_t		in_use;		/* Indicates if the entry is in use */
	int	dep_tab_index;			/* corresponding index in dep_tab */
    unsigned int	count;		/* Count saved for Dump routines */
    unsigned int	offset;		/* Offset saved for Dump routines */
    struct ladtime	start;		/* Time RPC call was made */
    struct ladtime	stop;		/* Time RPC reply was received */
    struct ladtime	timeout;	/* Time RPC call will time out */
};

#endif /* sfs_def.h */
