#ifndef lint
static char sfs_c_vldSid[] = "@(#)sfs_2_vld.c 2.1     97/10/23";
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
 * ------------------------------ sfs_c_vld.c ---------------------------
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
 *      int compare_sattr(char *, char *, sattr *, fattr *)
 *      int compare_fattr(char *, char *, fattr *, fattr *)
 *      uint16_t sum(unsigned char *, uint_t)
 *      void validate_remove(void)
 *      void validate_cleanup(void)
 *      void validate_exit(void)
 *      void verror(int, ValMsgType, char *, ...)
 *
 *.Revision History
 *      04-Dec-91       Keith           Define string.h for SYSV/SVR4.
 *      25-Jun-91       Wiryaman        Created
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

#include <sys/types.h>
#include <sys/stat.h> 
 
#include <unistd.h>

#include "sfs_c_def.h"

extern struct hostent	*Server_hostent;

/*
 * -----------------------  External Definitions  -----------------------
 */

/* forward definitions for local routines */
/*
 * validate options
 * BATCH        - do complete pass of validation, reporting errors if any
 * VERBOSE      - prints step-by-step validation actions being performed
 * INTERACTIVE  - VERBOSE and if any errors encountered, ask to continue
 *                validation or not.
 */
#define VAL_BATCH       1
#define VAL_VERBOSE     2
#define VAL_INTERACTIVE 3

typedef enum {
        I = 1,
        W = 2,
        E = 3
} ValMsgType;

#define NUMREGFILES     7
#define NUMDIRS         5
#define NUMLINKS        5
#define NUMSYMLINKS     5
#define NUMFILES        NUMREGFILES + NUMDIRS + NUMLINKS + NUMSYMLINKS
#define NUMFRAGS        8

static void validate_init_rpc(void);
static void validate_exit(void);
static void validate_creation(void);
static void validate_attributes(void);
static void validate_read_write(void);
static void validate_rename(void);
static void validate_remove(void);
static void validate_cleanup(void);
static int compare_sattr(char *, char *, sattr *, fattr *);
static int compare_fattr(char *, char *, fattr *, fattr *);
static uint16_t sum(unsigned char *, uint_t);
static void verror(int, ValMsgType, char *, ...);

static void val_op_null(void);
static void val_op_getattr(fhandle_t *, attrstat *);
static void val_op_setattr(sattrargs *, attrstat *);
static void val_op_lookup(diropargs *, diropres *);
static void val_op_readlink(fhandle_t *, readlinkres *);
static void val_op_read(readargs *, readres *);
static void val_op_write(writeargs *, attrstat *);
static void val_op_create(createargs *, diropres *);
static void val_op_remove(diropargs *, nfsstat *);
static void val_op_rename(renameargs *, nfsstat *);
static void val_op_link(linkargs *, nfsstat *);
static void val_op_symlink(symlinkargs *, nfsstat *);
static void val_op_mkdir(mkdirargs *, diropres *);
static void val_op_rmdir(diropargs *, nfsstat *);
static void val_op_readdir(readdirargs *, readdirres *);
static void val_op_statfs(fhandle_t *, statfsres *);
static void create_tmp_handles(void);
static void delete_tmp_handles(void);

/*
 * ----------------------  Static Declarations ----------------------
 */

int Validate;

static int Validate_errors = 0;
static char Testdirname[SFS_MAXPATHLEN];    /* test dir component name */

/*
 * ----------------------  SFS Validation Suite  ----------------------
 */

