/* Try to change thread scheduling and uses three threads */
#ifndef lint
static char sfs_c_chdSid[] = "@(#)sfs_c_chd.c	2.1	97/10/23";
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
 * -------------------------- sfs_c_chd.c -------------------------
 *
 *      The sfs child.  Routines to initialize child parameters,
 *	initialize test directories, and generate load.
 *
 *.Exported_Routines
 *	void child(int, float, int, char *);
 *	void init_fileinfo(void);
 *	void init_counters(void);
 *	sfs_fh_type * randfh(int, int, uint_t, sfs_state_type,
 *				sfs_file_type);
 *	int check_access(struct *stat)
 *	int check_fh_access();
 *
 *.Local_Routines
 *	void check_call_rate(void);
 *	void init_targets(void);
 *	void init_dirlayout(void);
 *	void init_rpc(void);
 *	void init_testdir(void);
 *	int do_op(void);
 *	int op(int);
 *
 *.Revision_History
 *	21-Aug-92	Wittle 		randfh() uses working set files array.
 *					init_fileinfo() sets up working set.
 *      02-Jul-92	Teelucksingh    Target file size now based on peak load
 *					instead of BTDT.
 *	04-Jan-92	Pawlowski	Added raw data dump hooks.
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
#include <math.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <unistd.h>

#include "sfs_c_def.h"
#include "sfs_m_def.h"
#include "rfs_c_def.h"
#include "generic_hash.h"
#include "nfsd_nfsfh_cust.h"

extern struct hostent   *Server_hostent;

#define PROB_SCALE 1000L
#define _M_MODULUS 2147483647L /* (2**31)-1 */

#define _GROUP_DIVISOR 500
#define _FILES_PER_GROUP 4
#define _MIN_GROUPS 12
#define _WORKING_SET_AT_25_OPS_PER_SEC 975


/*
 * -----------------------  External Definitions  -----------------------
 */
extern uint32_t    biod_clnt_call(CLIENT *, uint32_t, xdrproc_t, void *);
extern enum clnt_stat proc_header(CLIENT *cl, xdrproc_t xdr_results, void *results_ptr);
extern int  biod_poll_wait(CLIENT *, uint32_t);
extern enum clnt_stat get_areply_udp (CLIENT * cl, uint32_t *xid, struct timeval *timeout);
extern char * parse_name (char * t, char * buf);

/* forward definitions for local functions */
static int init_rpc(void);

/* RFS: forward definitions for local functions */
void init_ops(void);
static void init_signal();
extern void init_file_system (void);
extern void init_dep_tab (void);
static int read_trace(void);
static void read_fh_map();
static void init_play (char * mount_point);
static void trace_play(void);
void print_result(void);
static int get_nextop(void);
static int check_timeout(void);
static struct biod_req * get_biod_req(int dep_tab_index);
static int lookup_biod_req (int xid);
static void init_time_offset(void);
void adjust_play_window (int flag, int * poll_timeout_arg);
static int poll_and_get_reply (int usecs);
static char * nfs3_strerror(int status);
static void check_clock(void);
static double time_so_far1(void);
static double get_resolution(void);
static void usage(void);
void init_dep_tab_entry (int dep_index);
extern inline fh_map_t * lookup_fh (char * trace_fh );
static void finish_request (int biod_index, int dep_index, int status, int dep_flag);
static inline void add_new_file_system_object (int proc, int dep_index, char * line, char * reply_line);
static inline char * find_lead_trace_fh(int proc, char * line);
static inline char * find_reply_trace_fh (char * line);

/*
 * -------------  Per Child Load Generation Rate Variables  -----------
 */
static uint_t Calls_this_period; /* calls made during the current run period */
static uint_t Calls_this_test;	/* calls made during the test so far */
static uint_t Reqs_this_period;	/* reqs made during the current run period */
static uint_t Reqs_this_test;	/* reqs made during the test so far */
static uint_t Sleep_msec_this_test; /* msec slept during the test so far */
static uint_t Sleep_msec_this_period;
static uint_t Previous_chkpnt_msec; /* beginning time of current run period */
static int Target_sleep_mspc;	/* targeted sleep time per call */

static char io_buf[BUFSIZ];	/* io buffer for print out messages */

char * sfs_Myname;
int     Log_fd;                         /* log fd */
char    Logname[NFS_MAXNAMLEN];         /* child processes sync logfile */
int 	Validate = 0;					/* fake variable */
int Child_num = 0;						/* fake: child index */
int Tcp = 0;							/* We implement UDP first */
int Client_num = 1;						/* fake: number of client */
uid_t Real_uid;
gid_t Cur_gid;
uid_t Cur_uid;
/* as long as the inital value is different, then it's OK */
int recv_num = 0;
int timeout_num = 0;
int send_num = 0;
int exit_flag = 0;
int async_rpc_sem;
int no_progress_flag = 0;
int num_out_reqs_statistics[MAX_OUTSTANDING_REQ+1];
int num_out_reqs_statistics_at_timeout[MAX_OUTSTANDING_REQ+1];

/*
 * -------------------------  SFS Child  -------------------------
 */

static int nfs2proc_to_rfsproc[18] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
									10, 11, 12, 13, 14, 15, 16, 17};
static int nfs3proc_to_rfsproc[NFS3_PROCEDURE_COUNT] = {0, 1, 2, 4, 18, 5, 6, 8, 9, 14, 
													13, 21, 10, 15, 11, 12, 16, 23, 17, 20, 
													22, 19};
void print_usage(int pos, int argc, char ** argv)
{
	int i;
	printf("sfs3 hostname:mount_dir trace_file|stdin fh_map_file play_scale warmup_time(in seconds) \n");
	printf("sfs3 -pair_trace trace_file\n");
	printf("sfs3 -pair_write trace_file\n");
	printf("sfs3 -help\n");
	printf ("pos %d argc %d", pos, argc);
	for (i=0; i<argc; i++) {
		printf(" %s", argv[i]);
	} 
	printf ("\n");
	exit;
}

void print_cyclic_buffers ()
{
	CYCLIC_PRINT(memory_trace_index);
	CYCLIC_PRINT(dep_tab_index);
	CYCLIC_PRINT(dep_window_index);
}

void add_to_dep_tab(int i)
{
	char * trace_fh;
	fh_map_t * fh_map_entry;
	dep_tab_t * ent;

    trace_fh = strstr (memory_trace[i].line, "fh");
    if (!trace_fh)
	printf ("memory_trace[%d].line %s\n", i, memory_trace[i].line);
    RFS_ASSERT (trace_fh);
    trace_fh += 3;
    fh_map_entry = lookup_fh (trace_fh);
    if (fh_map_entry && (fh_map_entry->flag==FH_MAP_FLAG_DISCARD) )  {
        req_num_with_discard_fh ++;
		return;
	}
    if (fh_map_entry)
        req_num_with_init_fh ++;
    else
        req_num_with_new_fh ++;
                                                                                                                              
	RFS_ASSERT (!CYCLIC_FULL(dep_tab_index));
	ent = &(dep_tab[dep_tab_index.head]);

    ent->disk_index = memory_trace[i].disk_index;
	ent->memory_index = i;
#ifdef REDUCE_MEMORY_TRACE_SIZE
    ent->trace_status = memory_trace[i].trace_status;
    ent->reply_trace_fh = memory_trace[i].reply_trace_fh;
#endif
    ent->line = memory_trace[i].line;
    init_dep_tab_entry(dep_tab_index.head);

    if (rfs_debug && (i%100000)==0)
    	printf ("dep_tab[%d].disk_index %d = memory_trace[%d].disk_index %d\n", dep_tab_index.head, ent->disk_index, i, memory_trace[i].disk_index);
    CYCLIC_MOVE_HEAD(memory_trace_index);
    CYCLIC_MOVE_HEAD(dep_tab_index);
}

void init_profile_variables ()
{
	init_profile ("total_profile", &total_profile);
	init_profile ("execute_next_request_profile", &execute_next_request_profile);
	init_profile ("valid_get_nextop_profile", &valid_get_nextop_profile);
	init_profile ("invalid_get_nextop_profile",&invalid_get_nextop_profile);
	init_profile ("prepare_argument_profile", &prepare_argument_profile);
	init_profile ("biod_clnt_call_profile", &biod_clnt_call_profile);
	init_profile ("receive_next_reply_profile", &receive_next_reply_profile);
	init_profile ("valid_poll_and_get_reply_profile", &valid_poll_and_get_reply_profile);
	init_profile ("invalid_poll_and_get_reply_profile", &invalid_poll_and_get_reply_profile);
	init_profile ("decode_reply_profile", &decode_reply_profile);
	init_profile ("check_reply_profile", &check_reply_profile);
	init_profile ("add_create_object_profile", &add_create_object_profile);
	init_profile ("check_timeout_profile", &check_timeout_profile);
	init_profile ("adjust_play_window_profile",&adjust_play_window_profile);
	init_profile ("fgets_profile",&fgets_profile);
	init_profile ("read_line_profile",&read_line_profile);
	init_profile ("read_trace_profile",&read_trace_profile);
}

static char trace_file[256]="anon-lair62-011130-1200.txt";
int print_memory_usage()
{
	printf("size of fh_map_t %d size of fh_map %d\n", sizeof(fh_map_t), sizeof(fh_map));
	printf("sizeof dep_tab_t %d sizeof dep_tab %d\n", sizeof(dep_tab_t), sizeof(dep_tab));
	printf("size of memory_trace_ent_t %d sizeof memory_trace %d\n", sizeof(memory_trace_ent_t), sizeof(memory_trace));
	printf("size of CREATE3args %d\n", sizeof( CREATE3args));
	printf("size of MKDIR3args %d\n", sizeof( MKDIR3args));
	printf("size of READ3args %d\n", sizeof( READ3args));
	printf("size of WRITE3args %d\n", sizeof( WRITE3args));
	printf("size of RENAME3args %d\n", sizeof( RENAME3args));
	printf("size of GETATTR3args %d\n", sizeof( GETATTR3args));
	printf("size of SETATTR3args %d\n", sizeof( SETATTR3args));
	printf("size of LINK3args %d\n", sizeof( LINK3args));
	printf("size of SYMLINK3args %d\n", sizeof( SYMLINK3args));
	printf("size of MKNOD3args %d\n", sizeof( MKNOD3args));
	printf("size of RMDIR3args %d\n", sizeof( RMDIR3args));
	printf("size of REMOVE3args %d\n", sizeof( REMOVE3args));
	printf("size of LOOKUP3args %d\n", sizeof( LOOKUP3args));
	printf("size of READDIR3args %d\n", sizeof( READDIR3args));
	printf("size of READDIRPLUS3args %d\n", sizeof( READDIRPLUS3args));
	printf("size of FSSTAT3args %d\n", sizeof( FSSTAT3args));
	printf("size of FSINFO3args %d\n", sizeof( FSINFO3args));
	printf("size of COMMIT3args %d\n", sizeof( COMMIT3args));
	printf("size of ACCESS3args %d\n", sizeof( ACCESS3args));
	printf("size of READLINK3args %d\n", sizeof( READLINK3args));


}

void recv_thread()
{
	int last_print_time = -1;
	int busy_flag;

	while (send_num ==0) {
		usleep(1000);
	}

	//while (!CYCLIC_EMPTY(dep_tab_index)) {
	while (!exit_flag) {
		if ((total_profile.in.tv_sec - last_print_time >= 10)) {
			static int recv_num_before_10_seconds = 0;
			last_print_time = total_profile.in.tv_sec;
			fprintf (stdout, "<<<<< recv_thread recv_num %d time %d num_out_reqs %d \n", recv_num, total_profile.in.tv_sec, num_out_reqs);
			if (recv_num == recv_num_before_10_seconds) {
				no_progress_flag = 1;
				RFS_ASSERT (0);
			} else 
				recv_num_before_10_seconds = recv_num;
		}
		//start_profile (&check_timeout_profile);
		check_timeout();
		//end_profile (&check_timeout_profile);

		pthread_yield();
		//usleep(1000);
		start_profile (&receive_next_reply_profile);
		/* actually the performance of two policy seems to be same */
//#define SEND_HIGHER_PRIORITY_POLICY
#define SEND_RECEIVE_EQUAL_PRIORITY_POLICY	
#ifdef SEND_HIGHER_PRIORITY_POLICY
		receive_next_reply(IDLE);
#endif
#ifdef SEND_RECEIVE_EQUAL_PRIORITY_POLICY
		busy_flag = IDLE;
		while (receive_next_reply(busy_flag)!=-1) {
			busy_flag = BUSY;
		}
#endif
		end_profile (&receive_next_reply_profile);
	}
	printf ("<<<< recv thread EXIT\n");
	exit_flag = 2;
}

int io_thread ()
{
/* number of seconds the I/O thread pauses after each time trying to read the requests */
#define IO_THREAD_PAUSE_TIME 1

	int i;
	int j = 0;

	disk_io_status = read_trace ();
	while (disk_io_status == TRACE_BUF_FULL) {

		usleep (10000);
        if ((j++%1000)==0) {
                printf("&&&&&&&&&& io thread, sleep %d seconds\n", j/100);
        }

		disk_io_status = read_trace ();
		//printf ("io_thread, after read_trace, disk_index %d\n", disk_index);

#ifdef SEQUEN_READ
		for (i=0; i<SEQUEN_READ_NUM; i++) {
			add_to_dep_tab(CYCLIC_MINUS(memory_trace_index.head,1,memory_trace_index.size));
		}
#endif
		//printf ("***************** IO THREAD SLEEP 1 s\n");
		//print_cyclic_buffers();
		//pthread_yield();
	}
	RFS_ASSERT (disk_io_status == TRACE_FILE_END);
	printf("io_thread EXIT\n");
}

int execute_thread()
{
	int i;
	struct timeval playstart_time, playend_time;

	sleep (10);		/* first let io_thread to run for a while */
	printf ("trace_play\n");

	gettimeofday(&playstart_time, NULL);

	init_time_offset();
	trace_play ();

	gettimeofday(&playend_time, NULL);

	{
		int usec, sec;
		sec = playend_time.tv_sec - playstart_time.tv_sec;
		usec = sec * 1000000 + (playend_time.tv_usec - playstart_time.tv_usec);
		sec = usec / 1000000;
		usec = usec % 1000000;
		printf("Total play time: %d sec %d usec\n", sec, usec);
		if (sec > WARMUP_TIME)
			print_result();
		else
			printf ("the trace play time %d is less than WARMUP_TIME %d, no statistical results\n");
	}
#ifdef RECV_THREAD
	exit_flag = 1;
	while (exit_flag == 1) {
		usleep (1000);
	}
#endif

    clnt_destroy(NFS_client);
    biod_term();
}

int init_thread ()
{
	pthread_attr_t	attr;
	int arg;
	int ret = 0;
	pthread_t io_thread_var;
#ifdef RECV_THREAD
	pthread_t recv_thread_var;
#endif
	pthread_t execute_thread_var;

	ret = pthread_attr_init (&attr);
	if (ret!=0) {
		perror("pthread_attr_init attr");
		return ret;
	}
#ifdef IO_THREAD
	ret = pthread_create (&(io_thread_var), &attr, &io_thread, (void *)&arg );
	if (ret!=0) {
		perror("io_pthread_attr_init");
		return ret;
	}
#endif

#ifdef RECV_THREAD
	ret = pthread_create (&(recv_thread_var), &attr, &recv_thread, (void *)&arg );
	if (ret!=0) {
		perror("recv_pthread_attr_init");
		return ret;
	}
#endif

/*
	ret = pthread_create (&(execute_thread_var), &attr, &execute_thread, (void *)&arg );
	if (ret!=0) {
		perror("io_pthread_attr_init");
		return ret;
	}
*/
}

void init_buffers()
{
	CYCLIC_INIT("memory_trace_index",memory_trace_index,MAX_MEMORY_TRACE_LINES);
	CYCLIC_INIT("dep_tab_index     ",dep_tab_index,DEP_TAB_SIZE);
	CYCLIC_INIT("dep_window_index  ",dep_window_index,DEP_TAB_SIZE);
}

int main(int argc, char ** argv)
{
	extern char * optarg;
	int i;
	struct timeval in={1000000, 100};
	
	init_profile_variables();
	if ((argc==1) || (argc==2 && !strcmp(argv[1],"-help"))) {
		print_usage(0, argc, argv);
		exit(0);
	}
	if (!strcmp(argv[1], "-pair_write")) {
		if (argc==3) 
			strcpy(trace_file, argv[2]);
		else
			strcpy(trace_file, "stdin");
		pair_write(trace_file);
		exit(0);
	}
	if (!strcmp(argv[1], "-pair_trace")) {
		if (argc==3) 
			strcpy(trace_file, argv[2]);
		else
			strcpy(trace_file, "stdin");
		pair_trace(trace_file);
		exit(0);
	}
	if (!strcmp(argv[1], "-check_aging")) {
		if (argc!=3) {
			print_usage(3, argc, argv);
			exit(0);
		}
		strcpy(trace_file, argv[2]);
		check_aging (trace_file);
		exit(0);
	}
	if (!strcmp(argv[1], "-check_statistics")) {
		if (argc!=3) {
			print_usage(1, argc, argv);
			exit(0);
		}
		strcpy(trace_file, argv[2]);
		memset(fh_htable, 0, sizeof (fh_htable));
		check_statistics (trace_file);
		exit(0);
	}

	if (argc!=6) {
		print_usage(2, argc, argv);
		exit(0);
	}

	PLAY_SCALE = atoi (argv[4]);
	RFS_ASSERT (PLAY_SCALE >=1 && PLAY_SCALE <=10000);

	WARMUP_TIME = atoi (argv[5]);
	RFS_ASSERT (WARMUP_TIME >=0 && WARMUP_TIME <=1000);
	
	print_memory_usage();
	check_clock();
    getmyhostname(lad_hostname, HOSTNAME_LEN);

    init_ops();
	/*
	 * Get the uid and gid information.
	 */
	Real_uid = getuid();
	Cur_gid = getgid();
	//Real_uid = 513;
	//Cur_gid = 513;

	Nfs_timers = Nfs_udp_timers;

	init_semaphores();
	init_file_system ();
	init_signal();
	init_play (argv[1]);
	//init_play ("capella:/p5/RFSFS");
	init_fh_map();
	read_fh_map (argv[3]);
	//read_fh_map ("fh-path-map-play");
	strcpy(trace_file, argv[2]);

/* If ordered by TIMESTAMP,
 * memory_trace_index.tail <= dep_tab_index.tail < dep_window_max <=
 * dep_tab_index.head <= memory_trace_index.head
 */

	init_global_variables();
	init_buffers();
	init_thread();
	pthread_yield();
	execute_thread();
}

