#ifndef lint
static char sfs_3_vldSid[] = "@(#)sfs_3_vld.c	2.1	97/10/23";
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
 *      Copyright 1991,1992  Legato Systems, Inc.                *
 *      Copyright 1991,1992  Auspex Systems, Inc.                *
 *      Copyright 1991,1992  Data General Corporation            *
 *      Copyright 1991,1992  Digital Equipment Corporation       *
 *      Copyright 1991,1992  Interphase Corporation              *
 *      Copyright 1991,1992  Sun Microsystems, Inc.              *
 *                                                               *
 *****************************************************************/

/*
 * ------------------------------ sfs_3_vld.c ---------------------------
 *
 * Validation suite for sfs.
 *
 *.Exported_routines
 *      void Validate_ops(int, char **)
 *
 *.Local_routines
 *      void validate_init_rpc()
 *      void validate_creation(void)
 *      void validate_attributes(void)
 *      void validate_read_write(void)
 *      void validate_rename(void)
 *      int compare_sattr(char *, char *, sattr3 *, fattr3 *)
 *      int compare_fattr(char *, char *, fattr3 *, fattr3 *)
 *      uint16_t sum(unsigned char *, int)
 *      void validate_remove(void)
 *      void validate_cleanup(void)
 *      void validate_exit(void)
 *      void verror(int, ValMsgType, char *, ...)
 *
 *.Revision History
 *	11-Jul-94	ChakChung Ng	Created
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
#include <stdarg.h>
 
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "sfs_c_def.h"

extern struct hostent   *Server_hostent;

/*
 * -----------------------  External Definitions  -----------------------
 */

/* forward definitions for local routines */
/*
 * validate options
 * BATCH 	- do complete pass of validation, reporting errors if any
 * VERBOSE	- prints step-by-step validation actions being performed
 * INTERACTIVE	- VERBOSE and if any errors encountered, ask to continue
 *		  validation or not.
 */
#define	VAL_BATCH	1
#define VAL_VERBOSE	2
#define	VAL_INTERACTIVE	3

typedef enum {
	I = 1,
	W = 2,
	E = 3
} ValMsgType;

#define NUMREGFILES	21
#define NUMDIRS		10
#define NUMLINKS	10
#define NUMSYMLINKS	10
#define NUMFILES	NUMREGFILES + NUMDIRS + NUMLINKS + NUMSYMLINKS
#define NUMFRAGS	8

static void val_op_null(void);
static void val_op_getattr(GETATTR3args *, GETATTR3res *);
static void val_op_setattr(SETATTR3args *, SETATTR3res *);
static void val_op_lookup(LOOKUP3args *, LOOKUP3res *);
static void val_op_access(ACCESS3args *, ACCESS3res *);
static void val_op_readlink(READLINK3args *, READLINK3res *);
static void val_op_read(READ3args *, READ3res *);
static void val_op_write(WRITE3args *, WRITE3res *);
static void val_op_create(CREATE3args *, CREATE3res *);
static void val_op_mkdir(MKDIR3args *, MKDIR3res *);
static void val_op_symlink(SYMLINK3args *, SYMLINK3res *);
static void val_op_mknod(MKNOD3args *, MKNOD3res *);
static void val_op_remove(REMOVE3args *, REMOVE3res *);
static void val_op_rmdir(RMDIR3args *, RMDIR3res *);
static void val_op_rename(RENAME3args *, RENAME3res *);
static void val_op_link(LINK3args *, LINK3res *);
static void val_op_readdir(READDIR3args *, READDIR3res *);
static void val_op_readdirplus(READDIRPLUS3args *, READDIRPLUS3res *);
static void val_op_fsstat(FSSTAT3args *, FSSTAT3res *);
static void val_op_fsinfo(FSINFO3args *, FSINFO3res *);
static void val_op_pathconf(PATHCONF3args *, PATHCONF3res *);
static void val_op_commit(COMMIT3args *, COMMIT3res *);

static void validate_init_rpc(void);
static void validate_exit(void);
static void validate_creation(void);
static void validate_attributes(void);
static void validate_read_write(void);
static void validate_rename(void);
static void validate_remove(void);
static void validate_cleanup(void);
static int compare_sattr(char *, char *, sattr3 *, fattr3 *);
static int compare_fattr(char *, char *, fattr3 *, fattr3 *);
static uint16_t sum(unsigned char *, int);
static void verror(int, ValMsgType, char *, ...);
static void create_3tmp_handles(void);
static void delete_3tmp_handles(void);

/*
 * ----------------------  Static Declarations ----------------------
 */

int Validate;

static int Validate_errors = 0;
static char Testdirname[SFS_MAXPATHLEN];    /* test dir component name */
/*
 * packed structure to keep track of file status
 */
struct sfs_fileinfo {
    int file_found:1,   /* file has been found */
        file_is_dup:1,  /* file has a duplicate */
        pad:30;         /* pad the rest */
};

typedef struct sfs_fileinfo sfs_fileinfo;
/*
 * This vector is used for readdirplus validation currently, but could be
 * extended to keep track of other interesting pieces of information.
 */
static sfs_fileinfo check_files[NUMFILES];

/*
 * ----------------------  SFS Validation Suite  ----------------------
 */

/*
 * XXXXX Must make sure that we validate that all servers return back
 * XXXXX All optional values
 */
void
Validate_ops(
    int         argc,
    char *      argv[])
{
    char *      valdir;
    CLIENT *    mount_client_ptr;
    int i;

    if (argc > 1) {
	verror(VAL_BATCH, E, "Can only validate one directory at a time.\n");
	exit(1);
    }

    Num_io_files = NUMFILES;
    Cur_uid = Real_uid;
    nfs_version = NFS_V3;

    if (argc == 0)
	valdir = ".";
    else
	valdir = argv++[0];

    (void) sprintf(Testdirname, "%s/validatedir", valdir);

    do {
	verror(VAL_BATCH, I, "validating sfs on \"%s\" directory ...\n",
		valdir);

	init_fileinfo();
	create_3tmp_handles();

	/*
	 * need priv port to do following
	 */
	mount_client_ptr = lad_getmnt_hand(valdir);
	if (mount_client_ptr == NULL) {
	    exit(1);
	}
	validate_init_rpc();

	/*
	 * should be all done doing priv port stuff
	 */
	if (setuid(Real_uid) != 0) {
	   (void) fprintf(stderr,"%s: %s%s\n",
		   sfs_Myname, "cannot perform setuid operation.\n",
		   "Do `make install` as root.\n");
	}

	init_mount_point(0, valdir, mount_client_ptr);

        /*
         * initialize the check_file array
         */
        (void) memset((void *) check_files, '\0', sizeof(check_files));

	verror(VAL_VERBOSE, I, "validating null operation ...\n");
	val_op_null();

	validate_creation();
	validate_attributes();
	validate_read_write();
	validate_rename();
	validate_remove();
	argc--;
	valdir = argv++[0];

	/*
	 * Cleanup mount client handle
	 */
	clnt_destroy(mount_client_ptr);

	delete_3tmp_handles();
	validate_cleanup();
    } while (argc > 0);

    validate_exit();

} /* Validate_ops */


/*
 * allocate and initialize client handles
 */
static void
validate_init_rpc(void)
{
	NFS_client = lad_clnt_create(Tcp? 1: 0, Server_hostent,
                                        (uint32_t) NFS_PROGRAM,
                                        (uint32_t) NFS_V3,
                                        RPC_ANYSOCK, &Nfs_timers[0]);

	if (NFS_client == ((CLIENT *) NULL)) { 
		    verror(VAL_BATCH, E,
				"portmap/nfsd server not responding\n"); 
		    exit(1); 
	}
 
	NFS_client->cl_auth = authunix_create(lad_hostname, Real_uid,
                                      Cur_gid, 0, NULL);
} /* validate_init_rpc */


