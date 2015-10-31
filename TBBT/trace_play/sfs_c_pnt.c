#ifndef lint
static char sfs_c_pntSid[] = "@(#)sfs_c_pnt.c	2.1	97/10/23";
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
 *.Exported_Routines
 *	void parent(int, int, char *, char *)
 *
 *.Local_Routines
 *	void synchronize_children(int)
 *	int signal_Prime_Client(char *, char *)
 *	void collect_counters(int)
 *	int check_parameters(char *, char *, int)
 *	int check_counters(void)
 *	void print_results(int, int, char *, int, int, char *)
 *
 *.Revision_History
 *	10-Jan-92	Teelucksingh
 *				Client passes standard deviation compute
 *				values to Prime-Client as well as an
 *				"INVALID RUN" flag.
 *
 *	16-Dec-91	Wittle		Created.
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
#include <signal.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <unistd.h>
#include <sys/wait.h>

#include "sfs_c_def.h"
#include "sfs_m_def.h"

#if !defined(_XOPEN_SOURCE)
#include <sys/socket.h>
#else
#define	AF_INET		2
#endif

/*
 * -------------------------  External Definitions  -------------------------
 */

/* forward definitions for local routines */
static void synchronize_children(int);
static int signal_Prime_Client(char *, char *);
static void collect_counters(int);
static int check_parameters(char *, char *, int);
static int check_counters(void);
static void print_results(int, int, char *, int, int, char *);
static void sfs_reaper(int);

/* Aggregate results storage */
static char Client_results[(NOPS+3)*MAX_LINE_LEN];

/*
 * -------------------------  SFS Parent Code  -------------------------
 */

/*
 * Parent: wait for kids to get ready, start them, wait for them to
 * finish, read and accumulate results.
 */
