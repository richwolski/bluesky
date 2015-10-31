#ifndef lint
static char sccsid[] = "@(#)sfs_m_prm.c	2.1	97/10/23";
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
 * This program is started from sfs_mgr.  It runs on some system
 * designated as the Prime Client and synchronizes the the different
 * phases of a SFS benchmark execution amongst multiple clients by
 * means of RPCs.  The Prime client can also be running SFS.
 *
 *.Exported_routines
 *	int main(int, char **)
 *
 *.Local_routines
 *	void multi_cleanup(int)
 *	void do_initialize(int, char **)
 *	void sync_PC_with_clients(void)
 *	int signal_sfs_clients(char *, int)
 *	void print_multi_results(void)
 *	void prog_usage(void)
 *	void printdeadclients(void)
 *
 *.Revision_history
 *	11-Jul-94	ChakChung Ng	Add codes for NFS/v3
 *      02-Jul-92	0.1.9   Teelucksingh
 *				Use tcp handles for synchronization
 *				instead of udp ones. Added code to make
 *                              call to shell script to start and stop
 *				external monitoring (no longer creates
 *				/tmp/SFS_START and /tmp/SFS_DONE).
 *	10-Jan-92	0.00.19 Teelucksingh
 *				Added code for the Prime-Client to report
 *				'INVALID RUN' from any of the clients.
 *			        Also added code to compute and report
 *				the aggregate 'average response time
 *				standard deviation values'.
 *	04-Jan-92	0.00.18 Pawlowski
 *				Add hooks for raw data dump support.
 *	04-Dec-91	0.00.15 Keith
 *				Include string.h for SYSV/SVR4
 *	28-Nov-91	0.00.13 Teelucksingh
 *				Fixed 'multiple signals' problem.
 *				Sync rpcs now pass a 'transaction id'
 *				to 'sfs_syncd', the sync server.
 *				'sfs_syncd' keeps track of previous rpc
 *				calls that successfully executed.
 *				Added ANSI C features.
 *
 *      23-Sep-91	0.00.11 Teelucksingh
 *                              Modified the format of sfs_prime output
 *	17-Jun-91	Teelucksingh 	Created.
 *
 *
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
#include <math.h>
#include <ctype.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <sys/file.h>
#include <fcntl.h>

#include <unistd.h>

#include "sfs_c_def.h"
#include "sfs_m_def.h"

#if !defined(_XOPEN_SOURCE)
#include <sys/socket.h>
#else
#define	AF_INET		2
#endif

/*
 * -----------------------  External Definitions  -----------------------
 */


/* sfs_mpr.c function declarations */
/* external sfs routines */

/* forward definitions for local routines */
static void multi_cleanup(int);
static void do_initialize(int, char **);
static void sync_PC_with_clients(void);
static int signal_sfs_clients(char *);
static void print_multi_results(void);
static void prog_usage(void);
static void printdeadclients(void);

/*
 * -----------------------  Static Definitions  -----------------------
 */

#define EXTRA_INIT_TIME 3600  /* extra time to initialize before timing out */

int Debug_level = 0;		/* flag indicates prime client debug mode */
char *sfs_Myname;			/* program name */
int nfs_version;

static int Asleep = 0;	        /* flag: parent sfs process is asleep */
static int Pc_log_fd;	               /* Prime client sync log fd */
static int Num_clients = 0;  	       /* number of clients used in run */
static char **Client_names; /* [HOSTNAME_LEN];   array of clients host names */
			     /* sleep period before issuing a go ahead signal */
static int Prime_sleep_time = 0;
			     /* time to wait before exiting with error */
static int Prime_time_out = 400;
static int Prime_runtime = DEFAULT_RUNTIME; /* seconds in benchmark run */

/*
 * the Prime_client only uses the P_* variables
 * for calculating and reporting the Aggregate
 * run parameters.
 */
static int P_children = DEFAULT_NPROCS; /* processes per client */
static int P_total_load = DEFAULT_LOAD; /* NFS operations per second */
static int P_percent_append = DEFAULT_APPEND; /* % of writes that append */
static int P_percent_access = DEFAULT_ACCESS; /* % of file set accessed */
static int P_kb_per_block = DEFAULT_KB_PER_BLOCK; /* i/o pkt block sz in KB */
static int P_dump_data = 0;		/* raw output switch */
static int P_testop = -1;		/* test mode operation number */
static int P_percent_fss_delta =	/* allowed change to file set */
				DEFAULT_DELTA_FSS;
					/* allowed change to file set */
static int P_warmuptime = DEFAULT_WARMUP; /* seconds to warmup */
static char *P_iodist_file = 0;		/* block io dist table file */
static char *P_mix_file = 0;		/* mix file */
static char *P_script_name = 0;		/* external script name */
static char *P_script_args = "";	/* external script parameters */
static int P_tcp = 0;			/* TCP */
static FILE *sum_result_fp = NULL;

/*
 * ---------------------  Biod Simulation Variables ---------------------
 */
static int P_biod_max_outstanding_reads = DEFAULT_BIOD_MAX_READ;
static int P_biod_max_outstanding_writes = DEFAULT_BIOD_MAX_WRITE;

/* list of nfs operations - used to verify results file */
static char *Ops_name[NOPS] = {	"null", "getattr", "setattr", "root", "lookup",
				"readlink", "read", "wrcache", "write",
				"create", "remove", "rename", "link",
				"symlink", "mkdir", "rmdir", "readdir",
				"fsstat", "access", "commit", "fsinfo",
				"mknod", "pathconf", "readdirplus" };


/*
 * --------------------------  Prime-client  --------------------------
 */


/*
 * SIGINT Signal Handler
 * - rpc to all clients to cleanup and exit
 */
static void
multi_cleanup(
    int		sig_id)
{
    /* wake up this process if asleep */
    if (Asleep == 1) {
	(void) generic_kill(0,SIGALRM);
    }
    (void) fprintf(stderr,"\n%s: Caught Signal %d SIGINT\n", sfs_Myname, sig_id);
    (void) fprintf(stderr,"\nSending interupt signal to clients ... ");
    (void) fflush(stderr);
    if ((int) signal_sfs_clients("PRIME_STOP") !=0)
	exit(9);
    (void) fprintf(stderr,"done\n");
    (void) fflush(stderr);
    (void) close(Pc_log_fd);
    (void) unlink(SFS_PRIME_SYNC_LOG);
    exit(10);

} /* multi_cleanup */


