#ifndef __sfs_c_nfs_h
#define __sfs_c_nfs_h

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
 * @(#)sfs_c_nfs.h	2.1	97/10/23
 *
 * -------------------------- sfs_c_nfs.h -------------------------
 *
 *	Literals and types for the NFS protocol, Version 2.
 *
 *.Revision_History
 *
 *	Richard Bean	17-May-90	Created.
 */


/*
 * for RPC calls
 */
/* #define NFS_PORT	2049 */
#define NFS_PROGRAM	((uint32_t)100003)
#define NFS_VERSION	((uint32_t)2)
#define NFS_V3		((uint32_t)3)

/*
 * fixed sizes
 */
#define NFS_MAXDATA	8192
//#define NFS_MAXDATA	16384
#define NFS_MAXPATHLEN	1024
#define NFS_MAXNAMLEN	255
#define NFS_COOKIESIZE	4
#define NFS_FHSIZE	32


#if !defined(AIX)
typedef struct {
	char	data[NFS_FHSIZE];
} fhandle_t;
#endif  /* AIX  */

/*
 * Definition of "standard" mount parameters
 */
#define MOUNTPROG	100005
#define MOUNTVERS	1
#define MOUNTPROC_MNT	1

struct fhstatus {
        int fhs_status;
        fhandle_t fhs_fh;
};

typedef fhandle_t nfs_fh;

/*
 * fattr modes
 */
/*
 * The mode mask is used to mask off server vendor specific mode bits
 * from the mode field.  This allows clients from different vendors to
 * validate servers from different vendors.
 */
#define NFSMODE_MASK 0177777
#define NFSMODE_FMT 0170000
#define NFSMODE_DIR 0040000
#define NFSMODE_CHR 0020000
#define NFSMODE_BLK 0060000
#define NFSMODE_REG 0100000
#define NFSMODE_LNK 0120000
#define NFSMODE_SOCK 0140000
#define NFSMODE_FIFO 0010000


/*
 * NFS Procedures
 */
#define NFSPROC_NULL		0
#define NFSPROC_GETATTR		1
#define NFSPROC_SETATTR		2
#define NFSPROC_ROOT		3
#define NFSPROC_LOOKUP		4
#define NFSPROC_READLINK	5
#define NFSPROC_READ		6
#define NFSPROC_WRITECACHE	7
#define NFSPROC_WRITE		8
#define NFSPROC_CREATE		9
#define NFSPROC_REMOVE		10
#define NFSPROC_RENAME		11
#define NFSPROC_LINK		12
#define NFSPROC_SYMLINK		13
#define NFSPROC_MKDIR		14
#define NFSPROC_RMDIR		15
#define NFSPROC_READDIR		16
#define NFSPROC_STATFS		17
#define NFS_PROCEDURE_COUNT	(NFSPROC_STATFS + 1)


/*
 * call and return types
 */
enum nfsstat {
	NFS_OK =		0,
	NFSERR_PERM =		1,
	NFSERR_NOENT =		2,
	NFSERR_IO =		5,
	NFSERR_NXIO =		6,
	NFSERR_ACCES =		13,
	NFSERR_EXIST =		17,
	NFSERR_XDEV =		18,
	NFSERR_NODEV =		19,
	NFSERR_NOTDIR =		20,
	NFSERR_ISDIR =		21,
	NFSERR_INVAL =		22,
	NFSERR_FBIG =		27,
	NFSERR_NOSPC =		28,
	NFSERR_ROFS =		30,
	NFSERR_OPNOTSUPP =	45,
	NFSERR_NAMETOOLONG =	63,
	NFSERR_NOTEMPTY =	66,
	NFSERR_DQUOT =		69,
	NFSERR_STALE =		70,
	NFSERR_REMOTE =		71,
	NFSERR_WFLUSH =		99
};
typedef enum nfsstat nfsstat;


enum ftype {
	NFNON = 0,
	NFREG = 1,
	NFDIR = 2,
	NFBLK = 3,
	NFCHR = 4,
	NFLNK = 5,
	NFSOCK = 6,
	NFBAD = 7,
	NFFIFO = 8
};
typedef enum ftype ftype;
#define NUM_TYPES 5


struct nfstime {
	unsigned int seconds;
	unsigned int useconds;
};
typedef struct nfstime nfstime;


struct fattr {
	ftype type;
	unsigned int mode;
	unsigned int nlink;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	unsigned int blocksize;
	unsigned int rdev;
	unsigned int blocks;
	unsigned int fsid;
	unsigned int fileid;
	nfstime atime;
	nfstime mtime;
	nfstime ctime;
};
typedef struct fattr fattr;


struct sattr {
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	nfstime atime;
	nfstime mtime;
};
typedef struct sattr sattr;


typedef char *filename;


typedef char *nfspath;


struct attrstat {
	nfsstat status;
	union {
		fattr attributes;
	} attrstat_u;
};
typedef struct attrstat attrstat;


struct sattrargs {
	nfs_fh file;
	sattr attributes;
};
typedef struct sattrargs sattrargs;


struct diropargs {
	nfs_fh dir;
	filename name;
};
typedef struct diropargs diropargs;


struct diropokres {
	nfs_fh file;
	fattr attributes;
};
typedef struct diropokres diropokres;


struct diropres {
	nfsstat status;
	union {
		diropokres diropres;
	} diropres_u;
};
typedef struct diropres diropres;


struct readlinkres {
	nfsstat status;
	struct {
		nfspath data;
		int len;	/* for convenience only, not in the protocol */
	} readlinkres_u;
};
typedef struct readlinkres readlinkres;


struct readargs {
	nfs_fh file;
	unsigned int offset;
	unsigned int count;
	unsigned int totalcount;	/* unused field, but in the protocol */
};
typedef struct readargs readargs;


struct readokres {
	fattr attributes;
	struct {
		unsigned int data_len;
		char *data_val;
	} data;
};
typedef struct readokres readokres;


struct readres {
	nfsstat status;
	union {
		readokres reply;
	} readres_u;
};
typedef struct readres readres;


struct writeargs {
	nfs_fh file;
	unsigned int beginoffset;	/* unused field, but in the protocol */
	unsigned int offset;
	unsigned int totalcount;	/* unused field, but in the protocol */
	struct {
		unsigned int data_len;
		char *data_val;
	} data;
};
typedef struct writeargs writeargs;


struct createargs {
	diropargs where;
	sattr attributes;
};
typedef struct createargs createargs;


struct renameargs {
	diropargs from;
	diropargs to;
};
typedef struct renameargs renameargs;


struct linkargs {
	nfs_fh from;
	diropargs to;
};
typedef struct linkargs linkargs;


struct symlinkargs {
	diropargs from;
	nfspath to;
	sattr attributes;
};
typedef struct symlinkargs symlinkargs;


struct mkdirargs {
	diropargs where;
	sattr attributes;
};
typedef struct mkdirargs mkdirargs;


typedef char nfscookie[NFS_COOKIESIZE];


struct readdirargs {
	nfs_fh dir;
	nfscookie cookie;
	unsigned int count;
};
typedef struct readdirargs readdirargs;


struct entry {
	bool_t valid;	/* bool for entry is present */
	unsigned int fileid;
	uint16_t name_len;
	filename name;
	nfscookie cookie;
};
typedef struct entry entry;


struct dirlist {
	int max_entries;	/* for convenience only, not in the protocol */
	entry *entries;		/* a stream of consecutive entry's */
	bool_t eof;
};
typedef struct dirlist dirlist;


struct readdirres {
	nfsstat status;
	union {
		dirlist reply;
	} readdirres_u;
};
typedef struct readdirres readdirres;


struct statfsokres {
	unsigned int tsize;
	unsigned int bsize;
	unsigned int blocks;
	unsigned int bfree;
	unsigned int bavail;
};
typedef struct statfsokres statfsokres;


struct statfsres {
	nfsstat status;
	union {
		statfsokres reply;
	} statfsres_u;
};
typedef struct statfsres statfsres;


/*
 *	Literals and types for the NFS protocol, Version 3.
 */


/*
 * for RPC calls
 */

/*
 * fixed sizes
 */
#define NFS3_FHSIZE	64
#define NFS3_COOKIEVERFSIZE	8
#define NFS3_CREATEVERFSIZE	8
#define NFS3_WRITEVERFSIZE	8

#define nfs3nametoolong	((char *)-1)

/*
 * Not all systems have a built in long long type so we define a
 * special version for sfs to use.
 */
typedef union {
	struct {
		uint32_t _u;
		uint32_t _l;
	} _p;
	char _f[8];
} nfs_uint64_t;

typedef char *filename3;

typedef char *nfspath3;

typedef char cookieverf3[NFS3_COOKIEVERFSIZE];

typedef char createverf3[NFS3_CREATEVERFSIZE];

typedef char writeverf3[NFS3_WRITEVERFSIZE];

struct nfs_fh3 {
	unsigned int fh3_length;
	union nfs_fh3_u {
		struct {
			char _u[NFS_FHSIZE];
			char _l[NFS_FHSIZE];
		} _p;
		char data[NFS3_FHSIZE];
	} fh3_u;
};
#define fh3_fsid	fh3_u.nfs_fh3_i.fh3_i.fh_fsid
#define fh3_len		fh3_u.nfs_fh3_i.fh3_i.fh_len /* fid length */
#define fh3_data	fh3_u.nfs_fh3_i.fh3_i.fh_data /* fid bytes */
#define fh3_xlen	fh3_u.nfs_fh3_i.fh3_i.fh_xlen
#define fh3_xdata	fh3_u.nfs_fh3_i.fh3_i.fh_xdata
typedef struct nfs_fh3 nfs_fh3;

struct diropargs3 {
	nfs_fh3 dir;
	filename3 name;
};
typedef struct diropargs3 diropargs3;

struct nfstime3 {
	uint32_t seconds;
	uint32_t nseconds;
};
typedef struct nfstime3 nfstime3;

struct specdata3 {
	uint32_t specdata1;
	uint32_t specdata2;
};
typedef struct specdata3 specdata3;


/*
 * call and return types
 */
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
	NFS3ERR_RFS_TIMEOUT = 10009,			// RFS
	NFS3ERR_RFS_MISS = 10010		// RFS reply missed in trace
};
typedef enum nfsstat3 nfsstat3;