void
parent(
    int		children,
    int		load,
    char *	mix_file,
    char *	iodist_file)
{
    char	string[80];	/* for interactive startup */
    int		result;
    int		invalid_run;	/* holds INVALID RUN status */
    int		runtime_val;	/* store Runtime value to be printed later */
    int		Saveerrno;
    char	*nameptr;
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    struct sigaction sig_act, old_sig_act;
#endif

    /*
     * Setup a SIGCHLD handler in case one of our beloved children dies
     * before its time.
     */
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    /* use XOPEN signal handling */

    sig_act.sa_handler = sfs_reaper;
    (void)sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;
    if (sigaction(SIGCHLD,&sig_act,&old_sig_act) == -1) {
        perror("sigaction failed: SIGCHLD");
        exit(66);
    }
#else
    (void) signal(SIGCHLD, sfs_reaper);
#endif

    /* Change my name for error logging */
    if ((nameptr = strrchr(sfs_Myname, '/')) != NULL)
        sfs_Myname = ++nameptr;

    /*
     * store the Runtime value; to be printed in results
     */
    if (Prime_client)
	runtime_val = Runtime - MULTICLIENT_OFFSET;
    else runtime_val = Runtime;

    /* print logfile header information */
    (void) fprintf(stdout,"\n");
    (void) fprintf(stdout,
    "************************************************************************");
    (void) fprintf(stdout,"\n");
    (void) fflush(stdout);

    /* print sfs information */
    if (Prime_client) {
	(void) fprintf(stderr,
		"\nSFS NFS Version %d Benchmark Client Logfile, %s\n",
			nfs_version, lad_timestamp());
	(void) fprintf(stderr, "\tClient hostname = %s\n", lad_hostname);
	(void) fprintf(stderr, "\tPrime Client hostname = %s\n",
			Prime_client);
    }

    (void) fprintf(stderr, "\nSPEC SFS Benchmark Version %s, Creation - %s\n",
				SFS_VERSION_NUM, SFS_VERSION_DATE);
    (void) fprintf(stderr, "NFS Protocol Version %d\n", nfs_version);

    /* mount test directories */
    (void) fprintf(stderr, "%s Mounting %d remote test directories.\n",
		lad_timestamp(), children);
    synchronize_children(children);
    (void) fprintf(stderr, "%s Completed.", lad_timestamp());

    /*
     * if multi-client execution then tell Prime-Client I'm done mounting
     * test directories.
     */
    if (Prime_client) {
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
		       "%s Sending DONE-MOUNT message to Prime Client(%s).\n",
			lad_timestamp(), Prime_client);
	if ((result =
	    (int) signal_Prime_Client("CLIENT_SIGNAL", ""))
		== (int) RPC_SUCCESS) {
	    (void) fprintf(stderr, "%s Completed.",lad_timestamp());
	    (void) fflush(stderr);
	} else {
	    (void) fprintf(stderr, "\n");
	    (void) fprintf(stderr,
		"%s:  error %d sending DONE-MOUNT message to Prime Client\n",
		sfs_Myname, result);
	    /* cleanup and exit */
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
	    sig_act.sa_handler = SIG_DFL;
	    (void)sigemptyset(&sig_act.sa_mask);
	    sig_act.sa_flags = 0;
	    if (sigaction(SIGCHLD,&sig_act,&old_sig_act) == -1) {
	        perror("sigaction failed: SIGCHLD");
	        exit(67);
	    }
#else
	    (void) signal(SIGCHLD, SIG_DFL);
#endif
	    (void) generic_kill(0, SIGINT);
	    exit(68);
	}

	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
		    "%s Waiting on DO-INIT message from Prime Client(%s).\n",
		    lad_timestamp(), Prime_client);
	(void) fflush(stderr);

	/*
	 * wait for DO-INIT message from Prime Client
	 * sfs_syncd (rpc server) sends a SIGUSR1 signal;
	 * user can also terminate experiment anytime they wish
	 * with SIGINT or SIGTERM signal
	 */
	(void) pause();
	(void) fprintf(stderr, "%s Received.",lad_timestamp());
	(void) fflush(stderr);

    } /* send DONE-MOUNT and got DO-INIT message */

    /* initialize test directories */
    (void) fprintf(stderr, "\n");
    (void) fprintf(stderr, "%s Initializing test directories.\n",
		    lad_timestamp());

    /* send SIGUSR1 to child processes */
    (void) generic_kill(0, SIGUSR1);
    synchronize_children(children);
    (void) fprintf(stderr, "%s Completed.", lad_timestamp());
    (void) fflush(stderr);

    /*
     * if multi-client execution then tell Prime-Client I'm done initializing
     * and wait for synchronized do warmupmessage.
     */
    if (Prime_client) {
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
		    "%s Sending DONE-INIT message to Prime Client(%s).\n",
			lad_timestamp(), Prime_client);
	if ((result =
	    (int) signal_Prime_Client("CLIENT_SIGNAL",""))
		== (int) RPC_SUCCESS) {
	    (void) fprintf(stderr, "%s Completed.",lad_timestamp());
	    (void) fflush(stderr);
	} else {
	    (void) fprintf(stderr, "\n");
	    (void) fprintf(stderr,
		    "%s:  error %d sending DONE-INIT message to Prime Client\n",
		    sfs_Myname, result);
	    /* cleanup and exit */
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
	    sig_act.sa_handler = SIG_DFL;
	    (void)sigemptyset(&sig_act.sa_mask);
	    sig_act.sa_flags = 0;
	    if (sigaction(SIGCHLD,&sig_act,&old_sig_act) == -1) {
	        perror("sigaction failed: SIGCHLD");
	        exit(69);
	    }
#else
	    (void) signal(SIGCHLD, SIG_DFL);
#endif
	    (void) generic_kill(0, SIGINT);
	    exit(70);
	}
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
		  "%s Waiting on DO-WARMUP message from Prime Client(%s).\n",
		    lad_timestamp(), Prime_client);
	(void) fflush(stderr);

	/*
	 * wait for DO-WARMUP message from Prime Client
	 * sfs_syncd (rpc server) sends a SIGUSR1 signal;
	 * user can also terminate experiment anytime they wish
	 * with SIGINT or SIGTERM signal
	 */
	(void) pause();
	(void) fprintf(stderr, "%s Received.",lad_timestamp());
	(void) fflush(stderr);

    } /* send DONE-INIT and got DO-WARMUP message */

    if (Populate_only) {
	(void) fprintf(stderr, "\nPopulating directories and exiting.\n");
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
	sig_act.sa_handler = SIG_DFL;
	(void)sigemptyset(&sig_act.sa_mask);
	sig_act.sa_flags = 0;
	if (sigaction(SIGCHLD,&sig_act,&old_sig_act) == -1) {
	    perror("sigaction failed: SIGCHLD");
	    exit(71);
	}
#else
	(void) signal(SIGCHLD, SIG_DFL);
#endif
	(void) generic_kill(0, SIGUSR1);
	while (wait((int *) 0) != -1) {
	    /* nop */
	}
	return;
    }

    /* do warm-up */
    if (Warmuptime) {
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr, "%s Performing %d seconds pretest warmup.\n",
		lad_timestamp(), Warmuptime);
	(void) generic_kill(0, SIGUSR1);
	(void) sleep(Warmuptime);
	(void) fprintf(stderr, "%s Completed.", lad_timestamp());
	(void) fflush(stderr);
    }

    if (Interactive) {
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr, "Hit <return> when ready to start test ...");
	(void) fgets(string,10,stdin);
    }

    /*
     * if multi-client execution then tell Prime-Client I'm done warm-up
     * and wait for synchronized Start message.
     */
    if (Prime_client) {
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
			"%s Sending READY message to Prime Client(%s).\n",
			lad_timestamp(), Prime_client);
	if ((result =
	    (int) signal_Prime_Client("CLIENT_SIGNAL",""))
		== (int) RPC_SUCCESS) {
	    (void) fprintf(stderr, "%s Completed.",lad_timestamp());
	    (void) fflush(stderr);
	} else {
	    (void) fprintf(stderr, "\n");
	    (void) fprintf(stderr,
		    "%s:  error %d sending READY message to Prime Client\n",
		    sfs_Myname, result);
	    /* cleanup and exit */
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
	    sig_act.sa_handler = SIG_DFL;
	    (void)sigemptyset(&sig_act.sa_mask);
	    sig_act.sa_flags = 0;
	    if (sigaction(SIGCHLD,&sig_act,&old_sig_act) == -1) {
	        perror("sigaction failed: SIGCHLD");
	        exit(72);
	    }
#else
	    (void) signal(SIGCHLD, SIG_DFL);
#endif
	    (void) generic_kill(0, SIGINT);
	    exit(73);
	}

	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
		    "%s Waiting on START message from Prime Client(%s).\n",
		    lad_timestamp(), Prime_client);
	(void) fflush(stderr);

	/*
	 * wait for START message from Prime Client
	 * sfs_syncd (rpc server) sends a SIGUSR1 signal;
	 * user can also terminate experiment anytime they wish
	 * with SIGINT or SIGTERM signal
	 */
	(void) pause();
	(void) fprintf(stderr, "%s Received.",lad_timestamp());
	(void) fflush(stderr);

    } /* send READY and got START message */

    (void) fprintf(stderr, "\n");
    if (Timed_run) {
	if (Prime_client) {
	    (void) fprintf(stderr, "%s Starting %d seconds test run.\n",
		    lad_timestamp(), Runtime - MULTICLIENT_OFFSET);
	} else {
	    (void) fprintf(stderr, "%s Starting %d seconds test run.\n",
		    lad_timestamp(), Runtime);
	}
    } else {
	(void) fprintf(stderr, "%s Starting %d call test run.\n",
		lad_timestamp(), Ops[TOTAL].target_calls);
    }
    (void) fflush(stderr);

    /* signal child processes to go */
    (void) generic_kill(0, SIGUSR1);

    if (Timed_run)
	(void) sleep(Runtime);