static void
validate_creation(void)
{
    int                 filenum;
    int                 target_filenum;
    CREATE3args		argcr;
    CREATE3res		repcr;
    MKDIR3args		argmk;
    MKDIR3res		repmk;
    LINK3args		argln;
    LINK3res		repln;
    SYMLINK3args	argsl;
    SYMLINK3res		repsl;
    LOOKUP3args		arglp;
    LOOKUP3res		replp;
    READLINK3args	argrl;
    READLINK3res	reprl;
    char                sl_target_path[NFS_MAXPATHLEN];
    char                sym_data[NFS_MAXPATHLEN];

    for (filenum=0; filenum < NUMFILES ; filenum++) {

	Cur_file_ptr = &Io_files[filenum];
	sfs_gettime(&Cur_time);

	if (filenum < NUMREGFILES) {

	    (void) sprintf(Cur_filename, Filespec, filenum);

	    /* regular file creation */
	    (void) memmove((char *) &argcr.where.dir, (char *) &Export_dir.fh3,
				sizeof (nfs_fh3));
	    argcr.where.name = Cur_filename;
	    argcr.how.mode = UNCHECKED;
	    argcr.how.createhow3_u.obj_attributes.mode.set_it = TRUE;
	    argcr.how.createhow3_u.obj_attributes.mode.mode =
							(NFSMODE_REG | 0666);
	    argcr.how.createhow3_u.obj_attributes.uid.set_it = TRUE;
	    argcr.how.createhow3_u.obj_attributes.uid.uid = Cur_uid;
	    argcr.how.createhow3_u.obj_attributes.gid.set_it = TRUE;
	    argcr.how.createhow3_u.obj_attributes.gid.gid = Cur_gid;
	    argcr.how.createhow3_u.obj_attributes.atime.set_it = TRUE;
	    argcr.how.createhow3_u.obj_attributes.atime.atime.seconds =
							Cur_time.esec;
	    argcr.how.createhow3_u.obj_attributes.atime.atime.nseconds =
							Cur_time.usec * 1000;
	    argcr.how.createhow3_u.obj_attributes.mtime.set_it = TRUE;
	    argcr.how.createhow3_u.obj_attributes.mtime.mtime.seconds =
							Cur_time.esec;
	    argcr.how.createhow3_u.obj_attributes.mtime.mtime.nseconds =
			                                Cur_time.usec * 1000;
	    argcr.how.createhow3_u.obj_attributes.size.set_it = TRUE;
	    argcr.how.createhow3_u.obj_attributes.size.size._p._u =
							(uint32_t) 0;
	    argcr.how.createhow3_u.obj_attributes.size.size._p._l =
							(uint32_t) 0;

	    (void) memset((char *) &repcr.resok.obj.handle, '\0',
							sizeof (nfs_fh3));
	    verror(VAL_VERBOSE, I, "validating create file %s ...\n",
		    Cur_filename);
	    val_op_create(&argcr, &repcr);

	    if (repcr.status == NFS3_OK) {
		Cur_file_ptr->state = Exists;
		(void) memmove((char *) &Cur_file_ptr->fh3,
				(char *) &repcr.resok.obj.handle,
				sizeof (nfs_fh3));
		(void) strcpy(Cur_file_ptr->file_name, Cur_filename);
		(void) memmove((char *) &Cur_file_ptr->attributes3,
				(char *) &repcr.resok.obj_attributes.attr,
				sizeof(Cur_file_ptr->attributes3));
		(void) compare_sattr(Ops[CREATE].name, Io_files[filenum].file_name,
					&argcr.how.createhow3_u.obj_attributes,
					&Cur_file_ptr->attributes3);
	    } else {
		Cur_file_ptr->state = Nonexistent;
		errno = (int)repcr.status;
		verror(VAL_BATCH, E, "create %s failed: %m\n",
			  Cur_filename);
		/*
		 * An error in file creation is fatal, because we use the
		 * created files to validate the other operations.
		 */
		validate_exit();
	    }

	} else if (filenum < NUMREGFILES + NUMDIRS) {

	    (void) sprintf(Cur_filename, Dirspec, filenum - NUMREGFILES);

	    /* directory creation */
	    (void) memmove((char *) &argmk.where.dir, (char *) &Export_dir.fh3,
			sizeof (nfs_fh3));
	    argmk.where.name = Cur_filename;
	    argmk.attributes.mode.set_it = TRUE;
	    argmk.attributes.mode.mode = (NFSMODE_DIR | 0777);
	    argmk.attributes.uid.set_it = TRUE;
	    argmk.attributes.uid.uid = Cur_uid;
	    argmk.attributes.gid.set_it = TRUE;
	    argmk.attributes.gid.gid = Cur_gid;
	    argmk.attributes.size.set_it = FALSE;
	    argmk.attributes.size.size._p._u = (uint32_t) 0;
	    argmk.attributes.size.size._p._l = (uint32_t) 0;
	    argmk.attributes.atime.set_it = TRUE;
	    argmk.attributes.atime.atime.seconds = Cur_time.esec;
	    argmk.attributes.atime.atime.nseconds = Cur_time.usec * 1000;
	    argmk.attributes.mtime.set_it = TRUE;
	    argmk.attributes.mtime.mtime.seconds = Cur_time.esec;
	    argmk.attributes.mtime.mtime.nseconds = Cur_time.usec * 1000;

	    (void) memset((char *) &repmk.resok.obj.handle, '\0', sizeof (nfs_fh3));
	    verror(VAL_VERBOSE, I, "validating mkdir %s ...\n", Cur_filename);
	    val_op_mkdir(&argmk, &repmk);

	    if (repmk.status == NFS3_OK) {
		Cur_file_ptr->state = Exists;
		(void) memmove((char *) &Cur_file_ptr->fh3,
				(char *) &repmk.resok.obj.handle,
				sizeof (nfs_fh3));
		(void) strcpy(Cur_file_ptr->file_name, Cur_filename);
		(void) memmove((char *) &Cur_file_ptr->attributes3,
				(char *) &repmk.resok.obj_attributes.attr,
				sizeof(Cur_file_ptr->attributes3));
		(void) compare_sattr(Ops[MKDIR].name, Io_files[filenum].file_name,
			        &argmk.attributes, &Cur_file_ptr->attributes3);
	    } else {
		Cur_file_ptr->state = Nonexistent;
		verror(VAL_BATCH, W, "mkdir %s failed:%m\n", Cur_filename);
	    }

	} else if (filenum < NUMREGFILES + NUMDIRS + NUMLINKS ) {

	    (void) sprintf(Cur_filename, Filespec, filenum - NUMDIRS);

	    /* hard link creation */
	    target_filenum = NUMFILES-NUMSYMLINKS-1-filenum;
	    (void) memmove((char *) &argln.file,
			(char *) &Io_files[target_filenum].fh3,
			sizeof (nfs_fh3));
	    (void) memmove((char *) &argln.link.dir, (char *) &Export_dir.fh3,
			sizeof (nfs_fh3));
	    argln.link.name = Cur_filename;

	    verror(VAL_VERBOSE, I, "validating link %s %s ...\n",
		    Io_files[target_filenum].file_name, Cur_filename);
	    val_op_link(&argln, &repln);

	    if (repln.status == NFS3_OK) {
		Cur_file_ptr->state = Exists;
		(void) memmove((char *) &Cur_file_ptr->fh3,
				(char *) &Io_files[target_filenum].fh3,
				sizeof (nfs_fh3));
		(void) strcpy(Cur_file_ptr->file_name, Cur_filename);
		Cur_file_ptr->attributes3 = Io_files[target_filenum].attributes3;
		Io_files[target_filenum].attributes3.nlink++;
		Cur_file_ptr->attributes3.nlink++;
	    } else {
		Cur_file_ptr->state = Nonexistent;
		verror(VAL_BATCH, W, "link %s failed: %m\n", Cur_filename);
		exit(1);
	    }

	} else {

	    (void) sprintf(Cur_filename, Symspec, filenum -
					(NUMREGFILES + NUMDIRS + NUMLINKS));

	    /* symbolic link creation */
	    target_filenum = NUMFILES-1-filenum;
	    (void) memmove((char *) &argsl.where.dir, (char *) &Export_dir.fh3,
			sizeof (nfs_fh3));
	    argsl.where.name = Cur_filename;
	    (void) sprintf(sl_target_path,
			   "./%s", Io_files[target_filenum].file_name);
	    argsl.symlink.symlink_attributes.size.set_it = TRUE;
	    argsl.symlink.symlink_attributes.size.size._p._u =
					(uint32_t) 0;
	    argsl.symlink.symlink_attributes.size.size._p._l =
					(uint32_t) strlen(sl_target_path);
	    argsl.symlink.symlink_data = sl_target_path;
	    argsl.symlink.symlink_attributes.mode.set_it = TRUE;
	    argsl.symlink.symlink_attributes.mode.mode = (NFSMODE_LNK | 0777);
	    argsl.symlink.symlink_attributes.uid.set_it = TRUE;
	    argsl.symlink.symlink_attributes.uid.uid = Cur_uid;
	    argsl.symlink.symlink_attributes.gid.set_it = TRUE;
	    argsl.symlink.symlink_attributes.gid.gid = Cur_gid;
	    argsl.symlink.symlink_attributes.atime.set_it = TRUE;
	    argsl.symlink.symlink_attributes.atime.atime.seconds =
							Cur_time.esec;
	    argsl.symlink.symlink_attributes.atime.atime.nseconds =
							Cur_time.usec * 1000;
	    argsl.symlink.symlink_attributes.mtime.set_it = TRUE;
	    argsl.symlink.symlink_attributes.mtime.mtime.seconds =
							Cur_time.esec;
	    argsl.symlink.symlink_attributes.mtime.mtime.nseconds =
							Cur_time.usec * 1000;

	    verror(VAL_VERBOSE, I, "validating symlink %s %s ...\n",
		    sl_target_path, Cur_filename);
	    val_op_symlink(&argsl, &repsl);

	    if (repsl.status == NFS3_OK) {
		Cur_file_ptr->state = Exists;

		/* do a lookup to get file handle and attributes */
		(void) memmove((char *) &arglp.what.dir, (char *) &Export_dir.fh3,
				sizeof (nfs_fh3));
		arglp.what.name = Cur_filename;
		(void) memset((char *) &replp.resok.object, '\0', sizeof (nfs_fh3));

		val_op_lookup(&arglp, &replp);

		if (replp.status == NFS3_OK) {
		    (void) memmove((char *) &Cur_file_ptr->fh3,
				(char *) &replp.resok.object,
				sizeof (nfs_fh3));
		    (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
		    (void) memmove((char *) &Cur_file_ptr->attributes3,
				(char *) &replp.resok.obj_attributes.attr,
				sizeof (Cur_file_ptr->attributes3));
		    (void) compare_sattr(Ops[SYMLINK].name,
			                Io_files[filenum].file_name,
			                &argsl.symlink.symlink_attributes,
			                &Cur_file_ptr->attributes3);
		} else {
		    verror(VAL_BATCH, W, "lookup %s failed: %m\n",
			    Cur_filename);
		    continue;
		}

		/* validate readlink */
		(void) memmove((char *) &argrl.symlink,
				(char *) &Cur_file_ptr->fh3,
				sizeof (nfs_fh3));
		reprl.resok.data = sym_data;

		verror(VAL_VERBOSE, I, "validating readlink %s ...\n",
			Cur_filename);
		val_op_readlink(&argrl, &reprl);

		if (reprl.status == NFS3_OK) {
		    if (strcmp(sl_target_path, sym_data)) {
			verror(VAL_BATCH, W,
			    "readlink %s error, result = %s, should be %s\n",
			    Cur_filename, reprl.resok.data,
			    sl_target_path);
		    }
		} else {
		    verror(VAL_BATCH, W, "readlink %s failed:%m\n",
			    Cur_filename);
		}

	    } else {
		Cur_file_ptr->state = Nonexistent;
		verror(VAL_BATCH, W, "symlink %s failed: %m\n", Cur_filename);
	    }
	}
    } /* end for each file */

} /* validate_creation */


static void
validate_attributes(void)
{
    int                 filenum;
    LOOKUP3args		arglp;
    LOOKUP3res		replp;
    GETATTR3args	argga;
    GETATTR3res		repga;
    SETATTR3args	argsa;
    SETATTR3res		repsa;

    /* validate fsstat */

    /* validate lookup */
    for (filenum = 0; filenum < NUMFILES; filenum++) {
	(void) memmove((char *) &arglp.what.dir, (char *) &Export_dir.fh3,
			sizeof (nfs_fh3));
	arglp.what.name = Io_files[filenum].file_name;
	(void) memset((char *) &replp.resok.object, '\0', sizeof (nfs_fh3));

	verror(VAL_VERBOSE, I, "validating lookup %s ...\n",
		Io_files[filenum].file_name);
	val_op_lookup(&arglp, &replp);

	if (replp.status == NFS3_OK) {
	    if (memcmp((char *) &(Io_files[filenum].fh3),
		     (char *) &(replp.resok.object), sizeof (nfs_fh3))) {
		verror(VAL_BATCH, W, "lookup %s: file handle mismatch\n",
				Io_files[filenum].file_name);
		exit(1);
	    }
	    (void) compare_fattr(Ops[LOOKUP].name, Io_files[filenum].file_name,
					&Io_files[filenum].attributes3,
					&replp.resok.obj_attributes.attr);
	} else {
	    verror(VAL_BATCH, W, "lookup %s failed:%m\n",
		    Io_files[filenum].file_name);
	}
    }

    /* validate getattr */
    for (filenum = 0; filenum < NUMFILES; filenum++) {
	(void) memmove((char *) &argga.object,
			(char *) &Io_files[filenum].fh3,
			sizeof (nfs_fh3));

	verror(VAL_VERBOSE, I, "validating getattr %s ...\n",
		Io_files[filenum].file_name);
	val_op_getattr(&argga, &repga);

	if (repga.status == NFS3_OK) {
	    (void) compare_fattr(Ops[GETATTR].name, Io_files[filenum].file_name,
			  &Io_files[filenum].attributes3,
			  &repga.resok.obj_attributes);
	} else {
	    verror(VAL_BATCH, W, "getattr %s failed: %m\n",
		    Io_files[filenum].file_name);
	}
    }

    /*validate setattr */
    for (filenum = 0; filenum < NUMFILES; filenum++) {
	sfs_gettime(&Cur_time);
	(void) memmove((char *) &argsa.object,
			(char *) &Io_files[filenum].fh3,
			sizeof (nfs_fh3));
	argsa.new_attributes.mode.set_it = TRUE;
	if (filenum >= NUMREGFILES && filenum < NUMREGFILES + NUMDIRS)
	  argsa.new_attributes.mode.mode = 0777;
	else
	  argsa.new_attributes.mode.mode = 0666;
	argsa.new_attributes.uid.set_it = FALSE;
	argsa.new_attributes.uid.uid = 0;
	argsa.new_attributes.gid.set_it = FALSE;
	argsa.new_attributes.gid.gid = 0;
	argsa.new_attributes.size.set_it = FALSE;
	argsa.new_attributes.size.size._p._u = 0;
	argsa.new_attributes.size.size._p._l = 0;
	argsa.new_attributes.atime.set_it = TRUE;
	argsa.new_attributes.atime.atime.seconds = Cur_time.esec;
	argsa.new_attributes.atime.atime.nseconds = Cur_time.usec * 1000;
	argsa.new_attributes.mtime.set_it = TRUE;
	argsa.new_attributes.mtime.mtime.seconds = Cur_time.esec;
	argsa.new_attributes.mtime.mtime.nseconds = Cur_time.usec * 1000;
	argsa.guard.check = FALSE;

	verror(VAL_VERBOSE, I, "validating setattr %s ...\n",
				Io_files[filenum].file_name);
	val_op_setattr(&argsa, &repsa);

	if (repsa.status == NFS3_OK) {
	    (void) memmove((char *) &Io_files[filenum].attributes3,
				(char *) &repsa.resok.obj_wcc.after.attr,
				sizeof (Io_files[filenum].attributes3));
	    (void) compare_sattr(Ops[SETATTR].name, Io_files[filenum].file_name,
			&argsa.new_attributes, &repsa.resok.obj_wcc.after.attr);

	    (void) memmove((char *) &argga.object,
				(char *) &Io_files[filenum].fh3,
				sizeof (nfs_fh3));

	    val_op_getattr(&argga, &repga);

	    if (repga.status == NFS3_OK) {
		(void) compare_fattr(Ops[GETATTR].name, Io_files[filenum].file_name,
						&Io_files[filenum].attributes3,
						&repga.resok.obj_attributes);
	    } else {
		verror(VAL_BATCH, W, "getattr %s failed: %m\n",
			Io_files[filenum].file_name);
	    }
	} else {
	    verror(VAL_BATCH, W, "setattr %s failed: %m\n",
		    Io_files[filenum].file_name);
	    exit(1);
	}
    }

} /* validate_attributes */

#define WAITFOREOF 1
/*
 * need to check if readdirplus returns a reasonable amount of data.
 */
static void
val_rddirplus_retsize(uint32_t dircount, uint32_t maxcount,
                      READDIRPLUS3resok *rp)
{
    entryplus3 *esp;
    static int eof_wait = 0;
    int i;
    int size = 0;
    int tsize = 0;
    int msize = 0;
    double mpcnt = 0;

    esp = rp->reply.entries;

    for (i = 0; i < rp->count; i++) {
            size += sizeof(esp[i].fileid);
            size += strlen(esp[i].name) * sizeof(char);
            size += sizeof(esp[i].cookie);
            tsize += sizeof(esp[i].name_attributes);
            tsize += sizeof(esp[i].name_handle.handle_follows);
            tsize += esp[i].name_handle.handle.fh3_length * sizeof(char);
    }

    msize = size + tsize;
    mpcnt = (double) msize / (double) maxcount * 100;

    if (rp->reply.eof) {
        verror(VAL_VERBOSE, I, "readdirplus on %s returned EOF.\n"
    "\treceived %d bytes of directory information and %d bytes including\n"
    "\tpost op attributes and filehandle.\n",
               Testdirname, size, msize);
    } else if (mpcnt < 80) {
    	eof_wait++;
        if (eof_wait > WAITFOREOF) {
            verror(VAL_BATCH, E,
    "readdirplus on %s did not return a reasonable amount of data.\n"
    "\treceived %d bytes. should receive close to %d bytes.\n",
		   Testdirname, msize, maxcount);
	} else {
	    verror(VAL_VERBOSE, I, "readdirplus on %s did not return EOF.\n"
    "\treceived %d bytes of directory information and %d bytes including\n"
    "\tpost op attributes and filehandle.\n",
		   Testdirname, size, msize);
	}
    } else {
        verror(VAL_VERBOSE, I, "readdirplus on %s did not return EOF.\n"
    "\treceived %d bytes of directory information and %d bytes including\n"
    "\tpost op attributes and filehandle.\n",
               Testdirname, size, msize);
    }    
}

static void
validate_read_write(void)
{
    struct {
	uint16_t  sum;                    /* checksum of data */
	uint16_t  len;                    /* length of len and data */
	char            data[DEFAULT_MAX_BUFSIZE - 2 * sizeof(uint16_t)];
    } block;
    WRITE3args		argwr;
    WRITE3res		repwr;
    READ3args		argrd;
    READ3res		reprd;
    READDIR3args	argdr;
    READDIR3res		repdr;
    READDIRPLUS3args    argdrp;
    READDIRPLUS3res     repdrp;
    int                 maxblks;
    int                 maxfiles;
    int                 i;
    int                 numfiles;
    int                 filenum;
    int                 blocknum;
    entry3              entry_stream[SFS_MAXDIRENTS];
    entryplus3          entryplus_stream[SFS_MAXDIRENTS];

    /* validate write */

    /* get the maximum number of blocks sfs will write */
    maxblks = Io_dist_ptr->max_bufs;
    maxfiles = maxblks > NUMREGFILES ? NUMREGFILES : maxblks;

    /* write maxblks - filenum + 1 blocks to each regular file */
    argwr.offset._p._u = argwr.offset._p._l = (uint32_t) 0;
    argwr.stable = FILE_SYNC;
    argwr.data.data_val = (char *)&block;

    for (blocknum = 0; blocknum <= maxblks ; blocknum++) {

	for (i=0; i < sizeof(block.data); i++)
	    block.data[i] = (char)blocknum;

	for (filenum=0; filenum < maxfiles; filenum++) {
	    /* Write fewer blocks to files with higher numbers. */
	    if (blocknum > (maxblks - filenum))
		break;

	    /* set the length field */
	    if (blocknum == (maxblks - filenum)) {
		block.len = ((maxfiles - filenum) *
		        (Bytes_per_block/Kb_per_block)) - (sizeof(block.len) + 
							   sizeof(block.sum));
		/*
                 * XXX - write kludge.
                 *
                 * Writes must be less than 8K in
                 * size or else the checksum will incur a buffer overflow
                 */
                block.len = (block.len % DEFAULT_MAX_BUFSIZE) -
                        (sizeof(block.len) + sizeof(block.sum));
	    } else {
		block.len = Bytes_per_block - (sizeof(block.len)
			    + sizeof(block.sum));
	    }
	    block.sum = sum((unsigned char *) &block.len,
			    (int)(block.len + sizeof(block.len)));

	    (void) memmove((char *) &argwr.file,
				(char *) &Io_files[filenum].fh3,
				sizeof (nfs_fh3));
	    argwr.data.data_len = block.len +
			          sizeof(block.len) + sizeof(block.sum);
	    argwr.count = argwr.data.data_len;

	    verror(VAL_VERBOSE, I,
		    "validating write %d bytes @ offset %lu to %s ...\n",
		    argwr.data.data_len, argwr.offset._p._l,
		    Io_files[filenum].file_name);

	    val_op_write(&argwr, &repwr);

	    if (repwr.status == NFS3_OK) {
		(void) compare_fattr(Ops[WRITE].name, Io_files[filenum].file_name,
			      &Io_files[filenum].attributes3,
			      &repwr.resok.file_wcc.after.attr);
		Io_files[filenum].attributes3 = repwr.resok.file_wcc.after.attr;
                /*
                 * XXX-bookeeping kludge.
                 *
                 * Need to update hardlink attributes, so readdirplus vali-
                 * dation doesn't barf.  This is necessary because setattr was
                 * validated on all the test files and the attributes in
                 * Io_files[] were updated accordingly.  Since the write
                 * op has been validated on just the regular files, we have to
                 * make sure that the corresponding indexes in Io_files[] that
                 * point to the hard links reflect the current file attributes.
                 */
                if (filenum < NUMLINKS) {
                    Io_files[NUMREGFILES+NUMDIRS+NUMLINKS-1-filenum].attributes3 = Io_files[filenum].attributes3;
                }

	    } else {
		verror(VAL_BATCH, W, "write %s failed: %m\n",
			Io_files[filenum].file_name);
	    }
	}
	argwr.offset._p._l += Bytes_per_block;
    }

    /* validate read */

    for (filenum = 0; filenum < maxfiles; filenum++) {
	(void) memmove((char *) &argrd.file,
			(char *) &Io_files[filenum].fh3,
			sizeof (nfs_fh3));
	argrd.offset._p._u = argrd.offset._p._l = (uint32_t) 0;
	argrd.count = 0;
	reprd.resok.data.data_len = 0;
	maxblks = Io_files[filenum].attributes3.size._p._l / Bytes_per_block;
	for (blocknum = 0; blocknum <= maxblks; blocknum ++) {

	    if (argrd.count != reprd.resok.data.data_len) {
		argrd.count -= reprd.resok.data.data_len;
		reprd.resok.data.data_val = (char *)&block +
			                reprd.resok.data.data_len;
		blocknum--;
	    } else {
		if (blocknum < maxblks)
		    argrd.count = Bytes_per_block;
		else
		    argrd.count = (maxfiles - filenum)
			          * (Bytes_per_block/Kb_per_block);
		reprd.resok.data.data_val = (char *)&block;
	    }

	    verror(VAL_VERBOSE, I,
		   "validating read %lu bytes @ offset %lu from %s ...\n",
		    argrd.count, argrd.offset._p._l,
		    Io_files[filenum].file_name);

	    val_op_read(&argrd, &reprd);

	    if (reprd.status == NFS3_OK) {
		(void) compare_fattr(Ops[READ].name, Io_files[filenum].file_name,
			      &Io_files[filenum].attributes3,
			      &reprd.resok.file_attributes.attr);
		Io_files[filenum].attributes3 = reprd.resok.file_attributes.attr;
		argrd.offset._p._l += reprd.resok.data.data_len;
	    } else {
		verror(VAL_BATCH, W, "read %s failed: %m\n",
			Io_files[filenum].file_name);
	    }

	    if ((argrd.count ==
		(block.sum != sum((unsigned char *) &block.len,
			          (int)(block.len + sizeof(block.len)))))) {
		verror(VAL_BATCH, W, "read %s checksum mismatch\n",
			Io_files[filenum].file_name);
	    }
	}
    }

    /* validate readdir */
    (void) memmove((char *) &argdr.dir, (char *) &Export_dir.fh3,
			sizeof (nfs_fh3));
    argdr.cookie._p._l = argdr.cookie._p._u = (uint32_t) 0;
    (void) memset((char *) argdr.cookieverf, '\0', NFS3_COOKIEVERFSIZE);
    argdr.count = DEFAULT_MAX_BUFSIZE;

    do {
        (void) memset((char *) entry_stream, '\0', sizeof (entry3) * SFS_MAXDIRENTS);
        (void) memset((char *) &repdr, '\0', sizeof(repdr));
        repdr.resok.count = SFS_MAXDIRENTS;
        repdr.resok.reply.entries = entry_stream;
	verror(VAL_VERBOSE, I, "validating readdir on %s ...\n", Testdirname);
	val_op_readdir(&argdr, &repdr);


	if (repdr.status == NFS3_OK) {
            for (i = 0; i < repdr.resok.count; i++) {
                for (filenum = 0; filenum < NUMFILES; filenum++) {
                    if (!strcmp(entry_stream[i].name, Io_files[filenum].file_name)) {                        if (entry_stream[i].fileid._p._l !=
                                    Io_files[filenum].attributes3.fileid._p._l) {
                            verror(VAL_BATCH, E,
                                "readdir %s error: file %s fileid mismatch\n",
                                Testdirname, entry_stream[i].name);
                            verror(VAL_BATCH, W,
                                "   fileid: got = %lu, original = %lu\n",
                                entry_stream[i].fileid._p._l,
                                Io_files[filenum].attributes3.fileid._p._l);
                        }
                        /*
                         * mark file as either found or dup
                         */
                        if (check_files[filenum].file_found &&
                            !check_files[filenum].file_is_dup)
                                check_files[filenum].file_is_dup = 1;
                        else
                                check_files[filenum].file_found = 1;
                        break;
                    }
                }
            }
        } else {
            verror(VAL_BATCH, W, "readdir %s failed: %m\n", Testdirname);
        }

	argdr.cookie = entry_stream[repdr.resok.count-1].cookie;

    } while (repdr.resok.reply.eof == 0);

    /*
     * check if any known files have not been found
     */
    for (i = 0; i < NUMFILES; i++) {
        if (!check_files[i].file_found)
                verror(VAL_BATCH, E,
                       "readdir %s error: file %s not found\n",
                       Testdirname, Io_files[i].file_name);
        else {
                if (check_files[i].file_is_dup)
                    verror(VAL_BATCH, E,
              "readdir %s error: file %s returned more than once\n",
                           Testdirname, Io_files[i].file_name);
        }
    }

    /* validate readdirplus */
    (void) memset((void *) check_files, '\0', sizeof(check_files));

    argdrp.cookie._p._l = argdrp.cookie._p._u = (uint32_t) 0;
    (void) memmove((char *) &argdrp.dir, (char *) &Export_dir.fh3,
                   sizeof (nfs_fh3));
    (void) memset((char *) argdrp.cookieverf, '\0', NFS3_COOKIEVERFSIZE);
    /*
     * We validate readdirplus with dircount and maxcount both set at 8K.
     * This is not the most efficient way, but this is how readdirplus is
     * called in SFS.  With the numbers set as such, maxcount becomes the
     * bottleneck and we will not get 8K worth of directory info.  What we
     * should get is 8K worth which includes directory info plus post_op_
     * attributes and filehandle.
     */
    argdrp.dircount = DEFAULT_MAX_BUFSIZE;
    argdrp.maxcount = DEFAULT_MAX_BUFSIZE;
    do {
        (void) memset((char *) entryplus_stream, '\0',
                      sizeof (entryplus_stream));

        (void) memset((char *) &repdrp, '\0', sizeof(repdrp));
        repdrp.resok.count = SFS_MAXDIRENTS;
        repdrp.resok.reply.entries = entryplus_stream;

        verror(VAL_VERBOSE, I, "validating readdirplus on %s ...\n",
                Testdirname);
        val_op_readdirplus(&argdrp, &repdrp);

        if (repdrp.status == NFS3_OK) {
            verror(VAL_VERBOSE, I, "readdirplus found %d entries in %s...\n",
                   repdrp.resok.count, Testdirname);
            val_rddirplus_retsize(argdrp.dircount, argdrp.maxcount,
                                  &repdrp.resok);
            for (i = 0; i < repdrp.resok.count; i++) {
                for (filenum = 0; filenum < NUMFILES; filenum++) {
                    if (!strcmp(entryplus_stream[i].name,
                                Io_files[filenum].file_name)) {
                        if (entryplus_stream[i].fileid._p._l !=
                            Io_files[filenum].attributes3.fileid._p._l) {
                            verror(VAL_BATCH, E,
                        "readdirplus %s error: file %s fileid mismatch\n",
                                   Testdirname, entryplus_stream[i].name);
                            verror(VAL_BATCH, W,
                                "   fileid: got = %lu, original = %lu\n",
                                   entryplus_stream[i].fileid._p._l,
                                   Io_files[filenum].attributes3.fileid._p._l);
                        }

                        /*
                         * mark file as either found or dup
                         */
                        if (check_files[filenum].file_found &&
                            !check_files[filenum].file_is_dup)
                                check_files[filenum].file_is_dup = 1;
                        else
                                check_files[filenum].file_found = 1;

			/*
                         * check to make sure post op attributes and
                         * file handle are returned.
                         */
                        if (!entryplus_stream[i].name_attributes.attributes)
                                verror(VAL_BATCH, E,
                "readdirplus %s warning: did not receive post op attributes for file %s\n\n",
                                       Testdirname, entryplus_stream[i].name);
                        else
                            (void) compare_fattr(Ops[READDIRPLUS].name,
                                                 Io_files[filenum].file_name,
                                                 &Io_files[filenum].attributes3,                                                 &entryplus_stream[i].name_attributes.attr);

                        if (!entryplus_stream[i].name_handle.handle_follows)
                                verror(VAL_BATCH, E,
                "readdirplus %s warning: did not receive file handle for file %s\n\n",
                                       Testdirname, entryplus_stream[i].name);
                        else
                            if (memcmp((void *) &Io_files[filenum].fh3.fh3_u.data,
                                       (void *) &entryplus_stream[i].name_handle.handle.fh3_u.data, Io_files[filenum].fh3.fh3_length) != 0)
                                   verror(VAL_BATCH, E,
                "readdirplus %s error: file %s, filehandles do not match\n\n",
                                          Testdirname, entryplus_stream[i].name);
                        break;
                    }
                }
            }
        } else {
            verror(VAL_BATCH, W, "readdirplus %s failed: %m\n", Testdirname);
        }
        argdrp.cookie = entryplus_stream[repdrp.resok.count-1].cookie;

    } while (repdrp.resok.reply.eof == 0);
    /*
     * check if any known files have not been found
     */
    for (i = 0; i < NUMFILES; i++) {
        if (!check_files[i].file_found)
                verror(VAL_BATCH, E,
                       "readdirplus %s error: file %s not found\n",
                       Testdirname, Io_files[i].file_name);
        else {
                if (check_files[i].file_is_dup)
                    verror(VAL_BATCH, E,
              "readdirplus %s error: file %s returned more than once\n",
                           Testdirname, Io_files[i].file_name);
        }
    }   
} /* validate_read_write */


static void
validate_rename(void)
{
    RENAME3args argrn;
    RENAME3res  reprn;
    int         filenum;
    char        newname[SFS_MAXNAMLEN];
    int         rncount = 0;

    (void) memmove((char *) &argrn.from.dir, (char *) &Export_dir.fh3,
			sizeof (nfs_fh3));
    (void) memmove((char *) &argrn.to.dir, (char *) &Export_dir.fh3,
		sizeof (nfs_fh3));

    for (filenum=0; filenum < NUMFILES; filenum++) {
	if (Io_files[filenum].state != Exists)
	    continue;

	rncount++;
	(void) sprintf(newname, "n%s", Io_files[filenum].file_name);
	argrn.from.name = Io_files[filenum].file_name;
	argrn.to.name = newname;

	verror(VAL_VERBOSE, I, "validating rename %s %s ...\n",
		argrn.from.name, argrn.to.name);

	val_op_rename(&argrn, &reprn);

	if (reprn.status == NFS3_OK) {
	    (void) strcpy(Io_files[filenum].file_name, newname);
	} else {
	    verror(VAL_BATCH, W, "rename %s to %s failed: %m\n",
		    Io_files[filenum].file_name, newname);
	}

    }

    if (!rncount) {
	verror(VAL_BATCH, E, "validate_rename: no files renamed\n");
	verror(VAL_BATCH, W, "    due to previous operation error\n");
    }

} /* validate_rename */


static int
compare_fattr(
    char *      op,
    char *      fname,
    fattr3 *     attr1,
    fattr3 *     attr2)
{
    int         ret = TRUE;
    int         prev_warn = FALSE; /* -1 info warning */
    int         flag_error = FALSE;

    if (attr1->type != attr2->type) {
	if (attr1->type == 0xFFFFFFFF) {
	    prev_warn = TRUE;
	    if (ret) {
		verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
		    fname, op);
	    }
	    verror(VAL_BATCH, I, "    type: current = %d, previous =  %d\n",
		   attr2->type, attr1->type);
	    attr1->type = attr2->type;
	    ret = FALSE;
	}
	else {
		if (ret) {
			verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			    fname, op);
		}
		verror(VAL_BATCH, E, "    type: current = %d, previous =  %d\n",
			attr2->type, attr1->type);
		ret = FALSE;
		flag_error = TRUE;
	}
    }

    if ((attr1->mode & NFSMODE_MASK) != (attr2->mode & NFSMODE_MASK)) {
	if (attr1->mode == (unsigned int)0xFFFFFFFF) {
		prev_warn = TRUE;
		if (ret) {
			verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			        fname, op);
		}
		verror(VAL_BATCH, I, "    mode: current = %7lo, previous =  %7lo\n",
			attr2->mode, attr1->mode);
		attr1->mode =  attr2->mode;
		ret = FALSE;
	}
	else {
		if (ret) {
			verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			    fname, op);
		}
		verror(VAL_BATCH, E, "    mode: current = %7lo, previous =  %7lo\n",
                        attr2->mode, attr1->mode);
		ret = FALSE;
		flag_error = TRUE;
	}
    }

    if (attr1->nlink != attr2->nlink) {
	if (attr1->nlink == (unsigned int)0xFFFFFFFF) {
		prev_warn = TRUE;
		if (ret) {
			verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			        fname, op);
		}
		verror(VAL_BATCH, I,
			"    nlink: current = %lu, previous =  %lu\n",
			attr2->nlink, attr1->nlink);
		ret = FALSE;
		attr1->nlink =  attr2->nlink;
	}
	else {
		if (ret) {
			verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			    fname, op);
		}
		verror(VAL_BATCH, E,
			"    nlink: current = %lu, previous =  %lu\n",
			attr2->nlink, attr1->nlink);
		ret = FALSE;
		flag_error = TRUE;
	}
    }


    /*
     * Check for user "nobody", UID -2, which may be untranslated from
     * sixteen-bit two's complement.
     */
    if (attr1->uid != attr2->uid && !((attr2->uid == (unsigned int)0xFFFFFFFE ||
	attr2->uid == 65534) && attr1->uid ==0)) {
	if (attr1->uid == (unsigned int)0xFFFFFFFF) {
		prev_warn = TRUE;
		if (ret) {
			verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			        fname, op);
		}
		verror(VAL_BATCH, I,
			"    uid: current = %lu, previous =  %lu\n",
			attr2->uid, attr1->uid);
		attr1->uid =  attr2->uid;
		ret = FALSE;
	}
	else {
		if (ret) {
			verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			    fname, op);
		}
		verror(VAL_BATCH, E,
			"    uid: current = %lu, previous =  %lu\n",
			attr2->uid, attr1->uid);
		ret = FALSE;
		flag_error = TRUE;
	}
    }

    if (attr1->gid != attr2->gid && attr2->gid != 0) {
/*
    if (attr1->gid != attr2->gid) {
*/
	if (attr1->gid == (unsigned int)0xFFFFFFFF) {
		prev_warn = TRUE;
		if (ret) {
			verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			        fname, op);
		}
		verror(VAL_BATCH, I,
			"    gid: current = %lu, previous =  %lu\n",
			attr2->gid, attr1->gid);
		attr1->gid =  attr2->gid;
		ret = FALSE;
	}
	else {
		if (ret) {
			verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			    fname, op);
		}
		verror(VAL_BATCH, E,
			"    gid: current = %lu, previous =  %lu\n",
			attr2->gid, attr1->gid);
		ret = FALSE;
		flag_error = TRUE;
	}
    }

    if (attr1->size._p._l != attr2->size._p._l) {
	if (strcmp(op, Ops[WRITE].name)) {
	    if (attr1->size._p._l == (unsigned int)0xFFFFFFFF) {
		prev_warn = TRUE;
		if (ret) {
			verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			        fname, op);
		}
		verror(VAL_BATCH, I,
			"    size: current = %lu, previous =  %lu\n",
			attr2->size._p._l, attr1->size._p._l);
		attr1->size._p._l =  attr2->size._p._l;
		ret = FALSE;
	    }
	    else {
		if (ret) {
			verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			    fname, op);
		}
		verror(VAL_BATCH, E,
			"    size: current = %lu, previous =  %lu\n",
			attr2->size._p._l, attr1->size._p._l);
		ret = FALSE;
		flag_error = TRUE;
	    }
	}
    }

    /* compare rdev only if type == NFCHR or NFBLK */
    if ((attr1->type == NF3CHR || attr1->type == NF3BLK) &&
			(attr1->rdev.specdata1 != attr2->rdev.specdata1 ||
			attr1->rdev.specdata2 != attr2->rdev.specdata2)) {
	if (attr1->rdev.specdata1 == (unsigned int)0xFFFFFFFF) {
	     prev_warn = TRUE;
	     if (ret) {
		     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			     fname, op);
	     }
	     verror(VAL_BATCH, I,
			"    rdev: current = %lu %lu, previous =  %lu %lu\n",
			attr2->rdev.specdata1, attr2->rdev.specdata2,
			attr1->rdev.specdata1, attr1->rdev.specdata2);
	     attr1->rdev.specdata1 =  attr2->rdev.specdata1;
	     attr1->rdev.specdata2 =  attr2->rdev.specdata2;
	     ret = FALSE;
	}
	else {
	     if (ret) {
		     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			 fname, op);
	     }
	     verror(VAL_BATCH, E,
			"    rdev: current = %lu %lu, previous =  %lu %lu\n",
			attr2->rdev.specdata1, attr2->rdev.specdata2,
			attr1->rdev.specdata1, attr1->rdev.specdata2);
	     ret = FALSE;
	     flag_error = TRUE;
	}
    }

    if (attr1->fsid._p._l != attr2->fsid._p._l) {
	if (attr1->fsid._p._l == (unsigned int)0xFFFFFFFF) {
	     prev_warn = TRUE;
	     if (ret) {
		     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			     fname, op);
	     }
	     verror(VAL_BATCH, I, "    fsid: current = %lu, previous =  %lu\n",
		     attr2->fsid._p._l, attr1->fsid._p._l);
	     attr1->fsid._p._l =  attr2->fsid._p._l;
	     ret = FALSE;
	}
	else {
	     if (ret) {
		     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			 fname, op);
	     }
	     verror(VAL_BATCH, E, "    fsid: current = %lu, previous =  %lu\n",
		     attr2->fsid._p._l, attr1->fsid._p._l);
	     ret = FALSE;
		flag_error = TRUE;
	}
    }

    if (attr1->fileid._p._l != attr2->fileid._p._l) {
	if (attr1->fileid._p._l == (unsigned int)0xFFFFFFFF) {
	     prev_warn = TRUE;
	     if (ret) {
		     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
			     fname, op);
	     }
	     verror(VAL_BATCH, I,
			"    fileid: current = %lu, previous =  %lu\n",
			attr2->fileid._p._l, attr1->fileid._p._l);
	     attr1->fileid._p._l =  attr2->fileid._p._l;
	     ret = FALSE;
	}
	else {
	     if (ret) {
		     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
			 fname, op);
	     }
	     verror(VAL_BATCH, E,
			"    fileid: current = %lu, previous =  %lu\n",
		     attr2->fileid._p._l, attr1->fileid._p._l);
	     ret = FALSE;
		flag_error = TRUE;
	}
    }

    if (prev_warn) {
	verror(VAL_BATCH, I,
		"\n        Warning: the previous value of a field is -1,\n");
	verror(VAL_BATCH, I,
		"        this resulted from an unused field returned by\n");
	verror(VAL_BATCH, I,
		"        the previous operation on this file/directory.\n");
	verror(VAL_BATCH, I,
	    "        The current value is now stored for future comparison\n\n");
    }

    if (flag_error)
	verror(VAL_BATCH, W,"\n");

    return(flag_error);

} /* ckompare_fattr */


