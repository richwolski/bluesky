#ifndef RFS_H
#define RFS_H
#include "sfs_c_def.h"
#include "profile.h"
#include "rfs_assert.h"

/* the maximum number of operations depended by one operation */
/* the dependency include read/write, write/write, all operations with a
 * one file handle/delete or rename's target if exists, the dependency
 * does not include the create/all operations with relevant file handle
 * This dependency is maintained by a flag in file handle mapping table
 */
#define MAX_DEP_OPS 10 
//#define DEP_TAB_SIZE	200000 /* the dependency table size */
/* Right now we don't wrap around with dep_tab, for each request, there
 * is one entry in dependency table and one entry in lines buffer.
 */
#define REDUCE_MEMORY_TRACE_SIZE

#define EVENT_ORDER_SIZE 1000000
#ifdef REDUCE_MEMORY_TRACE_SIZE
//#define DEP_TAB_SIZE	 1100000 /* the dependency table size */
#define DEP_TAB_SIZE	 100000 /* the dependency table size */
//#define DEP_TAB_SIZE	 10000 /* the dependency table size */
#define MAX_MEMORY_TRACE_LINES	DEP_TAB_SIZE 
//#define MAX_TRACE_LINE_LENGTH 768
#define MAX_TRACE_LINE_LENGTH 300
#else
#define MAX_TRACE_LINE_LENGTH 768
#define DEP_TAB_SIZE	500000 /* the dependency table size */
#define MAX_MEMORY_TRACE_LINES	DEP_TAB_SIZE*2 
#endif

#define FH_MAP_SIZE 400000
//#define FH_MAP_SIZE 40000
#define FH_HTABLE_SIZE FH_MAP_SIZE								 

/* trace play policy related defines */
#define SPEED_UP		// only one of SPEED_UP and	SLOW_DOWN is defined
//#define SLOW_DOWN
extern int PLAY_SCALE;
//#define TIME_PLAY /* play according original timestamp or scaled timestamp together with dependency */
#define NO_DEPENDENCY_TABLE
//#define MAX_COMMAND_REPLY_DISTANCE 1000
#define MAX_COMMAND_REPLY_DISTANCE_FOR_PAIR 1000
#define MAX_COMMAND_REPLY_DISTANCE 2

#define WORKLOAD_PLAY /* play according dependency and as fast as it can, ignore timestamp */

#ifdef TIME_PLAY
#define MAX_PLAY_WINDOW 8
#define MAX_OUTSTANDING_REQ 4	/* too big outstanding window reduces throughput */
#else
#define MAX_PLAY_WINDOW 1000
//#define MAX_PLAY_WINDOW 8
#define MAX_OUTSTANDING_REQ 8
//#define MAX_OUTSTANDING_REQ 16
#endif
#define UDP	/* currently we only support using UDP, support for TCP will be added later */

/* the flag to indicate the current status of a trace request */
#define DEP_FLAG_FREE 0			/* free entry */
#define DEP_FLAG_INIT 1			/* initialized */
#define DEP_FLAG_CANDIDATE 2
#define DEP_FLAG_WAIT_FHANDLE 3	/* waiting for leading fhandle */
#define DEP_FLAG_FHANDLE_READY 4
#define DEP_FLAG_WAIT_DIRECTORY 5
#define DEP_FLAG_DIRECTORY_READY 6
#define DEP_FLAG_WAIT_DELETE 7	/* waiting for deleting the object */
#define DEP_FLAG_SENT 8			/* Request has been sent out, waiting for replies */
#define DEP_FLAG_DONE 9			/* Reply has been received and processed */
#define BUSY	1
#define IDLE	0

#define MAX_ARG_RES_SIZE 512
#define MAX_BUF1_SIZE 65536 	/* for readdirplus, 208 each entry, max 128 entries */
#define MAX_BUF2_SIZE NFS_MAXPATHLEN

extern int TRACE_COMMAND_REPLY_FLAG_POS;
extern int TRACE_VERSION_POS;
extern int TRACE_MSGID_POS;
extern int TRACE_FH_SIZE;

/* Two setbuf_t procedure exists for each NFS operation, one for setting up NFS 
 * operation argument buffer, another for setting up NFS operation result receiving buffer.
 *
 * When setting up argument, the function takes two argument, the first 
 * argument is index to dep_tab, the second argument is the buffer pointer to 
 * be setup. Sometimes, there is no extra argument other than index, for example: read
 * Sometimes, there is one extra argument, e.g the buffer for lookup name.
 * Sometimes, there are two extra arguments, e..g rename, symlink.
 *
 * void (*setbuf_t)(int index, char * arg, char * arg);
 *
 * When setting up receiving result buffer, the function takes one arguement,
 * the buffer pointer.
 * void (*setbuf_t)(char * res, char * arg);
 */
typedef void (*setbuf_t)();