#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
    sig_act.sa_handler = SIG_DFL;
    (void)sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;
    if (sigaction(SIGCHLD,&sig_act,&old_sig_act) == -1) {
        perror("sigaction failed: SIGCHLD");
        exit(74);
    }
#else
    (void) signal(SIGCHLD, SIG_DFL);
#endif

    if (Timed_run) {
	/*
	 * The parent and the prime are both sleeping for Runtime.
	 * If the parent wakes up first, he'll tell the children to stop.
	 * If the prime wakes up first, he'll send an SIGALRM (via syncd)
	 * to the parent.  That alarm may arrive while the parent is still
	 * asleep, which is ok, or after he has starting running.  Since
	 * the parent SIGARLM catcher does nothing, there is no harm done
	 * by the extra signal in this case.
	 *
	 * Perhaps, if running multi we should just wait (pause()) for
	 * the STOP signal, like we waited for the start signal.  It would
	 * be more obvious.  The only drawback is the OTW rpc delay in
	 * receiving the stop signal from the prime.
	 */
	(void) generic_kill(0, SIGUSR2); /* tell children to finish */
    }

    /* Wait for all the children to finish/die */
    while (wait((int *) 0) != -1) {
	/* nop */
    }

    (void) fprintf(stderr, "%s Completed.", lad_timestamp());
    (void) fflush(stdout);
    (void) fflush(stderr);

    /* Initialize and sum up counters */
    collect_counters(children);
    if ((invalid_run = check_counters()) == 0)
	invalid_run = check_parameters(iodist_file, mix_file, runtime_val);

    /* print test results */
    print_results(children, load, mix_file,
		  invalid_run, runtime_val, iodist_file);

    /*
     * if multi-client execution then tell Prime client that
     * I'm done with 'real' work and wait for move-data message
     * and send data across
     */
    if (Prime_client) {
	(void) fprintf(stderr,
			"%s Sending DONE-TEST message to Prime Client(%s).\n",
			lad_timestamp(), Prime_client);
	if ((result =
	    (int) signal_Prime_Client("CLIENT_SIGNAL",""))
		== (int) RPC_SUCCESS) {
	    (void) fprintf(stderr, "%s Completed.", lad_timestamp());
	    (void) fflush(stderr);
	} else {
	    Saveerrno = errno;
	    (void) fprintf(stderr, "\n");
	    (void) fprintf(stderr,
		    "%s:  error %d sending DONE-TEST message to Prime Client\n",
		    sfs_Myname, result);
	    errno = Saveerrno;
	    perror("signal_Prime_Client");
	    /* cleanup and exit */
	    (void) generic_kill(0, SIGINT);
	    exit(75);
	}

	/*
	 * wait for MOVE-DATA message from Prime Client before
	 * sending send results.
	 */
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr,
		"%s Waiting on MOVE-DATA message from Prime Client(%s).\n",
		lad_timestamp(), Prime_client);
	(void) fflush(stderr);
	(void) pause();
	(void) fprintf(stderr, "%s Received.", lad_timestamp());
	(void) fprintf(stderr, "\n");
	(void) fprintf(stderr, "%s Sending results to Prime Client(%s)\n",
		lad_timestamp(), Prime_client);
	(void) fflush(stderr);


	if ((result = (int) signal_Prime_Client("CLIENT_DATA",
	    Client_results)) == (int) RPC_SUCCESS) {
	    (void) fprintf(stderr, "%s Completed.\n", lad_timestamp());
	    (void) fflush(stderr);
	} else {
	    Saveerrno = errno;
	    (void) fprintf(stderr, "\n");
	    (void) fprintf(stderr,
		    "%s: error %d sending client's result to Prime Client\n",
		    sfs_Myname, result);
	    errno = Saveerrno;
	    perror("signal_Prime_Client");
	    /* cleanup and exit */
	    (void) generic_kill(0, SIGINT);
	    exit(76);
	}
    } /* sent done, got move-data and sent data */

    (void) fprintf(stdout,"\n");
    (void) fprintf(stdout,
    "************************************************************************");
    (void) fprintf(stdout,"\n");

} /* parent */