uint32_t sattr_types[8] =	{
			  0000000,		/* NF3NON */
			  0100000,		/* NF3REG */
			  0040000,		/* NF3DIR */
			  0060000,		/* NF3BLK */
			  0020000,		/* NF3CHR */
			  0120000,		/* NF3LNK */
			  0140000,		/* NF3SOCK */
			  0010000 };		/* NF3FIFO */
static int
compare_sattr(
    char *      op,
    char *      fname,
    sattr3 *    attr1,
    fattr3 *    attr2)
{
    int         ret = TRUE;
    char        msg[80];

    msg[0] = '\0';

#ifdef notyet
    if (attr1->mode.mode.set_it == TRUE &&
	(((attr1->mode.mode & NFSTYPE_FMT) != sattr_types[attr2->type]) &&
	((attr1->mode.mode & NFSTYPE_FMT) != sattr_types[0]) ||
	((attr1->mode.mode & NFSMODE_FMT) != (attr2->mode & NFSMODE_FMT)))) {
	if (ret) {
	    verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
						fname, op);

	}
	verror(VAL_BATCH, E, "    mode: returned = %7o, specified = %7o\n",
				attr1->mode.mode, attr2->mode);
	ret = FALSE;
    }
#endif

    /*
     * Check for user "nobody", UID -2, which may be untranslated from
     * sixteen-bit two's complement.
     */
    if (attr1->uid.set_it == TRUE && attr1->uid.uid != attr2->uid &&
                        !((attr2->uid == (unsigned int)0xFFFFFFFE ||
						attr2->uid == 65534) &&
						attr1->uid.uid == 0)) {
	if (ret) {
	    verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
						fname, op);
	}
	if (attr1->uid.uid == 0)
	    (void) strcat(msg," (is root UID mapped to other UID?)");
	verror(VAL_BATCH, E, "    uid: returned = %lu, specified = %lu %s\n",
				attr2->uid, attr1->uid.uid, msg);
	ret = FALSE;
    }

    if (attr1->gid.set_it == TRUE && attr1->gid.gid != attr2->gid &&
							attr2->gid != 0) {
/*
   if (attr1->gid.set_it == TRUE && attr1->gid != attr2->gid) {
*/
	if (ret) {
	    verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
		    fname, op);
	}
	verror(VAL_BATCH, E, "    gid: returned = %lu, specified = %lu\n",
		attr2->gid, attr1->gid.gid);
	ret = FALSE;
    }

    if (attr1->size.set_it == TRUE &&
				attr1->size.size._p._l != attr2->size._p._l) {
	if (ret) {
	    verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
		    fname, op);
	}
	verror(VAL_BATCH, E, "    size: returned = %lu, specified = %lu\n",
		attr2->size._p._l, attr1->size.size._p._l);
	ret = FALSE;
    }

    if (!ret)
	verror(VAL_BATCH, W,"\n");

    return(ret);

} /* compare_sattr */