int init_global_variables()
{
	memset (num_out_reqs_statistics, 0, sizeof(num_out_reqs_statistics));
	memset (num_out_reqs_statistics_at_timeout, 0, sizeof(num_out_reqs_statistics_at_timeout));
}

int init_semaphores()
{
	async_rpc_sem = dest_and_init_sem (ASYNC_RPC_SEM_KEY, 1, "async_rpc_sem");
}

void init_ops (void)
{
	Ops = nfsv3_Ops;
	nfs_version = NFS_V3;
}

/* Set up the signal handlers for all signals */
void init_signal()
{
#if (defined(_XOPEN_SOURCE) || defined(USE_POSIX_SIGNALS))
	struct sigaction sig_act, old_sig_act;

	/* use XOPEN signal handling */

	sig_act.sa_handler = generic_catcher;
	(void)sigemptyset(&sig_act.sa_mask);
	sig_act.sa_flags = 0;

	/* signals handlers for signals used by sfs */
	sig_act.sa_handler = sfs_cleanup;
	if (sigaction(SIGINT,&sig_act,&old_sig_act) == -1) {
	    perror("sigaction failed: SIGINT");
	    exit(135);
	}

	sig_act.sa_handler = sfs_cleanup;
	if (sigaction(SIGTERM,&sig_act,&old_sig_act) != 0)  {
	    perror("sigaction failed: SIGTERM");
	    exit(137);
	}
#else
    /* signals handlers for signals used by sfs */
    (void) signal(SIGINT, sfs_cleanup);
    // RFS (void) signal(SIGALRM, sfs_alarm);
	(void) signal(SIGTERM, sfs_cleanup);
#endif
}

void
init_play (
    char	* mount_point)		/* Mount point for remote FS */
{
    char	namebuf[NFS_MAXNAMLEN] = "trace_play";	/* unique name for this program */
    CLIENT *	mount_client_ptr;	/* Mount client handle */

	if (!rfs_debug);
    	(void) setvbuf(stderr, io_buf, _IOLBF, BUFSIZ);

    sfs_Myname = namebuf;

    /*
     * May require root priv to perform bindresvport operation
     */
    mount_client_ptr = lad_getmnt_hand(mount_point);
    if (mount_client_ptr == NULL) {
		exit(145);
    }

    /*
     * should be all done doing priv port stuff
     */

    if (init_rpc() == -1) {
		(void) fprintf(stderr, "%s: rpc initialization failed\n", sfs_Myname);
		(void) generic_kill(0, SIGINT);
		exit(146);
    }


    /*
     * finish all priv bindresvport calls
     * reset uid
     */
    if (setuid(Real_uid) != (uid_t)0) {
	(void) fprintf(stderr,"%s: %s%s", sfs_Myname,
	    "cannot perform setuid operation.\n",
	    "Do `make install` as root.\n");
    }

    init_mount_point(0, mount_point, mount_client_ptr);


    /*
     * Cleanup client handle for mount point
     */
    clnt_destroy(mount_client_ptr);

	init_counters();
}

#ifdef REDUCE_MEMORY_TRACE_SIZE
void inline format_line (char * line_before, char * line)
{
	char * pv = line_before;
	char * pl = line;
	char * p;
	int i;
	
	//printf("format_line before %s\n", line_before);
	p = strstr (pv, "fh");
	while (p!=NULL) {
		while (pv<=p)
			*pl++ = *pv++;
		*pl++ = *pv++;
		if (*pv==2) {
			*pl++ = *pv++;
		}
		*pl++ = *pv++;
		i = 0;
		while (*pv !=' ') {
			RFS_ASSERT ((*pv >='0' && *pv <='9') || (*pv >='a' && *pv<='f'));
			*pl++ = *pv++;
			i++;
		}
		RFS_ASSERT ((i==48) || (i==40) || (i==24));
		while (i<48) {
			*pl++ = '0';
			i++;
		}
		p = strstr (pv, "fh");
	}
	while ((*pv)!='\0') {
		*pl++ = *pv++;
	}
	//printf("format_line after %s\n", line);
}

char * read_line (int disk_index)
{
	static FILE * fp=NULL;
	static int start=0;
	static int start_disk_index=0;
	int i;
	static int finish_flag = 0;
	static int varfh_flag = 0;
#define READ_LINE_BUF_SIZE (MAX_COMMAND_REPLY_DISTANCE_FOR_PAIR+2)
#define SAFE_BYTES 1000
#define READ_LINE_LENGTH (MAX_TRACE_LINE_LENGTH+SAFE_BYTES)

	static char line_buf[READ_LINE_BUF_SIZE][READ_LINE_LENGTH];
	char varfh_line_buf[READ_LINE_LENGTH];

	start_profile (&read_line_profile);

	if (fp==NULL) {
		if (strcmp(trace_file, "stdin")) {
			fp = fopen(trace_file, "r");

			if (strstr(trace_file, ".varfh")) {
				varfh_flag = 1;
			};
			if (strstr(trace_file, ".fmt1")) {
				TRACE_COMMAND_REPLY_FLAG_POS += 12;
				TRACE_VERSION_POS +=12;
				TRACE_MSGID_POS +=12;
				TRACE_FH_SIZE =48;
			}
			if (!fp) {
				printf("can not open files %s\n", fp);
				perror("open");
			}
		} else {
			fp = stdin;
		}
		RFS_ASSERT (fp!=NULL);
		for (i=0; i<READ_LINE_BUF_SIZE; i++) {
			start_profile(&fgets_profile);
			if (varfh_flag) {
				if (!fgets(varfh_line_buf, READ_LINE_LENGTH, fp)) {
					RFS_ASSERT (0);
				}
				format_line (varfh_line_buf, line_buf[i]);
			} else {
				if (!fgets(line_buf[i], READ_LINE_LENGTH, fp)) {
					RFS_ASSERT (0);
				}
			}
			end_profile(&fgets_profile);
			//printf ("read_line, line_buf[%d]:%s", i, line_buf[i]);
		}
	}
	
	if (disk_index > start_disk_index+READ_LINE_BUF_SIZE) {
		printf ("disk_index %d start_disk_index %d READ_LINE_BUF_SIZE %d\n", 
			disk_index, start_disk_index, READ_LINE_BUF_SIZE);
	}
	RFS_ASSERT (disk_index <= start_disk_index+READ_LINE_BUF_SIZE)
	if (disk_index==(start_disk_index+READ_LINE_BUF_SIZE)) {
		if (finish_flag) {
			return NULL;
		}
		start_profile(&fgets_profile);
		if (!fgets(line_buf[start], READ_LINE_LENGTH, fp)) {
			end_profile(&fgets_profile);
			finish_flag = 1;
			return NULL;
		}
		end_profile(&fgets_profile);
		//printf ("read_line, line_buf[%d]:%s", start, line_buf[start]);
		start = (start+1) % READ_LINE_BUF_SIZE;
		start_disk_index ++;
	}
	RFS_ASSERT (disk_index < start_disk_index+READ_LINE_BUF_SIZE)
	i = (start+disk_index-start_disk_index)%READ_LINE_BUF_SIZE;

/*
	if (!(strlen(line_buf[i])>80)) {
		printf ("start %d start_disk_index %d disk_index %d strlen %d line_buf[%d] %s\n", start, start_disk_index, disk_index, strlen(line_buf[i]), i, line_buf[i]);
		RFS_ASSERT (strlen(line_buf[i])>80);
	}
	if (!((strlen(line_buf[i])>80) && (strlen(line_buf[i])<MAX_TRACE_LINE_LENGTH)))
		printf ("disk_index %d strlen %d line_buf[%d] %s\n", disk_index, strlen(line_buf[i]), i, line_buf[i]);
	RFS_ASSERT ((strlen(line_buf[i])>80) && (strlen(line_buf[i])<MAX_TRACE_LINE_LENGTH));
*/

	end_profile (&read_line_profile);
	return (line_buf[i]);
}

#define OPS_FLAG_INC 0
#define OPS_FLAG_PRINT 1
int read_write_fh_statistics (int flag, char * buf, int timestamp)
{
	static FILE * fp = NULL;
	char * p;
	char c;
	if (!fp) {
		fp = fopen ("read_write_fh.output", "w");
		RFS_ASSERT (fp);
	}
	if (flag == OPS_FLAG_INC) {
		p = strstr (buf, "fh");
		RFS_ASSERT (p);
		c = p[40+3];
		p[40+3]=0;
		fprintf(fp, "%s\n", p+3+24);
		p[40+3]=c;
	};
	if (flag == OPS_FLAG_PRINT) { 
		int disk_index = (int) buf;
		fprintf(fp, "###### disk_index %d timestamp %d\n", disk_index, timestamp);
	}
}

int write_statistics(int flag, char * buf, char * reply_buf, int trace_status)
{
	static FILE * fp = NULL;
	int pre_size, size, count;
	char * p = reply_buf;
	if (!fp) {
		fp = fopen ("write.output", "w");
		RFS_ASSERT (fp);
	}
	if (flag == OPS_FLAG_INC) {
		p = strstr (p, "pre-size");
		RFS_ASSERT (p);
		sscanf (p, "pre-size %x", &pre_size); 
		p += strlen("pre-size");
		p = strstr (p, "size");
		RFS_ASSERT (p);
		sscanf (p, "size %x", &size); 
		p = strstr (p, "count");
		if (!p) 
			fprintf (fp, "%s status %x pre-size %x size %x count %x\n", buf, trace_status, pre_size, size, count);
		RFS_ASSERT (p);
		sscanf (p, "count %x", &count); 
		fprintf (fp, "%s status %x pre-size %x size %x count %x\n", buf, trace_status, pre_size, size, count);
	}
	if (flag == OPS_FLAG_PRINT) {
		int disk_index = (int) buf;
		int timestamp = (int) reply_buf;
		fprintf(fp, "###### disk_index %d timestamp %d\n", disk_index, timestamp);
	}
}

void ops_statistics (int ops_flag, int proc, int timestamp)
{
	static FILE * fp = NULL;
	static struct {
		int count;
		int count_K;
		int update_flag;
		char name[32];
	} ops[NOPS]={{0, 0, 0, "NULL"},{0, 0, 0, "GETATTR"},{0, 0, 1, "SETATTR"},{0, 0, 0, "ROOT"},
				  {0, 0, 0, "LOOKUP"},{0, 0, 0, "READLINK"},{0, 0, 0, "READ"},{0, 0, 1, "WRCACHE"},
				  {0, 0, 1, "WRITE"}, {0, 0, 1, "CREATE"},{0, 0, 1, "REMOVE"},{0, 0, 1, "RENAME"},
				  {0, 0, 1, "LINK"}, {0, 0, 1, "SYMLINK"},{0, 0, 1, "MKDIR"},{0, 0, 1, "RMDIR"},
				  {0, 0, 0, "READDIR"},{0, 0, 0, "FSSTAT"},{0, 0, 0, "ACCESS"},{0, 0, 0, "COMMIT"},
				  {0, 0, 0, "FSINFO"},{0, 0, 1, "MKNOD"}, {0, 0, 0, "PATHCONF"}, {0, 0, 0, "READDIRPLUS"}};

	if (ops_flag == OPS_FLAG_INC) {
		RFS_ASSERT (proc>=0 && proc<NOPS);
		ops[proc].count ++;
		if (ops[proc].count == 1000) {
			ops[proc].count_K ++;
			ops[proc].count = 0;
		}
	}
	if (ops_flag == OPS_FLAG_PRINT) {
		int i;
		int update_K=0, update=0, total_K=0, total=0;
		float f1, f2;
		int disk_index = proc;

		if (!fp) {
			fp = fopen ("mix.output", "w");
			RFS_ASSERT (fp);
		}
		for (i=0; i<NOPS; i++) {
			total_K += ops[i].count_K;
			total += ops[i].count;
			if (ops[i].update_flag) {
				update_K += ops[i].count_K;
				update += ops[i].count;
			}
		}
		update_K += update/1000;
		update = update%1000;
		total_K += total/1000;
		total = total%1000;

		f1 = total_K;
		f1 = f1*1000+total;
		for (i=0; i<NOPS; i++) {
			f2 = ops[i].count_K;
			f2 = f2*1000+ops[i].count;
			fprintf (fp, "%12s %8d,%03d %3.2f\%\n", ops[i].name, ops[i].count_K, ops[i].count, f2*100/f1);	
			fprintf (stderr, "%12s %8d,%03d %3.2f\%\n", ops[i].name, ops[i].count_K, ops[i].count, f2*100/f1);	
		}
		f2 = update_K;
		f2 = f2*1000+update;
		fprintf (fp, "       total %8d,%03d\n", total_K, total);
		fprintf (stderr, "       total %8d,%03d\n", total_K, total);
		fprintf (fp, "      update %8d,%03d %2.2f\%\n", update_K, update, f2*100/f1);	
		fprintf (stderr, "      update %8d,%03d %2.2f\%\n", update_K, update, f2*100/f1);	
		fprintf(fp, "###### disk_index %d timestamp %d\n", disk_index, timestamp);
		fprintf(stderr, "###### disk_index %d timestamp %d\n", disk_index, timestamp);
	}
}


void truncate_statistics (int flag, int proc, char * buf, char * reply_buf) 
{
#define TRACE_FH_SIZE2 16
#define BLOCK_SIZE 4096
	static int no_size_num = 0;
	static int have_size_num = 0;
	static int equal_size_num = 0;
	static int first_size_num = 0;
	static int truncate_num = 0;
	static int truncate_size = 0;
	static int truncate_KB = 0;
	static int truncate_block_num = 0;
	static int padding_num = 0;
	static int padding_KB = 0;
	static int padding_size = 0;
	static FILE * fp = NULL;
	char * p;
	char * trace_fh;
	int pre_size, size, count;
	struct generic_entry * ent;

	if (flag == OPS_FLAG_PRINT) {
		int disk_index = proc;
		int timestamp = (int)buf;
		if (!fp) {
			fp = fopen ("truncate.output", "w");
			RFS_ASSERT (fp);
		}
		truncate_KB += truncate_size/1000;
		truncate_size %= 1000;
		padding_KB += padding_size/1000;
		padding_size %= 1000;
		fprintf(fp, "###### disk_index %d timestamp %d no_size_req %d have_size_req %d equal_size_req %d trunc_req %d trunc_KB %d trunc_block_num %d padding_num %d padding_KB %d\n", disk_index, timestamp, no_size_num, have_size_num, equal_size_num, truncate_num, truncate_KB, truncate_block_num, padding_num, padding_KB);
		fprintf(stderr, "###### disk_index %d timestamp %d no_size_req %d have_size_req %d equal_size_req %d trunc_req %d trunc_KB %d trunc_block_num %d padding_num %d padding_KB %d\n", disk_index, timestamp, no_size_num, have_size_num, equal_size_num, truncate_num, truncate_KB, truncate_block_num, padding_num, padding_KB);
		return;
	}

	p = strstr (&buf[TRACE_MSGID_POS], "fh");
	RFS_ASSERT (p);
	p+=3; 
	trace_fh = p;
		
	if (proc==SETATTR) {
		p = strstr (buf, " size ");
	} else {
		RFS_ASSERT (proc==WRITE);
		p = strstr (reply_buf, " ftype 1 ");
		RFS_ASSERT (p);
		p = strstr (p, " size ");
		RFS_ASSERT (p);
		if (strstr(p, " ftype 1 ")) {
			fprintf (fp, "#### %s%s\n", buf, reply_buf);
			fprintf (stderr, "#### %s%s\n", buf, reply_buf);
		}
		RFS_ASSERT (!strstr(p, " ftype 1 "));
	}
	if (!p) {
		no_size_num ++;
		return;
	}
	have_size_num ++;

	sscanf (p, " size %x", &size); 
	if (size <0 || size > 2000000000) {
		fprintf (fp, "#### size too big %x %s %s\n", size, buf, reply_buf);
		fprintf (stderr, "#### size too big %x %s %s\n", size, buf, reply_buf);
	}
							
	RFS_ASSERT (size >=0 && size <2000000000);		
   	ent = generic_lookup (trace_fh+24, TRACE_FH_SIZE2, 0, fh_htable, FH_HTABLE_SIZE);
	if (ent) {
		if (ent->key3 != size) {
			if (proc==SETATTR) {
				//printf ("%s\n", buf);
				//printf ("size change fh %s pre-size %x size %x\n", trace_fh, ent->key3, size);
				if (ent->key3 > size) {
					truncate_num ++;
					truncate_size += ent->key3 - size;
					truncate_block_num += (ent->key3+BLOCK_SIZE-1)/BLOCK_SIZE;
					if (size!=0) {
						//fprintf (stderr, "truncate: pre_size %x size %x %s\n", ent->key3, size, buf);
						//fprintf (fp, "truncate: pre_size %x size %x %s\n", ent->key3, size, buf);
						truncate_block_num -= (size + BLOCK_SIZE-1)/BLOCK_SIZE;
					}
					if (truncate_size > 1000000000) {
						truncate_KB += truncate_size/1000;
						truncate_size %= 1000;
					}
				} else {
					padding_num ++; 
					//printf ("%s\n", buf);
					//printf ("padding fh %s pre-size %x size %x\n", trace_fh, ent->key3, size);
					padding_size += size - ent->key3;
					if (padding_size > 1000000000) {
						padding_KB += padding_size/1000;
						padding_size %= 1000;
					}
				}
			}
			ent->key3 = size; 
		}else 
			equal_size_num++;
	} else {
   		generic_insert(trace_fh+24, TRACE_FH_SIZE2, size, fh_htable, FH_HTABLE_SIZE);
		first_size_num ++;
	}
};

int get_timestamp_usec (char * buf)
{
	char str[128];
	int ret;
	strncpy(str, buf, 100);
	RFS_ASSERT (str[10]=='.');
	RFS_ASSERT (str[17]==' ');
	str[17]=0;
	ret = atoi(&str[11]);
	RFS_ASSERT (ret >=0 && ret <=999999);
	return ret;
}

int get_timestamp_sec (char * buf)
{
	char str[128];
	int ret;
	strncpy(str, buf, 100);
	RFS_ASSERT (str[10]=='.');
	str[10]=0;
	ret = atoi(str);
	RFS_ASSERT (ret >1000000000 && ret <2000000000);
	return ret;
}