/*
 * ------------------------  Utility Routines  --------------------------
 */


/*
 * Monitor Logfile until its size reaches 'children' bytes.
 * This means that all of the children are waiting for the next instruction.
 */
static void
synchronize_children(
    int		children)
{
    struct stat	statb;	/* for fstat */

    do {
	(void) sleep(1);
	if (fstat(Log_fd, &statb) == -1) {
	    (void) fprintf(stderr, "%s: can't stat log %s", sfs_Myname, Logname);
	    (void) generic_kill(0, SIGINT);
	    exit(77);
	}
    } while (statb.st_size < children);

    /*
     * Truncate the log file
     */
    (void)close(Log_fd);
    Log_fd = open(Logname, (O_RDWR | O_CREAT | O_TRUNC | O_APPEND), 0666);
    if (Log_fd == -1) {
	(void) fprintf(stderr, "%s: can't truncate log %s",
				sfs_Myname, Logname);
	(void) generic_kill(0, SIGINT);
	exit(78);
    }

}  /* synchronize_children */


/*
 * Multi-client execution support routine.
 * Call remote procedure on Prime client
 * to send message to sfs_prime_clnt program.
 * The message will contain a type field set to 'type',
 * and a data field set to 'data'.
 */
static int
signal_Prime_Client(
    char *		type,
    char *		data)
{
    static CLIENT *	clnt_handle = NULL;
    static int		transaction_id = 0;
    int *		result;
    static int		socket;
    sync_string		sync_signal;
    char		transaction_string[MAX_STR1_LEN];
    char		buf[128];

    if ((int)strlen(data) > MAX_STR2_LEN) {
	(void) fprintf(stderr,
			"%s: %s too much data len = %d max = %d\n",
			sfs_Myname, Prime_client, strlen(data),
			MAX_STR2_LEN);
	return((int)RPC_CANTENCODEARGS);
    }

    if (clnt_handle == NULL) {
	struct sockaddr_in	prime_addr;
	struct hostent *	host_info;

	socket = RPC_ANYSOCK;

	/* get host information for prime_client */
	if ((host_info = gethostbyname(Prime_client)) == NULL) {
	    (void) fprintf(stderr, "%s: %s is unknown host\n",
			sfs_Myname, Prime_client);
	    return ((int) RPC_UNKNOWNHOST);
	}

	(void) memset((char *) &prime_addr, '\0', sizeof(prime_addr));
	(void) memmove((char *) &prime_addr.sin_addr,
			(char *) host_info->h_addr, host_info->h_length);

	prime_addr.sin_family = AF_INET;
	prime_addr.sin_port =  0;

	/*
	 * Create client "handle" used for calling SFS_SYNCPROG on the
	 * Prime Client. We tell the RPC package to use the "tcp"
	 * protocol when contacting the prime_client.
	 */
	clnt_handle = clnttcp_create(&prime_addr, SFS_SYNCPROG,
			SFS_SYNCVERS, &socket, MAX_STR2_LEN, MAX_STR2_LEN);

	if (clnt_handle == ((CLIENT *) NULL)) {
	    /*
	     * Couldn't establish connection with the Prime_Client.
	     * Print error message and return error.
	     */
	    clnt_pcreateerror(Prime_client);
	    (void) fprintf(stderr,
		"%s: %s Could not establish client handle to contact %s\n",
		lad_timestamp(), sfs_Myname, Prime_client);
	    return((int) RPC_FAILED);
	}
    }

    /* fill up xdr structure with data to send to Prime Client */
    (void) sprintf(transaction_string,"%d_%i", Client_num, ++transaction_id);
    sync_signal.clnt_type = type;
    sync_signal.clnt_id = Client_num;
    sync_signal.clnt_data = data;
    sync_signal.clnt_transaction = transaction_string;

    /* Call the remote procedure "signal_sfs_1" on the Prime Client */
    result = signal_sfs_1(&sync_signal, clnt_handle);
    if (result == NULL) {
	/*
	 * An error occurred while making RPC to the Prime Client.
	 * Print error message and return error.
	 */
	sprintf(buf, "%s Transaction %s: Could not call prime client %s",
			lad_timestamp(), transaction_string, Prime_client);
	clnt_perror(clnt_handle, buf);
	return((int) RPC_CANTSEND);
    }

    /* OK, we successfully called the remote procedure. */
    if (*result == 0) {
	/*
	 * remote procedure was unable to successfully perform required
	 * operation on the Prime Client.
	 * Print error message and return error.
	 */
	(void) fprintf(stderr,
			"%s: %s Prime Client couldn't write to PC_sync file \n",
			sfs_Myname, Prime_client);
	return((int) RPC_FAILED);
    }

    /* remote procedure success - wrote to Prime Client sync file */
    return((int) RPC_SUCCESS);

} /* signal_Prime_Client */


