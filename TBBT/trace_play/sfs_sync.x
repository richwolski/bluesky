/*
 * SFS sync daemon rpcgen configuration file
 *	@(#)sfs_sync.x	2.1	97/10/23
 *
 * XXX Not currently used to generate source code
 */

const MAX_STR1_LEN = 31;
const MAX_STR2_LEN = 2560;

struct sync_string {
	int	clnt_id;			/* client number */
	string	clnt_type<MAX_STR1_LEN>;	/* message type, hard coded */
	string	clnt_transaction<MAX_STR1_LEN>;	/* transaction id */
	string	clnt_data<MAX_STR2_LEN>;	/* results strings */
};

program SFS {
	version SFS {
		int
		SIGNAL_NULLPROC (void) = 0;
		int
		SIGNAL_SFS (sync_string) = 1;
	} = 1;
} = 100500;