/*
 * main
 * synchronizes multi-client sfs run
 * - wait for 'DONE-MOUNT' from all clients
 * - tell them to `DO-INIT`
 * - wait for `DONE-INIT` from all clients
 * - tell them to `DO-WARMUP`
 * - wait for 'READY from all clients
 * - tells them to 'START' and goes to sleep
 * - wakes up and tells clients to 'STOP'
 * - wait for 'DONE-TEST' from all clients
 * - tells clients to 'MOVE-DATA'
 * - averages multi-client data and exits
 */
int
main(
    int		argc,
    char *	argv[])
{
    int		i;
    char	monitor_cmd[SFS_MAXPATHLEN+5];
    int		nsigs = 32;		/* reasonable default */
    FILE	*pid_fp;

#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    struct sigaction sig_act, old_sig_act;
#endif

    /*
     * Place pid in pid log file
     */  
    if ((pid_fp = fopen(SFS_PRM_PID, "a+")) == NULL) {
        perror(SFS_PRM_PID);
        exit(1);
    }
    (void) fprintf(pid_fp, "%d\n", getpid());
    (void) fclose(pid_fp);

    (void) fprintf(stderr, "\nSPEC SFS Benchmark Version %s, Creation - %s\n",
                                SFS_VERSION_NUM, SFS_VERSION_DATE);
    (void) fflush(stderr);

    /* initialize variables, parse input, etc.  */
    do_initialize(argc, argv);

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

    /* trap for all signals */

#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
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
     /* attempt to set up signal handler for these signals gives an error. */
	    if (i!=SIGCHLD && i!=SIGKILL && i!=SIGSTOP && i!=SIGCONT) {
                if (sigaction(i,&sig_act,&old_sig_act) == -1) {
                        if (errno == EINVAL)
                                (void) fprintf (stderr,
					"Skipping invalid signal %d\n", i);
                        else {
                                perror("sigaction failed");
                                exit(11);
                        }
                }
	    }
	}
    }

    /* signals handlers for signals used by sfs_prime */
    sig_act.sa_handler = multi_cleanup;
    if (sigaction(SIGINT,&sig_act,&old_sig_act) != 0) {
		perror("sigaction failed: SIGINT");
		exit(12);
    }
#else

    if (DEBUG_PARENT_GENERAL) {
	if (nsigs == 0) {
	    (void) fprintf (stderr,
		    "WARNING: nsigs not defined, no extra signals caught\n");
	}
	for (i = 1; i < nsigs; i++) {
	    (void) signal(i, generic_catcher);
	}
    }

    /* set up SIGINT signal handler */
    (void) signal(SIGINT, multi_cleanup);