/*
 * Read results arrays for 'children' children from Logfile
 * and accumulate them in "Ops".
 * Complain about any problems we see with the log file.
 */
static void
collect_counters(
    int			children)
{
    int				i;
    int				j;
    struct stat			statb;
    sfs_results_report_type	report; /* final results log */
    int				Saveerrno;

    if (fstat(Log_fd, &statb) == -1) {
	Saveerrno = errno;
	(void) fprintf(stderr, "%s: can't stat log %s ", sfs_Myname, Logname);
	errno = Saveerrno;
	perror(Logname);
	(void) generic_kill(0, SIGINT);
	exit(79);
    }

    if (statb.st_size != (children * sizeof(report))) {
	(void) fprintf(stderr, "%s: log file %s has bad format\n",
			sfs_Myname, Logname);
	(void) generic_kill(0, SIGINT);
	exit(80);
    }

    if (lseek(Log_fd, 0L, 0) == -1) {
	Saveerrno = errno;
	(void) fprintf(stderr, "%s: can't lseek log %s ", sfs_Myname, Logname);
	errno = Saveerrno;
	perror("lseek");
	(void) generic_kill(0, SIGINT);
	exit(81);
    }

    for (j = 0; j < NOPS + 1; j++) {
	Ops[j].results.good_calls = 0;
	Ops[j].results.bad_calls = 0;
	Ops[j].results.fast_calls = 0;
	Ops[j].results.time.sec = 0;
	Ops[j].results.time.usec = 0;
	Ops[j].results.msec2 = 0;
    }
    Total_fss_bytes = 0;
    Least_fss_bytes = 0;
    Most_fss_bytes = 0;
    Base_fss_bytes = 0;

    for (i = 0; i < children; i++) {
	if (read(Log_fd, (char *) &report, sizeof(report)) == -1) {
	    Saveerrno = errno;
	    (void) fprintf(stderr, "%s: can't read log %s", sfs_Myname, Logname);
	    errno = Saveerrno;
	    perror("Logname");
	    (void) generic_kill(0, SIGINT);
	    exit(82);
	}

	for (j = 0; j < NOPS + 1; j++) {
	    Ops[j].results.good_calls += report.results_buf[j].good_calls;
	    Ops[j].results.bad_calls += report.results_buf[j].bad_calls;
	    Ops[j].results.fast_calls += report.results_buf[j].fast_calls;
	    ADDTIME(Ops[j].results.time, report.results_buf[j].time);
	    Ops[j].results.msec2 += report.results_buf[j].msec2;
	}
	Total_fss_bytes += report.total_fss_bytes;
	Least_fss_bytes += report.least_fss_bytes;
	Most_fss_bytes += report.most_fss_bytes;
	Base_fss_bytes += report.base_fss_bytes;
    }

} /* collect_counters */


/*
 * Check the parameters for validity.
 */
static int
check_parameters(
char *	iodist_file,
char *	mix_file,
int	runtime_val)
{
    int retval = 0;
    char detail[40];

    detail[0] = '\0';

    if (iodist_file != NULL) {
	retval = INVALID_IODIST;
    }
    if (mix_file != NULL) {
	retval = INVALID_MIX;
    }
    if (runtime_val != DEFAULT_RUNTIME) {
	(void) sprintf(detail, "%d != %d", runtime_val, DEFAULT_RUNTIME);
	retval = INVALID_RUNTIME;
    }
    if (Access_percent != DEFAULT_ACCESS) {
	(void) sprintf(detail, "%d != %d", Access_percent, DEFAULT_ACCESS);
	retval = INVALID_ACCESS;
    }
    if (Append_percent != DEFAULT_APPEND) {
	(void) sprintf(detail, "%d != %d", Append_percent, DEFAULT_APPEND);
	retval = INVALID_APPEND;
    }
    if (Kb_per_block != DEFAULT_KB_PER_BLOCK) {
	(void) sprintf(detail, "%d != %d", Kb_per_block, DEFAULT_KB_PER_BLOCK);
	retval = INVALID_KB;
    }
    if (Files_per_dir != DEFAULT_FILES_PER_DIR) {
	(void) sprintf(detail, "%d != %d", Files_per_dir, DEFAULT_FILES_PER_DIR);
	retval = INVALID_NDIRS;
    }
    if (Fss_delta_percent != DEFAULT_DELTA_FSS) {
	(void) sprintf(detail, "%d != %d",
				Fss_delta_percent, DEFAULT_DELTA_FSS);
	retval = INVALID_FSS;
    }
    if (Biod_max_outstanding_reads < DEFAULT_BIOD_MAX_READ) {
	(void) sprintf(detail, "%d < %d",
			Biod_max_outstanding_reads, DEFAULT_BIOD_MAX_READ);
	retval = INVALID_BIODREAD;
    }
    if (Tot_client_num_symlinks != DEFAULT_NSYMLINKS) {
	(void) sprintf(detail, "%d != %d",
			Tot_client_num_symlinks, DEFAULT_NSYMLINKS);
	retval = INVALID_NSYMLINKS;
    }
    if (Biod_max_outstanding_writes < DEFAULT_BIOD_MAX_WRITE) {
	(void) sprintf(detail, "%d < %d",
			Biod_max_outstanding_writes, DEFAULT_BIOD_MAX_WRITE);
	retval = INVALID_BIODWRITE;
    }
    if (Warmuptime != DEFAULT_WARMUP) {
	(void) sprintf(detail, "%d != %d",
			Warmuptime, DEFAULT_WARMUP);
	retval = INVALID_WARMUP;
    }

    if (retval != 0)
	(void) fprintf(stdout,
		"%s: INVALID RUN, ILLEGAL PARAMETER: Non-standard %s %s\n",
		sfs_Myname, invalid_str[retval], detail);
    return (retval);
}