int check_aging (char * tracefile)
{
	int disk_index=-1;
	char *buf; 
	char *reply_buf;
	int i;
	int trace_status;
	int debug = 0;
	int nfs3proc, msgid, proc;

	while ((buf=read_line(++disk_index))!=NULL) {
		if (buf[TRACE_COMMAND_REPLY_FLAG_POS]!='C') 
			continue;
		if (buf[TRACE_VERSION_POS]!='3') 
			continue;
		sscanf (&buf[TRACE_MSGID_POS], "%x %x", &msgid, &nfs3proc);
		
		RFS_ASSERT (nfs3proc>=0 && nfs3proc<NFS3_PROCEDURE_COUNT);
		
		proc = nfs3proc_to_rfsproc[nfs3proc];
		ops_statistics (OPS_FLAG_INC, proc, -1);
		
		switch (proc) {
		int off, count, size;
		char * t;
		case CREATE: printf("%s\n", "create"); break;
		case MKDIR: printf("%s\n", "mkdir"); break;
		case REMOVE: printf("%s\n", "remove"); break;
		case RMDIR: printf("%s\n", "rmdir"); break;
		case WRITE: 
			t = buf;
			t = strstr (t, "off");
	        RFS_ASSERT (t);
   		    t+=4;
			sscanf (t, "%x", &off);
			RFS_ASSERT (off>=0 && off<0x7FFFFFFF)
			t = strstr (t, "count");
		    RFS_ASSERT (t);
   			t+=6;
			sscanf (t, "%x", &count);
			RFS_ASSERT (count <= 32768);
			printf("%s off %x count %x\n", "write", off, count); 
			//printf("%s count %x\n", "write", count); 
			break;
		case SETATTR: 
			t = strstr (buf, " size ");
			if (t) {
				sscanf (t, " size %x", &size);
				printf ("%s size %x\n", "setattr", size);
			}
		}
		if ((disk_index%10000)==0) {
			fprintf(stderr, "%d disk trace passed\n", disk_index);
		}
	};

	fprintf(stderr, "%d disk trace parsed\n", disk_index);
	ops_statistics (OPS_FLAG_PRINT, disk_index, -1);
}


int check_statistics (char * tracefile)
{
	int disk_index=-1;
	char *buf; 
	char *reply_buf;
	int i;
	char * p;
	int trace_status;
	int debug = 0;
	int nfs3proc, msgid, proc;
	static int last_timestamp_sec = -1;
	int timestamp_sec;
	int memory_trace_size = 0;

	while ((buf=read_line(++disk_index))!=NULL) {
		if (buf[TRACE_COMMAND_REPLY_FLAG_POS]!='C') 
			continue;
		if (buf[TRACE_VERSION_POS]!='3') 
			continue;
		sscanf (&buf[TRACE_MSGID_POS], "%x %x", &msgid, &nfs3proc);
		
		RFS_ASSERT (nfs3proc>=0 && nfs3proc<NFS3_PROCEDURE_COUNT);
		timestamp_sec = get_timestamp_sec(buf);
		
		proc = nfs3proc_to_rfsproc[nfs3proc];
		ops_statistics (OPS_FLAG_INC, proc, -1);
		
		if (proc!= WRITE && proc!=SETATTR && proc!=READ) {
			continue;
		}
		RFS_ASSERT (buf[strlen(buf)-1]=='\n');
		buf [strlen(buf)-1] = 0;
		if (!((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH)))
			printf("disk_index %d strlen(buf) %d buf %s \n", disk_index, strlen(buf), buf);
		RFS_ASSERT ((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH));
		if (debug)
			printf("read line disk_index %d %s\n", disk_index, buf);

		trace_status = NFS3ERR_RFS_MISS;
		for (i=disk_index+1; i<disk_index+MAX_COMMAND_REPLY_DISTANCE; i++) {
			reply_buf = read_line(i);
			if (debug)
				printf("searching for reply: read line %s\n", reply_buf);
			if (!reply_buf)
				break;
       		if (reply_buf[TRACE_COMMAND_REPLY_FLAG_POS]=='R') {
           		p = strchr (&reply_buf[TRACE_MSGID_POS], ' ');
           		RFS_ASSERT (p);
           		if (!strncmp(&reply_buf[TRACE_MSGID_POS], &buf[TRACE_MSGID_POS], p-&(reply_buf[TRACE_MSGID_POS]))) {
	               	trace_status = find_reply_status(reply_buf);
					if (trace_status == NFS3_OK) {
						if (proc==READ || proc==WRITE) 
							read_write_fh_statistics (OPS_FLAG_INC, buf, 0);
						if (proc == WRITE)
							write_statistics (OPS_FLAG_INC, buf, reply_buf, trace_status);
						if (proc==WRITE || proc==SETATTR) 
							truncate_statistics (OPS_FLAG_INC, proc, buf, reply_buf);
					}
				};
   	    	}
		}
		//if (memory_trace[memory_trace_size].trace_status == NFS3ERR_RFS_MISS)
		if (trace_status == NFS3ERR_RFS_MISS) {
			//printf ("%s no reply\n", buf);
			missing_reply_num ++;
		}

#if	0	/* commented out by G. Jason Peng */
		if *
		if ((missing_reply_num > memory_trace_size/10) && (missing_reply_num > 100)) {
			printf ("missing_reply_num %d too high for memory_trace_size %d\n", missing_reply_num, memory_trace_size);
			exit (0);
		}
#endif

		memory_trace_size ++;

		if (last_timestamp_sec == -1) {
			last_timestamp_sec = timestamp_sec;
		} else if (timestamp_sec - last_timestamp_sec >=3600) {
			ops_statistics (OPS_FLAG_PRINT, disk_index, timestamp_sec);
			truncate_statistics (OPS_FLAG_PRINT, disk_index, (char *)timestamp_sec, NULL);
			read_write_fh_statistics(OPS_FLAG_PRINT, (char *)disk_index, timestamp_sec);
			write_statistics(OPS_FLAG_PRINT, (char *)disk_index, (char *)timestamp_sec, -1);
			last_timestamp_sec = timestamp_sec;
		}
/*
		if ((memory_trace_size%10000)==0) {
			fprintf(stderr, "%d disk trace parsed, missing_reply %d\n", disk_index, missing_reply_num);
			ops_statistics (OPS_FLAG_PRINT, -1);
			truncate_statistics (OPS_FLAG_PRINT, -1, NULL, NULL);
		}
*/
	};

	fprintf(stderr, "%d disk trace parsed, missing_reply %d\n", disk_index, missing_reply_num);
	ops_statistics (OPS_FLAG_PRINT, disk_index, timestamp_sec);
	truncate_statistics (OPS_FLAG_PRINT, disk_index, (char *)timestamp_sec, NULL);
	read_write_fh_statistics(OPS_FLAG_PRINT, (char *)disk_index, timestamp_sec);
	write_statistics(OPS_FLAG_PRINT, (char *)disk_index, (char *)timestamp_sec, -1);
}


/* This routine output all the requests, together with their replies */
int pair_trace (char * tracefile)
{
	int disk_index=-1;
	char *buf; 
	char *reply_buf;
	int i;
	char * p;
	int trace_status;
	int debug = 0;
	int nfs3proc, msgid;
	int ops[NFS3_PROCEDURE_COUNT]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int memory_trace_size = 0;
	FILE * outputfp;
	char output_file[1024];

	strcpy (output_file, tracefile);
	strcat (output_file, ".pair");
	outputfp = fopen (output_file, "w");
	RFS_ASSERT (outputfp);

	while ((buf=read_line(++disk_index))!=NULL) {
		if (disk_index == 258)
			f();
		if (buf[TRACE_COMMAND_REPLY_FLAG_POS]!='C') 
			continue;
		if (buf[TRACE_VERSION_POS]!='3') 
			continue;
		sscanf (&buf[TRACE_MSGID_POS], "%x %x", &msgid, &nfs3proc);
		
		RFS_ASSERT (nfs3proc>=0 && nfs3proc<NFS3_PROCEDURE_COUNT);
		ops[nfs3proc]++;

		buf [strlen(buf)-1] = 0;
		if (!((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH)))
			printf("disk_index %d strlen(buf) %d buf %s \n", disk_index, strlen(buf), buf);
		RFS_ASSERT ((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH));
		if (debug)
			printf("read line disk_index %d %s\n", disk_index, buf);
		fprintf (outputfp, "%s\n", buf);

		trace_status = NFS3ERR_RFS_MISS;
		for (i=disk_index+1; i<disk_index+MAX_COMMAND_REPLY_DISTANCE_FOR_PAIR; i++) {
			reply_buf = read_line(i);
			if (debug)
				printf("searching for reply: read line %s\n", reply_buf);
			if (!reply_buf)
				break;
       		if (reply_buf[TRACE_COMMAND_REPLY_FLAG_POS]=='R') {
           		p = strchr (&reply_buf[TRACE_MSGID_POS], ' ');
           		RFS_ASSERT (p);
           		if (!strncmp(&reply_buf[TRACE_MSGID_POS], &buf[TRACE_MSGID_POS], p-&(reply_buf[TRACE_MSGID_POS]))) {
					fprintf(outputfp, "%s", reply_buf);
					trace_status = find_reply_status(reply_buf);
					if (debug)
						fprintf(stderr, "reply found trace_status %d\n", find_reply_status(reply_buf));
					break;
				};
   	    	}
		}

		if (trace_status == NFS3ERR_RFS_MISS) {
			fprintf (stderr, "%s no reply\n", buf);
			fprintf(outputfp, "missing_reply\n");
			missing_reply_num ++;
		}

		if (missing_reply_num > memory_trace_size/10 && missing_reply_num >100) {
			fprintf (stderr, "missing_reply_num %d too high for memory_trace_size %d\n", missing_reply_num, memory_trace_size);
			exit (0);
		}

		memory_trace_size ++;

		if ((memory_trace_size%10000)==0)
			fprintf(stderr, "total %d disk lines %d memory lines missing_reply_num %d\n", disk_index, memory_trace_size, missing_reply_num );
	};

	fprintf(stderr, "total %d disk lines %d memory lines missing_reply_num %d\n", disk_index, memory_trace_size, missing_reply_num );
    //fprintf (stderr, "init_dep_tab, req_num_with_init_fh %d req_num_with_new_fh %d discard %d\n", req_num_with_init_fh, req_num_with_new_fh, req_num_with_discard_fh);

}
/* This routine output all the write requests, together with their replies. It is used for
 * analysis of write requests: appended bytes, overwrite bytes etc
 */
int pair_write (char * tracefile)
{
	int disk_index=-1;
	char *buf; 
	char *reply_buf;
	int i;
	char * p;
	int trace_status;
	int pair_write_debug = 0;
	int nfs3proc, msgid;
	int ops[NFS3_PROCEDURE_COUNT]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int memory_trace_size = 0;

	while ((buf=read_line(++disk_index))!=NULL) {
		if (buf[TRACE_COMMAND_REPLY_FLAG_POS]!='C') 
			continue;
		if (buf[TRACE_VERSION_POS]!='3') 
			continue;
		sscanf (&buf[TRACE_MSGID_POS], "%x %x", &msgid, &nfs3proc);
		
		RFS_ASSERT (nfs3proc>=0 && nfs3proc<NFS3_PROCEDURE_COUNT);
		ops[nfs3proc]++;

		if (!strstr(buf, "write")) 
			continue;

		buf [strlen(buf)-1] = 0;
		if (!((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH)))
			printf("disk_index %d strlen(buf) %d buf %s \n", disk_index, strlen(buf), buf);
		RFS_ASSERT ((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH));
		if (pair_write_debug)
			printf("read line disk_index %d %s\n", disk_index, buf);

		/* store the request to memory */
		//strcpy (memory_trace[memory_trace_size].line, buf);
		//memory_trace[memory_trace_size].disk_index = disk_index;

		/* find and store the reply status and reply fhandle to memory */
		//memory_trace[memory_trace_size].trace_status = NFS3ERR_RFS_MISS;
		trace_status = NFS3ERR_RFS_MISS;
		for (i=disk_index+1; i<disk_index+MAX_COMMAND_REPLY_DISTANCE_FOR_PAIR; i++) {
			reply_buf = read_line(i);
			if (pair_write_debug)
				printf("searching for reply: read line %s\n", reply_buf);
			if (!reply_buf)
				break;
       		if (reply_buf[TRACE_COMMAND_REPLY_FLAG_POS]=='R') {
           		p = strchr (&reply_buf[TRACE_MSGID_POS], ' ');
           		RFS_ASSERT (p);
           		if (!strncmp(&reply_buf[TRACE_MSGID_POS], &buf[TRACE_MSGID_POS], p-&(reply_buf[TRACE_MSGID_POS]))) {
					int pre_size, size, count;
               		//memory_trace[memory_trace_size].trace_status = find_reply_status(reply_buf);
					if (pair_write_debug)
						printf("reply found trace_status %d\n", find_reply_status(reply_buf));
						//break;
	               	trace_status = find_reply_status(reply_buf);
					if (trace_status == NFS3_OK) {
						p = strstr (p, "pre-size");
						RFS_ASSERT (p);
						sscanf (p, "pre-size %x", &pre_size); 
						p += strlen("pre-size");
						p = strstr (p, "size");
						RFS_ASSERT (p);
						sscanf (p, "size %x", &size); 
						p = strstr (p, "count");
						if (!p) 
							printf ("%s status %x pre-size %x size %x count %x\n", buf, trace_status, pre_size, size, count);
						RFS_ASSERT (p);
						sscanf (p, "count %x", &count); 
						printf ("%s status %x pre-size %x size %x count %x\n", buf, trace_status, pre_size, size, count);
						break;
					}
				};
   	    	}
		}
		//if (memory_trace[memory_trace_size].trace_status == NFS3ERR_RFS_MISS)
		if (trace_status == NFS3ERR_RFS_MISS) {
			printf ("%s no reply\n", buf);
			missing_reply_num ++;
		}

#if	0	/* commented out by G. Jason Peng */
		if (missing_reply_num > memory_trace_size/10) {
			printf ("missing_reply_num %d too high for memory_trace_size %d\n", missing_reply_num, memory_trace_size);
			exit (0);
		}
#endif

		memory_trace_size ++;

		/*
		if (memory_trace_size >= MAX_MEMORY_TRACE_LINES) {
			fprintf (stderr, "memory trace size %d is not enough\n", MAX_MEMORY_TRACE_LINES);
			break;
		}
		*/
		if ((memory_trace_size%10000)==0)
			fprintf(stderr, "total %d disk lines %d memory lines missing_reply_num %d\n", disk_index, memory_trace_size, missing_reply_num );
	};

	fprintf(stderr, "total %d disk lines %d memory lines missing_reply_num %d\n", disk_index, memory_trace_size, missing_reply_num );
    //fprintf (stderr, "init_dep_tab, req_num_with_init_fh %d req_num_with_new_fh %d discard %d\n", req_num_with_init_fh, req_num_with_new_fh, req_num_with_discard_fh);

}

#ifdef notdef
/* This function is not finished writing */
int calculate_performance()
{
	char *buf; 
	char *reply_buf;
	int i;
	char * p;
	int debug = 0;

typedef struct {
	struct timeval start;
	struct timeval stop;
	int trace_status;
	int op;
} trace_performance_ent_t;

	struct timeval req_time;
	struct timeval reply_time;

	trace_performance_ent_t * ent = NULL;

	while (!CYCLIC_FULL(memory_trace_index)) {

		if (ent!=NULL && (ent->trace_status == NFS3ERR_RFS_MISS))
			buf = reply_buf;
		if ((buf=read_line(++disk_index))==NULL) {
END:		fprintf(stderr, "total %d disk lines %d memory lines missing_reply_num %d\n", disk_index, CYCLIC_NUM(memory_trace_index), missing_reply_num );
    		fprintf (stderr, "init_dep_tab, req_num_with_init_fh %d req_num_with_new_fh %d discard %d\n", req_num_with_init_fh, req_num_with_new_fh, req_num_with_discard_fh);
			end_profile (&read_trace_profile);
			return TRACE_FILE_END;
		}

		get_timestamp (&ent->req_time, buf);
		if (MAX_COMMAND_REPLY_DISTANCE ==1) {
			ent->trace_status == NFS3ERR_RFS_MISS;
		} else {
			reply_buf=read_line(++disk_index);
			RFS_ASSERT (reply_buf);
			if (!strcmp(reply_buf, "missing_reply\n")) {
				ent->trace_status == NFS3ERR_RFS_MISS;
			} else {
				get_timestamp (&ent->reply_time, buf);
				ent->trace_status = find_reply_status(reply_buf);
			}
		}
	}
}
#endif

int read_trace ()
{
	char *buf; 
	char *reply_buf;
	int i;
	char * p;
	int debug = 0;
	memory_trace_ent_t * ent=NULL;

	start_profile (&read_trace_profile);

	while (!CYCLIC_FULL(memory_trace_index)) {
		if (ent!=NULL && (ent->trace_status == NFS3ERR_RFS_MISS))
			buf = reply_buf;
		if ((buf=read_line(++disk_index))==NULL) {
END:		fprintf(stderr, "total %d disk lines %d memory lines missing_reply_num %d\n", disk_index, CYCLIC_NUM(memory_trace_index), missing_reply_num );
    		fprintf (stderr, "init_dep_tab, req_num_with_init_fh %d req_num_with_new_fh %d discard %d\n", req_num_with_init_fh, req_num_with_new_fh, req_num_with_discard_fh);
			end_profile (&read_trace_profile);
			return TRACE_FILE_END;
		}
	
		if (rfs_debug)
			printf ("disk_index %d %s\n", disk_index, buf);

		if (disk_index==0) {
			trace_timestamp1 = get_timestamp_sec (buf);
			trace_starttime.sec = get_timestamp_sec (buf);
			trace_starttime.usec = get_timestamp_usec (buf);
			trace_starttime.esec = 0;
			printf ("trace starttime %d %d %d\n", trace_starttime.sec, trace_starttime.usec, trace_starttime.esec);
		} else
			trace_timestamp2 = get_timestamp_sec (buf);

		/* store the request to memory */
		ent = &(memory_trace[memory_trace_index.head]);
		strcpy (ent->line, buf);
		ent->disk_index = disk_index;

		if (MAX_COMMAND_REPLY_DISTANCE ==1) {
			ent->trace_status == NFS3ERR_RFS_MISS;
		} else {
			reply_buf=read_line(++disk_index);
			RFS_ASSERT (reply_buf);
			if (!strcmp(reply_buf, "missing_reply\n")) {
				ent->trace_status == NFS3ERR_RFS_MISS;
			} else {
				ent->trace_status = find_reply_status(reply_buf);
			}
		};

		if (ent->trace_status == NFS3ERR_RFS_MISS)
			missing_reply_num ++;

		if (MAX_COMMAND_REPLY_DISTANCE > 1) {
			if ((missing_reply_num > disk_index/5) && (missing_reply_num > 100)) {
				printf ("missing_reply_num %d too high for disk_index %d\n", missing_reply_num, disk_index);
				exit (0);
			}
		}

		/* find and store the reply trace fhandle for create-class requests */
		if (ent->trace_status==NFS3_OK) {
			if (strstr(buf, "create") || strstr(buf, "mkdir") 
				|| (strstr(buf, "symlink") && (buf[TRACE_VERSION_POS]!='2')) 
				|| strstr(buf, "mknod") ) {
				p = find_reply_trace_fh(reply_buf);
				if (p==NULL) {
					printf("skip line disk_index %d %s \n", disk_index, buf);
					continue;
				}
				memcpy(ent->reply_trace_fh, p, TRACE_FH_SIZE);
			} else
				memset(ent->reply_trace_fh, 0, TRACE_FH_SIZE);
		}

		add_to_dep_tab(memory_trace_index.head);

		if (((disk_index+1)%20000)==0) {
			fprintf(stderr, "%d disk trace parsed \n", disk_index+1);
		};
	};

	end_profile (&read_trace_profile);
	return TRACE_BUF_FULL;
}
#else	/* not defined REDUCE_MEMORY_TRACE_SIZE */
int read_trace ()
{
	FILE * fp;
	char buf[1024];
	// char * t=buf;	
	int disk_index=0;

	fp = fopen(trace_file, "r");
	RFS_ASSERT (fp!=NULL);
	while (fgets(buf, MAX_TRACE_LINE_LENGTH, fp)) {
		if (!((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH)))
			printf("strlen(buf) %d buf %s \n", strlen(buf), buf);
		RFS_ASSERT ((strlen(buf)>80) && (strlen(buf)<MAX_TRACE_LINE_LENGTH));

		/* store the request to memory */
		strcpy (memory_trace[memory_trace_size].line, buf);
		memory_trace[memory_trace_size].disk_index = disk_index;
		memory_trace_size ++;

		if (memory_trace_size >= MAX_MEMORY_TRACE_LINES) {
			fprintf (stderr, "memory trace size %d is not enough\n", MAX_MEMORY_TRACE_LINES);
			break;
		}
		if ((disk_index%100000)==0)
			fprintf(stderr, "%d disk trace parsed, store %d trace lines to memory\n", disk_index, memory_trace_size);
		disk_index ++;
	}

	fprintf(stderr, "total %d disk lines %d memory lines \n", disk_index, memory_trace_size );
}
#endif