/*
 * Return the BSD checksum of buf[0..len-1]
 */
static uint16_t
sum(
    unsigned char *     buf,
    int                 len)
{
    uint16_t      cksum;

    cksum = 0;
    for (; len--; buf++) {
	if (cksum & 01)
	    cksum = (cksum >> 1) + 0x8000;
	else
	    cksum >>= 1;
	cksum += (uint16_t) *buf;
	cksum &= 0xFFFF;
    }
    return(cksum);

} /* sum */


static void
validate_remove(void)
{
    REMOVE3args	argrm;
    REMOVE3res	reprm;
    RMDIR3args	argrd;
    RMDIR3res	reprd;
    LOOKUP3args	arglp;
    LOOKUP3res	replp;
    nfsstat3	reply;
    int         filenum;
    char *      op;

    for (filenum = 0; filenum < NUMFILES; filenum++) {

	if (Io_files[filenum].state != Exists)
	    continue;

	if (Io_files[filenum].attributes3.type == NF3DIR) {
	    op = Ops[RMDIR].name;
	    verror(VAL_VERBOSE, I, "validating rmdir %s ...\n",
		    Io_files[filenum].file_name);

	    (void) memmove((char *) &argrd.object.dir,
				(char *) &Export_dir.fh3,
				sizeof (nfs_fh3));
	    argrd.object.name = Io_files[filenum].file_name;
	    val_op_rmdir(&argrd, &reprd);
	    reply = reprd.status;
	} else {
	    op = Ops[REMOVE].name;
	    verror(VAL_VERBOSE, I, "validating remove %s ...\n",
		    Io_files[filenum].file_name);

	    (void) memmove((char *) &argrm.object.dir, (char *) &Export_dir.fh3,
				sizeof (nfs_fh3));
	    argrm.object.name = Io_files[filenum].file_name;
	    val_op_remove(&argrm, &reprm);
	    reply = reprm.status;
	}

	if (reply == NFS3_OK) {
	    /* make sure the file is removed from the directory */
	    (void) memmove((char *) &arglp.what.dir, (char *) &Export_dir.fh3,
				sizeof (nfs_fh3));
	    arglp.what.name = Io_files[filenum].file_name;
	    val_op_lookup(&arglp, &replp);

	    if (replp.status == NFS3ERR_NOENT) {
		Io_files[filenum].state = Nonexistent;
	    } else if (replp.status == NFS3_OK) {
		verror(VAL_BATCH, W, "%s %s: file not removed\n",
		       op, Io_files[filenum].file_name);
	    } else {
		verror(VAL_BATCH, W, "lookup %s failed: %m\n",
		       Io_files[filenum].file_name);

	    }
	} else {
	    verror(VAL_BATCH, W, "%s %s failed: %m\n", op,
		    Io_files[filenum].file_name);
	}

    }

} /* validate_remove */


