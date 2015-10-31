#ifndef _NFS3_PROT_H_RPCGEN
#define _NFS3_PROT_H_RPCGEN

#include <rpc/rpc.h>

#include <glib.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef u_quad_t uint64;

typedef quad_t int64;

typedef u_int uint32;

typedef int int32;
#define NFS3_FHSIZE 64
#define NFS3_COOKIEVERFSIZE 8
#define NFS3_CREATEVERFSIZE 8
#define NFS3_WRITEVERFSIZE 8

extern char nfsd_instance_verf_cookie[NFS3_WRITEVERFSIZE];

typedef char *filename3;

typedef char *nfspath3;

typedef char cookieverf3[NFS3_COOKIEVERFSIZE];

typedef char createverf3[NFS3_CREATEVERFSIZE];

typedef char writeverf3[NFS3_WRITEVERFSIZE];

enum nfsstat3 {
	NFS3_OK = 0,
	NFS3ERR_PERM = 1,
	NFS3ERR_NOENT = 2,
	NFS3ERR_IO = 5,
	NFS3ERR_NXIO = 6,
	NFS3ERR_ACCES = 13,
	NFS3ERR_EXIST = 17,
	NFS3ERR_XDEV = 18,
	NFS3ERR_NODEV = 19,
	NFS3ERR_NOTDIR = 20,
	NFS3ERR_ISDIR = 21,
	NFS3ERR_INVAL = 22,
	NFS3ERR_FBIG = 27,
	NFS3ERR_NOSPC = 28,
	NFS3ERR_ROFS = 30,
	NFS3ERR_MLINK = 31,
	NFS3ERR_NAMETOOLONG = 63,
	NFS3ERR_NOTEMPTY = 66,
	NFS3ERR_DQUOT = 69,
	NFS3ERR_STALE = 70,
	NFS3ERR_REMOTE = 71,
	NFS3ERR_BADHANDLE = 10001,
	NFS3ERR_NOT_SYNC = 10002,
	NFS3ERR_BAD_COOKIE = 10003,
	NFS3ERR_NOTSUPP = 10004,
	NFS3ERR_TOOSMALL = 10005,
	NFS3ERR_SERVERFAULT = 10006,
	NFS3ERR_BADTYPE = 10007,
	NFS3ERR_JUKEBOX = 10008,
	NFS3ERR_FPRINTNOTFOUND = 10009,
	NFS3ERR_ABORTED = 10010,
};
typedef enum nfsstat3 nfsstat3;

enum ftype3 {
	NF3REG = 1,
	NF3DIR = 2,
	NF3BLK = 3,
	NF3CHR = 4,
	NF3LNK = 5,
	NF3SOCK = 6,
	NF3FIFO = 7,
};
typedef enum ftype3 ftype3;

struct specdata3 {
	uint32 major;
	uint32 minor;
};
typedef struct specdata3 specdata3;