#ifdef REDUCE_MEMORY_TRACE_SIZE
inline int disk_index_to_memory_index (int disk_index)
{
	static int memory_index = 0;

	RFS_ASSERT (!CYCLIC_EMPTY(memory_trace_index));
	RFS_ASSERT (memory_trace[memory_trace_index.tail].disk_index <= disk_index);
	RFS_ASSERT (memory_trace[CYCLIC_MINUS(memory_trace_index.head,1,memory_trace_index.size)].disk_index >=disk_index);
	if (disk_index > memory_trace[memory_index].disk_index) {
		while (memory_trace[memory_index].disk_index < disk_index) {
			memory_index = CYCLIC_ADD(memory_index,1,memory_trace_index.size);
		}
	};
	if (disk_index < memory_trace[memory_index].disk_index) {
		while (memory_trace[memory_index].disk_index > disk_index) {
			memory_index = CYCLIC_MINUS(memory_index,1,memory_trace_index.size);
		}
	};

	RFS_ASSERT (disk_index == memory_trace[memory_index].disk_index);
	return memory_index;
}
#else
#define disk_index_to_memory_index(disk_index)	disk_index
#endif

#define get_line_by_disk_index(disk_index) \
	memory_trace[disk_index_to_memory_index(disk_index)].line

inline char * find_reply_line (char * command_line, int cur_disk_index)
{
	int i;
	char * line;
	char * p;
	int request_memory_index = disk_index_to_memory_index (cur_disk_index);
	for (i=request_memory_index+1; i<request_memory_index+MAX_COMMAND_REPLY_DISTANCE && i<MAX_MEMORY_TRACE_LINES; i++) {
		line = memory_trace[i].line;
		if (line[TRACE_COMMAND_REPLY_FLAG_POS]=='R') {
	        p = strchr (&line[TRACE_MSGID_POS], ' ');
    	    RFS_ASSERT (p);
			if (!strncmp(&line[TRACE_MSGID_POS], &command_line[TRACE_MSGID_POS], p-&(line[TRACE_MSGID_POS]))) 
				return line;
		}
	}
	return NULL;
}

inline int find_reply_status (char * line)
{
	char * p;
	int i=0;

	if (rfs_debug)
		printf ("line %s  flag %c\n", line, line[TRACE_COMMAND_REPLY_FLAG_POS]);
	if (!(line[TRACE_COMMAND_REPLY_FLAG_POS]=='R')) {
		printf ("disk_index %d %s\n", disk_index, line);
	};
	RFS_ASSERT (line[TRACE_COMMAND_REPLY_FLAG_POS]=='R');
	p = line+TRACE_MSGID_POS+2;	/* at least one letter for msgid and one letter for space */
	if (strstr(p, "OK"))
		return NFS3_OK;
	if (strstr(p, "lookup 2") || strstr(p, "lookup   2"))
		return 0x2;
	if (strstr(p, "create d"))
		return 0xd;
	if (strstr(p, "setattr 1"))
		return 0x1;
	if (strstr(p, "setattr 2712")) /* 10002 NFS3ERR_NOT_SYNC */
		return 0x2712;
	if (strstr(p, "lookup d"))
		return 0xd;
	if (strstr(p, "read d"))
		return 0xd;
	if (strstr(p, "write d"))
		return 0xd;
	if (strstr(p, "write 46"))
		return 0x46;
	if (strstr(p, "getattr 46"))
		return 0x46;
	if (strstr(p, "mkdir d"))
		return 0xd;
	printf ("line %s  flag %c return value weird\n", line, line[TRACE_COMMAND_REPLY_FLAG_POS]);
	printf ("!!!!!!!!!!!!!!!!!!!!!!!!\n");
	fprintf (stderr, "line %s  flag %c return value weird\n", line, line[TRACE_COMMAND_REPLY_FLAG_POS]);
	fprintf (stderr, "!!!!!!!!!!!!!!!!!!!!!!!!\n");
}

inline int find_reply_status_old (char * line)
{
	char * p;
	int i=0;

	//printf ("line %s  flag %c\n", line, line[TRACE_COMMAND_REPLY_FLAG_POS]);
	RFS_ASSERT (line[TRACE_COMMAND_REPLY_FLAG_POS]=='R');
	if (!strstr(line, "OK")) {
		p=strstr(line, " 6 read ");
		if (p) {
			p+=strlen(" 6 read ");
		} else {
			p = strstr (line, "status=XXX");
			RFS_ASSERT (p);
			p--;
			RFS_ASSERT (*p==' ');
			while (*p==' ')
				p--;
			while (*p!=' ') {
				p--;
			}
			p++;
		}
		sscanf (p, "%x", &i);
		if ((i<=0) || (i>10000))
			printf("line %s\n", line);
		RFS_ASSERT (i>0 && i<10009);
	}
	return i;
}

inline char * find_reply_trace_fh (char * line)
{
	char * p;	
	p = strstr (line, "OK fh");
	if (!p) {
		printf ("disk_index %d find_reply_trace_fh line %s\n", disk_index, line);
		return NULL;
	} else
		return p+6;
}

#ifndef NO_DEPENDENCY_TABLE
inline int disk_index_to_dep_index(int cur_dep_index, int disk_index)
{
	int i;
	for (i=cur_dep_index; i>min_dep_index; i--) {
		if (dep_tab[i].disk_index == disk_index)
			return i;
	} 
	RFS_ASSERT (0);
}
#endif

inline int is_play_candidate (int dep_index)
{
	int proc = dep_tab[dep_index].proc;
	int status = dep_tab[dep_index].status;
	int trace_status = dep_tab[dep_index].trace_status;

#ifndef TAKE_CARE_CREATE_MODE_BY_DAN
	/* for a failed create in trace, trace_replay just ignore many time the trace create fail
	 * due to access control, but trace_play will success because our access control
	 * may be loose (all uid/gid is mapped to single one 513:513, so we just skip these requests 
	 */
	if ((proc==CREATE || proc==MKDIR) && (trace_status!=NFS3_OK) && (status!=NFS3ERR_RFS_MISS)) {
		if (dependency_debug)
			printf ("disk[%d] ignore failed create/mkdir in trace, trace_status %d line %s", 
				dep_tab[dep_index].disk_index, trace_status, dep_tab[dep_index].line);
		failed_create_command_num ++;
		return FALSE;
	}
#endif
#ifndef TAKE_CARE_OTHER_FAILED_COMMAND
	if (((trace_status == NFS3ERR_ACCES) && (proc==READ || proc==WRITE || proc==LOOKUP)) || 
	    ((trace_status == NFS3ERR_PERM) && (proc==SETATTR)) 									){
		if (dependency_debug)
			printf ("disk[%d] ignore other failed command in trace, trace_status %d line %s", 
				dep_tab[dep_index].disk_index, trace_status, dep_tab[dep_index].line);
		
		failed_other_command_num ++;
		return FALSE;
	}
#endif
#ifndef TAKE_CARE_SYMBOLIC_LINK
	if ((dep_tab[dep_index].proc==READLINK) ) { /* send request */
		skipped_readlink_command_num ++;
		return FALSE;
	}
#endif
/* This is actually take care in get_nextop by checking fh_map error when dep_index==min_dep_index */
#ifndef TAKE_CARE_CUSTOM_COMMAND
	/* this line has a file handle which should belong to discard but it is not
	 * the file handle directly appears as parent directory in a lookup request
	 * the return value is NOENT, the parent directory should have been initialized
	 * but the initialization code just ignored all lookup request which didn't success
	 * including NOENT even though the parent directory is still valid.
	 */
/*
	if ((    ((dep_tab[dep_index].disk_index==262213) || (dep_tab[dep_index].disk_index==214402))
		  && !(strcmp(trace_file, "anon-lair62-011130-1100.txt")) 
		) || 
		(	 ((dep_tab[dep_index].disk_index==238460) || (dep_tab[dep_index].disk_index ==238470))
		  && !(strcmp(trace_file, "anon-lair62-011130-1000.txt"))
		)) {
		skipped_custom_command_num++;
		return FALSE;
	}
*/
	if ((    ((dep_tab[dep_index].disk_index==423727) || (0))
		  && !(strncmp(trace_file, "anon-lair62-011130-1500.txt", strlen("anon-lair62-011130-1500.txt"))) 
		) || 
		(	 ((dep_tab[dep_index].disk_index==238460) || (dep_tab[dep_index].disk_index ==238470))
		  && !(strcmp(trace_file, "anon-lair62-011130-1000.txt"))
		)) {
		skipped_custom_command_num++;
		return FALSE;
	}
	/* this line is about the mkdir 116d9d originally in anon-lair62-011130-1400.txt */
	if (!strncmp(dep_tab[dep_index].line, "1007147245.194201", strlen("1007147245.194201"))) {
		skipped_custom_command_num++;
		return FALSE;
	}
#endif
#ifndef TAKE_CARE_FSSTAT_COMMAND
	/* the file handle used in this command is not processed properly by pre-processing */
	if (proc==FSSTAT) {
		char * trace_fh = find_lead_trace_fh(proc, dep_tab[dep_index].line);
		fh_map_t * fh = lookup_fh (trace_fh);
		if (!fh) {
			skipped_fsstat_command_num++;
			return FALSE;
		}
	}
#endif
	return TRUE;
}

inline int is_dir_op (int proc)
{
	switch (proc) {
	case MKDIR:
	case CREATE:
	case LINK:
	case SYMLINK:
	case MKNOD:
	case REMOVE:
	case RMDIR:
	case RENAME:
		return 1;
	default:
		return 0;
	}
}	

inline int is_create_op (int proc)
{
	if (proc==CREATE || proc==MKDIR || proc==LINK || proc==SYMLINK || proc==MKNOD || proc==RENAME)
		return 1;
	return 0;
}

inline int is_delete_op (int proc)
{
	if (proc==REMOVE || proc==RMDIR || proc==RENAME)
		return 1;
	return 0;
}	

static inline char * find_lead_trace_fh(int proc, char * line)
{
	char * p;
	/* check the file handle availability */ 
	p = strstr (line, "fh");
	RFS_ASSERT (p);
	p+=3; //printf ("check dependency dep_tab[%d] trace_fh %s line %s \n", dep_index, trace_fh, line);
	return p;
}

inline char * find_another_trace_fh(int proc, char * line)
{
	char * p;
	/* check the file handle availability */ 
	p = strstr (line, "fh2");
	RFS_ASSERT (p);
	p+=4; //printf ("check dependency dep_tab[%d] trace_fh %s line %s \n", dep_index, trace_fh, line);
	return p;
}

/* return the index of next request in dep_tab.
 * Return -1 if there is no suitable request to send
 */
inline int get_nextop(void)
{
	int i,j, k;
	int * t;
	static int dep_index = -2;
	char * line;
	char * p;
#define INIT_MIN_WAIT_VALUE -999
	static int min_wait_fhandle_dep_index = INIT_MIN_WAIT_VALUE;
	int proc;
	int flag;

	if (min_wait_fhandle_dep_index == -999)
		min_wait_fhandle_dep_index = dep_window_index.head;

	for (i=0; i<CYCLIC_NUM(dep_window_index); i++) {
		dep_index = (dep_window_index.tail+i) % dep_window_index.size;
	
		proc = dep_tab[dep_index].proc;
		flag = dep_tab[dep_index].flag;

		if (dependency_debug)
			printf ("get_nextop check dep_tab[%d].disk_index %d\n", dep_index, dep_tab[dep_index].disk_index);
#ifdef NO_DEPENDENCY_TABLE
		if (dep_tab[dep_index].flag == DEP_FLAG_INIT) {
			if (is_play_candidate(dep_index)==TRUE) {
				/* the trace_fh is the file handle for the operation directory, trace_fh_2 is other file handle
				 * used in the request */
				if (proc==LINK || proc==RENAME) {
					dep_tab[dep_index].trace_fh = find_another_trace_fh (proc, dep_tab[dep_index].line);
					dep_tab[dep_index].trace_fh_2 = find_lead_trace_fh(proc, dep_tab[dep_index].line);
					dep_tab[dep_index].fh = 0;
					dep_tab[dep_index].fh_2 = 0;
				} else {
					dep_tab[dep_index].trace_fh = find_lead_trace_fh(proc, dep_tab[dep_index].line);
					dep_tab[dep_index].fh = 0;
					dep_tab[dep_index].fh_2 = (fh_map_t *)1;
				};
				dep_tab[dep_index].flag = DEP_FLAG_CANDIDATE;
#ifdef TIME_PLAY
				dep_tab[dep_index].skip_sec = skip_sec;
#endif
				if (dependency_debug)
					printf ("disk[%d] state DEP_FLAG_INIT to DEP_FLAG_CANDIDATE\n", dep_tab[dep_index].disk_index);
			} else {
				if (dependency_debug)
					printf ("disk[%d] state DEP_FLAG_INIT to DEP_FLAG_DONE\n", dep_tab[dep_index].disk_index);
				dep_tab[dep_index].flag = DEP_FLAG_DONE;
				continue;
			}
		}

		if ((dep_tab[dep_index].flag == DEP_FLAG_CANDIDATE) || (dep_tab[dep_index].flag == DEP_FLAG_WAIT_FHANDLE) ) {

			if (!dep_tab[dep_index].fh)
				dep_tab[dep_index].fh = lookup_fh (dep_tab[dep_index].trace_fh);
			if (!dep_tab[dep_index].fh_2)
				dep_tab[dep_index].fh_2 = lookup_fh (dep_tab[dep_index].trace_fh_2);

			/* need to wait for file handle */
			if ((!dep_tab[dep_index].fh) || (!dep_tab[dep_index].fh_2)) {
				if (dependency_debug)
					printf("disk[%d] can not lookup file handle\n", dep_tab[dep_index].disk_index);
				if (dep_tab[dep_index].flag == DEP_FLAG_CANDIDATE) {
					if (dependency_debug)
						printf ("disk[%d] state DEP_FLAG_CANDIDATE to DEP_FLAG_WAIT_FHANDLE\n", dep_tab[dep_index].disk_index);
					dep_tab[dep_index].flag = DEP_FLAG_WAIT_FHANDLE;
					sfs_gettime (&dep_tab[dep_index].start);
					if (CYCLIC_LESS(dep_tab_index,dep_index,min_wait_fhandle_dep_index)) 
						min_wait_fhandle_dep_index = dep_index;
				} else {
					struct ladtime tmp;
					if (dep_index==dep_window_index.tail) {
						if (!profile_debug) 
							printf ("fh_path_map error disk[%d] state DEP_FLAG_WAIT_FHANDLE to DEP_FLAG_DONE\n", dep_tab[dep_index].disk_index);
						fh_path_map_err_num ++;
						dep_tab[dep_index].flag = DEP_FLAG_DONE;
						continue;
					}
					sfs_gettime (&tmp);
					SUBTIME (tmp, dep_tab[dep_index].start);
#define DEPENDENCY_TIMEOUT 50
#ifdef TIME_PLAY
					RFS_ASSERT (tmp.sec < DEPENDENCY_TIMEOUT + (skip_sec - dep_tab[dep_index].skip_sec));	
#else
					if (tmp.sec >= DEPENDENCY_TIMEOUT) {
						printf("dep_tab[%d].flag %d disk_index %d line %s\n", dep_index,
							dep_tab[dep_index].flag, dep_tab[dep_index].disk_index,
							dep_tab[dep_index].line);
					}
					RFS_ASSERT (tmp.sec < DEPENDENCY_TIMEOUT );	
#endif
				}
				continue;
			}

			/* file handle ready, adjust_min_wait_fhandle_dep_index */
			if ((dep_tab[dep_index].flag == DEP_FLAG_WAIT_FHANDLE)) {
				if (dep_index == min_wait_fhandle_dep_index) {
					min_wait_fhandle_dep_index = dep_window_index.head;
					for (j=CYCLIC_ADD(dep_index,1,dep_window_index.size); CYCLIC_LESS(dep_window_index,j,dep_window_index.head); j++) {
						if (dep_tab[j].flag ==DEP_FLAG_WAIT_FHANDLE) {
							min_wait_fhandle_dep_index = j;
							break;
						}
					}
				}
			}
			if (dependency_debug)
				printf("disk[%d] found file handle\n", dep_tab[dep_index].disk_index);
			dep_tab[dep_index].flag = DEP_FLAG_FHANDLE_READY;

			/* the normal file operation can be executed now */
			if (!is_dir_op (dep_tab[dep_index].proc)) {
				if (dependency_debug)
					printf ("return disk[%d]\n", dep_tab[dep_index].disk_index);
				return dep_index;
			}

			if (dependency_debug)
				printf("disk[%d] directory operation \n", dep_tab[dep_index].disk_index);
			/* the directory operation need to lock the directory first */
			if (dep_tab[dep_index].fh->lock) {
				if (dependency_debug)
					printf ("disk[%d] state %d to DEP_FLAG_WAIT_DIRECTORY\n", dep_tab[dep_index].disk_index, dep_tab[dep_index].flag);
				dep_tab[dep_index].flag = DEP_FLAG_WAIT_DIRECTORY;
				continue;
			}
		}
				
		if ((dep_tab[dep_index].flag == DEP_FLAG_FHANDLE_READY) || (dep_tab[dep_index].flag == DEP_FLAG_WAIT_DIRECTORY)) {
			int j = dep_tab[dep_index].fh - fh_map;
			if (dependency_debug) {
				printf ("dep_tab[%d].disk_index %d, fh_map[%d] lock=%d\n",dep_index, dep_tab[dep_index].disk_index, j, dep_tab[dep_index].fh->lock);
				printf ("trace_fh %s path %s\n", dep_tab[dep_index].fh->trace_fh, dep_tab[dep_index].fh->path);
				printf ("trace_fh %s path %s\n", fh_map[j].trace_fh, fh_map[j].path);
			}
			if ((dep_tab[dep_index].fh->lock) || ((proc==RENAME) && (dep_tab[dep_index].fh_2->lock)) ) {
				if (dependency_debug) 
					printf ("continue to wait for directory lock\n");
				continue;
			}
			if (dependency_debug) 
				printf ("dep_tab[%d] disk index %d LOCK fh_map[%d] \n", dep_index, dep_tab[dep_index].disk_index, j);
			dep_tab[dep_index].fh->lock = 1;
			if (proc==RENAME)
				dep_tab[dep_index].fh_2->lock = 1;

			/* the non-delete directory operation can proceed now */
			if (!is_delete_op (dep_tab[dep_index].proc)) {
				if (dependency_debug) 
					printf ("return disk[%d]\n", dep_tab[dep_index].disk_index);
				return dep_index;
			}

			/* the delete operation can proceed if nobody ahead is waiting for fhandle */
			/* probably this condition is not strong enough */
//			if ((min_wait_fhandle_dep_index<dep_index) ) {
			if (dep_index!=dep_window_index.tail) {
				if (dependency_debug) 
					printf ("disk[%d] state %d to DEP_FLAG_WAIT_DELETE\n", dep_tab[dep_index].disk_index, dep_tab[dep_index].flag);
				dep_tab[dep_index].flag = DEP_FLAG_WAIT_DELETE;
				continue;
			} 
			dep_tab[dep_index].flag = DEP_FLAG_DIRECTORY_READY;
		}

		if ((dep_tab[dep_index].flag == DEP_FLAG_DIRECTORY_READY) || (dep_tab[dep_index].flag == DEP_FLAG_WAIT_DELETE)) {
//			if (min_wait_fhandle_dep_index > dep_index) {
			if (dep_index==dep_window_index.tail) {
				if (dependency_debug) 
					printf ("return disk[%d]\n", dep_tab[dep_index].disk_index);
				return dep_index;
			}
		}
#else /*NO_DEPENDENCY_TABLE undefined */
	/* this part of code will be invalid after CYCLIC buffer design */
		if (dep_tab[dep_index].flag == DEP_FLAG_INIT){
			for (j=0, t=&(dep_tab[dep_index].dep_ops[0]);
				(j<dep_tab[dep_index].init_dep_num) && (dep_tab[dep_index].cur_dep_num>0); 
				j++, t++) {
				if (*t !=-1) {
					if (dep_tab[disk_index_to_dep_index(dep_index, *t)].flag == DEP_FLAG_DONE) { 
						/* The depended request has been finished */ 
						*t = -1;
						dep_tab[dep_index].cur_dep_num --;
					}
				} 
			}

			if (dep_tab[dep_index].cur_dep_num == 0) {
				return dep_index;
			}
		}
#endif
	}

	if (dependency_debug) 
		printf ("get_nexop return -1\n");
	return -1;
}