static void
validate_cleanup(void)
{
    free(Io_files);
    free(Non_io_files);
    free(Dirs);
    free(Symlinks);
    clnt_destroy(NFS_client);
} /* validate_cleanup */


static void
validate_exit(void)
{
    if (!Validate_errors) {
	verror(VAL_BATCH, I, "validation completed successfully.\n");
	exit(0);
    } else {
	verror(VAL_BATCH, I, "validation terminated with errors\n");
	exit(1);
    }

} /* validate_exit */


/* PRINTFLIKE3 */ 
static void
verror(
    int         opt,
    ValMsgType  msgtype,
    char *      fmt,
    ...)
{
    va_list	ap;
    char        buf[1024];
    char *      bp = buf;
    char *      fp;
    char *      sp;
    int         repeat;

    va_start(ap, fmt);

    /*
     * Expand the "%m" format character into the descriptive string
     * for the current value of errno.  Printf handles the other "%"
     * formatting characters.
     */
    if (Validate >= opt) {
	for (fp = fmt; *fp; fp++) {
	    if (*fp == '%' && fp[1] == 'm') {
		if ((sp = strerror(errno)) == NULL) {
		    (void) sprintf(bp, "unknown error %d", errno);
		} else {
		    (void) strcpy(bp, sp);
		}
		bp = buf + strlen(buf);
		fp++;
	    } else {
		*bp++ = *fp;
	    }
	}
	*bp = '\0';
	(void) vfprintf(stderr, buf, ap);
    }
    va_end(ap);

    if (msgtype != I)
	Validate_errors++;

    if (msgtype == W && Validate == VAL_INTERACTIVE) {
	repeat = 1;
	while (repeat) {
	    char ans[80];

	    (void) fprintf(stderr, "continue? (y or n)  ");
	    if (!fgets(ans,80,stdin)) {
		(void) fprintf(stderr, "\n");
		continue;
	    }
	    if (ans[0] == 'n' || ans[0] == 'N') {
		validate_exit();
		exit(1);
	    } else if (ans[0] == 'y' || ans[0] == 'Y') {
		repeat = 0;
		break;
	    }
	}
    }

} /* verror */