typedef struct {
	int nfsv3_proc;
	setbuf_t setarg;
	setbuf_t setres;
	xdrproc_t xdr_arg;
	xdrproc_t xdr_res;
} rfs_op_type;

#define MAX_TRACE_FH_SIZE 64
#define TRACE_FH_FILE_SIZE 48
#define TRACE_FH_DIR_SIZE 40
#define MAX_PLAY_PATH_SIZE 256

/* flags in fh_map_t */
#define FH_MAP_FLAG_FREE 0
#define FH_MAP_FLAG_DISCARD 1
#define FH_MAP_FLAG_PARTIAL 2
#define FH_MAP_FLAG_COMPLETE 3
typedef struct {
	int flag;
	int lock;
	char trace_fh [MAX_TRACE_FH_SIZE+1];
	char path[MAX_PLAY_PATH_SIZE];
	nfs_fh3 play_fh;
} fh_map_t;

typedef struct {
	int disk_index;
	int trace_status;
	char reply_trace_fh[MAX_TRACE_FH_SIZE+1];
	char line[MAX_TRACE_LINE_LENGTH];
} memory_trace_ent_t;
	

typedef struct {
	int flag;	
	int disk_index;
	int memory_index;
	char * line;
	char * reply_line;
	fh_map_t * fh;
	fh_map_t * fh_2;
	char * trace_fh;
	char * trace_fh_2;
#ifdef REDUCE_MEMORY_TRACE_SIZE
	char * reply_trace_fh;
#endif
	int proc;	/* the prototype independent NFS operation number defined in sfs_c_def.h
				 * e.g. NULLCALL NFS3PROC_READ etc */
	struct timeval timestamp;	/* The timestamp of a request in the trace */
#ifdef NO_DEPENDENCY_TABLE
	int dep_ops[MAX_DEP_OPS]; /* Other requests need to be processed prior this request */
								 /* dep_tab[i] == -1 means the dependency has been resolved */
	int init_dep_num;			/* The number of request being depended initially */
	int cur_dep_num;			/* The number of remaining ones which have not been finished */ 
								/* at initialization time, init_dep_num == cur_dep_num */
#endif
	int biod_req_index;	/* Index in to the biod_req array which contains all outstanding requests */
	struct ladtime start;
	struct ladtime stop;
	struct ladtime interval;
	int skip_sec;
	int status;
	int trace_status;
} dep_tab_t;

typedef struct {
	char name[32];
	int head;
	int tail;
	int size;
} cyclic_index_t;

#define CYCLIC_INIT(str,index,SIZE) \
	{index.head=0; index.tail=0; index.size=SIZE; \
	 RFS_ASSERT(strlen(str)<sizeof(index.name)); \
	 strcpy(index.name, str); }

#define CYCLIC_PRINT(index) \
	{printf("%s head %d tail %d, size %d\n", index.name, index.head, index.tail, index.size);}

#define CYCLIC_FULL(index) \
	(((index.head+1)%index.size)==index.tail)

#define CYCLIC_EMPTY(index)	\
	(index.tail==index.head)

#define CYCLIC_NUM(index) \
	((index.head + index.size - index.tail) % index.size)

#define CYCLIC_MOVE_HEAD(index) \
	{ \
		if (CYCLIC_FULL(index)) { \
			CYCLIC_PRINT(index) \
		} \
		RFS_ASSERT (!CYCLIC_FULL(index)); \
		index.head=(index.head+1)%(index.size); \
	}

#define CYCLIC_MOVE_TAIL(index) \
	{ \
		RFS_ASSERT (!CYCLIC_EMPTY(index)) \
		index.tail=(index.tail+1)%(index.size); \
	}

/*
#define CYCLIC_MOVE_TAIL_TO(index,dest) \
	{ \
		int oldnum = CYCLIC_NUM(index); \
		if (! ((dest>=0) && (dest<index.size) && (dest!=index.head))) { \
			CYCLIC_PRINT(index); \
			printf("dest %d\n", dest); \
		} \
		RFS_ASSERT ((dest>=0) && (dest<index.size) && (dest!=index.head)) \
		index.tail = dest; \
		if (CYCLIC_NUM(index) > oldnum) { \
			CYCLIC_PRINT(index); \
			printf("dest %d old_num %d \n", dest, oldnum); \
		} \
		RFS_ASSERT (CYCLIC_NUM(index) <= oldnum); \
	}
*/

#define CYCLIC_MINUS(A,B,size) ((A+size-B)%size)
#define CYCLIC_ADD(A,B,size) ((A+B)%size)
#define CYCLIC_LESS(index,A,B) \
(CYCLIC_MINUS(A,index.tail,index.size)<CYCLIC_MINUS(B,index.tail,index.size))

extern struct ladtime current;
extern struct ladtime trace_starttime;
extern struct ladtime time_offset;

extern rfs_op_type rfs_Ops[];
extern sfs_op_type nfsv3_Ops[];
extern sfs_op_type * Ops;
extern int num_out_reqs;