struct nfs_fh3 {
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct nfs_fh3 nfs_fh3;

struct nfstime3 {
	uint32 seconds;
	uint32 nseconds;
};
typedef struct nfstime3 nfstime3;

struct fattr3 {
	ftype3 type;
	uint32 mode;
	uint32 nlink;
	uint32 uid;
	uint32 gid;
	uint64 size;
	uint64 used;
	specdata3 rdev;
	uint64 fsid;
	uint64 fileid;
	nfstime3 atime;
	nfstime3 mtime;
	nfstime3 ctime;
};
typedef struct fattr3 fattr3;

struct post_op_attr {
	bool_t present;
	union {
		fattr3 attributes;
	} post_op_attr_u;
};
typedef struct post_op_attr post_op_attr;

struct wcc_attr {
	uint64 size;
	nfstime3 mtime;
	nfstime3 ctime;
};
typedef struct wcc_attr wcc_attr;

struct pre_op_attr {
	bool_t present;
	union {
		wcc_attr attributes;
	} pre_op_attr_u;
};
typedef struct pre_op_attr pre_op_attr;

struct wcc_data {
	pre_op_attr before;
	post_op_attr after;
};
typedef struct wcc_data wcc_data;

struct post_op_fh3 {
	bool_t present;
	union {
		nfs_fh3 handle;
	} post_op_fh3_u;
};
typedef struct post_op_fh3 post_op_fh3;

struct set_uint32 {
	bool_t set;
	union {
		uint32 val;
	} set_uint32_u;
};
typedef struct set_uint32 set_uint32;

struct set_uint64 {
	bool_t set;
	union {
		uint64 val;
	} set_uint64_u;
};
typedef struct set_uint64 set_uint64;

enum time_how {
	DONT_CHANGE = 0,
	SET_TO_SERVER_TIME = 1,
	SET_TO_CLIENT_TIME = 2,
};
typedef enum time_how time_how;

struct set_time {
	time_how set;
	union {
		nfstime3 time;
	} set_time_u;
};
typedef struct set_time set_time;

struct sattr3 {
	set_uint32 mode;
	set_uint32 uid;
	set_uint32 gid;
	set_uint64 size;
	set_time atime;
	set_time mtime;
};
typedef struct sattr3 sattr3;

struct diropargs3 {
	nfs_fh3 dir;
	filename3 name;
};
typedef struct diropargs3 diropargs3;

struct diropres3ok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct diropres3ok diropres3ok;

struct diropres3 {
	nfsstat3 status;
	union {
		diropres3ok resok;
		wcc_data resfail;
	} diropres3_u;
};
typedef struct diropres3 diropres3;

struct wccstat3 {
	nfsstat3 status;
	union {
		wcc_data wcc;
	} wccstat3_u;
};
typedef struct wccstat3 wccstat3;

struct getattr3res {
	nfsstat3 status;
	union {
		fattr3 attributes;
	} getattr3res_u;
};
typedef struct getattr3res getattr3res;

struct sattrguard3 {
	bool_t check;
	union {
		nfstime3 ctime;
	} sattrguard3_u;
};
typedef struct sattrguard3 sattrguard3;

struct setattr3args {
	nfs_fh3 object;
	sattr3 new_attributes;
	sattrguard3 guard;
};
typedef struct setattr3args setattr3args;

struct lookup3resok {
	nfs_fh3 object;
	post_op_attr obj_attributes;
	post_op_attr dir_attributes;
};
typedef struct lookup3resok lookup3resok;

struct lookup3res {
	nfsstat3 status;
	union {
		lookup3resok resok;
		post_op_attr resfail;
	} lookup3res_u;
};
typedef struct lookup3res lookup3res;
#define ACCESS3_READ 0x0001
#define ACCESS3_LOOKUP 0x0002
#define ACCESS3_MODIFY 0x0004
#define ACCESS3_EXTEND 0x0008
#define ACCESS3_DELETE 0x0010
#define ACCESS3_EXECUTE 0x0020

struct access3args {
	nfs_fh3 object;
	uint32 access;
};
typedef struct access3args access3args;

struct access3resok {
	post_op_attr obj_attributes;
	uint32 access;
};
typedef struct access3resok access3resok;

struct access3res {
	nfsstat3 status;
	union {
		access3resok resok;
		post_op_attr resfail;
	} access3res_u;
};
typedef struct access3res access3res;

struct readlink3resok {
	post_op_attr symlink_attributes;
	nfspath3 data;
};
typedef struct readlink3resok readlink3resok;

struct readlink3res {
	nfsstat3 status;
	union {
		readlink3resok resok;
		post_op_attr resfail;
	} readlink3res_u;
};
typedef struct readlink3res readlink3res;

struct read3args {
	nfs_fh3 file;
	uint64 offset;
	uint32 count;
};
typedef struct read3args read3args;

struct read3resok {
	post_op_attr file_attributes;
	uint32 count;
	bool_t eof;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct read3resok read3resok;

struct read3res {
	nfsstat3 status;
	union {
		read3resok resok;
		post_op_attr resfail;
	} read3res_u;
};
typedef struct read3res read3res;

enum stable_how {
	UNSTABLE = 0,
	DATA_SYNC = 1,
	FILE_SYNC = 2,
};
typedef enum stable_how stable_how;

struct write3args {
	nfs_fh3 file;
	uint64 offset;
	uint32 count;
	stable_how stable;
	struct {
		u_int data_len;
		char *data_val;
	} data;
};
typedef struct write3args write3args;

struct write3resok {
	wcc_data file_wcc;
	uint32 count;
	stable_how committed;
	writeverf3 verf;
};
typedef struct write3resok write3resok;

struct write3res {
	nfsstat3 status;
	union {
		write3resok resok;
		wcc_data resfail;
	} write3res_u;
};
typedef struct write3res write3res;

enum createmode3 {
	UNCHECKED = 0,
	GUARDED = 1,
	EXCLUSIVE = 2,
};
typedef enum createmode3 createmode3;

struct createhow3 {
	createmode3 mode;
	union {
		sattr3 obj_attributes;
		createverf3 verf;
	} createhow3_u;
};
typedef struct createhow3 createhow3;

struct create3args {
	diropargs3 where;
	createhow3 how;
};
typedef struct create3args create3args;

struct mkdir3args {
	diropargs3 where;
	sattr3 attributes;
};
typedef struct mkdir3args mkdir3args;

struct symlinkdata3 {
	sattr3 symlink_attributes;
	nfspath3 symlink_data;
};
typedef struct symlinkdata3 symlinkdata3;

struct symlink3args {
	diropargs3 where;
	symlinkdata3 symlink;
};
typedef struct symlink3args symlink3args;

struct devicedata3 {
	sattr3 dev_attributes;
	specdata3 spec;
};
typedef struct devicedata3 devicedata3;

struct mknoddata3 {
	ftype3 type;
	union {
		devicedata3 device;
		sattr3 pipe_attributes;
	} mknoddata3_u;
};
typedef struct mknoddata3 mknoddata3;

struct mknod3args {
	diropargs3 where;
	mknoddata3 what;
};
typedef struct mknod3args mknod3args;

struct rename3args {
	diropargs3 from;
	diropargs3 to;
};
typedef struct rename3args rename3args;

struct rename3wcc {
	wcc_data fromdir_wcc;
	wcc_data todir_wcc;
};
typedef struct rename3wcc rename3wcc;

struct rename3res {
	nfsstat3 status;
	union {
		rename3wcc res;
	} rename3res_u;
};
typedef struct rename3res rename3res;

struct link3args {
	nfs_fh3 file;
	diropargs3 link;
};
typedef struct link3args link3args;

struct link3wcc {
	post_op_attr file_attributes;
	wcc_data linkdir_wcc;
};
typedef struct link3wcc link3wcc;

struct link3res {
	nfsstat3 status;
	union {
		link3wcc res;
	} link3res_u;
};
typedef struct link3res link3res;

struct readdir3args {
	nfs_fh3 dir;
	uint64 cookie;
	cookieverf3 cookieverf;
	uint32 count;
};
typedef struct readdir3args readdir3args;

struct entry3 {
	uint64 fileid;
	filename3 name;
	uint64 cookie;
	struct entry3 *nextentry;
};
typedef struct entry3 entry3;

struct dirlist3 {
	entry3 *entries;
	bool_t eof;
};
typedef struct dirlist3 dirlist3;

struct readdir3resok {
	post_op_attr dir_attributes;
	cookieverf3 cookieverf;
	dirlist3 reply;
};
typedef struct readdir3resok readdir3resok;

struct readdir3res {
	nfsstat3 status;
	union {
		readdir3resok resok;
		post_op_attr resfail;
	} readdir3res_u;
};
typedef struct readdir3res readdir3res;

struct readdirplus3args {
	nfs_fh3 dir;
	uint64 cookie;
	cookieverf3 cookieverf;
	uint32 dircount;
	uint32 maxcount;
};
typedef struct readdirplus3args readdirplus3args;

struct entryplus3 {
	uint64 fileid;
	filename3 name;
	uint64 cookie;
	post_op_attr name_attributes;
	post_op_fh3 name_handle;
	struct entryplus3 *nextentry;
};
typedef struct entryplus3 entryplus3;

struct dirlistplus3 {
	entryplus3 *entries;
	bool_t eof;
};
typedef struct dirlistplus3 dirlistplus3;

struct readdirplus3resok {
	post_op_attr dir_attributes;
	cookieverf3 cookieverf;
	dirlistplus3 reply;
};
typedef struct readdirplus3resok readdirplus3resok;

struct readdirplus3res {
	nfsstat3 status;
	union {
		readdirplus3resok resok;
		post_op_attr resfail;
	} readdirplus3res_u;
};
typedef struct readdirplus3res readdirplus3res;

struct fsstat3resok {
	post_op_attr obj_attributes;
	uint64 tbytes;
	uint64 fbytes;
	uint64 abytes;
	uint64 tfiles;
	uint64 ffiles;
	uint64 afiles;
	uint32 invarsec;
};
typedef struct fsstat3resok fsstat3resok;

struct fsstat3res {
	nfsstat3 status;
	union {
		fsstat3resok resok;
		post_op_attr resfail;
	} fsstat3res_u;
};
typedef struct fsstat3res fsstat3res;
#define FSF3_LINK 0x0001
#define FSF3_SYMLINK 0x0002
#define FSF3_HOMOGENEOUS 0x0008
#define FSF3_CANSETTIME 0x0010

struct fsinfo3resok {
	post_op_attr obj_attributes;
	uint32 rtmax;
	uint32 rtpref;
	uint32 rtmult;
	uint32 wtmax;
	uint32 wtpref;
	uint32 wtmult;
	uint32 dtpref;
	uint64 maxfilesize;
	nfstime3 time_delta;
	uint32 properties;
};
typedef struct fsinfo3resok fsinfo3resok;

struct fsinfo3res {
	nfsstat3 status;
	union {
		fsinfo3resok resok;
		post_op_attr resfail;
	} fsinfo3res_u;
};
typedef struct fsinfo3res fsinfo3res;

struct pathconf3resok {
	post_op_attr obj_attributes;
	uint32 linkmax;
	uint32 name_max;
	bool_t no_trunc;
	bool_t chown_restricted;
	bool_t case_insensitive;
	bool_t case_preserving;
};
typedef struct pathconf3resok pathconf3resok;

struct pathconf3res {
	nfsstat3 status;
	union {
		pathconf3resok resok;
		post_op_attr resfail;
	} pathconf3res_u;
};
typedef struct pathconf3res pathconf3res;

struct commit3args {
	nfs_fh3 file;
	uint64 offset;
	uint32 count;
};
typedef struct commit3args commit3args;

struct commit3resok {
	wcc_data file_wcc;
	writeverf3 verf;
};
typedef struct commit3resok commit3resok;

struct commit3res {
	nfsstat3 status;
	union {
		commit3resok resok;
		wcc_data resfail;
	} commit3res_u;
};
typedef struct commit3res commit3res;

/* Structure for tracking a single incoming TCP connection for RPCs.  For now,
 * used for NFS only. */
typedef struct {
    GIOChannel *channel;

    /* The reassembled message, thus far. */
    GString *msgbuf;

    /* Remaining number of bytes in this message fragment; 0 if we next expect
     * another fragment header. */
    uint32_t frag_len;

    /* If frag_len is zero: the number of bytes of the fragment header that
     * have been read so far. */
    int frag_hdr_bytes;

    /* Mutex protecting send operations on the socket (to ensure that replies
     * are not accidentally interleaved). */
    GMutex *send_lock;

    /* Is this a UDP connection? */
    gboolean udp_transport;

    /* For UDP connections only, the address of the sender. */
    struct sockaddr_in peer;

    /* For UDP only, a buffer for accumulating the full contents of a message
     * before it is sent */
    GString *sendbuf;
} RPCConnection;

/* Linked list of cleanup functions to call when a request is completed. */
struct cleanup_list {
    void (*func)(void *arg);
    void *arg;
    struct cleanup_list *next;
};

struct BlueSkyProfile;

/* Used to track a single outstanding RPC request.  Not all of the fields are
 * initially filled in, but more are filled in as the request is processed. */
typedef struct {
    /* The corresponding connection on which the request was made. */
    RPCConnection *connection;

    /* To track the time to complete this request and a timing breakdown. */
    struct BlueSkyProfile *profile;

    /* Transaction ID of the request, in host byte order. */
    uint32_t xid;

    /* Raw XDR arguments for the call, including headers (everything except the
     * fragment headers).  Also, the offset to the actual arguments (number of
     * bytes making up the headers). */
    GString *raw_args;
    size_t raw_args_header_bytes;

    /* Decoded header information. */
    int req_proc;

    /* The XDR-decoded argument of the call, and the procedure to use for
     * freeing these arguments.  The actual freeing is done automatically when
     * the response is sent. */
    void *args;
    xdrproc_t xdr_args_free;

    /* Procedure to be used for encoding the eventual return value into XDR. */
    xdrproc_t xdr_result;

    /* Functions to be called when the response is sent to clean up any
     * resources. */
    struct cleanup_list *cleanup;
} RPCRequest;

extern void async_rpc_send_reply(RPCRequest *req, void *result);

#define NFS_PROGRAM 100003
#define NFS_V3 3

#define NFSPROC3_NULL 0
extern void nfsproc3_null_3_svc(void *, RPCRequest *);
#define NFSPROC3_GETATTR 1
extern void nfsproc3_getattr_3_svc(nfs_fh3 *, RPCRequest *);
#define NFSPROC3_SETATTR 2
extern void nfsproc3_setattr_3_svc(setattr3args *, RPCRequest *);
#define NFSPROC3_LOOKUP 3
extern void nfsproc3_lookup_3_svc(diropargs3 *, RPCRequest *);
#define NFSPROC3_ACCESS 4
extern void nfsproc3_access_3_svc(access3args *, RPCRequest *);
#define NFSPROC3_READLINK 5
extern void nfsproc3_readlink_3_svc(nfs_fh3 *, RPCRequest *);
#define NFSPROC3_READ 6
extern void nfsproc3_read_3_svc(read3args *, RPCRequest *);
#define NFSPROC3_WRITE 7
extern void nfsproc3_write_3_svc(write3args *, RPCRequest *);
#define NFSPROC3_CREATE 8
extern void nfsproc3_create_3_svc(create3args *, RPCRequest *);
#define NFSPROC3_MKDIR 9
extern void nfsproc3_mkdir_3_svc(mkdir3args *, RPCRequest *);
#define NFSPROC3_SYMLINK 10
extern void nfsproc3_symlink_3_svc(symlink3args *, RPCRequest *);
#define NFSPROC3_MKNOD 11
extern void nfsproc3_mknod_3_svc(mknod3args *, RPCRequest *);
#define NFSPROC3_REMOVE 12
extern void nfsproc3_remove_3_svc(diropargs3 *, RPCRequest *);
#define NFSPROC3_RMDIR 13
extern void nfsproc3_rmdir_3_svc(diropargs3 *, RPCRequest *);
#define NFSPROC3_RENAME 14
extern void nfsproc3_rename_3_svc(rename3args *, RPCRequest *);
#define NFSPROC3_LINK 15
extern void nfsproc3_link_3_svc(link3args *, RPCRequest *);
#define NFSPROC3_READDIR 16
extern void nfsproc3_readdir_3_svc(readdir3args *, RPCRequest *);
#define NFSPROC3_READDIRPLUS 17
extern void nfsproc3_readdirplus_3_svc(readdirplus3args *, RPCRequest *);
#define NFSPROC3_FSSTAT 18
extern void nfsproc3_fsstat_3_svc(nfs_fh3 *, RPCRequest *);
#define NFSPROC3_FSINFO 19
extern void nfsproc3_fsinfo_3_svc(nfs_fh3 *, RPCRequest *);
#define NFSPROC3_PATHCONF 20
extern void nfsproc3_pathconf_3_svc(nfs_fh3 *, RPCRequest *);
#define NFSPROC3_COMMIT 21
extern void nfsproc3_commit_3_svc(commit3args *, RPCRequest *);

/* the xdr functions */

extern  bool_t xdr_uint64 (XDR *, uint64*);
extern  bool_t xdr_int64 (XDR *, int64*);
extern  bool_t xdr_uint32 (XDR *, uint32*);
extern  bool_t xdr_int32 (XDR *, int32*);
extern  bool_t xdr_filename3 (XDR *, filename3*);
extern  bool_t xdr_nfspath3 (XDR *, nfspath3*);
extern  bool_t xdr_cookieverf3 (XDR *, cookieverf3);
extern  bool_t xdr_createverf3 (XDR *, createverf3);
extern  bool_t xdr_writeverf3 (XDR *, writeverf3);
extern  bool_t xdr_nfsstat3 (XDR *, nfsstat3*);
extern  bool_t xdr_ftype3 (XDR *, ftype3*);
extern  bool_t xdr_specdata3 (XDR *, specdata3*);
extern  bool_t xdr_nfs_fh3 (XDR *, nfs_fh3*);
extern  bool_t xdr_nfstime3 (XDR *, nfstime3*);
extern  bool_t xdr_fattr3 (XDR *, fattr3*);
extern  bool_t xdr_post_op_attr (XDR *, post_op_attr*);
extern  bool_t xdr_wcc_attr (XDR *, wcc_attr*);
extern  bool_t xdr_pre_op_attr (XDR *, pre_op_attr*);
extern  bool_t xdr_wcc_data (XDR *, wcc_data*);
extern  bool_t xdr_post_op_fh3 (XDR *, post_op_fh3*);
extern  bool_t xdr_set_uint32 (XDR *, set_uint32*);
extern  bool_t xdr_set_uint64 (XDR *, set_uint64*);
extern  bool_t xdr_time_how (XDR *, time_how*);
extern  bool_t xdr_set_time (XDR *, set_time*);
extern  bool_t xdr_sattr3 (XDR *, sattr3*);
extern  bool_t xdr_diropargs3 (XDR *, diropargs3*);
extern  bool_t xdr_diropres3ok (XDR *, diropres3ok*);
extern  bool_t xdr_diropres3 (XDR *, diropres3*);
extern  bool_t xdr_wccstat3 (XDR *, wccstat3*);
extern  bool_t xdr_getattr3res (XDR *, getattr3res*);
extern  bool_t xdr_sattrguard3 (XDR *, sattrguard3*);
extern  bool_t xdr_setattr3args (XDR *, setattr3args*);
extern  bool_t xdr_lookup3resok (XDR *, lookup3resok*);
extern  bool_t xdr_lookup3res (XDR *, lookup3res*);
extern  bool_t xdr_access3args (XDR *, access3args*);
extern  bool_t xdr_access3resok (XDR *, access3resok*);
extern  bool_t xdr_access3res (XDR *, access3res*);
extern  bool_t xdr_readlink3resok (XDR *, readlink3resok*);
extern  bool_t xdr_readlink3res (XDR *, readlink3res*);
extern  bool_t xdr_read3args (XDR *, read3args*);
extern  bool_t xdr_read3resok (XDR *, read3resok*);
extern  bool_t xdr_read3res (XDR *, read3res*);
extern  bool_t xdr_stable_how (XDR *, stable_how*);
extern  bool_t xdr_write3args (XDR *, write3args*);
extern  bool_t xdr_write3resok (XDR *, write3resok*);
extern  bool_t xdr_write3res (XDR *, write3res*);
extern  bool_t xdr_createmode3 (XDR *, createmode3*);
extern  bool_t xdr_createhow3 (XDR *, createhow3*);
extern  bool_t xdr_create3args (XDR *, create3args*);
extern  bool_t xdr_mkdir3args (XDR *, mkdir3args*);
extern  bool_t xdr_symlinkdata3 (XDR *, symlinkdata3*);
extern  bool_t xdr_symlink3args (XDR *, symlink3args*);
extern  bool_t xdr_devicedata3 (XDR *, devicedata3*);
extern  bool_t xdr_mknoddata3 (XDR *, mknoddata3*);
extern  bool_t xdr_mknod3args (XDR *, mknod3args*);
extern  bool_t xdr_rename3args (XDR *, rename3args*);
extern  bool_t xdr_rename3wcc (XDR *, rename3wcc*);
extern  bool_t xdr_rename3res (XDR *, rename3res*);
extern  bool_t xdr_link3args (XDR *, link3args*);
extern  bool_t xdr_link3wcc (XDR *, link3wcc*);
extern  bool_t xdr_link3res (XDR *, link3res*);
extern  bool_t xdr_readdir3args (XDR *, readdir3args*);
extern  bool_t xdr_entry3 (XDR *, entry3*);
extern  bool_t xdr_dirlist3 (XDR *, dirlist3*);
extern  bool_t xdr_readdir3resok (XDR *, readdir3resok*);
extern  bool_t xdr_readdir3res (XDR *, readdir3res*);
extern  bool_t xdr_readdirplus3args (XDR *, readdirplus3args*);
extern  bool_t xdr_entryplus3 (XDR *, entryplus3*);
extern  bool_t xdr_dirlistplus3 (XDR *, dirlistplus3*);
extern  bool_t xdr_readdirplus3resok (XDR *, readdirplus3resok*);
extern  bool_t xdr_readdirplus3res (XDR *, readdirplus3res*);
extern  bool_t xdr_fsstat3resok (XDR *, fsstat3resok*);
extern  bool_t xdr_fsstat3res (XDR *, fsstat3res*);
extern  bool_t xdr_fsinfo3resok (XDR *, fsinfo3resok*);
extern  bool_t xdr_fsinfo3res (XDR *, fsinfo3res*);
extern  bool_t xdr_pathconf3resok (XDR *, pathconf3resok*);
extern  bool_t xdr_pathconf3res (XDR *, pathconf3res*);
extern  bool_t xdr_commit3args (XDR *, commit3args*);
extern  bool_t xdr_commit3resok (XDR *, commit3resok*);
extern  bool_t xdr_commit3res (XDR *, commit3res*);

extern void xdr_string_create(XDR *xdrs, GString *string, enum xdr_op op);

#ifdef __cplusplus
}
#endif

#endif /* !_NFS3_PROT_H_RPCGEN */