#endif /* USE_POSIX_SIGNALS */

    (void) fprintf(stderr, "Executing SFS Benchmark on %d Client(s).\n",
		    Num_clients);
    (void) fflush(stderr);

    /* wait for 'DONE-MOUNT message from the clients */
    (void) fprintf(stderr,
	   "%s Waiting on DONE-MOUNT message from %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);

    sync_PC_with_clients();             /* wait for clients DONE-MOUNT */

    (void) fprintf(stderr, "%s Received.\n", lad_timestamp());
    (void) fflush(stderr);

    /* send DO-INIT message to all the clients  */
    (void) fprintf(stderr, "%s Sending DO-INIT message to %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    if ((int) signal_sfs_clients("PRIME_SIGNAL") !=0)
	exit(13);
    (void) fprintf(stderr, "%s Completed.\n", lad_timestamp());
    (void) fflush(stderr);

    /* wait for 'DONE-INIT' message from the clients */
    (void) fprintf(stderr,
	   "%s Waiting on DONE-INIT message from %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);

    /*
     * add an extra time to time_out value.
     * initializing the SFS testdirs on clients can take a while.
     */
    Prime_time_out += EXTRA_INIT_TIME;
    sync_PC_with_clients();             /* wait for clients DONE-INIT */
    Prime_time_out -= EXTRA_INIT_TIME;        /* reset time_out */

    (void) fprintf(stderr, "%s Received.\n", lad_timestamp());
    (void) fflush(stderr);

    /* send DO-WARMUP message to all the clients  */
    (void) fprintf(stderr, "%s Sending DO-WARMUP message to %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    if ((int) signal_sfs_clients("PRIME_SIGNAL") !=0)
	exit(14);
    (void) fprintf(stderr, "%s Completed.\n", lad_timestamp());
    (void) fflush(stderr);

    /* wait for 'READY' message from the clients */
    (void) fprintf(stderr, "%s Waiting on READY message from %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);

    (void) sleep(P_warmuptime);

    sync_PC_with_clients();		/* wait for clients READY */

    (void) fprintf(stderr, "%s Received.\n", lad_timestamp());
    (void) fflush(stderr);

    /*
     * call the program to trigger START of external monitoring
     */
     if (P_script_name) {
	(void) sprintf(monitor_cmd,"%s %s %s",P_script_name,
			"START",P_script_args);
	if (system(monitor_cmd) != 0) {
		(void) fprintf(stderr,"%s: external monitoring command (%s) failed - %d - continuing.\n ",
			sfs_Myname, monitor_cmd, errno);
		(void) fflush(stderr);
		P_script_name = NULL;
	}

     }

    /*
     * wait period before telling clients to START - gives external
     * performance monitoring utilities enough time to start up.
     * Value set in sfs_rc (PRIME_SLEEP) - default is 0 seconds.
     */
    (void) sleep(Prime_sleep_time);

    /* send START message to all the clients  */
    (void) fprintf(stderr, "%s Sending START message to %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    if ((int) signal_sfs_clients("PRIME_SIGNAL") !=0)
	exit(15);
    (void) fprintf(stderr, "%s Completed.\n", lad_timestamp());
    (void) fflush(stderr);

    /*
     * Sleep for <Prime_runtime> seconds
     * and then send STOP message to tell the sfs clients
     * that is time to wrap things up.
     */
    /* set the Asleep flag go to sleep while clients are executing sfs */
    Asleep = 1;
    (void) sleep(Prime_runtime);
    Asleep = 0;

    (void) fprintf(stderr, "%s Sending STOP message to %d client(s).\n",
			lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    if ((int) signal_sfs_clients("PRIME_ALARM") !=0)
	exit(16);
    (void) fprintf(stderr, "%s Completed.\n", lad_timestamp());
    (void) fflush(stderr);


    /* wait for DONE-TEST message from clients indicating they completed run */
    (void) fprintf(stderr, "%s Waiting on DONE-TEST message from %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    sync_PC_with_clients();
    (void) fprintf(stderr, "%s Received.\n", lad_timestamp());
    (void) fflush(stderr);

    /*
     * call the program to trigger STOP of external monitoring
     */
     if (P_script_name) {
	(void) sprintf(monitor_cmd,"%s %s %s",P_script_name,
			"DONE",P_script_args);
	if (system(monitor_cmd) != 0) {
		(void) fprintf(stderr,"%s: external monitoring command (%s) failed - %d - continuing.\n ",
			sfs_Myname, monitor_cmd, errno);
		(void) fflush(stderr);
		P_script_name = NULL;
	}
     }

    /* give enough time to stop external performance monitoring utilities.  */
    (void) sleep(Prime_sleep_time);

    /*
     * send MOVE-DATA message to Clients to move data across
     */
    (void) fprintf(stderr, "%s Sending MOVE-DATA message to %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    if ((int) signal_sfs_clients("PRIME_SIGNAL") !=0)
	exit(17);
    (void) fprintf(stderr, "%s Completed.\n", lad_timestamp());
    (void) fflush(stderr);

    /* wait for SEND-DATA message from all the clients */
    (void) fprintf(stderr,
		    "%s Waiting on SEND-DATA message from %d client(s).\n",
		    lad_timestamp(), Num_clients);
    (void) fflush(stderr);
    sync_PC_with_clients();
    (void) fprintf(stderr, "%s Received.\n", lad_timestamp());
    (void) fflush(stderr);

    /* summarize and print aggregate results */
    print_multi_results();

    /* close files and exit success */
    (void) close(Pc_log_fd);
    (void) unlink(SFS_PRIME_SYNC_LOG);
    (void) unlink(SFS_PRM_PID);
    return(0);

}  /* main */

/*
 * initialize control variables, open logfiles etc.
 */
static void
do_initialize(
    int		argc,
    char	*argv[])
{
    int		c;
    FILE	*check_fp;
    int i;
    char	*cp;
    extern char *optarg;
    extern int optind;


    sfs_Myname = argv[0];
    if (argc <= 1) {
	prog_usage();
	/* NOTREACHED */
    }

    while ((c = getopt(argc, argv, "a:A:b:B:C:d:f:k:K:l:m:p:QR:s:t:T:W:w:x:z")) != EOF)
	switch (c) {
	case 'a': /* Percent of file set to access;
		   * used in aggregate report.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal acces value %s\n",
					sfs_Myname, optarg);
		exit(18);
	    }
	    P_percent_access = atoi(optarg);
	    if (P_percent_access < 0 || P_percent_access > 100) {
		(void) fprintf(stderr,
		       "%s: %% access must be between 0 and 100\n",
			sfs_Myname);
		exit(19);
	    }
	    break;

	case 'A': /* Percent of writes that append;
		   * used in aggregate report.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal append value %s\n",
					sfs_Myname, optarg);
		exit(20);
	    }
	    P_percent_append = atoi(optarg);
	    if (P_percent_append < 0 || P_percent_append > 100) {
		(void) fprintf(stderr,
			       "%s: %% append must be between 0 and 100\n",
				sfs_Myname);
		exit(21);
	    }
	    break;

	case 'b': /* Set block size distribution table file
		   * used in aggregate report.
		   */
	    if ((check_fp = fopen(optarg, "r")) == NULL) {
		cp = strerror(errno);
		(void) fprintf(stderr, "%s: bad block size file %s: %s\n",
					sfs_Myname, optarg, cp);
		exit(22);
	    }
	    P_iodist_file = optarg;
	    (void) fclose(check_fp);
	    break;


	case 'B': /* Set the per packet maximum block size
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal block size value %s\n",
					sfs_Myname, optarg);
		exit(23);
	    }
	    P_kb_per_block = atoi(optarg);
	    if ((P_kb_per_block < 1) ||
			(P_kb_per_block > (DEFAULT_MAX_BUFSIZE/1024))) {
		(void) fprintf(stderr, "%s: illegal block size value %s\n",
					sfs_Myname, optarg);
		exit(24);
	    }
	    break;

	case 'C': /*
		   * Set summary result file
		   */
            if ((sum_result_fp = fopen(optarg, "a+")) == NULL) {
		cp = strerror(errno);
		(void) fprintf(stderr,
			"%s: Unable to create summary result file %s: %s\n",
					sfs_Myname, optarg, cp);
		exit(222);
	    }
	    break;

	case 'd': /*
		   * Set Debug_level
		   */
	    Debug_level = set_debug_level(optarg);
	    break;

	case 'f': /* Percent change in file set size
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal file set delta value %s\n",
					sfs_Myname, optarg);
		exit(26);
	    }
	    P_percent_fss_delta = atoi(optarg);
	    if (P_percent_fss_delta < 0 || P_percent_fss_delta > 100) {
		(void) fprintf(stderr,
			   "%s: %% file set delta must be between 0 and 100\n",
			    sfs_Myname);
		exit(27);
	    }
	    break;

	case 'k': /*
		   * program to start and stop external
		   * performance monitoring. Called at start
		   * and completion of core load generation period.
		   */
	    if ((check_fp = fopen(optarg, "r")) == NULL) {
		cp = strerror(errno);
		(void) fprintf(stderr,
			"%s: program %s protected or missing: %s\n",
			sfs_Myname, optarg, cp);
		exit(28);
	    }
	    P_script_name = optarg;
	    (void) fclose(check_fp);
	    break;

	case 'K': /*
		   * Command-line parameters for the external monitor
		   * (see the "-k" option, above)
		   */
	    P_script_args = optarg;
	    break;

	case 'l': /* Set load
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal load value %s\n",
					sfs_Myname, optarg);
		exit(29);
	    }
	    P_total_load = atoi(optarg);
	    if (P_total_load < 0) {
		(void) fprintf(stderr, "%s: load must be > 0\n", sfs_Myname);
		exit(30);
	    }
	    break;

	case 'm': /* Set mix from a file
		   * used in aggregate reporting only.
		   */
	    if ((check_fp = fopen(optarg, "r")) == NULL) {
		cp = strerror(errno);
		(void) fprintf(stderr, "%s: bad mix file: %s: %s\n",
				sfs_Myname, optarg, cp);
		exit(31);
	    }
	    P_mix_file = optarg;
	    (void) fclose(check_fp);
	    break;

	case 'p': /* Set number of child processes
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal procs value %s\n",
					sfs_Myname, optarg);
		exit(32);
	    }
	    P_children = atoi(optarg);
	    if (P_children < 0) {
		(void) fprintf(stderr, "%s: number of children must be > 0\n",
				sfs_Myname);
		exit(33);
	    }
	    break;

	case 'Q': /* Set NFS/TCP behaviour */
	    P_tcp = 1;
	    break;

	case 'R': /* set maximum async read concurrency level
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal read count value %s\n",
				sfs_Myname, optarg);
		exit(34);
	    }
	    P_biod_max_outstanding_reads = atoi(optarg);
	    if (P_biod_max_outstanding_reads < 0) {
		(void) fprintf(stderr, "%s: read count must be >= 0\n",
				sfs_Myname);
		exit(35);
	    }
	    break;

	case 's':
	     /*
	      * Set sleep time so external processes will
	      * have time to startup
	      */
	      if (!isdigit(optarg[0])) {
		  (void) fprintf(stderr, "%s: illegal sleep value %s\n",
				  sfs_Myname, optarg);
		  exit(36);
	      }
	      Prime_sleep_time = atoi(optarg);
	      break;

	case 't': /* Set SFS Runtime value */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal time value %s\n",
				sfs_Myname, optarg);
		exit(37);
	    }
	    Prime_runtime = atoi(optarg);
	    if (Prime_runtime < 0) {
		(void) fprintf(stderr, "%s: run time must be >= 0\n",
				sfs_Myname);
		exit(38);
	    }
	    break;

	case 'T': /* Set Test mode operation
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal time_out value %s\n",
				sfs_Myname, optarg);
	       exit(39);
	    }
	    P_testop = atoi(optarg);
	    if (P_testop >= NOPS) {
		(void) fprintf(stderr, "%s: illegal test value %d\n",
					sfs_Myname, P_testop);
		exit(40);
	    }
	    break;

	case 'W': /* set maximum async write concurrency level
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal write count value %s\n",
					sfs_Myname, optarg);
		exit(41);
	    }
	    P_biod_max_outstanding_writes = atoi(optarg);
	    if (P_biod_max_outstanding_writes < 0) {
		(void) fprintf(stderr, "%s: write count must be >= 0\n",
				sfs_Myname);
		exit(42);
	    }
	    break;

	case 'w': /* Set warmup time
		   * used in aggregate reporting only.
		   */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr, "%s: illegal warmup value %s\n",
				sfs_Myname, optarg);
		exit(43);
	    }
	    P_warmuptime = atoi(optarg);
	    if (P_warmuptime < 0) {
		(void) fprintf(stderr, "%s: warmup time must be >= 0\n",
				sfs_Myname);
		exit(44);
	    }
	    break;

	case 'x': /* Set Prime-Client time_out value */
	    if (!isdigit(optarg[0])) {
		(void) fprintf(stderr,
				"%s: illegal time_out value %s\n",
				sfs_Myname, optarg);
	       exit(45);
	    }
	    Prime_time_out = atoi(optarg);
	    break;

	case 'z': /* Do raw data dumps
		   *  used in aggregate reporting only.
		   */
	    P_dump_data++;
	    break;

	default:
	    prog_usage();
	    /* NOTREACHED */
	} /* end switch on argument */

    Num_clients = argc - optind;

    /*
     * allocate space and store clients names
     */
    Client_names = (char **) malloc(Num_clients * sizeof(char *));
    if (Client_names == (char **) 0) {
	(void) fprintf(stderr, "%s: client name malloc %d bytes failed",
		    sfs_Myname, Num_clients * sizeof(char **));
	exit(46);
    }

    for (i = 0; optind < argc; i++, optind++) {
	Client_names[i] = argv[optind];
	if (gethostbyname(argv[optind]) == NULL) {
	    (void) fprintf(stderr, "\n%s: unknown client - %s\n",
			sfs_Myname, argv[optind]);
	    exit(47);
	}
    }

    if (sum_result_fp == NULL)
	sum_result_fp = stdout;

    Pc_log_fd = open(SFS_PRIME_SYNC_LOG, (O_RDWR | O_CREAT), 0666);
    if (Pc_log_fd == -1) {
	perror(SFS_PRIME_SYNC_LOG);
	exit(48);
    }

} /* do_initialize */


/*
 * Small utility routine to pretty print out the names
 * of the clients which did not respond to the message.
 */
static void
printdeadclients(void)
{
    int	*clients;
    int client;
    int i;
    FILE *fd;

    if ((clients = (int *)malloc(sizeof(int) * Num_clients)) == NULL) {
	(void) fprintf(stderr, "%s: malloc failed\n", sfs_Myname);
	(void) fflush(stderr);
	return;
    }

    for (i = 0; i < Num_clients; i++) {
	clients[i] = 0;
    }

    fd = fopen(SFS_PRIME_SYNC_LOG, "r");
    if (fd == NULL) {
	(void) fprintf(stderr,"%s: Cannot open %s\n",
	    sfs_Myname, SFS_PRIME_SYNC_LOG);
	(void) fflush(stderr);
	return;
    }

    while(fread(&client, sizeof(int), 1, fd) == 1) {
	if (client > 0 && client <= Num_clients)
		clients[client - 1] = 1;
    }

    for (i = 0; i < Num_clients; i++) {
	if (clients[i] == 0) {
	    (void) fprintf(stderr, "\n%s: Did not get signal from client %s\n",
		    sfs_Myname, Client_names[i]);
	    (void) fflush(stderr);
	}
    }
    (void) fclose(fd);
    free (clients);
}

/*
 * monitor Logfile until all Clients write to it.
 *
 * Each client appends its client id to the file. So the size
 * of the log file divided by sizeof(int) is the number of
 * clients that have responded.
 */
static void
sync_PC_with_clients(void)
{
    struct stat		statb;		/* for fstat */
    int			num_secs;	/* keep count of time */
    int			clientsremaining;

    num_secs = 0;
    do {
	(void) sleep(1);
	if (fstat(Pc_log_fd, &statb) == -1) {
	    (void) fprintf(stderr, "%s: can't stat Prime Client log %s",
				    sfs_Myname, SFS_PRIME_SYNC_LOG);
	    exit(49);
	}
	num_secs++;
	clientsremaining = Num_clients - (statb.st_size / sizeof(int));
    } while ( (clientsremaining > 0) && (num_secs < Prime_time_out));

    /* if clients not responding then terminate experiment */
    if (clientsremaining > 0) {
	(void) fprintf(stderr,
	"\n%s: Prime Client timeout - did not get signal from %d client(s)\n",
			sfs_Myname, clientsremaining);
	printdeadclients();
	/* send message to clients to stop */
	(int) signal_sfs_clients("PRIME_STOP");
	exit(50);
    }

    /* if more clients than exist responded then syncd is telling us to exit */
    if (clientsremaining < 0) {
	(void) fprintf(stderr,
	"\n%s: Prime Client got too many signals - expected %d got %ld\n",
		sfs_Myname, Num_clients, statb.st_size / sizeof(int));
	/* send message to clients to stop */
	(int) signal_sfs_clients("PRIME_STOP");
	exit(51);
    }

    /* success, so go ahead and truncate the sync logfile */
    (void)close(Pc_log_fd);
    Pc_log_fd = open(SFS_PRIME_SYNC_LOG,
			(O_RDWR | O_CREAT | O_TRUNC | O_APPEND), 0666);
    if (Pc_log_fd == -1) {
	/* problem in truncating sync logfile - stop experiment */
	(void) fprintf(stderr, "%s: can't truncate Prime Client log %s",
		       sfs_Myname, SFS_PRIME_SYNC_LOG);
	(void) fflush(stderr);
	(int) signal_sfs_clients("PRIME_STOP");
	exit(52);
    }

} /* sync_PC_with_clients */


#ifdef CLOSE_CLNT_HANDLE
static int close_clnt_handle = 1;
#else
static int close_clnt_handle = 0;
#endif
/*
 * makes RPC to all clients, dependent on message
 * Save the client handles to keep from doing unnecessary portmapper calls.
 * This can help if we have to route through the busy server.
 */
static int
signal_sfs_clients(char *message)
{
    static int Transac_num = 0;            /* transaction number */
    static CLIENT **	sfs_clnt_handle = NULL;
    static int		*sfs_socket = NULL;
    int *		result;
    int			i;
    sync_string		sync_signal;
    char		transaction_string[MAX_STR1_LEN];

    (void) sprintf(transaction_string,"Prime_%i",++Transac_num);
    sync_signal.clnt_type = message;
    sync_signal.clnt_data = "";
    sync_signal.clnt_transaction = transaction_string;

    if (sfs_clnt_handle == NULL) {
	sfs_socket = (int *) calloc(Num_clients, sizeof(int));
	if (sfs_socket == (int *) 0) {
	   (void) fprintf(stderr, "%s: socket malloc failed.\n",
			  sfs_Myname);
	   exit(53);
	}

	/* allocate space for tcp handles */
	sfs_clnt_handle =  (CLIENT **) calloc(Num_clients, sizeof(CLIENT));
	if (sfs_clnt_handle == (CLIENT **)0 ) {
	     (void) fprintf(stderr, "%s: clnttcp_create out of memory\n",
			sfs_Myname);
	     exit(54);
	}
    }

    /* set up the tcp handles for all the clients */
    for (i = 0; i < Num_clients; i++) {
	if (sfs_clnt_handle[i] == NULL) {
	    struct hostent	*host_info;
	    struct sockaddr_in	clnt_addr;
#ifdef SUNOS
	    int fd;
#endif

	    sfs_socket[i] = RPC_ANYSOCK;

	    if ((host_info = gethostbyname(Client_names[i])) == NULL)
		 return((int) RPC_UNKNOWNHOST);
	    (void) memset((char *) &clnt_addr, '\0', sizeof(clnt_addr));
	    (void) memmove((char *) &clnt_addr.sin_addr,
				(char *) host_info->h_addr,
				host_info->h_length);
	    clnt_addr.sin_family = AF_INET;
	    clnt_addr.sin_port =  0;

	    /*
	     * Create client "handle" used for calling CL_MESSAGEPROG on the
	     * sfs client(s). We tell the RPC package to use the "tcp"
	     * protocol when contacting the clients.
	     */
	    sfs_clnt_handle[i] = clnttcp_create(&clnt_addr, SFS_SYNCPROG,
					SFS_SYNCVERS, &sfs_socket[i],
					MAX_STR2_LEN, MAX_STR2_LEN);

	    if (sfs_clnt_handle[i] == (CLIENT *) NULL) {
		/*
		 * Couldn't establish connection with the sfs Client.
		 * print error message and return status
		 */
		clnt_pcreateerror(Client_names[i]);
		return((int) RPC_FAILED);
	    }
#ifdef SUNOS
	    /*
	     * Some commands in 4.X will fail if there are too many file
	     * descriptors open, if the sfs_ext_mon uses one
	     * of those then it will fail.  We set the close-on-exec
	     * flag to help them.
	     */
	    if (clnt_control(sfs_clnt_handle[i], CLGET_FD, (char *)&fd) ==
			FALSE) {
		clnt_pcreateerror(Client_names[i]);
		return((int) RPC_FAILED);
	    }
	    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		perror("F_SETFD");
		return((int) RPC_FAILED);
	    }
#endif
	} /* end no client handle */

	/*
	 * Call the remote procedure "signal_sfs_cl_1" on each of the clients
	 * pass the client number so sfs_syncd will know which client files
	 * to look for in a shared /tmp area
	 */
	sync_signal.clnt_id = i + 1;
	result = signal_sfs_1(&sync_signal, sfs_clnt_handle[i]);

	if (result == NULL) {
	    /* Error occurred.  Print error message and return error. */
	    clnt_perror(sfs_clnt_handle[i], Client_names[i]);
	    return((int) RPC_CANTSEND);
	}

	/* Okay, we successfully called the remote procedure.  */
	if (*result == 0) {
	    /*
	     * remote procedure was unable to write to its sync file.
	     * Print error message and return failure.
	     */
	    (void) fprintf(stderr,
			"\n%s: Unable to perform remote procedure on %s . \n",
			    sfs_Myname, Client_names[i]);
	    (void) fflush(stderr);
	    return((int) RPC_FAILED);
	}
	if (close_clnt_handle) {
		clnt_destroy(sfs_clnt_handle[i]);
		sfs_clnt_handle[i] = NULL;
	}
    } /* end for each client */

    return(0);

} /* signal_sfs_clients */


/*
 * summarize and print multi-client results
 */
static void
print_multi_results(void)
{
    FILE *	fd;		/* results files */
    int 	i;
    int		k;
    char	res_file[SFS_MAXPATHLEN];		/* results filename */
    char	str[MAX_LINE_LEN];

    double	stdev_msec, var_msec;
    float	tot_squared_time_msec, tot_sum2_msec;
    float	tot_got, tot_secs, tot_msec_calls, tot_res_time;
    int		tot_want, tot_calls, tot_errors;

    double	sq_conf_interval_msec;
    double	conf_interval_msec;
    int		totals;
    int		invalid = 0;

#define MAXOPNAME	19
    struct client_stats {
	/* First line */
	int	version;
	int	num_io_files;
	int	num_access_io_files;
	int	num_non_io_files;
	int	num_symlinks;
	int	num_dirs;
	/* NOPS lines */
	struct client_op_stats {
		char	op[MAXOPNAME];
		int	want;
		float	got;
		int	calls;
		int	errors;
		float	secs;
		float	msec_calls;
		float	res_time;
		float	squared_time_msec;
		float	sum2_msec;
	} op_stats[NOPS];
	/* Last line */
	float	sum_calls_sec;
	float	sum_msec_calls;
	int	sum_secs;
	int	sum_calls;
	int	client_invalid_flag;
	uint32_t total_fss_bytes;
	uint32_t least_fss_bytes;
	uint32_t most_fss_bytes;
	uint32_t base_fss_bytes;
    } *client_stats;


    /*
     * malloc space for statistics
     *  Note last entry is for running totals.
     */
     client_stats = (struct client_stats *) calloc(Num_clients+1,
						sizeof(struct client_stats));
     if (client_stats == (struct client_stats *)0) {
	(void) fprintf(stderr, "%s: client_stats malloc failed.\n",
		      sfs_Myname);
	(void) fflush(stderr);
	exit(55);
     }

    totals = Num_clients;
    /*
     * Read each client file one at a time gathering statistics
     */
    for (i = 0; i < Num_clients;i++) {
	(void) sprintf(res_file, "%s%i", PRIME_RESULTS_LOG, i + 1);
	fd = fopen(res_file, "r");
	if (fd == (FILE *)NULL) {
	    (void) fprintf(stderr, "%s: Cannot open results file - %s\n",
			    sfs_Myname, res_file);
	    (void) fflush(stderr);
	    exit(56);
	}

	/*
	 * the SFS clients compute its File set info. at runtime;
	 * the clients pass back the computed value in the results
	 * (RPC) info. Get the fileset info used by the clients and compute
	 * the aggregate value.
	 */
	if (fgets(str, MAX_LINE_LEN, fd) == NULL) {
	    (void) fprintf(stderr,"%s: can't read data in results file - %s\n",
				sfs_Myname, res_file);
	    (void) fflush(stderr);
	    exit(57);
	}
	if (sscanf(str,"%d %d %d %d %d %d",
				&client_stats[i].version,
				&client_stats[i].num_io_files,
				&client_stats[i].num_access_io_files,
				&client_stats[i].num_non_io_files,
				&client_stats[i].num_symlinks,
				&client_stats[i].num_dirs) != 6) {
	    (void) fprintf(stderr,"%s: data in results file unparseable - %s\n",
				sfs_Myname, res_file);
	    (void) fflush(stderr);
	    exit(58);
	}

	client_stats[totals].num_io_files += client_stats[i].num_io_files;
	client_stats[totals].num_access_io_files +=
					client_stats[i].num_access_io_files;
	client_stats[totals].num_non_io_files +=
					client_stats[i].num_non_io_files;
	client_stats[totals].num_symlinks +=
					client_stats[i].num_symlinks;
	client_stats[totals].num_dirs += client_stats[i].num_dirs;

	/* Gather per operation statistics */
	for (k = 0; k < NOPS; k++) {
	    if (fgets(str, MAX_LINE_LEN, fd) == NULL) {
		(void) fprintf(stderr,
				"%s: can't read data in results file - %s\n",
				sfs_Myname, res_file);
		(void) fflush(stderr);
		exit(59);
	    }
	    if (sscanf(str, "%s %d%% %f%% %d %d %f %f %f%% %f %f",
			client_stats[i].op_stats[k].op,
			&client_stats[i].op_stats[k].want,
			&client_stats[i].op_stats[k].got,
			&client_stats[i].op_stats[k].calls,
			&client_stats[i].op_stats[k].errors,
			&client_stats[i].op_stats[k].secs,
			&client_stats[i].op_stats[k].msec_calls,
			&client_stats[i].op_stats[k].res_time,
			&client_stats[i].op_stats[k].squared_time_msec,
			&client_stats[i].op_stats[k].sum2_msec) != 10) {
		(void) fprintf(stderr,"%s: data in results file unparseable - %s\n",
				sfs_Myname, res_file);
		(void) fflush(stderr);
		exit(60);
	    }
	    if (strcmp(client_stats[i].op_stats[k].op,Ops_name[k]) != 0) {
		(void) fprintf(stderr, "%s: bad data in results file\n",
				    sfs_Myname);
		(void) fflush(stderr);
		exit(61);
	    }
	}

	if (fgets(str, 100, fd) == NULL) {
	    (void) fprintf(stderr,"%s: can't read data in results file - %s\n",
				sfs_Myname, res_file);
		(void) fflush(stderr);
	    exit(62);
	}
	if (sscanf(str,"%f %f %d %d %d %u %u %u %u",
		&client_stats[i].sum_calls_sec,
		&client_stats[i].sum_msec_calls,
		&client_stats[i].sum_secs,
		&client_stats[i].sum_calls,
		&client_stats[i].client_invalid_flag,
		&client_stats[i].total_fss_bytes,
		&client_stats[i].least_fss_bytes,
		&client_stats[i].most_fss_bytes,
		&client_stats[i].base_fss_bytes) != 9) {
	    (void) fprintf(stderr,"%s: data in results file unparseable - %s\n",
				sfs_Myname, res_file);
	    (void) fflush(stderr);
	    exit(63);
	}

	client_stats[totals].sum_secs += client_stats[i].sum_secs;
	client_stats[totals].sum_calls += client_stats[i].sum_calls;
	client_stats[totals].sum_calls_sec +=
					client_stats[i].sum_calls_sec;
	client_stats[totals].sum_msec_calls +=
					client_stats[i].sum_msec_calls;
	client_stats[totals].total_fss_bytes +=
					client_stats[i].total_fss_bytes;
	client_stats[totals].least_fss_bytes +=
					client_stats[i].least_fss_bytes;
	client_stats[totals].most_fss_bytes +=
					client_stats[i].most_fss_bytes;
	client_stats[totals].base_fss_bytes +=
					client_stats[i].base_fss_bytes;

	(void) fclose(fd);
    } /* for Num_clients */

    nfs_version = client_stats[0].version;

    /*
     * print the aggregate test parameters
     */
    (void) fprintf(stdout, "\nAggregate Test Parameters: \n");
    (void) fprintf(stdout, "    Number of processes = %d\n",
		   P_children * Num_clients);
    (void) fprintf(stdout, "    Requested Load (NFS V%d operations/second) = %d\n",
			nfs_version, P_total_load * Num_clients);
    (void) fprintf(stdout, "%s%d\n",
		    "    Maximum number of outstanding biod writes = ",
		    P_biod_max_outstanding_writes);
    (void) fprintf(stdout, "%s%d\n",
		    "    Maximum number of outstanding biod reads = ",
		    P_biod_max_outstanding_reads);

    (void) fprintf(stdout, "%s%d\n%s%d\n",
		    "    Warm-up time (seconds) = ", P_warmuptime,
		    "    Run time (seconds) = ", Prime_runtime);
    if (P_mix_file)
	(void) fprintf(stdout,"%s%s\n", "    NFS Mixfile = ", P_mix_file);
    if (P_iodist_file)
	(void) fprintf(stdout,"%s%s\n", "    Block Size Distribution file = ",
			P_iodist_file);


    (void) fprintf(stdout, "%s%6d%s\n", "    File Set = ",
		client_stats[totals].num_io_files,
		" Files created for I/O operations");
    (void) fprintf(stdout, "%s%10d%s\n", "           ",
		client_stats[totals].num_access_io_files,
		" Files accessed for I/O operations");
    (void) fprintf(stdout, "%s%10d%s\n", "           ",
		client_stats[totals].num_non_io_files,
		" Files for non-I/O operations");
    (void) fprintf(stdout, "%s%10d%s\n", "           ",
		client_stats[totals].num_symlinks,
		" Symlinks");
    (void) fprintf(stdout, "%s%10d%s\n", "           ",
		client_stats[totals].num_dirs,
		" Directories");
    (void) fprintf(stdout, "%s%s\n", "           ",
		"           Additional non-I/O files created as necessary\n");


    (void) fprintf(stdout,"SFS Aggregate Results for %d Client(s), %s\n",
		    Num_clients, lad_timestamp());
    (void) fprintf(stderr, "SPEC SFS Benchmark Version %s, Creation - %s\n",
                                SFS_VERSION_NUM, SFS_VERSION_DATE);
    (void) fprintf(stdout, "NFS Protocol Version %d\n", nfs_version);

    /* print column headers for per operation statistics */
    (void) fprintf(stdout,
"------------------------------------------------------------------------------");
(void) fprintf(stdout, "\n");
(void) fprintf(stdout,"%s\n%s\n%s\n%s\n%s\n",
"NFS         Target Actual     NFS    NFS    Mean    Std Dev  Std Error   Pcnt ",
"Op           NFS    NFS       Op     Op    Response Response of Mean,95%  of  ",
"Type         Mix    Mix     Success Error   Time     Time    Confidence  Total",
"             Pcnt   Pcnt     Count  Count  Msec/Op  Msec/Op  +- Msec/Op  Time ",
"------------------------------------------------------------------------------");
    (void) fflush(stdout);

    /* print per operation statistics */
    for (k = 0; k < NOPS; k++) {
	tot_got = 0;
	tot_secs = 0;
	tot_msec_calls = 0;
	tot_res_time =  0;
	tot_want = 0;
	tot_calls = 0;
	tot_errors = 0;
	tot_sum2_msec = 0;
	tot_squared_time_msec = 0;

	/* total the results from each client */
	for (i = 0; i < Num_clients; i++) {

	    tot_want += client_stats[i].op_stats[k].want;
	    tot_got += client_stats[i].op_stats[k].got;
	    tot_calls += client_stats[i].op_stats[k].calls;
	    tot_errors += client_stats[i].op_stats[k].errors;
	    tot_secs += client_stats[i].op_stats[k].secs;
	    tot_msec_calls += client_stats[i].op_stats[k].msec_calls;
	    tot_res_time += client_stats[i].op_stats[k].res_time;
	    tot_squared_time_msec +=
			client_stats[i].op_stats[k].squared_time_msec;
	    tot_sum2_msec += client_stats[i].op_stats[k].sum2_msec;

	} /* end for each client */

	/*
	 * If the total wanted is zero and no operations succeeded or
	 * errored don't print out the
	 * summary.  However leave it in the individual client logs
	 * in case there is some interesting error.
	 */
	if (tot_want == 0 && tot_calls == 0 && tot_errors == 0)
	    continue;

	/* compute the standard deviation for the mean response time */
	if (tot_calls <= 1) {
	    stdev_msec = 0;
	    var_msec = 0;
	} else {
	    /* variance = 1/(n-1) * (sum(x^2) - 1/n * (sum(x))^2)  */
	    var_msec = (tot_squared_time_msec - (tot_sum2_msec / tot_calls)) /
			(tot_calls-1);

	    if(var_msec == 0.0) {
		stdev_msec = 0.0;
	    } else
		stdev_msec = sqrt(var_msec);
	}

	/* compute the confidence interval */
	if (tot_calls > 0) {
	    sq_conf_interval_msec = DEFAULT_CHI_SQR_CI *
				    (stdev_msec / tot_calls);
	    if (sq_conf_interval_msec == 0.0) {
    		if (DEBUG_PARENT_GENERAL) {
		    (void) fprintf(stderr,
			"Error computing confidence interval for mean\n");
		    (void) fflush(stderr);
		}
		conf_interval_msec = 0.0;
	    } else
		conf_interval_msec = sqrt(sq_conf_interval_msec);
	} else {
	    conf_interval_msec = 0.0;
	}

	/* print the per op statistics */
	(void) fprintf(stdout,
	    "%-12s%3d%%  %5.1f%%   %7d %5d %8.2f %8.2f  %8.2f    %5.1f%%\n",
	    Ops_name[k],					/* op name */
	    tot_want / Num_clients,				/* target mix */
	    tot_got / Num_clients,				/* actual mix */
	    tot_calls,						/* successes */
	    tot_errors,						/* errors */
	    tot_msec_calls / Num_clients,			/* mean */
	    stdev_msec, 					/* std dev */
	    conf_interval_msec,					/* conf int */
	    tot_res_time / Num_clients);			/* % of time */

    } /* end for each op */

    (void) fprintf(stdout,
"------------------------------------------------------------------------------\n");

    /* check and report client INVALID RUN */
    for (i = 0; i < Num_clients; i++) {
	if (client_stats[i].client_invalid_flag != 0) {
	    invalid++;
	    (void) fprintf(stdout,"INVALID RUN reported for Client %d (%s).\n",
		   i+1, Client_names[i]);
	    if (client_stats[i].client_invalid_flag >= INVALID_GOODCALLS) {
	        (void) fprintf(stdout,"INVALID RUN, %s\n",
			invalid_str[client_stats[i].client_invalid_flag]);
	    } else {
		(void) fprintf(stdout,
                        "INVALID RUN, ILLEGAL PARAMETER: Non-standard %s\n",
			invalid_str[client_stats[i].client_invalid_flag]);
	    }
	}
    }

    (void) fprintf(stdout, "\n");
    (void) fprintf(stdout,
	"        --------------------------------------------------------\n");
    (void) fprintf(stdout,
	"        | SPEC SFS VERSION %6s AGGREGATE RESULTS SUMMARY    |\n",
							SFS_VERSION_NUM);
    (void) fprintf(stdout,
	"        --------------------------------------------------------\n");
    (void) fprintf(stdout, "NFS V%d THROUGHPUT: ", nfs_version);
    (void) fprintf(stdout,"%7.0f Ops/Sec   AVG. RESPONSE TIME: %7.1f Msec/Op\n",
	client_stats[totals].sum_calls_sec,
	client_stats[totals].sum_msec_calls / Num_clients);

    (void) fprintf(stdout, "%s PROTOCOL\n", P_tcp ? "TCP" : "UDP");
    (void) fprintf(stdout, "NFS MIXFILE:");
    if (P_mix_file)
	(void) fprintf(stdout,"%s\n", P_mix_file);
    else (void)
	(void) fprintf(stdout," [ SFS default ]\n");

    (void) fprintf(stdout, "AGGREGATE REQUESTED LOAD: %d Ops/Sec \n",
		    (Num_clients * P_total_load));

    (void) fprintf(stdout,"TOTAL NFS OPERATIONS: %8d      TEST TIME: %d Sec \n",
		    client_stats[totals].sum_calls,
		    client_stats[totals].sum_secs / Num_clients);
    (void) fprintf(stdout,"NUMBER OF SFS CLIENTS: %d\n", Num_clients);
    (void) fprintf(stdout,
		"TOTAL FILE SET SIZE CREATED: %6.1f MB\n" ,
		client_stats[totals].total_fss_bytes/1024.0);
    (void) fprintf(stdout,
		"TOTAL FILE SET SIZE ACCESSED: %6.1f - %6.1f MB  (%lu%% to %lu%% of Base)\n",
		client_stats[totals].least_fss_bytes/1024.0,
		client_stats[totals].most_fss_bytes/1024.0,
		(100 * client_stats[totals].least_fss_bytes) /
			client_stats[totals].base_fss_bytes,
		(100 * client_stats[totals].most_fss_bytes) /
			client_stats[totals].base_fss_bytes);
    (void) fprintf(stdout, "\n");

    (void) fprintf(stdout,
    "------------------------------------------------------------------------");
    (void) fprintf(stdout,"\n\n");
    (void) fflush(stdout);

    /*
     * Summary results file format:
     *                 response total   elps                  prcs biod  vers
     *  load   thruput time     ops     time V P fileset_sz clnt   rd wr
     * DDDDDDD FFFFFFF FFFFFFF DDDDDDDD DDDD D C DDDDDDDDDD DDD DD DD DD SSSS
     */
    (void) fprintf(sum_result_fp,
"%7s %7d %7.0f %7.1f %8d %4d %1d %c %10lu %3d %2d %2d %2d %s\n",
			invalid ? "INVALID" : "",
			Num_clients * P_total_load,
			client_stats[totals].sum_calls_sec,
			client_stats[totals].sum_msec_calls / Num_clients,
			client_stats[totals].sum_calls,
			client_stats[totals].sum_secs / Num_clients,
			nfs_version,
			P_tcp ? 'T' : 'U',
			(unsigned long)client_stats[totals].total_fss_bytes,
			Num_clients,
			P_children,
			P_biod_max_outstanding_reads,
			P_biod_max_outstanding_writes,
			SFS_VERSION_NUM);
    (void) fflush(sum_result_fp);

    (void) free(client_stats);
} /* print_multi_results */

static void
prog_usage(void)
{
   (void) fprintf(stderr,
      "Usage: %s [-a access_pcnt] [-A append_pcnt] [-b blocksz_file] [-B block_size]\n",
	sfs_Myname);
    (void) fprintf(stderr,
      "\t[-C summary_file] [-d debug_level] [-f file_set_delta]\n");
    (void) fprintf(stderr,
      "\t[-k script_name] [-K script_args] [-l load] [-m mix_file]\n");
    (void) fprintf(stderr,
      "\t[-p procs] [-Q] [-R biod_reads] [-s sleeptime]\n");
    (void) fprintf(stderr,
      "\t[-t time] [-T op_num] [-w warmup] [-x timeout]\n");
    (void) fprintf(stderr,
      "\t[-W biod_writes] [-z] <hostname1> [<hostname2>...]\n");


    (void) fflush(stderr);
    (int) signal_sfs_clients("PRIME_STOP");
    exit(64);
    /* NOTREACHED */
} /* prog_usage */

/* sfs_m_prm.c */
