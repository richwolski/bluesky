#include "rfs_c_def.h"
#include "generic_hash.h"
dep_tab_t dep_tab[DEP_TAB_SIZE];
int req_num_with_new_fh = 0;
int	req_num_with_discard_fh = 0;
int req_num_with_init_fh =0;

int event_order [EVENT_ORDER_SIZE];
int event_order_index = 0;

memory_trace_ent_t memory_trace[MAX_MEMORY_TRACE_LINES];
	
/* the offset between the replay time and timestamp in the log */
struct ladtime current;
#if	0
//struct ladtime trace_starttime = {1003636801, 39949, 0};
struct ladtime trace_starttime = {1003723201, 313373, 0};
#else
/* timestamp extracted from the used trace. G. Jason Peng */
//struct ladtime trace_starttime = {1005620400, 499221, 0};
/* timestamp extracted nfsdump.gzip.pair Ningning for osdi measurement */
struct ladtime trace_starttime = {1085067131, 107476, 0};
#endif
struct ladtime time_offset;

fh_map_t fh_map [FH_MAP_SIZE];
struct generic_entry * fh_htable[FH_HTABLE_SIZE];
int fh_i=0;
int fh_map_debug = 0;
struct generic_entry * fh_htable [FH_HTABLE_SIZE];

#ifdef notdef
/* the array is indexed by sfs operation number */
rfs_op_type rfs_Ops[TOTAL] = {
{NFSPROC3_NULL,		setbuf_void, 		setbuf_void, xdr_void, xdr_void},
{NFSPROC3_GETATTR, 	setarg_GETATTR3, 	setbuf_void, xdr_GETATTR3args, xdr_GETATTR3res},
{NFSPROC3_SETATTR, 	setarg_SETATTR3, 	setbuf_void, xdr_SETATTR3args, xdr_SETATTR3res},
{NFSPROC3_INVALID,	setbuf_invalid, 	setbuf_invalid, xdr_invalid, xdr_invalid},
{NFSPROC3_LOOKUP, 	setarg_LOOKUP3, 	setres_lookup, xdr_LOOKUP3args, xdr_LOOKUP3res},
{NFSPROC3_READLINK, setarg_READLINK3, 	setres_readlink, xdr_READLINK3args, xdr_READLINK3res},
{NFSPROC3_READ, 	setarg_READ3, 		setres_read, xdr_READ3args, xdr_READ3res},
{NFSPROC3_INVALID, 	setarg_invalid, 	setbuf_invalid, xdr_invalid, xdr_invalid},
{NFSPROC3_WRITE, 	setarg_WRITE3, 		setbuf_void, xdr_WRITE3args, xdr_WRITE3res},
{NFSPROC3_CREATE, 	setarg_CREATE3, 	setbuf_void, xdr_CREATE3args, xdr_CREATE3res},
{NFSPROC3_REMOVE, 	setarg_REMOVE3, 	setbuf_void, xdr_REMOVE3args, xdr_REMOVE3res},
{NFSPROC3_RENAME, 	setarg_RENAME3, 	setbuf_void, xdr_RENAME3args, xdr_RENAME3res},
{NFSPROC3_LINK, 	setarg_LINK3, 		setbuf_void, xdr_LINK3args, xdr_LINK3res},
{NFSPROC3_SYMLINK, 	setarg_SYMLINK3, 	setbuf_void, xdr_SYMLINK3args, xdr_SYMLINK3res},
{NFSPROC3_MKDIR, 	setarg_MKDIR3, 		setbuf_void, xdr_MKDIR3args, xdr_MKDIR3res},
{NFSPROC3_RMDIR, 	setarg_RMDIR3,		setbuf_void, xdr_RMDIR3args, xdr_RMDIR3res},
{NFSPROC3_READDIR, 	setarg_READDIR3, 	setres_readdir, xdr_READDIR3args, xdr_READDIR3res},
{NFSPROC3_FSSTAT, 	setarg_FSSTAT3, 	setbuf_void, xdr_FSSTAT3args, xdr_FSSTAT3res},
{NFSPROC3_ACCESS, 	setarg_ACCESS3, 	setbuf_void, xdr_ACCESS3args, xdr_ACCESS3res},
{NFSPROC3_COMMIT, 	setarg_COMMIT3, 	setbuf_void, xdr_COMMIT3args, xdr_COMMIT3res},
{NFSPROC3_FSINFO, 	setarg_FSINFO3,		setbuf_void,  xdr_FSINFO3args, xdr_FSINFO3res},
{NFSPROC3_MKNOD, 	setarg_MKNOD3, 		setbuf_void, xdr_MKNOD3args, xdr_MKNOD3res},
{NFSPROC3_PATHCONF, setarg_PATHCONF3, 	setbuf_void, xdr_PATHCONF3args, xdr_PATHCONF3res}
{NFSPROC3_READDIRPLUS, setarg_READDIRPLUS3, setres_readdirplus, xdr_READDIRPLUS3args, xdr_READDIRPLUS3res}};