int check_timeout(void)
{
	static int biod_index = 0;
	int i;
	int dep_index;	/* index into dep_tab */
	int proc;
	sfs_op_type *op_ptr;		/* per operation info */
	struct ladtime timeout;
	struct ladtime current_time;

	sfs_gettime (&current_time);	

	for (i=0; i<max_biod_reqs; i++, biod_index = (biod_index+1)%max_biod_reqs) {
		if (biod_reqp[biod_index].in_use==TRUE) {
			timeout = biod_reqp[biod_index].timeout;
			if ((current_time.sec>timeout.sec) ||
				((current_time.sec==timeout.sec) && (current_time.usec>timeout.usec))) {

				dep_index = biod_reqp[biod_index].dep_tab_index;
				proc = dep_tab[dep_index].proc;
				op_ptr = &Ops[proc];
				op_ptr->results.timeout_calls++;
				Ops[TOTAL].results.timeout_calls++;


				if (is_create_op(proc)) {
					dep_tab[dep_index].flag = DEP_FLAG_CANDIDATE;
					printf ("resend dep_tab[%d], disk_index %d\n", dep_index, dep_tab[dep_index].disk_index);
					finish_request (biod_index, dep_index, NFS3ERR_RFS_TIMEOUT, DEP_FLAG_CANDIDATE);
				} else {
					finish_request (biod_index, dep_index, NFS3ERR_RFS_TIMEOUT, DEP_FLAG_DONE);
				}
				timeout_num ++;
				num_out_reqs_statistics_at_timeout[num_out_reqs]++;

				//RFS_ASSERT (!is_create_op(proc));

				if (per_packet_debug)
					printf ("timeout request: disk_index %d, dep_index %d biod_reqp[%d].start %d:%d timeout %d:%d current %d:%d\n", dep_tab[dep_index].disk_index, dep_index, biod_index, biod_reqp[biod_index].start.sec, biod_reqp[biod_index].start.usec, timeout.sec, timeout.usec, current.sec, current.usec);
			}
		}
	}
}

/* Allocate a biod_req entry to send and receive request dep_tab[dep_index]
 * build the cross reference between dep_tab entry and biod_req entry
 */
struct biod_req * get_biod_req(int dep_index) /* index into dep_tab */
{
	static int biod_index = 0;
	int i;
	for (i=0; i<max_biod_reqs; i++, biod_index = (biod_index+1)%max_biod_reqs) {
		if (!biod_reqp[biod_index].in_use) {
			biod_reqp[biod_index].in_use = 1;
			biod_reqp[biod_index].dep_tab_index = dep_index;
			dep_tab[dep_index].biod_req_index = biod_index;
    		num_out_reqs++;
			return &(biod_reqp[biod_index]);
		}
	}
	return NULL;
}

/* Return index into biod_reqp
 * return -1 upon failure 
 */
int lookup_biod_req (int xid)
{
	static int biod_index = 0;
	int i;
	for (i=0; i<max_biod_reqs; i++, biod_index = (biod_index+1)%max_biod_reqs) {
		/* give a NULL as timeout pointer may cause indefinitely block */
		if ((biod_reqp[biod_index].in_use == TRUE) &&( biod_reqp[biod_index].xid == xid)) {
			return biod_index;
		}
	}
	return -1;
}

extern struct ladtime test_start;
void init_time_offset(void)
{
	struct ladtime tmp1;
	struct ladtime tmp2;

	test_start.sec = 0;
	test_start.usec = 0;
	sfs_gettime (&tmp1);		/* called at initial time: tmp1 = play_starttime */
#ifdef SPEED_UP
	DIVTIME (tmp1, PLAY_SCALE) /* tmp1 = play_starttime / SCALE */
#endif
#ifdef SLOW_DOWN
	MULTIME (tmp1, PLAY_SCALE) /* tmp1 = play_starttime * SCALE */
#endif

	tmp2 = trace_starttime; /* tmp2 = trace_starttime */
	SUBTIME (tmp2, tmp1);	/* tmp2 = trace_starttime - play_starttime *|/ SCALE */
	time_offset = tmp2;		/* time_offset = trace_starttime - play_starttime *|/ SCALE */ 
}

/* initialize timestamp and proc field of dep_tab entry */
void init_dep_tab_entry (int dep_index)
{
	char * line;
	int version;
	int nfsproc;
	int msgid;

	//line = get_line_by_disk_index (dep_tab[dep_index].disk_index);
	line = dep_tab[dep_index].line;
	sscanf (line, "%d.%d", &(dep_tab[dep_index].timestamp.tv_sec), &(dep_tab[dep_index].timestamp.tv_usec));
	sscanf (&line[TRACE_MSGID_POS], "%x %x", &msgid, &nfsproc);
	//printf ("msgid %x nfsproc %x\n", msgid, nfsproc);
	if (line[TRACE_VERSION_POS]=='2') {
		dep_tab[dep_index].proc = nfs2proc_to_rfsproc[nfsproc];
		RFS_ASSERT (nfsproc <18);
	} else {
		/* This is for debug purpose */
		if (line[TRACE_VERSION_POS] !='3') {
			fprintf(stderr, "line[TRACE_VERSION_POS] %c line %s\n", line[TRACE_VERSION_POS], line);
			line = get_line_by_disk_index (dep_tab[dep_index].disk_index-1);
			if (!line)
				line = get_line_by_disk_index (dep_tab[dep_index].disk_index-2);
			RFS_ASSERT (line);
			fprintf(stderr, "previousline %s\n", line);
		}
		RFS_ASSERT (line[TRACE_VERSION_POS] =='3');
		if (nfsproc >= NFS3_PROCEDURE_COUNT) {
			fprintf(stderr, "proc %d line %s\n", nfsproc, line);
			
		}
		RFS_ASSERT (nfsproc <NFS3_PROCEDURE_COUNT);
		dep_tab[dep_index].proc = nfs3proc_to_rfsproc[nfsproc];
	}
	RFS_ASSERT (dep_tab[dep_index].proc >= 0 && dep_tab[dep_index].proc < NOPS);
	dep_tab[dep_index].flag = DEP_FLAG_INIT;
}

void adjust_play_window (int flag, int * poll_timeout_arg)
{
	struct ladtime max_window_time;
	static struct ladtime max_poll_time = {0, 2000, 0};
	struct ladtime t;
	int i;
	char * line;
	cyclic_index_t old_dep_window_index = dep_window_index;

#ifdef notdef
	printf ("^^^^^^^^^^^^^^^ adjust_play_window, begin\n");
	CYCLIC_PRINT (dep_tab_index);
	printf ("dep_tab[%d].memory_index %d\n", dep_tab_index.tail, dep_tab[dep_tab_index.tail].memory_index);
	CYCLIC_PRINT (dep_window_index);
	CYCLIC_PRINT (memory_trace_index);
	printf ("                adjust_play_window, begin\n");
#endif

	while ((!CYCLIC_EMPTY(dep_window_index)) && (dep_tab[dep_window_index.tail].flag == DEP_FLAG_DONE)) {
#ifdef notdef
		//CYCLIC_PRINT (memory_trace_index);
		//printf("MOVE_TAIL_TO memory_index %d\n", dep_tab[dep_tab_index.tail].memory_index);
		RFS_ASSERT (!CYCLIC_EMPTY(memory_trace_index));	
		RFS_ASSERT (CYCLIC_LESS (memory_trace_index, dep_tab[dep_tab_index.tail].memory_index, memory_trace_index.head));
		printf("%d is done\n", dep_window_index.tail);
#endif
		CYCLIC_MOVE_TAIL(dep_tab_index);
		CYCLIC_MOVE_TAIL(dep_window_index);

#ifdef notdef
		CYCLIC_PRINT (dep_tab_index);
		CYCLIC_PRINT (dep_window_index);

		if (! (dep_tab_index.tail == dep_window_index.tail)) {
			CYCLIC_PRINT(dep_tab_index);
			CYCLIC_PRINT(dep_window_index);
		};
		RFS_ASSERT ( dep_tab_index.tail == dep_window_index.tail);
#endif

		if (!CYCLIC_EMPTY(dep_tab_index)) {
#ifdef notdef
			RFS_ASSERT (!CYCLIC_EMPTY(memory_trace_index));	
			if (!(CYCLIC_LESS (memory_trace_index, dep_tab[dep_tab_index.tail].memory_index, memory_trace_index.head))) {
				CYCLIC_PRINT(memory_trace_index);
				CYCLIC_PRINT(dep_tab_index);
				printf("dep_tab[head-1].memory_index, %d [tail].memory_index %d\n", 
					dep_tab[CYCLIC_MINUS(dep_tab_index.head,1,dep_tab_index.size)].memory_index,
					dep_tab[dep_tab_index.tail].memory_index);
			}
			RFS_ASSERT (CYCLIC_LESS (memory_trace_index, dep_tab[dep_tab_index.tail].memory_index, memory_trace_index.head));
#endif
			CYCLIC_SET_TAIL_TO(&memory_trace_index, dep_tab[dep_tab_index.tail].memory_index);
			//printf ("set memory_trace_index to %d=%d, dep_tab_index.tail %d\n", memory_trace_index.tail, dep_tab[dep_tab_index.tail].memory_index, dep_tab_index.tail);
		} else {
		//	CYCLIC_MOVE_TAIL (memory_trace_index);
		}
	}

	while (CYCLIC_EMPTY(dep_tab_index)) {
		
		if (disk_io_status == TRACE_FILE_END) 
			return;
		else {
			//printf ("************** ADJUST_PLAY_WINDOW sleep 1 s\n"); 
			//print_cyclic_buffers();
			//pthread_yield();
			//usleep (1000);
		}
	}

	/* max_trace_window_time = current *|/ SCALE + trace_starttime */
	sfs_gettime (&current);

#ifdef TIME_PLAY
#ifdef SPEED_UP
	MULTIME (current, PLAY_SCALE);
#endif
#ifdef SLOW_DOWN
	DIVTIME (current, PLAY_SCALE);
#endif
	ADDTIME (current, trace_starttime);
	max_window_time = current;

	/* Right now it is not clear how to deal with the situation where MAX_PLAY_WINDOW is reached */
	if (CYCLIC_NUM(dep_window_index) == MAX_PLAY_WINDOW) {
		//printf ("can not catch up the speed, dep_tab_size %d dep_window_max %d reach min_dep_index %d+MAX_PLAY_WINDOW\n", dep_tab_size, dep_window_max, min_dep_index);
		//printf (".");
		can_not_catch_speed_num ++;
	}
	while ((CYCLIC_NUM(dep_window_index) < MAX_PLAY_WINDOW) &&
		   (CYCLIC_NUM(dep_window_index) < CYCLIC_NUM(dep_tab_index)) ) {
		struct ladtime t;
		int dep_index = (dep_window_index.tail+i) % dep_window_index.size;
        t.sec = dep_tab[dep_index].timestamp.tv_sec;
        t.usec = dep_tab[dep_index].timestamp.tv_usec;
		if ((t.sec>max_window_time.sec)||(t.sec==max_window_time.sec && t.usec>max_window_time.usec))
            break;
		CYCLIC_MOVE_HEAD(dep_window_index);
	}
#else
	ADDTIME (current, trace_starttime);
	max_window_time = current;
	while ((CYCLIC_NUM(dep_window_index) < MAX_PLAY_WINDOW) &&
		   (CYCLIC_NUM(dep_window_index) < CYCLIC_NUM(dep_tab_index)) ) {
		CYCLIC_MOVE_HEAD(dep_window_index);
	}
#endif

	if (flag == BUSY)
		*poll_timeout_arg = 0;
   	else if (CYCLIC_NUM(dep_window_index)==CYCLIC_NUM(dep_tab_index)) {
		*poll_timeout_arg = 1000;	/* poll_timeout set to 1 second for the last request */
	} else {
#ifdef TIME_PLAY
		struct ladtime tmp;
		struct ladtime tmp1;
		tmp.sec = dep_tab[dep_window_index.head].timestamp.tv_sec;
		tmp.usec = dep_tab[dep_window_index.head].timestamp.tv_usec;
		if (adjust_play_window_debug>=2)
			printf ("dep_tab[dep_window_index.head %d].timestamp %d:%d, max_window_time %d:%d\n",
				dep_window_index.head, tmp.sec, tmp.usec, max_window_time.sec, max_window_time.usec);

		SUBTIME (tmp, max_window_time);
#ifdef SPEED_UP
		DIVTIME (tmp, PLAY_SCALE);
#endif
#ifdef SLOW_DOWN
		MULTIME (tmp, PLAY_SCALE);
#endif
/*
		tmp1 = tmp;

		if (tmp.sec > max_poll_time.sec) {

			if (rfs_debug) 
				printf ("dep_tab[%d].timestamp %d:%d, max_window_time %d:%d\n",
				dep_window_max, dep_tab[dep_window_max].timestamp.tv_sec, dep_tab[dep_window_max].timestamp.tv_usec, max_window_time.sec, max_window_time.usec);
			printf ("skip %d seconds\n", tmp.sec-max_poll_time.sec);
			SUBTIME (tmp, max_poll_time);
			tmp.usec = 0;
			skip_sec += tmp.sec;
			SUBTIME (test_start, tmp);
			tmp = max_poll_time;
		}
*/

		//RFS_ASSERT ((tmp.sec < 1000));
		if (tmp.sec > 1000)
			tmp.sec = 1000;
		if ((tmp.sec ==0) && (tmp.usec==0)) {
			*poll_timeout_arg = 0;
		} else
			*poll_timeout_arg = tmp.sec*1000000+tmp.usec;
#else 
		/*
		struct ladtime tmp;
		struct ladtime tmp1;
		tmp.sec = dep_tab[dep_window_max].timestamp.tv_sec;
		tmp.usec = dep_tab[dep_window_max].timestamp.tv_usec;
		tmp1.sec = dep_tab[dep_window_max-1].timestamp.tv_sec;
		tmp1.usec = dep_tab[dep_window_max-1].timestamp.tv_usec;
		SUBTIME (tmp, tmp1);
		RFS_ASSERT ((tmp.sec < 1000));
		RFS_ASSERT ((tmp.sec>0) || ((tmp.sec==0) && (tmp.usec>0)));
		*poll_timeout = tmp.sec*1000000+tmp.usec;
		*/

		*poll_timeout_arg = 100000;
#endif
	}	
	if (rfs_debug)
		printf ("adjust_play_window: flag %d min %d -> %d, max %d -> %d poll_timeout_arg %d \n", 
		flag, old_dep_window_index.tail, dep_window_index.tail, old_dep_window_index.head,
		dep_window_index.head, *poll_timeout_arg);

#ifdef notdef
	printf ("^^^^^^^^^^^^^^^ adjust_play_window, end\n");
	CYCLIC_PRINT (dep_tab_index);
	printf ("dep_tab[%d].memory_index %d\n", dep_tab_index.tail, dep_tab[dep_tab_index.tail].memory_index);
	CYCLIC_PRINT (dep_window_index);
	CYCLIC_PRINT (memory_trace_index);
	printf ("	 adjust_play_window, end\n\n");
#endif
	//CYCLIC_ASSERT(4);
}