void
Validate_ops(
    int         argc,
    char *      argv[])
{
    char *      valdir;
    CLIENT *    mount_client_ptr;

    if (argc > 1) {
        verror(VAL_BATCH, E, "Can only validate one directory at a time.\n");
        exit(1);
    }

    Num_io_files = NUMFILES;
    Cur_uid = Real_uid;
    nfs_version = NFS_VERSION;

    if (argc == 0)
        valdir = ".";
    else
        valdir = argv++[0];

    (void) sprintf(Testdirname, "%s/validatedir", valdir);

    do {
        verror(VAL_BATCH, I, "validating sfs on \"%s\" directory ...\n",
                valdir);

        init_fileinfo();
	create_tmp_handles();

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

	delete_tmp_handles();
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
                                        (uint32_t) NFS_VERSION,
                                        RPC_ANYSOCK, &Nfs_timers[0]);
 
	if (NFS_client == ((CLIENT *) NULL)) { 
		verror(VAL_BATCH, E, "portmap/nfsd server not responding\n"); 
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
    diropargs           arglp;
    createargs          argcr;
    mkdirargs           argmk;
    linkargs            argln;
    symlinkargs         argsl;
    char                sl_target_path[NFS_MAXPATHLEN];
    diropres            reply;
    readlinkres         rlreply;
    char                sym_data[NFS_MAXPATHLEN];

    for (filenum=0; filenum < NUMFILES ; filenum++) {

        Cur_file_ptr = &Io_files[filenum];
        sfs_gettime(&Cur_time);

        if (filenum < NUMREGFILES) {

            (void) sprintf(Cur_filename, Filespec, filenum);

            /* regular file creation */
            argcr.attributes.mode= (NFSMODE_REG | 0666);
            argcr.attributes.uid = Cur_uid;
            argcr.attributes.gid = Cur_gid;
            argcr.attributes.atime.seconds = (unsigned int) Cur_time.esec;
            argcr.attributes.atime.useconds = (unsigned int) Cur_time.usec;
            argcr.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
            argcr.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
            argcr.attributes.size = 0;
            (void) memmove((char *) &argcr.where.dir, (char *) &Export_dir.fh2,
			NFS_FHSIZE);
            argcr.where.name = Cur_filename;

            verror(VAL_VERBOSE, I, "validating create file %s ...\n",
					Cur_filename);
            val_op_create(&argcr, &reply);

            if (reply.status == NFS_OK) {
                Cur_file_ptr->state = Exists;
                (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file, NFS_FHSIZE);
                (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
                (void) memmove((char *) &Cur_file_ptr->attributes2,
				(char *) &reply.diropres_u.diropres.attributes,
				sizeof(Cur_file_ptr->attributes2));
                (void) compare_sattr(Ops[CREATE].name, Io_files[filenum].file_name,
                            &argcr.attributes, &Cur_file_ptr->attributes2);
            } else {
                Cur_file_ptr->state = Nonexistent;
                errno = (int)reply.status;
                verror(VAL_BATCH, E, "create %s failed: %m\n", Cur_filename);
                /*
                 * An error in file creation is fatal, because we use the
                 * created files to validate the other operations.
                 */
                validate_exit();
            }

        } else if (filenum < NUMREGFILES + NUMDIRS) {

            (void) sprintf(Cur_filename, Dirspec, filenum);

            /* directory creation */
            argmk.attributes.mode= (NFSMODE_DIR | 0777);
            argmk.attributes.uid = Cur_uid;
            argmk.attributes.gid = Cur_gid;
            argmk.attributes.size = 0xFFFFFFFF;
            argmk.attributes.atime.seconds = (unsigned int) Cur_time.esec;
            argmk.attributes.atime.useconds = (unsigned int) Cur_time.usec;
            argmk.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
            argmk.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
            (void) memmove((char *) &argmk.where.dir, (char *) &Export_dir.fh2,
			NFS_FHSIZE);
            argmk.where.name = Cur_filename;

            verror(VAL_VERBOSE, I, "validating mkdir %s ...\n", Cur_filename);
            val_op_mkdir(&argmk, &reply);

            if (reply.status == NFS_OK) {
                Cur_file_ptr->state = Exists;
                (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file,
			NFS_FHSIZE);
                (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
                (void) memmove((char *) &Cur_file_ptr->attributes2,
				(char *) &reply.diropres_u.diropres.attributes,
				sizeof(Cur_file_ptr->attributes2));
                (void) compare_sattr(Ops[MKDIR].name, Io_files[filenum].file_name,
                                &argmk.attributes, &Cur_file_ptr->attributes2);
            } else {
                Cur_file_ptr->state = Nonexistent;
                verror(VAL_BATCH, W, "mkdir %s failed:%m\n", Cur_filename);
            }

        } else if(filenum < NUMREGFILES + NUMDIRS + NUMLINKS ) {

            (void) sprintf(Cur_filename, Filespec, filenum);

            /* hard link creation */
            target_filenum = NUMFILES-NUMSYMLINKS-1-filenum;
            (void) memmove((char *) &argln.from,
			(char *) &Io_files[target_filenum].fh2, NFS_FHSIZE);
            (void) memmove((char *) &argln.to.dir, (char *) &Export_dir.fh2,
			NFS_FHSIZE);
            argln.to.name = Cur_filename;

            verror(VAL_VERBOSE, I, "validating link %s %s ...\n",
				Io_files[target_filenum].file_name, Cur_filename);
            val_op_link(&argln, &reply.status);

            if (reply.status == NFS_OK) {
                Cur_file_ptr->state = Exists;
                (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &Io_files[target_filenum].fh2,
			NFS_FHSIZE);
                (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
                Cur_file_ptr->attributes2 = Io_files[target_filenum].attributes2;
                Io_files[target_filenum].attributes2.nlink++;
                Cur_file_ptr->attributes2.nlink++;
            } else {
                Cur_file_ptr->state = Nonexistent;
                verror(VAL_BATCH, W, "link %s failed: %m\n", Cur_filename);
            }

        } else {

            (void) sprintf(Cur_filename, Symspec, filenum);

            /* symbolic link creation */
            target_filenum = NUMFILES-1-filenum;
            (void) memmove((char *) &argsl.from.dir, (char *) &Export_dir.fh2,
			NFS_FHSIZE);
            argsl.from.name = Cur_filename;
            (void) sprintf(sl_target_path,
                           "./%s", Io_files[target_filenum].file_name);
            argsl.attributes.size = strlen(sl_target_path);
            argsl.to = sl_target_path;
            argsl.attributes.mode = (NFSMODE_LNK | 0777);
            argsl.attributes.uid = Cur_uid;
            argsl.attributes.gid = Cur_gid;
            argsl.attributes.atime.seconds = (unsigned int)Cur_time.esec;
            argsl.attributes.atime.useconds = (unsigned int)Cur_time.usec;
            argsl.attributes.mtime.seconds = (unsigned int)Cur_time.esec;
            argsl.attributes.mtime.useconds = (unsigned int)Cur_time.usec;

            verror(VAL_VERBOSE, I, "validating symlink %s %s ...\n",
					sl_target_path, Cur_filename);
            val_op_symlink(&argsl, &reply.status);

            if (reply.status == NFS_OK) {
                Cur_file_ptr->state = Exists;

                /* do a lookup to get file handle and attributes */
                (void) memmove((char *) &arglp.dir, (char *) &Export_dir.fh2,
			NFS_FHSIZE);
                arglp.name = Cur_filename;

                val_op_lookup(&arglp, &reply);

                if (reply.status == NFS_OK) {
                    (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file, NFS_FHSIZE);
                    (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
                    Cur_file_ptr->attributes2 =
                                        reply.diropres_u.diropres.attributes;
                    (void) compare_sattr(Ops[SYMLINK].name,
                                        Io_files[filenum].file_name,
                                        &argsl.attributes,
                                        &Cur_file_ptr->attributes2);
                } else {
                    verror(VAL_BATCH, W, "lookup %s failed: %m\n",
						Cur_filename);
                    continue;
                }

                /* validate readlink */
                rlreply.readlinkres_u.data = sym_data;

                verror(VAL_VERBOSE, I, "validating readlink %s ...\n",
						Cur_filename);
                val_op_readlink(&Cur_file_ptr->fh2, &rlreply);

                if (rlreply.status == NFS_OK) {
                    sym_data[rlreply.readlinkres_u.len] = '\0';
                    if (strcmp(sl_target_path, sym_data)) {
                        verror(VAL_BATCH, W,
                            "readlink %s error, result = %s, should be %s\n",
                            Cur_filename, rlreply.readlinkres_u.data,
                            sl_target_path);
                    }
                } else {
                    verror(VAL_BATCH, W, "readlink %s failed:%m\n",
                            Cur_filename);
                }

            } else {
                Cur_file_ptr->state = Nonexistent;
                verror(VAL_BATCH, W, "symlink %s failed: %m\n",
                        Cur_filename);
            }
        }
    } /* end for each file */

} /* validate_creation */


static void
validate_attributes(void)
{
    int                 filenum;
    diropargs           arglp;
    diropres            lreply;
    fhandle_t           fh;
    sattrargs           argsa;
    attrstat            areply;

    /* validate fsstat */

    /* validate lookup */
    for (filenum = 0; filenum < NUMFILES; filenum++) {
        (void) memmove((char *) &arglp.dir, (char *) &Export_dir.fh2,
			NFS_FHSIZE);
        arglp.name = Io_files[filenum].file_name;

        verror(VAL_VERBOSE, I, "validating lookup %s ...\n",
                Io_files[filenum].file_name);
        val_op_lookup(&arglp, &lreply);

        if (lreply.status == NFS_OK) {
            if (memcmp((char *) &(Io_files[filenum].fh2),
                     (char *) &(lreply.diropres_u.diropres.file),
                        NFS_FHSIZE)) {
                verror(VAL_BATCH, W, "lookup %s: file handle mismatch\n",
                        Io_files[filenum].file_name);
            }
            (void) compare_fattr(Ops[LOOKUP].name, Io_files[filenum].file_name,
                          &Io_files[filenum].attributes2,
                          &lreply.diropres_u.diropres.attributes);
        } else {
            verror(VAL_BATCH, W, "lookup %s failed:%m\n",
                    Io_files[filenum].file_name);
        }
    }

    /* validate getattr */
    for (filenum = 0; filenum < NUMFILES; filenum++) {
        (void) memmove((char *) &fh, (char *) &Io_files[filenum].fh2,
			NFS_FHSIZE);

        verror(VAL_VERBOSE, I, "validating getattr %s ...\n",
                Io_files[filenum].file_name);
        val_op_getattr(&fh, &areply);

        if (areply.status == NFS_OK) {
            (void) compare_fattr(Ops[GETATTR].name, Io_files[filenum].file_name,
                          &Io_files[filenum].attributes2,
                          &areply.attrstat_u.attributes);
        } else {
            verror(VAL_BATCH, W, "getattr %s failed: %m\n",
                    Io_files[filenum].file_name);
        }
    }

    /*validate setattr */
    for (filenum = 0; filenum < NUMFILES; filenum++) {
        sfs_gettime(&Cur_time);
        if (filenum >= NUMREGFILES && filenum < NUMREGFILES + NUMDIRS)
          argsa.attributes.mode= 0777;
        else
          argsa.attributes.mode= 0666;
        argsa.attributes.uid  = 0xFFFFFFFF;
        argsa.attributes.gid  = 0xFFFFFFFF;
        argsa.attributes.size = 0xFFFFFFFF;
        argsa.attributes.atime.seconds =  (unsigned int)Cur_time.esec;
        argsa.attributes.atime.useconds = (unsigned int)Cur_time.usec;
        argsa.attributes.mtime.seconds =  (unsigned int)Cur_time.esec;
        argsa.attributes.mtime.useconds = (unsigned int)Cur_time.usec;
        (void) memmove((char *) &argsa.file, (char *) &Io_files[filenum].fh2,
			NFS_FHSIZE);

        verror(VAL_VERBOSE, I, "validating setattr %s ...\n",
                Io_files[filenum].file_name);
        val_op_setattr(&argsa, &areply);

        if (areply.status == NFS_OK) {
            if (argsa.attributes.mode != areply.attrstat_u.attributes.mode){
                argsa.attributes.mode |=
                        (Io_files[filenum].attributes2.mode & NFSMODE_FMT);
                argsa.attributes.mode &= Io_files[filenum].attributes2.mode;
            }
            Io_files[filenum].attributes2 = areply.attrstat_u.attributes;
            (void) compare_sattr(Ops[SETATTR].name, Io_files[filenum].file_name,
                          &argsa.attributes, &areply.attrstat_u.attributes);

            val_op_getattr(&argsa.file, &areply);

            if (areply.status == NFS_OK) {
                (void) compare_fattr(Ops[GETATTR].name, Io_files[filenum].file_name,
                              &Io_files[filenum].attributes2,
                              &areply.attrstat_u.attributes);
            } else {
                verror(VAL_BATCH, W, "getattr %s failed: %m\n",
                        Io_files[filenum].file_name);
            }
        } else {
            verror(VAL_BATCH, W, "setattr %s failed: %m\n",
                    Io_files[filenum].file_name);
        }
    }

} /* validate_attributes */


static void
validate_read_write(void)
{
    struct {
        uint16_t  sum;                    /* checksum of data */
        uint16_t  len;                    /* length of len and data */
        char            data[DEFAULT_MAX_BUFSIZE - 2 * sizeof(uint16_t)];
    } block;
    writeargs           argwr;
    attrstat            wrreply;
    readargs            argrd;
    readres             rdreply;
    int                 maxblks;
    int                 maxfiles;
    uint_t		i;
    int                 numfiles;
    int                 filenum;
    int                 blocknum;
    readdirargs         argrdir;
    readdirres          rdirreply;
    int                 entry_cnt = 9;
    entry               entry_stream[9];
    char                name_stream[9 * SFS_MAXNAMLEN];

    /* validate write */

    /* get the maximum number of blocks sfs will write */
    maxblks = Io_dist_ptr->max_bufs;
    maxfiles = maxblks > NUMREGFILES ? NUMREGFILES : maxblks;

    /* write maxblks - filenum + 1 blocks to each regular file */
    argwr.offset = 0;
    argwr.beginoffset = 0; /* unused */
    argwr.totalcount = 0; /* unused */
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
                            (Bytes_per_block/Kb_per_block)) - (sizeof(block.len)
                             + sizeof(block.sum));
            } else {
                block.len = Bytes_per_block - (sizeof(block.len)
                            + sizeof(block.sum));
            }
            block.sum = sum((unsigned char *) &block.len,
                            block.len + sizeof(block.len));

            (void) memmove((char *) &argwr.file,
			(char *) &Io_files[filenum].fh2, NFS_FHSIZE);
            argwr.data.data_len = block.len +
                                  sizeof(block.len) + sizeof(block.sum);

            verror(VAL_VERBOSE, I,
                    "validating write %d bytes @ offset %d to %s ...\n",
                    argwr.data.data_len, argwr.offset,
                    Io_files[filenum].file_name);

            val_op_write(&argwr, &wrreply);

            if (wrreply.status == NFS_OK) {
                (void) compare_fattr(Ops[WRITE].name, Io_files[filenum].file_name,
                              &Io_files[filenum].attributes2,
                              &wrreply.attrstat_u.attributes);
                Io_files[filenum].attributes2 = wrreply.attrstat_u.attributes;
            } else {
                verror(VAL_BATCH, W, "write %s failed: %m\n",
                        Io_files[filenum].file_name);
            }
        }
        argwr.offset += Bytes_per_block;
    }

    /* validate read */

    for (filenum = 0; filenum < maxfiles; filenum++) {
        (void) memmove((char *) &argrd.file, (char *) &Io_files[filenum].fh2,
			NFS_FHSIZE);
        argrd.offset = 0;
        argrd.count = 0;
        rdreply.readres_u.reply.data.data_len = 0;
        maxblks = Io_files[filenum].attributes2.size / Bytes_per_block;
        for (blocknum = 0; blocknum <= maxblks; blocknum ++) {

            if (argrd.count != rdreply.readres_u.reply.data.data_len) {
                argrd.count -= rdreply.readres_u.reply.data.data_len;
                rdreply.readres_u.reply.data.data_val = (char *)&block +
                                        rdreply.readres_u.reply.data.data_len;
                blocknum--;
            } else {
                if (blocknum < maxblks)
                    argrd.count = Bytes_per_block;
                else
                    argrd.count = (maxfiles - filenum)
                                  * (Bytes_per_block/Kb_per_block);
                rdreply.readres_u.reply.data.data_val = (char *)&block;
            }
            argrd.totalcount = argrd.count; /* unused */

            verror(VAL_VERBOSE, I,
                   "validating read %d bytes @ offset %d from %s ...\n",
                    argrd.count, argrd.offset,
                    Io_files[filenum].file_name);

            val_op_read(&argrd, &rdreply);

            if (rdreply.status == NFS_OK) {
                (void) compare_fattr(Ops[READ].name, Io_files[filenum].file_name,
                              &Io_files[filenum].attributes2,
                              &rdreply.readres_u.reply.attributes);
                Io_files[filenum].attributes2 =
                                        rdreply.readres_u.reply.attributes;
                argrd.offset += rdreply.readres_u.reply.data.data_len;
            } else {
                verror(VAL_BATCH, W, "read %s failed: %m\n",
                        Io_files[filenum].file_name);
            }

            if (argrd.count ==
                (block.sum != sum((unsigned char *) &block.len,
                                  block.len + sizeof(block.len)))) {
                verror(VAL_BATCH, W, "read %s checksum mismatch\n",
                        Io_files[filenum].file_name);
            }
        }
    }

    /* validate readdir */
    numfiles = 0;

    (void) memmove((char *) &argrdir.dir, (char *) &Export_dir.fh2, NFS_FHSIZE);
    (void) memset((char *) argrdir.cookie, '\0', NFS_COOKIESIZE);
    argrdir.count = DEFAULT_MAX_BUFSIZE;

    (void) memset((char *) &rdirreply, '\0', sizeof(rdirreply));
    rdirreply.readdirres_u.reply.max_entries = entry_cnt;
    rdirreply.readdirres_u.reply.entries = entry_stream;
    for (i = 0; i < entry_cnt; i++) {
        entry_stream[i].name = &name_stream[i * SFS_MAXNAMLEN];
    }

    do {
        verror(VAL_VERBOSE, I, "validating readdir %d entries of %s ...\n",
                rdirreply.readdirres_u.reply.max_entries, Testdirname);
        val_op_readdir(&argrdir, &rdirreply);

        if (rdirreply.status == NFS_OK) {
            for (i = 0; i < rdirreply.readdirres_u.reply.max_entries; i++) {
                numfiles++;
                entry_stream[i].name[entry_stream[i].name_len] = '\0';
                if (!entry_stream[i].valid) {
                    verror(VAL_BATCH, W,
                          "readdir %s error: entry %d (%s) is marked invalid\n",
                            Testdirname, i, entry_stream[i].name);
                }
                if ((!strcmp(entry_stream[i].name, ".")) ||
                    (!strcmp(entry_stream[i].name, "..")) ) {
                    numfiles--;
                    continue;
                }
                for (filenum = 0; filenum < NUMFILES; filenum++) {
                    if (!strcmp(entry_stream[i].name, Io_files[filenum].file_name)) {
                        if (entry_stream[i].fileid !=
                                    Io_files[filenum].attributes2.fileid) {
                            verror(VAL_BATCH, E,
                                  "readdir %s error: file %s fileid mismatch\n",
                                    Testdirname, entry_stream[i].name);

                            verror(VAL_BATCH, W,
                                   "   fileid: got = %d, original = %d\n",
                                    entry_stream[i].fileid,
				    Io_files[filenum].attributes2.fileid);
                        }
                        break;
                    }
                }
                if (filenum == NUMFILES) {
                    verror(VAL_BATCH, W,
               "readdir %s error: file \"%s\" was not created within sfs\n",
                            Testdirname, entry_stream[i].name);
                }
            }

            if (i < entry_cnt && entry_stream[i].valid) {
                verror(VAL_BATCH, W,
                        "readdir %s error: valid entries exceeded maximum\n",
                        Testdirname);
            }

        } else {
            verror(VAL_BATCH, W, "readdir %s failed: %m\n", Testdirname);
        }

        (void) memmove((char *)argrdir.cookie,
        (char *)entry_stream[rdirreply.readdirres_u.reply.max_entries-1].cookie,
			NFS_COOKIESIZE);

    } while (rdirreply.readdirres_u.reply.eof == 0);

    if (numfiles != NUMFILES) {
        verror(VAL_BATCH, W,
                        "readdir %s error: the number of files found\n\
does not match with the number of files created within sfs\n", Testdirname);
    }

} /* validate_read_write */


static void
validate_rename(void)
{
    renameargs  argrn;
    nfsstat     rnreply;
    int         filenum;
    char        newname[SFS_MAXNAMLEN];
    int         rncount = 0;

    (void) memmove((char *) &argrn.from.dir, (char *) &Export_dir.fh2, NFS_FHSIZE);
    (void) memmove((char *) &argrn.to.dir, (char *) &Export_dir.fh2, NFS_FHSIZE);

    for (filenum=0; filenum < NUMFILES; filenum++) {
        if (Io_files[filenum].state != Exists)
            continue;

        rncount++;
        (void) sprintf(newname, "n%s", Io_files[filenum].file_name);
        argrn.from.name = Io_files[filenum].file_name;
        argrn.to.name = newname;

        verror(VAL_VERBOSE, I, "validating rename %s %s ...\n",
                argrn.from.name, argrn.to.name);

        val_op_rename(&argrn, &rnreply);

        if (rnreply == NFS_OK) {
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
    fattr *     attr1,
    fattr *     attr2)
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
                verror(VAL_BATCH, I, "    mode: current = %7o, previous =  %7o\n",
                        attr2->mode, attr1->mode);
                attr1->mode =  attr2->mode;
                ret = FALSE;
        }
        else {
                if (ret) {
                        verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                            fname, op);
                }
                verror(VAL_BATCH, E, "    mode: current = %d, previous =  %d\n",
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
                verror(VAL_BATCH, I, "    nlink: current = %d, previous =  %d\n",
                        attr2->nlink, attr1->nlink);
                attr1->nlink =  attr2->nlink;
                ret = FALSE;
        }
        else {
                if (ret) {
                        verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                            fname, op);
                }
                verror(VAL_BATCH, E, "    nlink: current = %d, previous =  %d\n",
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
                verror(VAL_BATCH, I, "    uid: current = %d, previous =  %d\n",
                        attr2->uid, attr1->uid);
                attr1->uid =  attr2->uid;
                ret = FALSE;
        }
        else {
                if (ret) {
                        verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                            fname, op);
                }
                verror(VAL_BATCH, E, "    uid: current = %d, previous =  %d\n",
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
                verror(VAL_BATCH, I, "    gid: current = %d, previous =  %d\n",
                        attr2->gid, attr1->gid);
                attr1->gid =  attr2->gid;
                ret = FALSE;
        }
        else {
                if (ret) {
                        verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                            fname, op);
                }
                verror(VAL_BATCH, E, "    gid: current = %d, previous =  %d\n",
                        attr2->gid, attr1->gid);
                ret = FALSE;
                flag_error = TRUE;
        }
    }

    if (attr1->size != attr2->size) {
        if (strcmp(op, Ops[WRITE].name)) {
            if (attr1->size == (unsigned int)0xFFFFFFFF) {
                prev_warn = TRUE;
                if (ret) {
                        verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
                                fname, op);
                }
                verror(VAL_BATCH, I, "    size: current = %d, previous =  %d\n",
                        attr2->size, attr1->size);
                attr1->size =  attr2->size;
                ret = FALSE;
            }
            else {
                if (ret) {
                        verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                            fname, op);
                }
                verror(VAL_BATCH, E, "    size: current = %d, previous =  %d\n",
                        attr2->size, attr1->size);
                ret = FALSE;
                flag_error = TRUE;
            }
        }
    }

    if (attr1->blocksize != attr2->blocksize) {
        if (attr1->blocksize == (unsigned int)0xFFFFFFFF) {
             prev_warn = TRUE;
             if (ret) {
                     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
                             fname, op);
             }
             verror(VAL_BATCH, I, "    blocksize: current = %d, previous =  %d\n",
                     attr2->blocksize, attr1->blocksize);
             attr1->blocksize =  attr2->blocksize;
             ret = FALSE;
        }
        else {
             if (ret) {
                     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                         fname, op);
             }
             verror(VAL_BATCH, E, "    blocksize: current = %d, previous =  %d\n",
                     attr2->blocksize, attr1->blocksize);
             ret = FALSE;
                flag_error = TRUE;
        }
    }


    /* compare rdev only if type == NFCHR or NFBLK */
    if ((attr1->type == NFCHR || attr1->type == NFBLK) &&
        attr1->rdev != attr2->rdev) {
        if (attr1->rdev == (unsigned int)0xFFFFFFFF) {
             prev_warn = TRUE;
             if (ret) {
                     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
                             fname, op);
             }
             verror(VAL_BATCH, I, "    rdev: current = %d, previous =  %d\n",
                     attr2->rdev, attr1->rdev);
             attr1->rdev =  attr2->rdev;
             ret = FALSE;
        }
        else {
             if (ret) {
                     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                         fname, op);
             }
             verror(VAL_BATCH, E, "    rdev: current = %d, previous =  %d\n",
                     attr2->rdev, attr1->rdev);
             ret = FALSE;
                flag_error = TRUE;
        }
    }


    /*
     * The NFS specification does not require that the number of blocks
     * associated with a file remain constant.  Certain file systems
     * may pre-allocate more blocks than necessary then trim them
     * back ("garbage collect") or even blow holes in files that have
     * all zero blocks.
     * We must check that we never get back -1.
     */
    if (attr1->blocks != attr2->blocks) {
        if (strcmp(op, Ops[WRITE].name)) {
            if (attr1->blocks == (unsigned int)0xFFFFFFFF) {
                 prev_warn = TRUE;
                 if (ret) {
                     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
                             fname, op);
                 }
                verror(VAL_BATCH, I, "    blocks: current = %d, previous =  %d\n",
                         attr2->blocks, attr1->blocks);
                 attr1->blocks =  attr2->blocks;
                 ret = FALSE;
            }
        }
    }


    if (attr1->fsid != attr2->fsid) {
        if (attr1->fsid == (unsigned int)0xFFFFFFFF) {
             prev_warn = TRUE;
             if (ret) {
                     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
                             fname, op);
             }
             verror(VAL_BATCH, I, "    fsid: current = %d, previous =  %d\n",
                     attr2->fsid, attr1->fsid);
             attr1->fsid =  attr2->fsid;
             ret = FALSE;
        }
        else {
             if (ret) {
                     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                         fname, op);
             }
             verror(VAL_BATCH, E, "    fsid: current = %d, previous =  %d\n",
                     attr2->fsid, attr1->fsid);
             ret = FALSE;
                flag_error = TRUE;
        }
    }

    if (attr1->fileid != attr2->fileid) {
        if (attr1->fileid == (unsigned int)0xFFFFFFFF) {
             prev_warn = TRUE;
             if (ret) {
                     verror(VAL_BATCH, I, "%s: %s attribute mismatch\n",
                             fname, op);
             }
             verror(VAL_BATCH, I, "    fileid: current = %d, previous =  %d\n",
                     attr2->fileid, attr1->fileid);
             attr1->fileid =  attr2->fileid;
             ret = FALSE;
        }
        else {
             if (ret) {
                     verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                         fname, op);
             }
             verror(VAL_BATCH, E, "    fileid: current = %d, previous =  %d\n",
                     attr2->fileid, attr1->fileid);
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


static int
compare_sattr(
    char *      op,
    char *      fname,
    sattr *     attr1,
    fattr *     attr2)
{
    int         ret = TRUE;
    char        msg[80];

    msg[0] = '\0';

    if (attr1->mode != (unsigned int)0xFFFFFFFF &&
        (attr1->mode & NFSMODE_MASK) != (attr2->mode & NFSMODE_MASK)) {
        if (ret) {
            verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                    fname, op);

        }
        verror(VAL_BATCH, E, "    mode: returned = %7o, specified = %7o\n",
                attr2->mode, attr1->mode);
        ret = FALSE;
    }

    /*
     * Check for user "nobody", UID -2, which may be untranslated from
     * sixteen-bit two's complement.
     */
    if (attr1->uid != (unsigned int)0xFFFFFFFF && attr1->uid != attr2->uid &&
		!((attr2->uid == (unsigned int)0xFFFFFFFE ||
		attr2->uid == 65534) &&
			attr1->uid == 0)) {
        if (ret) {
            verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                    fname, op);
        }
        if (attr1->uid == 0)
            (void) strcat(msg," (is root UID mapped to other UID?)");
        verror(VAL_BATCH, E, "    uid: returned = %d, specified = %d %s\n",
                attr2->uid, attr1->uid, msg);
        ret = FALSE;
    }
    if (attr1->gid != (unsigned int)0xFFFFFFFF &&
			attr1->gid != attr2->gid &&
			attr2->gid != 0) {
/*
   if (attr1->gid != 0xFFFFFFFF && attr1->gid != attr2->gid) {
*/
        if (ret) {
            verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                    fname, op);
        }
        verror(VAL_BATCH, E, "    gid: returned = %d, specified = %d\n",
                attr2->gid, attr1->gid);
        ret = FALSE;
    }

    if (attr1->size != (unsigned int)0xFFFFFFFF &&
			attr1->size != attr2->size) {
        if (ret) {
            verror(VAL_BATCH, E, "%s: %s attribute mismatch\n",
                    fname, op);
        }
        verror(VAL_BATCH, E, "    size: returned = %d, specified = %d\n",
                attr2->size, attr1->size);
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
    uint_t              len)
{
    uint16_t      cksum;

    cksum = 0;
    for (; len--; buf++) {
        if (cksum & 1)
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
    diropargs   args;
    nfsstat     reply;
    diropres    lreply;
    int         filenum;
    char *      op;

    (void) memmove((char *) &args.dir, (char *) &Export_dir.fh2, NFS_FHSIZE);

    for (filenum = 0; filenum < NUMFILES; filenum++) {

        if (Io_files[filenum].state != Exists)
            continue;

        args.name = Io_files[filenum].file_name;

        if (Io_files[filenum].attributes2.type == NFDIR) {
            op = Ops[RMDIR].name;
            verror(VAL_VERBOSE, I, "validating rmdir %s ...\n",
                    args.name);
            val_op_rmdir(&args, &reply);
        } else {
            op = Ops[REMOVE].name;
            verror(VAL_VERBOSE, I, "validating remove %s ...\n",
                    args.name);
            val_op_remove(&args, &reply);
        }

        if (reply == NFS_OK) {
            /* make sure the file is removed from the directory */
            val_op_lookup(&args, &lreply);

            if (lreply.status == NFSERR_NOENT) {
                Io_files[filenum].state = Nonexistent;
            } else if (lreply.status == NFS_OK) {
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
    int         repeat;
    char *	sp;

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
        (void) vprintf(buf, ap);
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
	    rpc_stat = clnt_call(NFS_client, NFSPROC_NULL, 
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
val_op_getattr(fhandle_t *args, attrstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_GETATTR, 
	    xdr_getattr, (char *)args, xdr_getattr, (char *)reply, 
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
val_op_setattr(sattrargs *args, attrstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_SETATTR, 
	        xdr_setattr, (char *)args, xdr_setattr, (char *)reply, 
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
val_op_lookup(diropargs *args, diropres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_LOOKUP, 
	        xdr_lookup, (char *)args, xdr_lookup, (char *)reply, 
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
val_op_readlink(fhandle_t *args, readlinkres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_READLINK, 
	        xdr_readlink, (char *)args, xdr_readlink, (char *)reply, 
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
val_op_read(readargs *args, readres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_READ, 
	        xdr_read, (char *)args, xdr_read, (char *)reply, 
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
val_op_write(writeargs *args, attrstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client,
		NFSPROC_WRITE, xdr_write, (char *)args, xdr_write, (char *)reply,
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
val_op_create(createargs *args, diropres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client,
	    NFSPROC_CREATE, xdr_create, (char *)args, xdr_create, (char *)reply, 
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
val_op_remove(diropargs *args, nfsstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client,
		NFSPROC_REMOVE, xdr_remove, (char *)args, xdr_remove, (char *)reply, 
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
val_op_rename(renameargs *args, nfsstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_RENAME, 
	        xdr_rename, (char *)args, xdr_rename, (char *)reply, 
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
val_op_link(linkargs *args, nfsstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_LINK, 
	        xdr_link, (char *)args, xdr_link, (char *)reply, 
	        Nfs_timers[Ops[LINK].call_class]);
            if (rpc_stat == RPC_SUCCESS)
                break;
            if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "link");
		Validate_errors++;
		validate_exit();
	    }
	}
}

static void
val_op_symlink(symlinkargs *args, nfsstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_SYMLINK, 
	        xdr_symlink, (char *)args, xdr_symlink, (char *)reply, 
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
val_op_mkdir(mkdirargs *args, diropres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_MKDIR, 
	        xdr_mkdir, (char *)args, xdr_mkdir, (char *)reply, 
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
val_op_rmdir(diropargs *args, nfsstat *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_RMDIR, 
	        xdr_rmdir, (char *)args, xdr_rmdir, (char *)reply, 
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
val_op_readdir(readdirargs *args, readdirres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_READDIR, 
	        xdr_readdir, (char *)args, xdr_readdir, (char *)reply, 
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
val_op_statfs(fhandle_t *args, statfsres *reply)
{
	int rpc_stat;

	/* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_STATFS, 
	        xdr_statfs, (char *)args, xdr_statfs, (char *)reply, 
	        Nfs_timers[Ops[FSSTAT].call_class]);
            if (rpc_stat == RPC_SUCCESS)
                break;
            if (rpc_stat != RPC_TIMEDOUT) {
		clnt_perror(NFS_client, "statfs");
		Validate_errors++;
		validate_exit();
	    }
	}
}
static void
create_tmp_handles(void)
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
delete_tmp_handles()
{
	int filenum;
	for (filenum = 0; filenum < NUMFILES; filenum++) {

		if(Io_files[filenum].fh_data != (sfs_fh_data *)0)
		{
			free(Io_files[filenum].fh_data);
			Io_files[filenum].fh_data=(sfs_fh_data *)0;
		}
	}
}