/*
 * --------------------  NFS ops vector --------------------
 */
/*
 * per operation information
 */
sfs_op_type nfsv3_Ops[] = {

/* name        mix    op    call  no  req  req  req  results */
/*             pcnt  class  targ call pcnt cnt  targ         */

 { "null",        0, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "getattr",    11, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "setattr",     1, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "root",        0, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "lookup",     27, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "readlink",    7, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "read",       18, Read,    0,  0,  0.0,  0,   0,  { 0, }},
 { "wrcache",     0, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "write",       9, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "create",      1, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "remove",      1, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "rename",      0, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "link",        0, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "symlink",     0, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "mkdir",       0, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "rmdir",       0, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "readdir",     2, Read,    0,  0,  0.0,  0,   0,  { 0, }},
 { "fsstat",      1, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "access",      7, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "commit",      5, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "fsinfo",      1, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "mknod",       0, Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "pathconf",    0, Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "readdirplus", 9, Read,   0,  0,  0.0,  0,   0,  { 0, }},
 { "TOTAL",     100, Lookup,  0,  0,  0.0,  0,   0,  { 0, }}
};
#endif

sfs_op_type *Ops;

int num_out_reqs = 0;

cyclic_index_t dep_tab_index;
cyclic_index_t dep_window_index;
cyclic_index_t memory_trace_index;
int dep_window_max = 0;

/* note that for each dep_tab entry, there is a memory trace line, but
 * not vise vesa because some memory trace line may not have corresponding
 * dep_tab entry. According entry TIMESTAMP value, the order is
 *
 * memory_trace_tail line < dep_tab_tail entry < dep_window_max entry <
 * dep_tab_head entry < memory_trace_head entry */

int rfs_debug = 0;
int per_packet_debug = 0;
int adjust_play_window_debug = 0;
int dependency_debug = 0;
int profile_debug = 0;
int quiet_flag = 0;
int stage = FIRST_STAGE;
int read_data_owe = 0;
int read_data_total = 0;
int write_data_owe = 0;
int write_data_total = 0;
int read_data_adjust_times = 0;
int write_data_adjust_times = 0;
int read_data_owe_GB = 0;
int write_data_owe_GB = 0;
int read_data_total_GB = 0;
int write_data_total_GB = 0;

int failed_create_command_num = 0;
int failed_other_command_num = 0;
int skipped_readlink_command_num = 0;
int skipped_custom_command_num = 0;
int fh_path_map_err_num = 0;
int skipped_fsstat_command_num = 0;
int missing_reply_num = 0;
int rename_rmdir_noent_reply_num = 0;
int rmdir_not_empty_reply_num = 0;
int loose_access_control_reply_num = 0;
int lookup_err_due_to_rename_num = 0;
int lookup_err_due_to_parallel_remove_num = 0;
int lookup_eaccess_enoent_mismatch_num = 0;
int read_io_err_num = 0;
int stale_fhandle_err_num = 0;
int proper_reply_num = 0;
int run_stage_proper_reply_num = 0;
int lookup_retry_num = 0;
int can_not_catch_speed_num = 0;
int can_not_catch_speed_num_total = 0;
int poll_timeout_0_num = 0;
int poll_timeout_pos_num = 0;
int abnormal_EEXIST_num = 0;
int abnormal_ENOENT_num = 0;

FILE * profile_fp = 0;
profile_t total_profile;
profile_t valid_get_nextop_profile;
profile_t invalid_get_nextop_profile;
profile_t valid_poll_and_get_reply_profile;
profile_t invalid_poll_and_get_reply_profile;
profile_t execute_next_request_profile;
profile_t receive_next_reply_profile;
profile_t decode_reply_profile;
profile_t check_reply_profile;
profile_t add_create_object_profile;
profile_t prepare_argument_profile;
profile_t biod_clnt_call_profile;
profile_t check_timeout_profile;
profile_t adjust_play_window_profile;
profile_t fgets_profile;
profile_t read_line_profile;
profile_t read_trace_profile;

int PLAY_SCALE = 1;
int skip_sec = 0;
int trace_timestamp1=0, trace_timestamp2=0;

int disk_io_status = TRACE_BUF_FULL;
int WARMUP_TIME = 0;	/* other values that has been used: 100 */
int disk_index = -1;

int TRACE_COMMAND_REPLY_FLAG_POS=36;
int TRACE_VERSION_POS=37;
int TRACE_MSGID_POS=39;
int TRACE_FH_SIZE=64;