/* poll for usecs and receive, after receive one reply,
 * return index in biod_reqp of the corresponding request
 */
int poll_and_get_reply (int usecs)
{
	int biod_index = -1;
	int xid;
	int error;
	struct timeval zero_time = {0, 0}; /* Immediately return */

#ifdef RECV_THREAD
	//printf("recv thread waitsem 1\n");
	waitsem (async_rpc_sem);
	//printf("recv thread got sem 1\n");
#endif
	do {
		error = biod_poll_wait (NFS_client, usecs);
		switch (error) {
		case -1:
			if (errno == EINTR) {
				error = 1;
				continue;
			}
			if (rfs_debug) {
				(void) fprintf(stderr, "biod_poll_wait error\n");
				perror ("");
			    (void) fflush(stderr);
			}
			break;
		case 0:
			break;
		default:
#ifdef UDP
			//printf("recv thread waitsem 2\n");
			//waitsem (async_rpc_sem);
			//printf("recv thread got sem 2\n");
			error = get_areply_udp (NFS_client, &xid, &zero_time);
			//postsem (async_rpc_sem);
			//printf("recv thread postsem 2\n");
			// RFS_ASSERT (error!= RPC_TIMEOUT);	/* we have polled and know there is data */
			// RFS_ASSERT (error!= RPC_CANTRECV);
			RFS_ASSERT (error == RPC_SUCCESS);

			biod_index = lookup_biod_req (xid);
			sfs_gettime (&(biod_reqp[biod_index].stop));
#else
			RFS_ASSERT (0);
#endif
		}
	} while (0);
#ifdef RECV_THREAD
	postsem (async_rpc_sem);
	//printf("recv thread postsem 1\n");
#endif
	return biod_index;
}

void print_result(void)
{
	int i, j;
	struct ladtime t;
	int dep_index;
	int avg_msecs;
	unsigned long long tmp;
	int avg_usecs;

    if (DEBUG_CHILD_GENERAL) {
		(void) fprintf(stdout, "trace play result:\n");
		(void) fprintf(stdout, "\t    percentage good_cnt bad_cnt timeout_cnt\telapsed time\t\t\taverage time\n");
		for (i=0; i<NOPS+1; i++) {
			if (Ops[i].results.good_calls==0) {
				avg_msecs = 0;
				avg_usecs = 0;
			} else {
				tmp = Ops[i].results.time.sec*1000000 + Ops[i].results.time.usec;
				avg_msecs = 0;
				avg_usecs = tmp/Ops[i].results.good_calls;
/*
				avg_msecs = (Ops[i].results.time.sec*1000 + Ops[i].results.time.usec/1000)/Ops[i].results.good_calls;
				avg_usecs = (Ops[i].results.time.usec%1000)/Ops[i].results.good_calls;
*/
			}
			(void) fprintf(stdout,	"%11s\t%4.1f\t%4d\t%4d\t%4d\t\tsec %8d usec %8d \tusec %8d\n", 
				Ops[i].name, 
				(float)(100*Ops[i].results.good_calls)/(float)Ops[TOTAL].results.good_calls, 
				Ops[i].results.good_calls, Ops[i].results.bad_calls, Ops[i].results.timeout_calls,
				Ops[i].results.time.sec, Ops[i].results.time.usec, avg_msecs*1000+avg_usecs);
		}
		(void) fflush (stdout);
    }

#if	0	/* commented out by G. Jason Peng */
	RFS_ASSERT (read_data_owe_GB==0);
	printf("read_data_total %d GB and %d bytes, owe %d GB and %d bytes, %d percent, adjusted %d times \n",read_data_total_GB, read_data_total, read_data_owe_GB, read_data_owe, (read_data_owe)/(read_data_total/100), read_data_adjust_times);
	printf("write_data_total %d GB and %d bytes, owe %d GB and %d bytes, %d percent, adjusted %d times \n",write_data_total_GB, write_data_total, write_data_owe_GB, write_data_owe, (write_data_owe)/(write_data_total/100), write_data_adjust_times);
	printf("poll_timeout_0_num %d poll_timeout_pos_num %d\n", poll_timeout_0_num, poll_timeout_pos_num);
	printf("failed_create_command_num_in_original_trace %d\nfailed_other_command_num_in_original_trace %d\nskipped_readlink_command_num %d\nskipped_custom_command_num %d\nfh_path_map_err_num %d\nskipped_fsstat_command_num %d\nmissing_reply_num %d\nrename_rmdir_noent_reply_num %d\nrmdir_not_empty_reply_num %d\nloose_access_control_reply_num %d\nlookup_err_due_to_rename %d\nlookup_err_due_to_parallel_remove %d\nlookup_eaccess_enoent_mismatch %d\nread_io_err_num %d\nstale_fhandle_err_num %d abnormal_EEXIST_num %d abnormal_ENOENT_num %d proper_reply_num %d run_stage_proper_reply_num %d\n", 
			failed_create_command_num,
			failed_other_command_num,
			skipped_readlink_command_num, 
			skipped_custom_command_num,
			fh_path_map_err_num, 
			skipped_fsstat_command_num, 
			missing_reply_num, 
			rename_rmdir_noent_reply_num, 
			rmdir_not_empty_reply_num, 
			loose_access_control_reply_num, 
			lookup_err_due_to_rename_num, 
			lookup_err_due_to_parallel_remove_num,
			lookup_eaccess_enoent_mismatch_num, 
			read_io_err_num, 
			stale_fhandle_err_num,
			abnormal_EEXIST_num,
			abnormal_ENOENT_num,
			proper_reply_num, run_stage_proper_reply_num);
#endif

//  print_dump(Client_num, Child_num);
} 

/*
 * allocate and initialize client handles
 */
static int
init_rpc(void)
{
	int dummy = 0;

    /*
     * Set up the client handles.  We get them all before trying one
     * out to insure that the client handle for LOOKUP class is allocated
     * before calling op_getattr().
     */
    if (DEBUG_CHILD_GENERAL) {
    	(void) fprintf(stderr, "%s: set up client handle\n", sfs_Myname);
    }

    NFS_client = lad_clnt_create(Tcp? 1: 0, Server_hostent,
					(uint32_t) NFS_PROGRAM,
					(uint32_t) nfs_version,
					RPC_ANYSOCK, &Nfs_timers[0]);
		
    if (NFS_client  == ((CLIENT *) NULL)) {
        return(-1);
    }

    /*
     * create credentials using the REAL uid
     */
    NFS_client->cl_auth = authunix_create(lad_hostname, (int)Real_uid,
				      (int)Cur_gid, 0, NULL);

	if (biod_init(dummy, dummy) == -1) {
		    return(-1);
	}

    return(0);
} /* init_rpc */

void
init_counters(void)
{
    uint_t i;
    uint_t start_msec;

    /* Ready to go - initialize operation counters */
    for (i = 0; i < NOPS + 1; i++) {
	Ops[i].req_cnt = 0;
	Ops[i].results.good_calls = 0;
	Ops[i].results.bad_calls = 0;
	Ops[i].results.timeout_calls = 0;	// RFS
	Ops[i].results.fast_calls = 0;
	Ops[i].results.time.sec = 0;
	Ops[i].results.time.usec = 0;
	Ops[i].results.msec2 = 0;
    }

    /* initialize timers and period variables */
    sfs_gettime(&Starttime);
    Cur_time = Starttime;
    start_msec = (Starttime.sec * 1000) + (Starttime.usec / 1000);
    Previous_chkpnt_msec = start_msec;
    Calls_this_period = 0;
    Reqs_this_period = 0;
    Sleep_msec_this_period = 0;
    Calls_this_test = 0;
    Reqs_this_test = 0;
    Sleep_msec_this_test = 0;
}

static char *
nfs3_strerror(int status)
{
    static char str[40];
    switch (status) {
	case NFS3_OK:
	    (void) strcpy(str, "no error");
	    break;
	case NFS3ERR_PERM:
	    (void) strcpy(str, "Not owner");
	    break;
	case NFS3ERR_NOENT:
	    (void) strcpy(str, "No such file or directory");
	    break;
	case NFS3ERR_IO:
	    (void) strcpy(str, "I/O error");
	    break;
	case NFS3ERR_NXIO:
	    (void) strcpy(str, "No such device or address");
	    break;
	case NFS3ERR_ACCES:
	    (void) strcpy(str, "Permission denied");
	    break;
	case NFS3ERR_EXIST:
	    (void) strcpy(str, "File exists");
	    break;
	case NFS3ERR_XDEV:
	    (void) strcpy(str, "Cross-device link");
	    break;
	case NFS3ERR_NODEV:
	    (void) strcpy(str, "No such device");
	    break;
	case NFS3ERR_NOTDIR:
	    (void) strcpy(str, "Not a directory");
	    break;
	case NFS3ERR_ISDIR:
	    (void) strcpy(str, "Is a directory");
	    break;
	case NFS3ERR_INVAL:
	    (void) strcpy(str, "Invalid argument");
	    break;
	case NFS3ERR_FBIG:
	    (void) strcpy(str, "File too large");
	    break;
	case NFS3ERR_NOSPC:
	    (void) strcpy(str, "No space left on device");
	    break;
	case NFS3ERR_ROFS:
	    (void) strcpy(str, "Read-only file system");
	    break;
	case NFS3ERR_MLINK:
	    (void) strcpy(str, "Too many links");
	    break;
	case NFS3ERR_NAMETOOLONG:
	    (void) strcpy(str, "File name too long");
	    break;
	case NFS3ERR_NOTEMPTY:
	    (void) strcpy(str, "Directory not empty");
	    break;
	case NFS3ERR_DQUOT:
	    (void) strcpy(str, "Disc quota exceeded");
	    break;
	case NFS3ERR_STALE:
	    (void) strcpy(str, "Stale NFS file handle");
	    break;
	case NFS3ERR_REMOTE:
	    (void) strcpy(str, "Object is remote");
	    break;
	case NFS3ERR_BADHANDLE:
	    (void) strcpy(str, "Bad file handle");
	    break;
	case NFS3ERR_NOT_SYNC:
	    (void) strcpy(str, "Not sync write");
	    break;
	case NFS3ERR_BAD_COOKIE:
	    (void) strcpy(str, "Bad cookie");
	    break;
	case NFS3ERR_NOTSUPP:
	    (void) strcpy(str, "Operation not supported");
	    break;
	case NFS3ERR_TOOSMALL:
	    (void) strcpy(str, "Value too small");
	    break;
	case NFS3ERR_SERVERFAULT:
	    (void) strcpy(str, "Server fault");
	    break;
	case NFS3ERR_BADTYPE:
	    (void) strcpy(str, "Bad type");
	    break;
	case NFS3ERR_JUKEBOX:
	    (void) strcpy(str, "Jukebox");
	    break;
	case NFS3ERR_RFS_TIMEOUT:
		(void) strcpy(str, "Timeout");
		break;
	default:
	    (void) sprintf(str, "Unknown status %d", status);
	    break;
    }
    return (str);
}

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

static void
usage(void)
{
	fprintf(stderr, "trace play usage");
}
extern void init_file_system (void)
{
	return;
}

void show_fhandle (nfs_fh3 * fhp)
{
	struct knfs_fh * kfhp = (struct knfs_fh *)fhp;

	int dev;

	if (quiet_flag)
		return;
		
	RFS_ASSERT (kfhp->fh_version == 1);
	RFS_ASSERT (kfhp->fh_fsid_type == 0);
	RFS_ASSERT (kfhp->fh_auth_type == 0);

	dev = ntohs(kfhp->fh_dev_major);
	dev = dev<<8;
	dev = dev + ntohs(kfhp->fh_dev_minor);

	/* kfhp->fh_dev_ino hold the inode number of export point of the mounted
	 * file system. For example, if /tmp/t1 is exported, /tmp/t1/t2 is mounted,
	 * then fh_dev_ino hold the inode number of t1, not t2
	 */

	switch (kfhp->fh_fileid_type) {
		case 0:
			printf("fh:type 0 root dev 0x%x dev_ino %d\n", dev, kfhp->fh_dev_ino); 
			break;
		case 1:
			printf("fh:type 1 %d %x dev %x dev_ino %x\n", 
				kfhp->fh_ino, kfhp->fh_generation, dev, kfhp->fh_dev_ino);
			break;
		case 2:
			printf("fh:type2 %d %x dirino %d dev 0x%x dev_ino %x\n", 
				kfhp->fh_ino, kfhp->fh_generation, kfhp->fh_dirino, dev, kfhp->fh_dev_ino);
			break;
		default:
			RFS_ASSERT (0);
	}
}

nfs_fh3 zero_fhandle;
int init_fh_map ()
{
	memset (fh_map, 0, sizeof (fh_map));
	memset(fh_htable, 0, sizeof (fh_htable));
	memset (&zero_fhandle, 0, sizeof(nfs_fh3));
	printf ("SIZE of fh map %d KB\n", sizeof (fh_map)/1000);
	fh_i = 0;
}

int add_fh (int map_flag, char * trace_fh, char * path, nfs_fh3 * play_fh)
{
	char * old_trace_fh;

	/* first lookup if the entry for fh is already in the table */
    struct generic_entry * p;

    p = generic_lookup (trace_fh, TRACE_FH_SIZE, 0, fh_htable, FH_HTABLE_SIZE);
	if (p) {
		RFS_ASSERT (fh_map[p->key3].flag = FH_MAP_FLAG_PARTIAL);
		RFS_ASSERT (map_flag ==FH_MAP_FLAG_COMPLETE);
		fh_map[p->key3].flag = map_flag;
		//RFS_ASSERT (!memcmp(fh_map[p->key3].trace_fh, trace_fh, TRACE_FH_SIZE));
		if (memcmp(fh_map[p->key3].trace_fh, trace_fh, TRACE_FH_SIZE)) {
			int i;
			printf ("fh_map[%d].trace_fh %s trace_fh %s", p->key3, fh_map[p->key3].trace_fh, trace_fh);
			for (i=0; i<fh_i; i++) {
				int * p1 = (int *)&(fh_map[i].play_fh);
#ifdef COMPRESS_TRACE_FH
				int * p = (int *)fh_map[i].trace_fh;
				printf("fh_map[%d].trace_fh %8x%8x%8x%8x%8x%8x%8x%8x path %s \nnew_filehandle %8x%8x%8x%8x%8x%8x%8x%8x\n",
				 i, *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5), *(p+6), *(p+7), fh_map[i].path,
				 *p1, *(p1+1), *(p1+2), *(p1+3), *(p1+4), *(p1+5), *(p1+6), *(p1+7));
#else
				printf("fh_map[%d].trace_fh %s path %s \nnew_filehandle %8x%8x%8x%8x%8x%8x%8x%8x\n",
				 i, fh_map[i].trace_fh, fh_map[i].path,
				 *p1, *(p1+1), *(p1+2), *(p1+3), *(p1+4), *(p1+5), *(p1+6), *(p1+7));
			}
#endif
			RFS_ASSERT (0);
		}
		RFS_ASSERT (!strcmp(fh_map[p->key3].path, path));
		/* It's possible that in fh-path-map, many trace_fh are corresponding to one path
		 * some of it may be the result of lookup after symlink, which is not handled
		 * properly as new created objects 
		 */
#ifdef TAKE_CARE_SYMBOLIC_LINK
		RFS_ASSERT (!memcmp(&fh_map[p->key3].play_fh, &zero_fhandle, sizeof(nfs_fh3)));
#endif
		memcpy (&fh_map[p->key3].play_fh, play_fh, sizeof (nfs_fh3));
		if ((fh_map_debug==1)) // || (stage ==TRACE_PLAY_STAGE)) 
			printf ("update the play_fh for trace_fh %s path %s \n", trace_fh, path);
		return 0;
	}

	fh_map[fh_i].flag = map_flag;
	fh_map[fh_i].lock = 0;
	strncpy(fh_map[fh_i].trace_fh, trace_fh, TRACE_FH_SIZE);

	RFS_ASSERT (strlen(path) < MAX_PLAY_PATH_SIZE);
	strcpy (fh_map [fh_i].path, path);
	if (map_flag==FH_MAP_FLAG_COMPLETE)
		memcpy (&fh_map[fh_i].play_fh, play_fh, sizeof(nfs_fh3));
	else 
		memset (&fh_map[fh_i].play_fh, 0, sizeof(nfs_fh3));

	if ((fh_map_debug==1)) { // || (stage ==TRACE_PLAY_STAGE)) {
		printf ("insert trace_fh %s path %s play_fh:\n", trace_fh, path);
		if (map_flag == FH_MAP_FLAG_COMPLETE) {
			//show_fhandle(play_fh);
		} else 
			printf("null\n");
	}

/*
	if (map_flag == FH_MAP_FLAG_DISCARD)
		printf ("insert flag %d trace_fh %s path %s play_fh:\n", map_flag, trace_fh, path);
*/

    generic_insert(trace_fh, TRACE_FH_SIZE, fh_i, fh_htable, FH_HTABLE_SIZE);
	
	fh_i = (fh_i+1);
	RFS_ASSERT (fh_i < FH_MAP_SIZE);

    return 0;
};

inline fh_map_t * lookup_fh (char * trace_fh )
{
    struct generic_entry * p;
    p = generic_lookup (trace_fh, TRACE_FH_SIZE, 0, fh_htable, FH_HTABLE_SIZE);
	if (fh_map_debug==1)
		printf ("lookup trace_fh %s\n", trace_fh);

    if (p) {
		if (fh_map_debug==1) {
			printf ("found: fh_i[%d] trace_fh %s path %s play_fh:\n", p->key3, fh_map[p->key3].trace_fh, fh_map[p->key3].path);
			//show_fhandle(&fh_map[p->key3].play_fh);
		}
		RFS_ASSERT (!memcmp(fh_map[p->key3].trace_fh, trace_fh, TRACE_FH_SIZE));
        return (&(fh_map[p->key3]));
    } else {
		//printf ("lookup_fh %s not found\n", trace_fh);
		if (stage != READ_DEP_TAB_STAGE && (fh_map_debug==1)) {
			printf ("lookup not found trace_fh %s\n", trace_fh);
		}
        return NULL;
	}
	RFS_ASSERT (0);
}

int delete_fh (char * trace_fh, int fh_map_index)
{
    generic_delete (trace_fh, TRACE_FH_SIZE, fh_map_index, fh_htable, FH_HTABLE_SIZE);
    return 0;
};