enum ftype3 {
	NF3NON = 0,
	NF3REG = 1,
	NF3DIR = 2,
	NF3BLK = 3,
	NF3CHR = 4,
	NF3LNK = 5,
	NF3SOCK = 6,
	NF3FIFO = 7
};
typedef enum ftype3 ftype3;
#define NUM_TYPES 5


struct fattr3 {
	ftype3		type;
	uint32_t		mode;
	uint32_t		nlink;
	uint32_t		uid;
	uint32_t		gid;
	nfs_uint64_t	size;
	nfs_uint64_t	used;
	specdata3	rdev;
	nfs_uint64_t	fsid;
	nfs_uint64_t	fileid;
	nfstime3	atime;
	nfstime3	mtime;
	nfstime3	ctime;
};
typedef struct fattr3 fattr3;

struct post_op_attr {
	bool_t	attributes;
	fattr3	attr;
};
typedef struct post_op_attr post_op_attr;

struct wcc_attr {
	nfs_uint64_t	size;
	nfstime3	mtime;
	nfstime3	ctime;
};
typedef struct wcc_attr wcc_attr;

struct pre_op_attr {
	bool_t		attributes;
	wcc_attr	attr;
};
typedef struct pre_op_attr pre_op_attr;

struct wcc_data {
	pre_op_attr	before;
	post_op_attr	after;
};
typedef struct wcc_data wcc_data;