static void
val_op_null(void)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_NULL, 
	        xdr_void, (char *)0, xdr_void, (char *)0, 
	        Nfs_timers[Ops[NULLCALL].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "null");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_getattr(GETATTR3args *args, GETATTR3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_GETATTR, 
	        xdr_GETATTR3args, (char *)args, xdr_GETATTR3res, (char *)reply, 
	        Nfs_timers[Ops[GETATTR].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "getattr");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_setattr(SETATTR3args *args, SETATTR3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_SETATTR, 
	        xdr_SETATTR3args, (char *)args, xdr_SETATTR3res, (char *)reply, 
	        Nfs_timers[Ops[SETATTR].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "setattr");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_lookup(LOOKUP3args *args, LOOKUP3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_LOOKUP, 
	        xdr_LOOKUP3args, (char *)args, xdr_LOOKUP3res, (char *)reply, 
	        Nfs_timers[Ops[LOOKUP].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "lookup");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_access(ACCESS3args *args, ACCESS3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_ACCESS, 
	        xdr_ACCESS3args, (char *)args, xdr_ACCESS3res, (char *)reply, 
	        Nfs_timers[Ops[ACCESS].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "access");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_readlink(READLINK3args *args, READLINK3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_READLINK, 
	        xdr_READLINK3args, (char *)args, xdr_READLINK3res, (char *)reply, 
	        Nfs_timers[Ops[READLINK].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "readlink");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_read(READ3args *args, READ3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_READ, 
	        xdr_READ3args, (char *)args, xdr_READ3res, (char *)reply, 
	        Nfs_timers[Ops[READ].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "read");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_write(WRITE3args *args, WRITE3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_WRITE, 
	        xdr_WRITE3args, (char *)args, xdr_WRITE3res, (char *)reply, 
	        Nfs_timers[Ops[WRITE].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "write");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_create(CREATE3args *args, CREATE3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_CREATE, 
	        xdr_CREATE3args, (char *)args, xdr_CREATE3res, (char *)reply, 
	        Nfs_timers[Ops[CREATE].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "create");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_mkdir(MKDIR3args *args, MKDIR3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_MKDIR, 
	        xdr_MKDIR3args, (char *)args, xdr_MKDIR3res, (char *)reply, 
	        Nfs_timers[Ops[MKDIR].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "mkdir");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_symlink(SYMLINK3args *args, SYMLINK3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_SYMLINK, 
	        xdr_SYMLINK3args, (char *)args, xdr_SYMLINK3res, (char *)reply, 
	        Nfs_timers[Ops[SYMLINK].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "symlink");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_mknod(MKNOD3args *args, MKNOD3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_MKNOD, 
	        xdr_MKNOD3args, (char *)args, xdr_MKNOD3res, (char *)reply, 
	        Nfs_timers[Ops[MKNOD].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "mknod");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_remove(REMOVE3args *args, REMOVE3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_REMOVE, 
	        xdr_REMOVE3args, (char *)args, xdr_REMOVE3res, (char *)reply, 
	        Nfs_timers[Ops[REMOVE].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "remove");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_rmdir(RMDIR3args *args, RMDIR3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_RMDIR, 
	        xdr_RMDIR3args, (char *)args, xdr_RMDIR3res, (char *)reply, 
	        Nfs_timers[Ops[RMDIR].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "rmdir");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_rename(RENAME3args *args, RENAME3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_RENAME, 
	        xdr_RENAME3args, (char *)args, xdr_RENAME3res, (char *)reply, 
	        Nfs_timers[Ops[RENAME].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "rename");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_link(LINK3args *args, LINK3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_LINK, 
	        xdr_LINK3args, (char *)args, xdr_LINK3res, (char *)reply, 
	        Nfs_timers[Ops[LINK].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, 
		    "link");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_readdir(READDIR3args *args, READDIR3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_READDIR, 
	        xdr_READDIR3args, (char *)args, xdr_READDIR3res, (char *)reply, 
	        Nfs_timers[Ops[READDIR].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "readdir");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_readdirplus(READDIRPLUS3args *args, READDIRPLUS3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client,
	        NFSPROC3_READDIRPLUS, xdr_READDIRPLUS3args, (char *)args, 
	        xdr_READDIRPLUS3res, (char *)reply, 
	        Nfs_timers[Ops[READDIRPLUS].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client,
		    "readdirplus");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_fsstat(FSSTAT3args *args, FSSTAT3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_FSSTAT, 
	        xdr_FSSTAT3args, (char *)args, xdr_FSSTAT3res, (char *)reply, 
	        Nfs_timers[Ops[FSSTAT].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "fsstat");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_fsinfo(FSINFO3args *args, FSINFO3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_FSINFO, 
	        xdr_FSINFO3args, (char *)args, xdr_FSINFO3res, (char *)reply, 
	        Nfs_timers[Ops[FSINFO].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "fsinfo");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_pathconf(PATHCONF3args *args, PATHCONF3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_PATHCONF, 
	        xdr_PATHCONF3args, (char *)args, xdr_PATHCONF3res, (char *)reply, 
	        Nfs_timers[Ops[PATHCONF].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "pathconf");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_commit(COMMIT3args *args, COMMIT3res *reply)
{
	int rpc_stat;
 
	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC3_COMMIT, 
		xdr_COMMIT3args, (char *)args, xdr_COMMIT3res, (char *)reply, 
		Nfs_timers[Ops[COMMIT].call_class]);
	    if (rpc_stat == RPC_SUCCESS)
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "commit");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
create_3tmp_handles(void)
{
	int filenum;
	for (filenum = 0; filenum < NUMFILES; filenum++) {

		if(Io_files[filenum].fh_data == (sfs_fh_data *)0)
		{
			Io_files[filenum].fh_data = 
					calloc(1,sizeof(sfs_fh_data));
			Io_files[filenum].attributes2.type = NFNON;
			Io_files[filenum].attributes3.type = NF3NON;
		}
	}
}

static void
delete_3tmp_handles(void)
{
	int filenum;
	for (filenum = 0; filenum < NUMFILES; filenum++) {

		if(Io_files[filenum].fh_data != (sfs_fh_data *)0)
		{
			free(Io_files[filenum].fh_data);
			Io_files[filenum].fh_data= (sfs_fh_data *)0;
		}
	}
}
/* sfs_3_vld.c */