int lookup_init_filesystem (nfs_fh3 * parent, char * name, nfs_fh3 * result)
{
    LOOKUP3args		args;
    LOOKUP3res		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
	static int i=0;

    /* set up the arguments */
    (void) memcpy((char *) &args.what.dir, (char *) parent,
							sizeof (nfs_fh3));
    args.what.name = name;
    (void) memset((char *) &reply.resok.object, '\0', sizeof (nfs_fh3));

    /* make the call */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC3_LOOKUP,
			xdr_LOOKUP3args, (char *) &args,
			xdr_LOOKUP3res, (char *) &reply,
			Nfs_timers[Init]);
    sfs_gettime(&stop);

	if (rpc_stat !=RPC_SUCCESS) {
		printf("rpc_stat %d\n", rpc_stat);
		perror("");
	}
    RFS_ASSERT (rpc_stat == RPC_SUCCESS);
	(void) memcpy((char *) result, (char *) &reply.resok.object, sizeof (nfs_fh3));
	return (reply.status);
}

void read_fh_map(char * fh_map_file)
{
	FILE * fp;
	int i = 0;
	char buf[1024];
	char trace_fh[MAX_TRACE_FH_SIZE];
	char intbuf[9];
	char * trace_path;
	char * p;
	int map_flag;
#define MAX_PATH_DEPTH 20
	nfs_fh3 parents[MAX_PATH_DEPTH];
	char * lookup_path_ptr[MAX_PATH_DEPTH];
	char lookup_path [MAX_PLAY_PATH_SIZE];
	int depth;
	int new_dir_flag = 0;
	int lineno = 0;

	depth = 0;
	memset(lookup_path_ptr, 0, sizeof(lookup_path_ptr));
	memcpy(&parents[depth], &(Export_dir.fh_data->sfs_fh_un.f_fh3), sizeof(nfs_fh3));
	strcpy(lookup_path, "/");
	lookup_path_ptr[depth]=&lookup_path[0];

	fp = fopen(fh_map_file, "r");
	if (!fp) {
		printf ("can not opern %s\n", fh_map_file);
		perror("open");
		exit (0);
	}
	RFS_ASSERT (fp!=NULL);
	if (strstr(fh_map_file, "fmt1")) {
		TRACE_FH_SIZE = 48;
	}
	
	intbuf[8]=0;

	memset(buf, 0, sizeof(buf));
	while (fgets(buf, 1024, fp)) {
		lineno ++;
		if (fh_i % 10000==0)
			printf("%d fh_map entry read\n", fh_i);

		RFS_ASSERT (buf[strlen(buf)-1]=='\n');
		buf[strlen(buf)-1]=0;
		if (fh_map_debug) {
			printf("%d fgets return %s\n", fh_i, buf);
			printf("depth %d lookup_path %s\n", depth, lookup_path);
		}
		//for (i=0; i<=depth; i++) 
			//printf("lookup_path_ptr[%d] %s ", i, lookup_path_ptr[i]);
		//printf("\n");
#ifdef COMPRESS_TRACE_FH 
		for (i=0; i<TRACE_FH_SIZE/8; i++) {
			strncpy(intbuf, buf+i*8, 8);
			sscanf(intbuf, "%x", trace_fh+i*8); // maybe it should be 4, anyway we don't compress for now 
		}
		trace_path = buf+TRACE_FH_SIZE*2+1;		/* +1 the trace contains only initial file handle */
#else
		memcpy(trace_fh, buf, TRACE_FH_SIZE);
		trace_path = buf + TRACE_FH_SIZE +1;
#endif
#ifdef TRACE_CONTAIN_LATER_FHANDLE
		trace_path = +=2;	/* +3 if the trace contains both initial and later created file handle */
#endif

#ifdef NO_DEPENDENCY_TABLE
		if (!strncmp (trace_path, "DISCARD", 7) ||
			!strncmp (trace_path, "LN", 2)			) {
			map_flag = FH_MAP_FLAG_DISCARD;
			add_fh (map_flag, buf, trace_path, 0);
			continue;
		}
#endif
		
		p = trace_path+strlen(trace_path)-2;
		while (*p!='/')
			p--;
		p++;
		//RFS_ASSERT (p-trace_path<=strlen(lookup_path)+1);
		//RFS_ASSERT (p>trace_path);

		if (strncmp(lookup_path, trace_path, p-trace_path)) {
			printf("strncmp lookup_path %s trace_path %s for length %d\n", lookup_path, trace_path, p-trace_path);
		}
		RFS_ASSERT (!strncmp(lookup_path, trace_path, p-trace_path));
		//while (strncmp(lookup_path, trace_path, p-trace_path)) {	/* one step deeper */
		while (strlen(lookup_path)>p-trace_path && depth>0) {
			//printf("depth--\n");
			if (depth<=0) 
				printf ("lookup_path %s trace_path %s p-trace_path %d depth %d\n", lookup_path, trace_path, p-trace_path, depth);
			RFS_ASSERT (depth>0);
			*lookup_path_ptr[depth]=0;
			lookup_path_ptr[depth]=0;
			depth--;
		}
		RFS_ASSERT (strlen(lookup_path)==(p-trace_path) || (depth==0));


#ifdef TRACE_CONTAIN_LATER_FHANDLE
		if (buf[TRACE_FH_SIZE*2+1]=='Y') {
			map_flag = FH_MAP_FLAG_COMPLETE;
		} else {
			map_flag = FH_MAP_FLAG_PARTIAL;
			RFS_ASSERT (buf[TRACE_FH_SIZE*2+1]=='N');
		}
#else
		map_flag = FH_MAP_FLAG_COMPLETE;
#endif
		if ((*(p+strlen(p)-1))=='/') {
			*(p+strlen(p)-1)=0;
			new_dir_flag = 1;
		} else 
			new_dir_flag = 0;

		if (map_flag == FH_MAP_FLAG_COMPLETE) {
			int ret = lookup_init_filesystem (&parents[depth], p, &parents[depth+1]);		
			if (ret!=NFS3_OK) {
				printf ("lineno %d %s\n", lineno, buf);
			}
 			RFS_ASSERT (ret == NFS3_OK);
			add_fh (map_flag, buf, trace_path, &parents[depth+1]);	
		} else 
			add_fh (map_flag, buf, trace_path, 0);

		if (new_dir_flag) {
			/* the new fhandle is of a directory */
			lookup_path_ptr[depth+1] = lookup_path+strlen(lookup_path);
			strcat (lookup_path, p);
			strcat (lookup_path, "/");

			//printf("depth++\n");
			depth++;
		}

		memset(buf, 0, sizeof(buf));
	}
			
	if (fh_map_debug) {
		for (i=0; i<fh_i; i++) {
			int * p1 = (int *)&(fh_map[i].play_fh);
#ifdef COMPRESS_TRACE_FH
			int * p = (int *)fh_map[i].trace_fh;
			printf("fh_map[%d].trace_fh %8x%8x%8x%8x%8x%8x%8x%8x path %s \nnew_filehandle %8x%8x%8x%8x%8x%8x%8x%8x\n",
			 i, *p, *(p+1), *(p+2), *(p+3), *(p+4), *(p+5), *(p+6), *(p+7), fh_map[i].path,
			 *p1, *(p1+1), *(p1+2), *(p1+3), *(p1+4), *(p1+5), *(p1+6), *(p1+7));
#else
			printf("fh_map[%d].trace_fh %s path %s \nnew_filehandle %8x%8x%8x%8x%8x%8x%8x%8x\n",
			 i, fh_map[i].trace_fh, fh_map[i].path,
			 *p1, *(p1+1), *(p1+2), *(p1+3), *(p1+4), *(p1+5), *(p1+6), *(p1+7));
		}
#endif

		fprintf(stderr, "total %d requests \n", i);
	}
}

int f()
{
	return 1;
}

inline free_biod_req (int biod_index)
{
	RFS_ASSERT (biod_reqp[biod_index].in_use == TRUE);
	biod_reqp[biod_index].in_use = FALSE;
	num_out_reqs --;
}

void finish_request (int biod_index, int dep_index, int status, int dep_flag)
{
	static int i = 0;
	/* the ending operation, same as when a request time out */

	dep_tab[dep_index].stop = biod_reqp[biod_index].stop;	/* RFS: to dump data */
	free_biod_req (biod_index);

	dep_tab[dep_index].status = status;

	if (event_order_index < EVENT_ORDER_SIZE)
		event_order[event_order_index++] = -dep_tab[dep_index].disk_index;

	dep_tab[dep_index].flag = dep_flag;
	if (is_dir_op(dep_tab[dep_index].proc)) {
		int j;
		RFS_ASSERT (dep_tab[dep_index].fh->lock = 1);
		dep_tab[dep_index].fh->lock = 0;
		if (dep_tab[dep_index].proc==RENAME)
			dep_tab[dep_index].fh_2->lock = 0;
		j = dep_tab[dep_index].fh-fh_map;
		if (dependency_debug) {
			printf ("fh_map[%d] is UNlocked\n",j);
			printf ("trace_fh %d path %s\n", dep_tab[dep_index].fh->trace_fh, dep_tab[dep_index].fh->path);
			printf ("trace_fh %d path %s\n", fh_map[j].trace_fh, fh_map[j].path);
		}
	}
}

/* the request argument may have pointers pointing to buffers, e.g. the name in lookup, 
 * the target of symlink, the write data */
char arg_res[MAX_ARG_RES_SIZE];
char buf1 [MAX_BUF1_SIZE]; 
char buf2 [MAX_BUF2_SIZE];

int execute_next_request ()
{
	int dep_index;
	int proc;
	char * line;
	struct biod_req * reqp;
	sfs_op_type *op_ptr;		/* per operation info */
	struct ladtime call_timeout;
	static int last_print_time = -1;

	if (num_out_reqs == max_biod_reqs) {
		return -1;
	}

	start_profile (&valid_get_nextop_profile);
	start_profile (&invalid_get_nextop_profile);
	dep_index = get_nextop();
	if (dep_index == -1) {
		end_profile (&invalid_get_nextop_profile);
		return dep_index;
	};
	end_profile (&valid_get_nextop_profile);

	start_profile (&prepare_argument_profile);
	line = dep_tab[dep_index].line;

	if (per_packet_debug)
		fprintf (stdout, "time %d processing dep_tab[%d] disk_index %d num_out_reqs %d can_not_catch_speed_num %d PLAY_SCALE %d \n", total_profile.in.tv_sec, dep_index, dep_tab[dep_index].disk_index, num_out_reqs, can_not_catch_speed_num, PLAY_SCALE);

	end_profile(&total_profile);
	if ((total_profile.in.tv_sec - last_print_time >= 10)) {
		last_print_time = total_profile.in.tv_sec;
		//fprintf (stdout, "time %d processing dep_tab[%d] disk_index %d num_out_reqs %d can_not_catch_speed_num %d PLAY_SCALE %d \n", total_profile.in.tv_sec, dep_index, dep_tab[dep_index].disk_index, num_out_reqs, can_not_catch_speed_num, PLAY_SCALE);
/*
		CYCLIC_PRINT (dep_tab_index);
		{
			int tmp = CYCLIC_MINUS(dep_tab_index.head,1,dep_tab_index.size);
			printf("dep_tab_index.head-1 %d disk_index %d tail %d disk_index %d\n", tmp, dep_tab[tmp].disk_index,
			dep_tab_index.tail, dep_tab[dep_tab_index.tail].disk_index);
		}
*/
#ifdef TIME_PLAY
#ifdef SPEED_UP
/*
		if (can_not_catch_speed_num < 2000) {
			PLAY_SCALE ++;
			printf ("set PLAY_SCALE to %d\n", PLAY_SCALE);
		};
		if (can_not_catch_speed_num > 50000) {
			PLAY_SCALE /= 2;
		} else {
			if (can_not_catch_speed_num > 5000) {
				PLAY_SCALE -= 2;
				if (PLAY_SCALE < 1)
					PLAY_SCALE = 1;
			}
		}
*/
#endif
		if ((total_profile.in.tv_sec > 100)) {
			can_not_catch_speed_num_total += can_not_catch_speed_num;
		}
		can_not_catch_speed_num = 0;
#endif
	}
	if (rfs_debug)
		printf ("processing dep_tab[%d] disk_index %d %s\n", dep_index, dep_tab[dep_index].disk_index, line);

	proc = dep_tab[dep_index].proc;
	rfs_Ops[proc].setarg (dep_index, line, arg_res, buf1, buf2);

	op_ptr = &Ops[proc];
	reqp = get_biod_req (dep_index);
	RFS_ASSERT (reqp);

#ifdef	notdef	/* place to set request timeout. G. Jason Peng */
	call_timeout.sec = 2; //Nfs_timers[op_ptr->call_class].tv_sec;
	call_timeout.usec = Nfs_timers[op_ptr->call_class].tv_usec;
#else
	call_timeout.sec = 0;
	call_timeout.usec = 300000;
	//call_timeout.usec = 14000;
	//call_timeout.usec = 13000;
	//call_timeout.usec = 6000;
	//call_timeout.usec = 8000;
	//call_timeout.usec = 10000;
#endif

    /* make the call */
    sfs_gettime(&(reqp->start));
	end_profile (&prepare_argument_profile);
	start_profile (&biod_clnt_call_profile);
#define REAL_PLAY
#ifdef REAL_PLAY

#ifdef RECV_THREAD
	//printf ("send thread waitsem\n");
	waitsem(async_rpc_sem);
	//printf ("send thread got sem\n");
#endif
    reqp->xid = biod_clnt_call(NFS_client, rfs_Ops[proc].nfsv3_proc, 
					rfs_Ops[proc].xdr_arg, arg_res);
#ifdef RECV_THREAD
	postsem(async_rpc_sem);
	//printf ("send thread postsem\n");
#endif

#else	// REAL_PLAY
	reqp->xid = dep_index+1;	/* just fake a message id and let it expire */
#endif
    RFS_ASSERT (reqp->xid != 0);
    reqp->timeout = reqp->start;
    ADDTIME (reqp->timeout, call_timeout);
    dep_tab[dep_index].flag = DEP_FLAG_SENT;
	if (event_order_index < EVENT_ORDER_SIZE)
		event_order[event_order_index++] = dep_tab[dep_index].disk_index;

	dep_tab[dep_index].start = reqp->start;	/* RFS: to dump data */
	end_profile (&biod_clnt_call_profile);

	send_num ++;
}

void check_reply (int proc, int biod_index, int dep_index, int status, char * errmsg, int trace_status)
{
	if (((status!=trace_status)) && (status!=NFS3_OK) && (trace_status!=NFS3ERR_RFS_MISS)) {
		if (!profile_debug)
			printf ("receive problem reply, xid %x nfs_ret %d %s trace_status %d start %d:%d stop %d:%d command disk index %d\n", biod_reqp[biod_index].xid, status, errmsg, trace_status, biod_reqp[biod_index].start.sec, biod_reqp[biod_index].start.usec, biod_reqp[biod_index].stop.sec, biod_reqp[biod_index].stop.usec, dep_tab[dep_index].disk_index); 
#ifndef TAKE_CARE_UNLOOKED_UP_NON_NEW_FILES
		/* these files is not looked up and is not create/mkdir/symlink/link/mknod ed before they
		 * are refered by name through rename, remove
		 */
		if ((proc==RENAME || proc==REMOVE) && (status==NFS3ERR_NOENT) && (trace_status ==0)) {
			/* current initialization doesnot take care of rename source, if there is no
			 * create or lookup before that source, the source object will not exist when
			 * rename occurs
			 */
			rename_rmdir_noent_reply_num++;
		} else 
#endif
#ifndef TAKE_CARE_SYMBOLIC_LINK
		if ((proc==LOOKUP) && (status==NFS3_OK) && (trace_status==NFS3ERR_NOENT)) {
			/* in the original trace, first lookup return NOENT, then symlink is executed, then lookup return OK
			 * the initialization considers only the lookup return OK and created the file in the initialization
			 * so in trace play the first lookup return OK
			 */
			RFS_ASSERT (1);
		} else // if ((proc==SYMLINK) && (status == NFS3ERR_EXIST) && (trace_status == 0)) {
				/* trace_status could be EAGAIN */
			if ((proc==SYMLINK) && (status == NFS3ERR_EXIST) ) {
			/* due to similar reason as above, the initialization code initializes the symbolic link as a normal
			 * file already
			 */
			RFS_ASSERT (1);
		} else
#endif
#ifndef TAKE_CARE_NOEMPTY_RMDIR
		/* the remove packet seems got lost in the trace capture, so replay can not finish */
		if ((proc==RMDIR) && (status==NFS3ERR_NOTEMPTY)) {
   			RENAME3args		args;
   			RENAME3res		reply;		/* the reply */
			RMDIR3args * rmdir_argp;
			enum clnt_stat rpc_stat;	/* result from RPC call */

			rfs_Ops[proc].setarg (dep_index, dep_tab[dep_index].line, arg_res, buf1, buf2);
			rmdir_argp = (RMDIR3args *)arg_res;

			memcpy(&args.from, &(rmdir_argp->object), sizeof (diropargs3));
			memcpy(&args.to.dir, &(Export_dir.fh_data->sfs_fh_un.f_fh3), sizeof(nfs_fh3));
			args.from.name = buf1;	/* the buf1 is already filled when parsing rmdir */
			args.to.name = buf2;
			sprintf(buf2, "rmdir_%d_%s", dep_tab[dep_index].disk_index, rmdir_argp->object.name);

  			rpc_stat = clnt_call(NFS_client, NFSPROC3_RENAME,
			xdr_RENAME3args, (char *) &args,
			xdr_RENAME3res, (char *) &reply,
				Nfs_timers[Init]);
			RFS_ASSERT (rpc_stat == RPC_SUCCESS);
			if (reply.status!=NFS3_OK)
				printf ("change rmdir into rename, reply.status %d\n", reply.status);
			RFS_ASSERT (reply.status==NFS3_OK);
			rmdir_not_empty_reply_num ++;
#endif
#ifndef TAKE_CARE_ACCESS_ERROR
		} else if ((status==0) && (trace_status==NFS3ERR_ACCES)) {
			loose_access_control_reply_num ++;
#endif
#ifdef NO_DEPENDENCY_TABLE 
		} else if ((proc==LOOKUP) && (status==NFS3ERR_NOENT) && (trace_status==NFS3_OK)) {
			lookup_err_due_to_rename_num ++;
		} else if ((proc==LOOKUP) && (status==NFS3_OK) && (trace_status == NFS3ERR_NOENT)) {
			/* if there is a remove in front of the lookup, but it is
 			 * actually executed later than the lookup
			 */
			lookup_err_due_to_parallel_remove_num ++;
#endif
#ifndef TAKE_CARE_LOOKUP_EACCESS_ENOENT_MISMATCH
		/* if the looked return EACCESS in the trace, means the object still exists
		 * should have initialized, right not don't initialize it, hence play status 
		 * could be ENOENT
		 */
		} else if ((proc==LOOKUP) && (status==NFS3ERR_NOENT) && (trace_status==NFS3ERR_ACCES)) {
			lookup_eaccess_enoent_mismatch_num ++;
#endif
#ifdef TOLERANT_READ_IO_ERR
		} else if ((proc==READ) && (status==NFS3ERR_IO) && (trace_status==NFS3_OK)) {
			read_io_err_num ++;
#endif
#ifdef TOLERANT_STALE_FHANDLE_ERR
		} else if ((status==NFS3ERR_STALE) && (trace_status==NFS3_OK)) {
			printf ("!!!!!!! STALE FILE HANDLE \n");
			//sleep(1);
			stale_fhandle_err_num ++;
#endif
		} else {
			int i;
			for (i=dep_window_index.tail; CYCLIC_LESS(dep_window_index,i,dep_window_index.head); i++) {
				if (dep_tab[i].flag!=1)
					printf ("dep_tab[%d].disk_index %d, flag %d line %s\n", i, dep_tab[i].disk_index, dep_tab[i].flag, dep_tab[i].line);
			}

			if (status==EEXIST) {
				abnormal_EEXIST_num ++;
			} else if (status == ENOENT) {
				abnormal_ENOENT_num ++;
			} else {
				printf ("!!!!!!!!!!!!!1 should fail\n");
				//RFS_ASSERT (0);
			}
		}
	} else {
		proper_reply_num ++;
		if (total_profile.in.tv_sec >= WARMUP_TIME) 
			run_stage_proper_reply_num ++;
	}

}