struct post_op_fh3 {
	bool_t	handle_follows;
	nfs_fh3 handle;
};
typedef struct post_op_fh3 post_op_fh3;

enum time_how {
	DONT_CHANGE = 0,
	SET_TO_SERVER_TIME = 1,
	SET_TO_CLIENT_TIME = 2
};
typedef enum time_how time_how;

struct set_mode3 {
	bool_t set_it;
	uint32_t mode;
};
typedef struct set_mode3 set_mode3;

struct set_uid3 {
	bool_t set_it;
	uint32_t uid;
};
typedef struct set_uid3 set_uid3;
 
struct set_gid3 {
	bool_t set_it;
	uint32_t gid;
};
typedef struct set_gid3 set_gid3;
 
struct set_size3 {
	bool_t		set_it;
	nfs_uint64_t	size;
};
typedef struct set_size3 set_size3;
 
struct set_atime {
	time_how set_it;
	nfstime3 atime;
};
typedef struct set_atime set_atime;
 
struct set_mtime {
	time_how set_it;
	nfstime3 mtime;
};
typedef struct set_mtime set_mtime;
 
struct sattr3 {
	set_mode3	mode;
	set_uid3	uid;
	set_gid3	gid;
	set_size3	size;
	set_atime	atime;
	set_mtime	mtime;
};
typedef struct sattr3 sattr3;


#define resok   res_u.ok
#define resfail res_u.fail

struct GETATTR3args {
	nfs_fh3 object;
};
typedef struct GETATTR3args GETATTR3args;

struct GETATTR3resok {
	fattr3 obj_attributes;
};
typedef struct GETATTR3resok GETATTR3resok;

struct GETATTR3res {
	nfsstat3 status;
	union {
		GETATTR3resok ok;
	} res_u;
};
typedef struct GETATTR3res GETATTR3res;

struct sattrguard3 {
	bool_t		check;
	nfstime3	obj_ctime;
};
typedef struct sattrguard3 sattrguard3;

struct SETATTR3args {
	nfs_fh3		object;
	sattr3		new_attributes;
	sattrguard3	guard;
};
typedef struct SETATTR3args SETATTR3args;