/*
 * Check the results in Ops[] for validity.
 */
static int
check_counters(void)
{
    double	mix_pcnt;
    int		bad_pcnt;
    int		i;
    int		ret = 0;

    if (Ops[TOTAL].results.good_calls <= 0) {
	(void) fprintf(stdout, "%s: INVALID RUN %s\n",
				sfs_Myname, invalid_str[INVALID_GOODCALLS]);
	ret = INVALID_GOODCALLS;
    }
    if (Ops[TOTAL].results.good_calls != 0)
        bad_pcnt = (Ops[TOTAL].results.bad_calls * 100)
	       / Ops[TOTAL].results.good_calls;
    else
	bad_pcnt = 100;

    if (bad_pcnt >= 1) {
	(void) fprintf(stdout, "%s: INVALID RUN, %d%% %s\n",

			sfs_Myname, bad_pcnt,
			invalid_str[INVALID_FAILEDRPC]);
	ret = INVALID_FAILEDRPC;
    }

    if (Ops[TOTAL].results.good_calls == 0) {
	(void) fprintf(stdout, "%s: INVALID RUN, no good calls\n", sfs_Myname);
	return (INVALID_NOTMIX);
    }

    for (i = 0; i < NOPS; i++) {
	mix_pcnt = ((double)Ops[i].results.good_calls /
					Ops[TOTAL].results.good_calls) * 100.0;
	if (mix_pcnt != (double)Ops[i].mix_pcnt) {
	    if ((mix_pcnt - (double)Ops[i].mix_pcnt > 1.5) ||
	        ((double)Ops[i].mix_pcnt - mix_pcnt > 1.5)) {
		(void) fprintf(stdout, "%s: INVALID RUN, %s target %d%% actual %4.1f%% %s\n",

			sfs_Myname, Ops[i].name, Ops[i].mix_pcnt, mix_pcnt,
			invalid_str[INVALID_NOTMIX]);
		ret = INVALID_NOTMIX;
	    }
	}
    }

    return (ret);

} /* check_counters */


/*
 * Print the test run results, for 'load' load, the operation percentages
 * in 'mixfile' percentages, and 'children' processes.
 */