/* return -1 if there is no reply being received 
 * return the dep_index if the corresponding reply has been received
 */
int receive_next_reply (int busy_flag)
{
	int dep_index;
	int biod_index;
	int proc;
	char * line;
	char * reply_line;
	sfs_op_type *op_ptr;		/* per operation info */
	int ret;
	int status;
	int trace_status;
	char * errmsg;
	int poll_timeout = 0;		/* timeout in usecs */

	/* wait for reply */
	start_profile (&valid_poll_and_get_reply_profile);
	start_profile (&invalid_poll_and_get_reply_profile);

	if (busy_flag == BUSY) {
		poll_timeout = 0;
		poll_timeout_0_num ++;
	} else {
		poll_timeout = 2000;	/* 10000 or 2000 is a better number in non-debugging state */
		//poll_timeout = 0;	/* 10000 or 2000 is a better number in non-debugging state */
		poll_timeout_pos_num ++;
	}

	biod_index = poll_and_get_reply (poll_timeout);
	if (biod_index==-1) {
		end_profile (&invalid_poll_and_get_reply_profile);
		return -1;
	};
	end_profile (&valid_poll_and_get_reply_profile);

	start_profile (&decode_reply_profile);
	/* check the corresponding request */
	dep_index = biod_reqp[biod_index].dep_tab_index;
	if (biod_reqp[biod_index].in_use==1) {
		RFS_ASSERT (dep_tab[dep_index].biod_req_index == biod_index);
	} else {
		printf ("biod_index %d reply received but the request has been time out\n", biod_index);
		return -1;
	}

	proc = dep_tab[dep_index].proc;
	op_ptr = &Ops[proc];

	if (dep_tab[dep_index].flag != DEP_FLAG_SENT) {
		printf("dep_tab[%d].flag %d proc %d status %d start %d:%d stop %d:%d\n",
			dep_index, dep_tab[dep_index].flag, proc, dep_tab[dep_index].status, 
			dep_tab[dep_index].start.sec, dep_tab[dep_index].start.usec,
			dep_tab[dep_index].stop.sec, dep_tab[dep_index].stop.usec );
		printf ("received reply for timeout requests dep_tab[%d].disk_index %d\n", dep_index, dep_tab[dep_index].disk_index);
		return dep_index;
	}
	RFS_ASSERT (dep_tab[dep_index].flag == DEP_FLAG_SENT);

	/* decode the reply */
	rfs_Ops[proc].setres (arg_res, buf1);
	ret = proc_header (NFS_client, rfs_Ops[proc].xdr_res, arg_res);
	RFS_ASSERT (ret == RPC_SUCCESS);
	status = *((int *)arg_res);
	errmsg = nfs3_strerror (status);
	end_profile (&decode_reply_profile);

	start_profile (&check_reply_profile);
	/* compare with the reply in the trace */
	line = dep_tab[dep_index].line;
	reply_line = dep_tab[dep_index].reply_line;
	trace_status = dep_tab[dep_index].trace_status;

	if (per_packet_debug || rfs_debug )
		fprintf (stdout, "dep_tab[%d], disk_index %d, receive reply, rpc_ret %d xid %x nfs_ret %d %s trace_status %d start %d:%d stop %d:%d \n", dep_index, dep_tab[dep_index].disk_index, ret, biod_reqp[biod_index].xid, status, errmsg, trace_status, biod_reqp[biod_index].start.sec, biod_reqp[biod_index].start.usec, biod_reqp[biod_index].stop.sec, biod_reqp[biod_index].stop.usec);

	/* error checking */
	check_reply (proc, biod_index, dep_index, status, errmsg, trace_status);

	/* free resources */
	finish_request (biod_index, dep_index, status, DEP_FLAG_DONE);
	recv_num ++;
	
	/* we set 100 seconds warm up time */
	if ((total_profile.in.tv_sec >= WARMUP_TIME)) {
	/* get statistics */
	if (status == trace_status || (status==NFS3_OK && trace_status==NFS3ERR_RFS_MISS) ) {
		op_ptr->results.good_calls++;
		Ops[TOTAL].results.good_calls++;
	} else {
		op_ptr->results.bad_calls++;
		Ops[TOTAL].results.bad_calls++;
	}
	sfs_elapsedtime (op_ptr, &(biod_reqp[biod_index].start), &(biod_reqp[biod_index].stop));
	end_profile (&check_reply_profile);
	}
	
	//start_profile (&add_create_object_profile);

	if (trace_status == NFS3_OK && (proc==CREATE || proc==MKDIR || proc==SYMLINK || proc==MKNOD)) {
#ifndef REDUCE_MEMORY_TRACE_SIZE
		RFS_ASSERT (reply_line);
#endif
		if (status!=NFS3_OK) {
			/* commented out for 1022 */
			printf ("!!!!!! Should have been an ASSERTION FAILURE \n");
			RFS_ASSERT (proc==SYMLINK);
			RFS_ASSERT (0);
		} else {
			if (proc!=SYMLINK || line[TRACE_VERSION_POS]!='2')
				add_new_file_system_object(proc, dep_index, line, reply_line);
		}
	}
	//end_profile (&add_create_object_profile);
}

inline void add_new_file_system_object (int proc, int dep_index, char * line, char * reply_line)
{
	char * child_trace_fh;
	fh_map_t * parent_entryp;
	char component_name[MAX_PLAY_PATH_SIZE];
	char * parent_trace_fh;
	char child_path[MAX_PLAY_PATH_SIZE];
	post_op_fh3 * post_op_fh3_child;
	char * reply_trace_fh;
	nfs_fh3 * child_fh3;

	parent_trace_fh = strstr (line, "fh");
	RFS_ASSERT (parent_trace_fh);
	parent_trace_fh +=3;
	parent_entryp = lookup_fh (parent_trace_fh);
	RFS_ASSERT (parent_entryp);
	parse_name (parent_trace_fh+65, component_name);
	strcpy (child_path, parent_entryp->path);
	strcat (child_path, "/");
	strcat (child_path, component_name);
				
	/* find the corresponding create request */
	//printf ("before find reply trace_fh reply_line %s\n", reply_line);
#ifdef REDUCE_MEMORY_TRACE_SIZE
	reply_trace_fh = dep_tab[dep_index].reply_trace_fh;
#else
	reply_trace_fh = find_reply_trace_fh (reply_line);
#endif
	RFS_ASSERT (reply_trace_fh != NULL);
	switch (proc) {
	case CREATE:
		RFS_ASSERT (((CREATE3res *)arg_res)->res_u.ok.obj.handle_follows==TRUE);
		child_fh3 = &((CREATE3res *)arg_res)->res_u.ok.obj.handle;
		break;
	case MKDIR:
		RFS_ASSERT (((MKDIR3res *)arg_res)->res_u.ok.obj.handle_follows==TRUE);
		child_fh3 = &((MKDIR3res *)arg_res)->res_u.ok.obj.handle;
		break;
	case SYMLINK:
		RFS_ASSERT (((SYMLINK3res *)arg_res)->res_u.ok.obj.handle_follows==TRUE);
		child_fh3 = &((SYMLINK3res *)arg_res)->res_u.ok.obj.handle;
		break;
	case MKNOD:
		RFS_ASSERT (((MKNOD3res *)arg_res)->res_u.ok.obj.handle_follows==TRUE);
		child_fh3 = &((MKNOD3res *)arg_res)->res_u.ok.obj.handle;
		break;
	case LOOKUP:
		RFS_ASSERT (proc==LOOKUP);
		child_fh3 = &((LOOKUP3res *)arg_res)->res_u.ok.object;
		break;
	default:
		RFS_ASSERT (0);
	}
#ifndef REDUCE_MEMORY_TRACE_SIZE
	RFS_ASSERT (reply_trace_fh[TRACE_FH_SIZE]==' ');
#endif
	reply_trace_fh[TRACE_FH_SIZE] = 0;
	add_fh (FH_MAP_FLAG_COMPLETE, reply_trace_fh, child_path, child_fh3); 	/* exist flag is not used now, set to 1 */
#ifndef REDUCE_MEMORY_TRACE_SIZE
	/* just to preserve the original reply line not changed */
	reply_trace_fh[TRACE_FH_SIZE] = ' ';
#endif
}

/* initialize timestamp and proc field of dep_tab entry */
void trace_play(void)
{
	
	/* The flag to indicate whether trace_player is BUSY. Trace_player is BUSY
	 * when either there is request to send or there is reply being
	 * received. Otherwise it is IDLE. The timeout for polling replies 
	 * is set to 0 when BUSY, it is set to the waiting time to the first
	 * request outside of current <min_dep_index, dep_window_max> window when IDLE.
	 */
   	int busy_flag = BUSY;		
	//int dep_index;		/* index into dependency table: dep_tab */
	//int biod_index; 	/* index into outstanding requests: biod_reqp */
	static int last_print_time = -1;
	int poll_timeout = 0;

#ifndef IO_THREAD
	disk_io_status = read_trace();
#endif

	RFS_ASSERT (!CYCLIC_EMPTY(dep_tab_index));
	CYCLIC_MOVE_HEAD(dep_window_index);

	adjust_play_window(busy_flag, &poll_timeout);

	start_profile (&total_profile);
	while (!CYCLIC_EMPTY(dep_tab_index)) {
		end_profile(&total_profile);
		if ((total_profile.in.tv_sec - last_print_time >= 10)) {
			int i;

			last_print_time = total_profile.in.tv_sec;
			fprintf (stdout, ">>>> sendng thread: time %d send_num %d recv_num %d timeout_num %d num_out_reqs %d can_not_catch_speed_num %d PLAY_SCALE %d \n", total_profile.in.tv_sec, send_num, recv_num, timeout_num, num_out_reqs, can_not_catch_speed_num, PLAY_SCALE);
			for (i=0; i<=MAX_OUTSTANDING_REQ; i++) {
				if (num_out_reqs_statistics[i]!=0) {
					printf("num_out_req[%d]=%d,", i, num_out_reqs_statistics[i]);
					num_out_reqs_statistics[i]=0;
				}
			}
			printf("\n");
			for (i=0; i<=MAX_OUTSTANDING_REQ; i++) {
				if (num_out_reqs_statistics_at_timeout[i]!=0) {
					printf("num_out_req_at_timeout[%d]=%d,", i, num_out_reqs_statistics_at_timeout[i]);
					num_out_reqs_statistics_at_timeout[i]=0;
				}
			}
			printf("\n");
			//	CYCLIC_PRINT(dep_tab_index);
		}

		if ((total_profile.in.tv_sec > 6000)) {
			printf ("the process has run for more than 600 seconds, exit\n");
			goto END;
		}

		if (busy_flag == IDLE) {
#ifndef RECV_THREAD
			//start_profile (&check_timeout_profile);
			check_timeout();
			//end_profile (&check_timeout_profile);
#endif
#ifndef IO_THREAD
			if (disk_io_status!=TRACE_FILE_END) {
				disk_io_status = read_trace();
			};
#endif
		}

		//start_profile (&adjust_play_window_profile);
	 	adjust_play_window (busy_flag, &poll_timeout);
		if (rfs_debug)
			printf("num_out_reqs %d\n", num_out_reqs);
		num_out_reqs_statistics[num_out_reqs]++;
		busy_flag = IDLE;
		//end_profile (&adjust_play_window_profile);

		start_profile (&execute_next_request_profile);
		while (execute_next_request()!=-1) {
			busy_flag = BUSY;
		}
		end_profile (&execute_next_request_profile);

#ifndef RECV_THREAD
		start_profile (&receive_next_reply_profile);
		/* actually the performance of two policy seems to be same */
//#define SEND_HIGHER_PRIORITY_POLICY
#define SEND_RECEIVE_EQUAL_PRIORITY_POLICY	

#ifdef SEND_HIGHER_PRIORITY_POLICY
		receive_next_reply(IDLE);
#endif
#ifdef SEND_RECEIVE_EQUAL_PRIORITY_POLICY
		busy_flag = IDLE;
		while (receive_next_reply(busy_flag)!=-1)
			busy_flag = BUSY;
#endif
		end_profile (&receive_next_reply_profile);
#endif
		CYCLIC_ASSERT (0);
	}
	end_profile (&total_profile);

	RFS_ASSERT (disk_io_status == TRACE_FILE_END);
	if (num_out_reqs !=0 ) {
		printf ("num_out_reqs %d\n", num_out_reqs);
		CYCLIC_PRINT(dep_tab_index);
	}
	RFS_ASSERT(num_out_reqs==0);

END:
	printf ("trace starttime %d, trace_end_time %d trace_duration %d\n", trace_timestamp1, trace_timestamp2,
		trace_timestamp2 - trace_timestamp1);
	printf ("can_not_catch_speed_num_total %d can_not_catch_speed_num_last_10_seconds %d", 
		can_not_catch_speed_num_total, can_not_catch_speed_num);
	printf ("total_profile.about: %s\n", total_profile.about);
	print_profile ("total_profile", &total_profile);
	printf("\n");
	//print_profile ("check_timeout", &check_timeout_profile);
	//printf("\n");
	//print_profile ("adjust_play_window", &adjust_play_window_profile);
	//printf("\n");
	print_profile ("execute_next_request_profile", &execute_next_request_profile);
	print_profile ("valid_get_nextop_profile", &valid_get_nextop_profile);
	print_profile ("invalid_get_nextop_profile", &invalid_get_nextop_profile);
	print_profile ("prepare_argument_profile", &prepare_argument_profile);
	print_profile ("biod_clnt_call_profile", &biod_clnt_call_profile);
	printf("\n");
	print_profile ("receive_next_reply_profile", &receive_next_reply_profile);
	print_profile ("valid_poll_and_get_reply_profile", &valid_poll_and_get_reply_profile);
	print_profile ("invalid_poll_and_get_reply_profile", &invalid_poll_and_get_reply_profile);
	print_profile ("decode_reply_profile", &decode_reply_profile);
	print_profile ("check_reply_profile", &check_reply_profile);
	print_profile ("fgets_profile", &fgets_profile);
	print_profile ("read_line_profile", &read_line_profile);
	print_profile ("read_trace_profile", &read_trace_profile);
	//print_profile ("add_create_object", &add_create_object_profile);
	printf("\n");
	
	printf ("dep_tab_index.tail %d dep_tab_index.head %d num_out_reqs %d\n", dep_tab_index.tail, dep_tab_index.head, num_out_reqs);
}


int CYCLIC_SET_TAIL_TO(cyclic_index_t * index, int dest) 
{ 
	cyclic_index_t indextmp, indextmp2;
	int oldnum, num;
	indextmp = *index;
	indextmp2 = indextmp;
	oldnum = CYCLIC_NUM(indextmp); 

	if (! ((dest>=0) && (dest<indextmp.size))) {
		CYCLIC_PRINT(indextmp);
		printf("dest %d\n", dest);
	}
	RFS_ASSERT ((dest>=0) && (dest<indextmp.size));
	index->tail = dest; 
	indextmp2.tail = dest;
	num = CYCLIC_NUM(indextmp2); 

	if (num > oldnum) { 
		CYCLIC_PRINT(indextmp);
		CYCLIC_PRINT(indextmp2);
		printf("dest %d old_num %d num %d\n", dest, oldnum, num);
	}
	RFS_ASSERT (num <= oldnum);
}

int flush_junk()
{
	int i;
	for (i=0; i<500; i++) {
		printf ("*************************************************************\n");
	}
	fflush(stdout);
}

int CYCLIC_ASSERT (int i)
{
	int j;
	if (!(dep_tab_index.tail == dep_window_index.tail)) {
		printf("%s head %d tail %d, size %d\n", dep_tab_index.name, dep_tab_index.head, dep_tab_index.tail, dep_tab_index.size);
		printf("%s head %d tail %d, size %d\n", dep_window_index.name, dep_window_index.head, dep_window_index.tail, dep_window_index.size);
		printf("pos %d\n", i); 
		flush_junk();
		sleep (10);
		RFS_ASSERT (0);
	};
 
	if (!((dep_window_index.head == dep_tab_index.head) || 
		   CYCLIC_LESS(dep_tab_index, dep_window_index.head, dep_tab_index.head ) )) {
		printf("%s head %d tail %d, size %d\n", dep_tab_index.name, dep_tab_index.head, dep_tab_index.tail, dep_tab_index.size);
		printf("%s head %d tail %d, size %d\n", dep_window_index.name, dep_window_index.head, dep_window_index.tail, dep_window_index.size);
		printf("pos %d\n", i); 
		flush_junk();
		sleep (10);
		RFS_ASSERT (0);
	};
	for (i=0, j=0; i<max_biod_reqs; i++) {
		if (biod_reqp[i].in_use == 1)
			j++;
	}
#ifndef RECV_THREAD
	RFS_ASSERT (num_out_reqs==j);
#endif
/*
		RFS_ASSERT ((dep_window_index.head == dep_tab_index.head) || 
			   CYCLIC_LESS(dep_tab_index, dep_window_index.head, dep_tab_index.head ));
*/
}

/* sfs_c_chd.c */