struct SETATTR3resok {
	wcc_data obj_wcc;
};
typedef struct SETATTR3resok SETATTR3resok;

struct SETATTR3resfail {
	wcc_data obj_wcc;
};
typedef struct SETATTR3resfail SETATTR3resfail;

struct SETATTR3res {
	nfsstat3 status;
	union {
		SETATTR3resok	ok;
		SETATTR3resfail fail;
	} res_u;
};
typedef struct SETATTR3res SETATTR3res;

struct LOOKUP3args {
	diropargs3 what;
};
typedef struct LOOKUP3args LOOKUP3args;

struct LOOKUP3resok {
	nfs_fh3		object;
	post_op_attr	obj_attributes;
	post_op_attr	dir_attributes;
};
typedef struct LOOKUP3resok LOOKUP3resok;

struct LOOKUP3resfail {
	post_op_attr dir_attributes;
};
typedef struct LOOKUP3resfail LOOKUP3resfail;

struct LOOKUP3res {
	nfsstat3 status;
	union {
		LOOKUP3resok	ok;
		LOOKUP3resfail	fail;
	} res_u;
};
typedef struct LOOKUP3res LOOKUP3res;

struct ACCESS3args {
	nfs_fh3	object;
	uint32_t	access;
};
typedef struct ACCESS3args ACCESS3args;
#define ACCESS3_READ	0x1
#define ACCESS3_LOOKUP	0x2
#define ACCESS3_MODIFY	0x4
#define ACCESS3_EXTEND	0x8
#define ACCESS3_DELETE	0x10
#define ACCESS3_EXECUTE 0x20

struct ACCESS3resok {
	post_op_attr obj_attributes;
	uint32_t access;
};
typedef struct ACCESS3resok ACCESS3resok;

struct ACCESS3resfail {
	post_op_attr obj_attributes;
};
typedef struct ACCESS3resfail ACCESS3resfail;

struct ACCESS3res {
	nfsstat3 status;
	union {
		ACCESS3resok	ok;
		ACCESS3resfail	fail;
	} res_u;
};
typedef struct ACCESS3res ACCESS3res;

struct READLINK3args {
	nfs_fh3 symlink;
};
typedef struct READLINK3args READLINK3args;

struct READLINK3resok {
	post_op_attr	symlink_attributes;
	nfspath3	data;
};
typedef struct READLINK3resok READLINK3resok;

struct READLINK3resfail {
	post_op_attr symlink_attributes;
};
typedef struct READLINK3resfail READLINK3resfail;

struct READLINK3res {
	nfsstat3 status;
	union {
		READLINK3resok		ok;
		READLINK3resfail	fail;
	} res_u;
};
typedef struct READLINK3res READLINK3res;

struct READ3args {
	nfs_fh3		file;
	nfs_uint64_t	offset;
	uint32_t		count;
};
typedef struct READ3args READ3args;

struct READ3resok {
	post_op_attr file_attributes;
	uint32_t count;
	bool_t eof;
	struct {
		unsigned int data_len;
		char *data_val;
	} data;
	unsigned int size;
};
typedef struct READ3resok READ3resok;

struct READ3resfail {
	post_op_attr file_attributes;
};
typedef struct READ3resfail READ3resfail;

struct READ3res {
	nfsstat3 status;
	union {
		READ3resok ok;
		READ3resfail fail;
	} res_u;
};
typedef struct READ3res READ3res;

enum stable_how {
	UNSTABLE = 0,
	DATA_SYNC = 1,
	FILE_SYNC = 2
};
typedef enum stable_how stable_how;

struct WRITE3args {
	nfs_fh3 file;
	nfs_uint64_t offset;
	uint32_t count;
	stable_how stable;
	struct {
		unsigned int data_len;
		char *data_val;
	} data;
};
typedef struct WRITE3args WRITE3args;

struct WRITE3resok {
	wcc_data file_wcc;
	uint32_t count;
	stable_how committed;
	writeverf3 verf;
};
typedef struct WRITE3resok WRITE3resok;

struct WRITE3resfail {
	wcc_data file_wcc;
};
typedef struct WRITE3resfail WRITE3resfail;

struct WRITE3res {
	nfsstat3 status;
	union {
		WRITE3resok ok;
		WRITE3resfail fail;
	} res_u;
};
typedef struct WRITE3res WRITE3res;