static void
print_results(
    int				children,
    int				load,
    char *			mix_file,
    int				invalid_flag,
    int				runtime_val,
    char *			iodist_file)
{
    uint_t			runtime;
    uint_t			total_msec;
    uint_t			msec;
    uint_t			total_calls;
    uint_t			calls;
    int				i;
    double			squared_time_msec;
    double			sum2_msec;
    double			var_msec;
    double			stdev_msec;
    double			sq_conf_interval_msec;
    double			conf_interval_msec;
    sfs_op_type *		op_ptr;
    sfs_results_type *		results_ptr;
    char			result_string[MAX_LINE_LEN];


    /* compute total time for all ops combined */
    total_msec = 0;
    for (i = 0; i < NOPS; i++) {
	total_msec += Ops[i].results.time.sec * 1000;
	total_msec += Ops[i].results.time.usec / 1000;
    }

    /*
     * Report statistics based on successful calls only.  The per
     * operation routines accumulate time and count only good_calls.
     */
    total_calls = Ops[TOTAL].results.good_calls;


    /*
     * Print the client's test parameters
     */
    (void) fprintf(stderr, "\n\nClient Test Parameters: \n");
    (void) fprintf(stderr, "\tNumber of processes = %d\n", children);
    (void) fprintf(stderr, "\tRequested Load (NFS V%d operations/second) = %d\n",
		    nfs_version, load);
    (void) fprintf(stderr, "\tMaximum number of outstanding biod writes = %d\n",
		    Biod_max_outstanding_writes);
    (void) fprintf(stderr, "\tMaximum number of outstanding biod reads = %d\n",
		    Biod_max_outstanding_reads);
    (void) fprintf(stderr, "\tWarm-up time (seconds) = %d\n\tRun time (seconds) = %d\n",
		    Warmuptime, runtime_val);
    if (mix_file)
	(void) fprintf(stderr,"\tNFS Mixfile = %s\n", mix_file);
    if (iodist_file)
	(void) fprintf(stderr,"\tBlock Size Distribution file = %s\n",
			iodist_file);
    (void) fprintf(stderr, "\tFile Set = %4d Files created for I/O operations\n",
		(Tot_client_num_io_files/children + 1) * children);
    (void) fprintf(stderr, "\t\t   %4d Files accessed for I/O operations\n",
		(((Tot_client_num_io_files/children + 1) * Access_percent)
		   / 100) * children);
    (void) fprintf(stderr, "\t\t   %4d Files for non-I/O operations\n",
		(Tot_client_num_non_io_files/children + 1) * children);
    (void) fprintf(stderr, "\t\t   %4d Symlinks\n",
		(Tot_client_num_symlinks/children + 1) * children);
    (void) fprintf(stderr, "\t\t   %4d Directories\n",
		((Tot_client_num_io_files/children + 1) / Files_per_dir ) * children);
    (void) fprintf(stderr, "\t\t\tAdditional non-I/O files created as necessary\n\n");

    (void) sprintf(Client_results,"%d %d %d %d %d %d\n",
	nfs_version,
	(Tot_client_num_io_files/children + 1) * children,
	(((Tot_client_num_io_files/children + 1) * Access_percent)
		/ 100) *children,
	(Tot_client_num_non_io_files/children + 1) * children,
	(Tot_client_num_symlinks/children + 1) * children,
	((Tot_client_num_io_files/children + 1) / Files_per_dir ) * children);

    /* print the client's results header information */
    (void) fprintf(stderr, "\nSPEC SFS Benchmark Version %s, Creation - %s\n",
                                SFS_VERSION_NUM, SFS_VERSION_DATE);
    (void) fprintf(stdout, "SFS Single Client (%s) Results, %s\n",
		    lad_hostname, lad_timestamp());
    (void) fflush(stdout);

    /* print column headers for per operation statistics */
    (void) fprintf(stdout,
"----------------------------------------------------------------------------\n");
    (void) fprintf(stdout, "\n");
    (void) fprintf(stdout,
"NFS V%d     Target Actual   NFS    NFS    Mean     Std Dev  Std Error   Pcnt \n",
nfs_version);
    (void) fprintf(stdout,
"Op           NFS    NFS     Op     Op    Response Response of Mean,95%%  of  \n");
    (void) fprintf(stdout,
"Type         Mix    Mix   Success Error   Time     Time    Confidence  Total\n");
    (void) fprintf(stdout,
"             Pcnt   Pcnt   Count  Count  Msec/Op  Msec/Op  +- Msec/Op  Time \n");
    (void) fprintf(stdout,
"----------------------------------------------------------------------------\n");
    (void) fflush(stdout);

    /* print per operation statistics */
    for (i = 0; i < NOPS; i++) {
	/* init to 0 */
	squared_time_msec = 0.0;
	sum2_msec = 0.0;
	calls = 0;
	msec = 0;
	stdev_msec = 0;

	op_ptr = &Ops[i];
	results_ptr = &op_ptr->results;

	/* get the number successful calls and total time */
	calls = op_ptr->results.good_calls;
	msec = (results_ptr->time.sec * 1000)
	       + (results_ptr->time.usec / 1000);

	/* compute the standard deviation for the mean response time */
	if (calls <= 1)
	    stdev_msec = 0;
	else {
	    /* get the standard deviation */
	    squared_time_msec = results_ptr->msec2;
	    /* compute the square of the total elapsed time */
	    sum2_msec = (results_ptr->time.sec * 1000.0)
			 + (results_ptr->time.usec / 1000.0);
	    sum2_msec *= sum2_msec;

	    /* variance = 1/(n-1) * (sum(x^2) - 1/n * (sum(x))^2) */
	    var_msec = (squared_time_msec - (sum2_msec / calls)) / (calls-1);
	    if (var_msec == 0.0) {
		stdev_msec = 0.0;
	    } else
		stdev_msec = sqrt(var_msec);
	}

	/* compute the confidence interval */
	if (calls != 0) {
	    sq_conf_interval_msec = DEFAULT_CHI_SQR_CI * (stdev_msec / calls);
	    if (sq_conf_interval_msec == 0.0) {
		conf_interval_msec = 0.0;
	    } else
		conf_interval_msec = sqrt(sq_conf_interval_msec);
	} else
	    conf_interval_msec = 0.0;

	/* print the per op statistics */
	(void) fprintf(stdout,
	"%-12s%3d%%   %4.1f%%   %5d %5d %5.2f %8.2f  %8.2f    %3.1f%%\n",
	    op_ptr->name,					/* op name */
	    op_ptr->mix_pcnt, 					/* target mix */
								/* actual mix */
	    total_calls ? ((double)calls / total_calls) * 100.0 : 0.0,
	    results_ptr->good_calls,				/* successes */
	    results_ptr->bad_calls,				/* errors */
	    calls ? ((double)msec / calls) : 0.0,		/* msec/call */
	    stdev_msec,						/* std dev */
	    conf_interval_msec,					/* conf int */
								/* % of time */
	    total_msec ? ((double)msec / total_msec) * 100 : 0.0);
	(void) fflush(stdout);

	/*
	 * Store client data in result_string.
	 * This string is different from client result display.
	 * The  squared_time_msec and sum2_msec values are passed along
	 * to be used by the prime client to calculate the stddev value for
	 * each operation.
	 */
	if (Prime_client) {
	(void) sprintf(result_string,
	"%-12s   %3d%% %3.1f%% %5d  %5d %4ld.%1ld %6.2f  %3.1f%% %f %f\n",
	    op_ptr->name,					/* op name */
	    op_ptr->mix_pcnt,					/* target mix */
								/* actual mix */
	    total_calls ? ((double)calls / total_calls) * 100.0 : 0.0,
	    results_ptr->good_calls,				/* successes */
	    results_ptr->bad_calls,				/* errors */
	    results_ptr->time.sec,				/* total time1*/
	    results_ptr->time.usec / 100000,			/* total time2*/
	    calls ? ((double)msec / calls) : 0.0,		/* msec/call */
								/* % of time */
	    total_msec ? ((double)msec / total_msec) * 100 : 0.0,
	    squared_time_msec,					/* sum of sqs */
	    sum2_msec);						/* sq of sums */
	    (void) strcat(Client_results, result_string);
	}

    } /* end for each op */

    (void) fprintf(stdout,
"----------------------------------------------------------------------------\n\n");
    (void) fflush(stdout);

    /* Average child runtime.  (should this be the longest runtime?) */
    runtime = Ops[TOTAL].results.time.sec / children;

    /* Print summary */
    (void) fprintf(stdout,
	"      ------------------------------------------------------------\n");
    (void) fprintf(stdout,
	"      | SPEC SFS VERSION %6s SINGLE CLIENT RESULTS SUMMARY    |\n",
							SFS_VERSION_NUM);
    (void) fprintf(stdout,
	"      ------------------------------------------------------------\n");
    (void) fprintf(stdout, "NFS V%d THROUGHPUT: ", nfs_version);
    (void) fprintf(stdout,
		    "%4d.%02d Ops/Sec   AVG. RESPONSE TIME: %4d.%02d Msec/Op\n",
		    runtime ? (total_calls / runtime) : 0,
		    runtime ? ((total_calls % runtime) * 100 / runtime) : 0,
		    total_calls ? (total_msec / total_calls) : 0,
		    total_calls ? ((total_msec % total_calls) * 100 / total_calls) : 0);
    (void) fprintf(stdout, "%s PROTOCOL\n", Tcp ? "TCP" : "UDP");
    (void) fprintf(stdout, "FAST CALLS: %d\n", Ops[TOTAL].results.fast_calls);
    (void) fprintf(stdout, "NFS MIXFILE: ");
    if (mix_file)
	(void) fprintf(stdout,"%s\n", mix_file);
    else
	(void) fprintf(stdout,"[ SFS Default ]\n");
    (void) fprintf(stdout, "CLIENT REQUESTED LOAD: %d Ops/Sec \n", load);
    (void) fprintf(stdout,
			"TOTAL NFS OPERATIONS: %-6d      TEST TIME: %d Sec \n",
			total_calls, runtime);
    (void) fprintf(stdout, "FILE SET SIZE CREATED: %d KB\n",
							Total_fss_bytes);
    (void) fprintf(stdout,
		"FILE SET SIZE ACCESSED: %d - %d KB  (%d%% to %d%% of Base)\n",
		Least_fss_bytes, Most_fss_bytes,
		(100 * Least_fss_bytes) / Base_fss_bytes,
		(100 * Most_fss_bytes) / Base_fss_bytes);
    (void) fprintf(stdout, "\n");
    (void) fprintf(stdout,
    "------------------------------------------------------------------------");
    (void) fprintf(stdout, "\n\n");
    (void) fflush(stdout);

    /*
     * store client summary results and Invalid run indicator
     * to send to the Prime_client
     */
    if (Prime_client) {
	(void) sprintf(result_string,"%d.%02d %d.%02d %d %d %d %d %d %d %d\n",
	    runtime ? (total_calls / runtime) : 0,	    /* ops/sec1 */
	    runtime ? ((total_calls % runtime) * 100 / runtime) : 0, /* ops/sec2 */
	    total_calls ? (total_msec / total_calls) : 0,	/* mean1 */
	    total_calls ? ((total_msec % total_calls) * 100 / total_calls) : 0, /* mean2 */
	    runtime,         				    /* run time */
	    total_calls,	 			    /* # ops */
	    invalid_flag,    				    /* valid flag */
	    Total_fss_bytes,   				    /* total fileset */
	    Least_fss_bytes,   				    /* fileset low */
	    Most_fss_bytes,				    /* fileset high */
	    Base_fss_bytes);				    /* fileset base */
	(void) strcat(Client_results, result_string);
    }

} /* print_results */


/* ARGSUSED */
static void
sfs_reaper(
    int		sig_id)
{
    (void) fprintf(stderr, "%s: caught unexpected SIGCHLD. Exiting...\n",
		       sfs_Myname);
    /* cleanup and exit */
    (void) signal_Prime_Client("CLIENT_STOP", "");
    (void) generic_kill(0, SIGINT);
    exit(83);
}
/* sfs_c_pnt.c */