extern cyclic_index_t dep_tab_index;
extern cyclic_index_t dep_window_index;
extern cyclic_index_t memory_trace_index;
int dep_window_max; /* the first entry outside of the dependency window */
/* If ordered by TIMESTAMP,
 * memory_trace_index.tail <= dep_tab_index.tail < dep_window_max <=
 * dep_tab_index.head <= memory_trace_index.head
 */

extern fh_map_t fh_map [FH_MAP_SIZE];
extern int fh_i;
extern int fh_map_debug ;
extern struct generic_entry * fh_htable [FH_HTABLE_SIZE];

extern dep_tab_t dep_tab[DEP_TAB_SIZE];
extern int req_num_with_new_fh;
extern int req_num_with_discard_fh;
extern int req_num_with_init_fh;
extern memory_trace_ent_t memory_trace[MAX_MEMORY_TRACE_LINES];
extern int event_order[];
extern int event_order_index;
extern struct biod_req * biod_reqp;
extern int max_biod_reqs;
extern int rfs_debug;
extern int per_packet_debug;
extern int profile_debug;
extern int adjust_play_window_debug;
extern int dependency_debug;
extern int quiet_flag;
extern int read_data_owe;
extern int write_data_owe;
extern int read_data_total;
extern int write_data_total;
extern int read_data_adjust_times;
extern int write_data_adjust_times;
extern int read_data_owe_GB;
extern int write_data_owe_GB;
extern int read_data_total_GB;
extern int write_data_total_GB;

extern int failed_create_command_num;
extern int failed_other_command_num;
extern int skipped_readlink_command_num;
extern int skipped_custom_command_num;
extern int fh_path_map_err_num;
extern int skipped_fsstat_command_num;
extern int missing_reply_num;
extern int lookup_noent_reply_num;
extern int rename_rmdir_noent_reply_num;
extern int rmdir_not_empty_reply_num;
extern int loose_access_control_reply_num;
extern int lookup_err_due_to_rename_num;
extern int lookup_err_due_to_parallel_remove_num;
extern int lookup_eaccess_enoent_mismatch_num;
extern int read_io_err_num;
extern int stale_fhandle_err_num;
extern int proper_reply_num;
extern int run_stage_proper_reply_num;
extern int lookup_retry_num;
extern int can_not_catch_speed_num_total;
extern int can_not_catch_speed_num;
extern int poll_timeout_0_num;
extern int poll_timeout_pos_num;
extern int abnormal_EEXIST_num;
extern int abnormal_ENOENT_num;

#define FIRST_STAGE 0
#define READ_DEP_TAB_STAGE 1
#define TRACE_PLAY_STAGE 2
extern int stage;

#define IGNORE_SETATTR_CTIME
//#define TAKE_CARE_SETATTR_GID
//#define TAKE_CARE_SETATTR_UID
//#define TAKE_CARE_CREATE_MODE_BY_DAN
//#define TAKE_CARE_OTHER_FAILED_COMMAND
//#define TAKE_CARE_CUSTOM_COMMAND
//#define TAKE_CARE_FSSTAT_COMMAND
//#define TAKE_CARE_UNLOOKED_UP_NON_NEW_FILES
//#define TAKE_CARE_NOEMPTY_RMDIR
//#define TAKE_CARE_LOOKUP_EACCESS_ENOENT_MISMATCH
//#define TAKE_CARE_ACCESS_ERROR
//#define TAKE_CARE_SYMBOLIC_LINK
#define TOLERANT_READ_IO_ERR
#define TOLERANT_STALE_FHANDLE_ERR
#define IO_THREAD
//#define RECV_THREAD	can not tune up the version with receive thread quickly

extern FILE * profile_fp;
extern profile_t total_profile;
extern profile_t valid_get_nextop_profile;
extern profile_t invalid_get_nextop_profile;
extern profile_t valid_poll_and_get_reply_profile;
extern profile_t invalid_poll_and_get_reply_profile;
extern profile_t execute_next_request_profile;
extern profile_t receive_next_reply_profile;
extern profile_t decode_reply_profile;
extern profile_t check_reply_profile;
extern profile_t add_create_object_profile;
extern profile_t prepare_argument_profile;
extern profile_t biod_clnt_call_profile;
extern profile_t check_timeout_profile;
extern profile_t adjust_play_window_profile;
extern profile_t fgets_profile;
extern profile_t read_line_profile;
extern profile_t read_trace_profile;

extern int skip_sec;
extern int trace_timestamp1, trace_timestamp2;
//#define SEQUEN_READ
//#define SEQUEN_READ_NUM 1000

#define TRACE_BUF_FULL 0
#define TRACE_FILE_END 1
#define ASYNC_RPC_SEM_KEY 60000
extern int disk_io_status;
extern int WARMUP_TIME;
extern int disk_index; 
//#define NO_SEM
#endif