enum createmode3 {
	UNCHECKED = 0,
	GUARDED = 1,
	EXCLUSIVE = 2
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

struct CREATE3args {
	diropargs3 where;
	createhow3 how;
};
typedef struct CREATE3args CREATE3args;

struct CREATE3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct CREATE3resok CREATE3resok;

struct CREATE3resfail {
	wcc_data dir_wcc;
};
typedef struct CREATE3resfail CREATE3resfail;

struct CREATE3res {
	nfsstat3 status;
	union {
		CREATE3resok ok;
		CREATE3resfail fail;
	} res_u;
};
typedef struct CREATE3res CREATE3res;

struct MKDIR3args {
	diropargs3 where;
	sattr3 attributes;
};
typedef struct MKDIR3args MKDIR3args;

struct MKDIR3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct MKDIR3resok MKDIR3resok;

struct MKDIR3resfail {
	wcc_data dir_wcc;
};
typedef struct MKDIR3resfail MKDIR3resfail;

struct MKDIR3res {
	nfsstat3 status;
	union {
		MKDIR3resok ok;
		MKDIR3resfail fail;
	} res_u;
};
typedef struct MKDIR3res MKDIR3res;

struct symlinkdata3 {
	sattr3 symlink_attributes;
	nfspath3 symlink_data;
};
typedef struct symlinkdata3 symlinkdata3;

struct SYMLINK3args {
	diropargs3 where;
	symlinkdata3 symlink;
};
typedef struct SYMLINK3args SYMLINK3args;

struct SYMLINK3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct SYMLINK3resok SYMLINK3resok;

struct SYMLINK3resfail {
	wcc_data dir_wcc;
};
typedef struct SYMLINK3resfail SYMLINK3resfail;

struct SYMLINK3res {
	nfsstat3 status;
	union {
		SYMLINK3resok ok;
		SYMLINK3resfail fail;
	} res_u;
};
typedef struct SYMLINK3res SYMLINK3res;

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

struct MKNOD3args {
	diropargs3 where;
	mknoddata3 what;
};
typedef struct MKNOD3args MKNOD3args;

struct MKNOD3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct MKNOD3resok MKNOD3resok;

struct MKNOD3resfail {
	wcc_data dir_wcc;
};
typedef struct MKNOD3resfail MKNOD3resfail;

struct MKNOD3res {
	nfsstat3 status;
	union {
		MKNOD3resok ok;
		MKNOD3resfail fail;
	} res_u;
};
typedef struct MKNOD3res MKNOD3res;

struct REMOVE3args {
	diropargs3 object;
};
typedef struct REMOVE3args REMOVE3args;

struct REMOVE3resok {
	wcc_data dir_wcc;
};
typedef struct REMOVE3resok REMOVE3resok;

struct REMOVE3resfail {
	wcc_data dir_wcc;
};
typedef struct REMOVE3resfail REMOVE3resfail;

struct REMOVE3res {
	nfsstat3 status;
	union {
		REMOVE3resok ok;
		REMOVE3resfail fail;
	} res_u;
};
typedef struct REMOVE3res REMOVE3res;

struct RMDIR3args {
	diropargs3 object;
};
typedef struct RMDIR3args RMDIR3args;

struct RMDIR3resok {
	wcc_data dir_wcc;
};
typedef struct RMDIR3resok RMDIR3resok;

struct RMDIR3resfail {
	wcc_data dir_wcc;
};
typedef struct RMDIR3resfail RMDIR3resfail;

struct RMDIR3res {
	nfsstat3 status;
	union {
		RMDIR3resok ok;
		RMDIR3resfail fail;
	} res_u;
};
typedef struct RMDIR3res RMDIR3res;

struct RENAME3args {
	diropargs3 from;
	diropargs3 to;
};
typedef struct RENAME3args RENAME3args;

struct RENAME3resok {
	wcc_data fromdir_wcc;
	wcc_data todir_wcc;
};
typedef struct RENAME3resok RENAME3resok;

struct RENAME3resfail {
	wcc_data fromdir_wcc;
	wcc_data todir_wcc;
};
typedef struct RENAME3resfail RENAME3resfail;

struct RENAME3res {
	nfsstat3 status;
	union {
		RENAME3resok ok;
		RENAME3resfail fail;
	} res_u;
};
typedef struct RENAME3res RENAME3res;

struct LINK3args {
	nfs_fh3 file;
	diropargs3 link;
};
typedef struct LINK3args LINK3args;

struct LINK3resok {
	post_op_attr file_attributes;
	wcc_data linkdir_wcc;
};
typedef struct LINK3resok LINK3resok;

struct LINK3resfail {
	post_op_attr file_attributes;
	wcc_data linkdir_wcc;
};
typedef struct LINK3resfail LINK3resfail;

struct LINK3res {
	nfsstat3 status;
	union {
		LINK3resok ok;
		LINK3resfail fail;
	} res_u;
};
typedef struct LINK3res LINK3res;

struct READDIR3args {
	nfs_fh3 dir;
	nfs_uint64_t cookie;
	cookieverf3 cookieverf;
	uint32_t count;
};
typedef struct READDIR3args READDIR3args;

struct entry3 {
	nfs_uint64_t fileid;
	char name[SFS_MAXNAMLEN];
	nfs_uint64_t cookie;
};
typedef struct entry3 entry3;

struct dirlist3 {
	entry3 *entries;
	bool_t eof;
};
typedef struct dirlist3 dirlist3;

struct READDIR3resok {
	post_op_attr dir_attributes;
	cookieverf3 cookieverf;
	dirlist3 reply;
	unsigned int size;
	unsigned int count;
	nfs_uint64_t cookie;
};
typedef struct READDIR3resok READDIR3resok;

struct READDIR3resfail {
	post_op_attr dir_attributes;
};
typedef struct READDIR3resfail READDIR3resfail;

struct READDIR3res {
	nfsstat3 status;
	union {
		READDIR3resok ok;
		READDIR3resfail fail;
	} res_u;
};
typedef struct READDIR3res READDIR3res;

struct READDIRPLUS3args {
	nfs_fh3 dir;
	nfs_uint64_t cookie;
	cookieverf3 cookieverf;
	uint32_t dircount;
	uint32_t maxcount;
};
typedef struct READDIRPLUS3args READDIRPLUS3args;

struct entryplus3 {
	nfs_uint64_t fileid;
	char name[SFS_MAXNAMLEN];
	nfs_uint64_t cookie;
	post_op_attr name_attributes;
	post_op_fh3 name_handle;
};
typedef struct entryplus3 entryplus3;

struct dirlistplus3 {
	entryplus3 *entries;
	bool_t eof;
};
typedef struct dirlistplus3 dirlistplus3;

struct READDIRPLUS3resok {
	post_op_attr dir_attributes;
	cookieverf3 cookieverf;
	dirlistplus3 reply;
	unsigned int size;
	unsigned int count;
	unsigned int maxcount;
	post_op_attr *attributes;
	post_op_fh3 *handles;
};
typedef struct READDIRPLUS3resok READDIRPLUS3resok;

struct READDIRPLUS3resfail {
	post_op_attr dir_attributes;
};
typedef struct READDIRPLUS3resfail READDIRPLUS3resfail;

struct READDIRPLUS3res {
	nfsstat3 status;
	union {
		READDIRPLUS3resok ok;
		READDIRPLUS3resfail fail;
	} res_u;
};
typedef struct READDIRPLUS3res READDIRPLUS3res;

struct FSSTAT3args {
	nfs_fh3 fsroot;
};
typedef struct FSSTAT3args FSSTAT3args;

struct FSSTAT3resok {
	post_op_attr obj_attributes;
	nfs_uint64_t tbytes;
	nfs_uint64_t fbytes;
	nfs_uint64_t abytes;
	nfs_uint64_t tfiles;
	nfs_uint64_t ffiles;
	nfs_uint64_t afiles;
	uint32_t invarsec;
};
typedef struct FSSTAT3resok FSSTAT3resok;

struct FSSTAT3resfail {
	post_op_attr obj_attributes;
};
typedef struct FSSTAT3resfail FSSTAT3resfail;

struct FSSTAT3res {
	nfsstat3 status;
	union {
		FSSTAT3resok ok;
		FSSTAT3resfail fail;
	} res_u;
};
typedef struct FSSTAT3res FSSTAT3res;

struct FSINFO3args {
	nfs_fh3 fsroot;
};
typedef struct FSINFO3args FSINFO3args;

struct FSINFO3resok {
	post_op_attr obj_attributes;
	uint32_t rtmax;
	uint32_t rtpref;
	uint32_t rtmult;
	uint32_t wtmax;
	uint32_t wtpref;
	uint32_t wtmult;
	uint32_t dtpref;
	nfs_uint64_t maxfilesize;
	nfstime3 time_delta;
	uint32_t properties;
};
typedef struct FSINFO3resok FSINFO3resok;

struct FSINFO3resfail {
	post_op_attr obj_attributes;
};
typedef struct FSINFO3resfail FSINFO3resfail;
#define FSF3_LINK 0x1
#define FSF3_SYMLINK 0x2
#define FSF3_HOMOGENEOUS 0x8
#define FSF3_CANSETTIME 0x10

struct FSINFO3res {
	nfsstat3 status;
	union {
		FSINFO3resok ok;
		FSINFO3resfail fail;
	} res_u;
};
typedef struct FSINFO3res FSINFO3res;

struct PATHCONF3args {
	nfs_fh3 object;
};
typedef struct PATHCONF3args PATHCONF3args;

struct PATHCONF3resok {
	post_op_attr obj_attributes;
	uint32_t link_max;
	uint32_t name_max;
	bool_t no_trunc;
	bool_t chown_restricted;
	bool_t case_insensitive;
	bool_t case_preserving;
};
typedef struct PATHCONF3resok PATHCONF3resok;

struct PATHCONF3resfail {
	post_op_attr obj_attributes;
};
typedef struct PATHCONF3resfail PATHCONF3resfail;

struct PATHCONF3res {
	nfsstat3 status;
	union {
		PATHCONF3resok ok;
		PATHCONF3resfail fail;
	} res_u;
};
typedef struct PATHCONF3res PATHCONF3res;

struct COMMIT3args {
	nfs_fh3 file;
	nfs_uint64_t offset;
	uint32_t count;
};
typedef struct COMMIT3args COMMIT3args;

struct COMMIT3resok {
	wcc_data file_wcc;
	writeverf3 verf;
};
typedef struct COMMIT3resok COMMIT3resok;

struct COMMIT3resfail {
	wcc_data file_wcc;
};
typedef struct COMMIT3resfail COMMIT3resfail;

struct COMMIT3res {
	nfsstat3 status;
	union {
		COMMIT3resok ok;
		COMMIT3resfail fail;
	} res_u;
};
typedef struct COMMIT3res COMMIT3res;

/*
 * NFS Procedures
 */
#define NFSPROC3_NULL		0
#define NFSPROC3_GETATTR	1
#define NFSPROC3_SETATTR	2
#define NFSPROC3_LOOKUP		3
#define NFSPROC3_ACCESS		4
#define NFSPROC3_READLINK	5
#define NFSPROC3_READ		6
#define NFSPROC3_WRITE		7
#define NFSPROC3_CREATE		8
#define NFSPROC3_MKDIR		9
#define NFSPROC3_SYMLINK	10
#define NFSPROC3_MKNOD		11
#define NFSPROC3_REMOVE		12
#define NFSPROC3_RMDIR		13
#define NFSPROC3_RENAME		14
#define NFSPROC3_LINK		15
#define NFSPROC3_READDIR	16
#define NFSPROC3_READDIRPLUS	17
#define NFSPROC3_FSSTAT		18
#define NFSPROC3_FSINFO		19
#define NFSPROC3_PATHCONF	20
#define NFSPROC3_COMMIT		21
#define NFS3_PROCEDURE_COUNT	(NFSPROC3_COMMIT + 1)


/*
 *	mount.h definitions
 */
#define MOUNTVER3	((uint32_t)3)
#define	MNTPATHLEN 1024
#define	MNTNAMLEN 255

#define MOUNTPROC_NULL ((uint32_t)(0))
#define MOUNTPROC_DUMP ((uint32_t)(2))
#define MOUNTPROC_UMNT ((uint32_t)(3))
#define MOUNTPROC_UMNTALL ((uint32_t)(4))
#define MOUNTPROC_EXPORT ((uint32_t)(5))
#define MOUNTPROC_EXPORTALL ((uint32_t)(6))
#define MOUNTPROC_PATHCONF ((uint32_t)(7))

struct fhandle3 {
	unsigned int	fhandle3_len;
	char	*fhandle3_val;
};
typedef struct fhandle3 fhandle3;

enum mntstat3 {
	MNT_OK = 0,
	MNT3ERR_PERM = 1,
	MNT3ERR_NOENT = 2,
	MNT3ERR_IO = 5,
	MNT3ERR_ACCES = 13,
	MNT3ERR_NOTDIR = 20,
	MNT3ERR_INVAL = 22,
	MNT3ERR_NAMETOOLONG = 63,
	MNT3ERR_NOTSUPP = 10004,
	MNT3ERR_SERVERFAULT = 10006
};
typedef enum mntstat3 mntstat3;

struct mntres3_ok {
	fhandle3 fhandle;
	struct {
		unsigned int auth_flavors_len;
		int *auth_flavors_val;
	} auth_flavors;
};
typedef struct mntres3_ok mntres3_ok;

struct mountres3 {
	mntstat3 fhs_status;
	union {
		mntres3_ok mntinfo;
	} mntres3_u;
};
typedef struct mountres3 mountres3;

typedef char *dirpath;


/*
 * External XDR functions
 */

/*
 * Mount protocol
 */
extern bool_t xdr_path(XDR *, char **);
extern bool_t xdr_fhstatus(XDR *, struct fhstatus *);
extern bool_t xdr_dirpath(XDR *, dirpath *);
extern bool_t xdr_mntres3(XDR *, mountres3 *);

/*
 * V2
 */
extern bool_t xdr_create(XDR *, char *);
extern bool_t xdr_getattr(XDR *xdrs, char *);
extern bool_t xdr_link(XDR *xdrs, char *);
extern bool_t xdr_lookup(XDR *xdrs, char *);
extern bool_t xdr_mkdir(XDR *xdrs, char *);
extern bool_t xdr_read(XDR *xdrs, char *);
extern bool_t xdr_readdir(XDR *xdrs,  char *);
extern bool_t xdr_readlink(XDR *xdrs, char *);
extern bool_t xdr_remove(XDR *xdrs, char *);
extern bool_t xdr_rename(XDR *xdrs, char *);
extern bool_t xdr_rmdir(XDR *xdrs, char *);
extern bool_t xdr_setattr(XDR *xdrs, char *);
extern bool_t xdr_statfs(XDR *xdrs, char *);
extern bool_t xdr_symlink(XDR *xdrs, char *);
extern bool_t xdr_write(XDR *xdrs, char *);

/*
 * V3
 */
extern bool_t xdr_GETATTR3args(XDR *, GETATTR3args *);
extern bool_t xdr_GETATTR3res(XDR *, GETATTR3res *);
extern bool_t xdr_SETATTR3args(XDR *, SETATTR3args *);
extern bool_t xdr_SETATTR3res(XDR *, SETATTR3res *);
extern bool_t xdr_LOOKUP3args(XDR *, LOOKUP3args *);
extern bool_t xdr_LOOKUP3res(XDR *, LOOKUP3res *);
extern bool_t xdr_ACCESS3args(XDR *, ACCESS3args *);
extern bool_t xdr_ACCESS3res(XDR *, ACCESS3res *);
extern bool_t xdr_READLINK3args(XDR *, READLINK3args *);
extern bool_t xdr_READLINK3res(XDR *, READLINK3res *);
extern bool_t xdr_READ3args(XDR *, READ3args *);
extern bool_t xdr_READ3res(XDR *, READ3res *);
extern bool_t xdr_WRITE3args(XDR *, WRITE3args *);
extern bool_t xdr_WRITE3res(XDR *, WRITE3res *);
extern bool_t xdr_CREATE3args(XDR *, CREATE3args *);
extern bool_t xdr_CREATE3res(XDR *, CREATE3res *);
extern bool_t xdr_MKDIR3args(XDR *, MKDIR3args *);
extern bool_t xdr_MKDIR3res(XDR *, MKDIR3res *);
extern bool_t xdr_SYMLINK3args(XDR *, SYMLINK3args *);
extern bool_t xdr_SYMLINK3res(XDR *, SYMLINK3res *);
extern bool_t xdr_MKNOD3args(XDR *, MKNOD3args *);
extern bool_t xdr_MKNOD3res(XDR *, MKNOD3res *);
extern bool_t xdr_REMOVE3args(XDR *, REMOVE3args *);
extern bool_t xdr_REMOVE3res(XDR *, REMOVE3res *);
extern bool_t xdr_RMDIR3args(XDR *, RMDIR3args *);
extern bool_t xdr_RMDIR3res(XDR *, RMDIR3res *);
extern bool_t xdr_RENAME3args(XDR *, RENAME3args *);
extern bool_t xdr_RENAME3res(XDR *, RENAME3res *);
extern bool_t xdr_LINK3args(XDR *, LINK3args *);
extern bool_t xdr_LINK3res(XDR *, LINK3res *);
extern bool_t xdr_READDIR3args(XDR *, READDIR3args *);
extern bool_t xdr_READDIR3res(XDR *, READDIR3res *);
extern bool_t xdr_READDIRPLUS3args(XDR *, READDIRPLUS3args *);
extern bool_t xdr_READDIRPLUS3res(XDR *, READDIRPLUS3res *);
extern bool_t xdr_FSSTAT3args(XDR *, FSSTAT3args *);
extern bool_t xdr_FSSTAT3res(XDR *, FSSTAT3res *);
extern bool_t xdr_FSINFO3args(XDR *, FSINFO3args *);
extern bool_t xdr_FSINFO3res(XDR *, FSINFO3res *);
extern bool_t xdr_PATHCONF3args(XDR *, PATHCONF3args *);
extern bool_t xdr_PATHCONF3res(XDR *, PATHCONF3res *);
extern bool_t xdr_COMMIT3args(XDR *, COMMIT3args *);
extern bool_t xdr_COMMIT3res(XDR *, COMMIT3res *);


#endif  /* __sfs_c_nfs_h */
