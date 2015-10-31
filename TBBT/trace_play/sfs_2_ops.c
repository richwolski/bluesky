#ifndef lint
static char sfs_c_opsSid[] = "@(#)sfs_2_ops.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_ops.c ---------------------
 *
 *      RPC routines to implement the NFS protocol.
 *
 *.Local Routines
 *	int op_null(void)
 *	int op_getattr(void)
 *	int op_setattr(int)
 *	int op_nosys(void)
 *	int op_lookup(void)
 *	int op_readlink(void)
 *	int op_read(int)
 *	int op_write(int, int, int)
 *	int op_create(void)
 *	int op_remove(void)
 *	int op_rename(void)
 *	int op_link(void)
 *	int op_symlink(void)
 *	int op_mkdir(void)
 *	int op_rmdir(void)
 *	int op_readdir(void)
 *	int op_fsstat(void)
 *
 *.Revision_History
 *	20-Apr-92	Wittle		Fix i/o offsets randomization.
 *	05-Jan-92	Pawlowski	Added hooks in for raw data dump.
 *      04-Dec-91	Keith		Define string.h for SYSV/SVR4.
 *	28-Nov-91	Teelucksingh	ANSI C
 *	01-Aug-91	Santa Wiryaman  fix declaration of sfs_srandom()
 *					and sfs_random()
 *	25-Jun-91	Santa Wiryaman  op_rmdir bug fix: when reply==NFS_OK
 *					Cur_file_ptr->state = Nonexistent
 *	17-May-90	Richard Bean	Created.
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
 
#include <sys/types.h>
#include <sys/stat.h> 

#include <unistd.h>

#include "sfs_c_def.h"

/*
 * --------------------  Local NFS ops function --------------------
 */
static int op_null(void);
static int op_getattr(void);
static int op_setattr(int);
static int op_lookup(void);
static int op_readlink(void);
static int op_read(int);
static int op_write(int, int, int);
static int op_create(void);
static int op_remove(void);
static int op_rename(void);
static int op_link(void);
static int op_symlink(void);
static int op_mkdir(void);
static int op_rmdir(void);
static int op_readdir(void);
static int op_fsstat(void);
static int op_nosys(void);
static char *nfs2_strerror(int);

/*
 * --------------------  NFS ops vector --------------------
 */
/*
 * per operation information
 */
static sfs_op_type nfsv2_Ops[] = {

/* name      mix   function        op    call  no  req  req  req  results */
/*           pcnt                 class  targ call pcnt cnt  targ         */

 { "null",        0, op_null,      Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "getattr",    26, op_getattr,   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "setattr",     1, op_setattr,   Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "root",        0, op_nosys,     Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "lookup",     36, op_lookup,    Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "readlink",    7, op_readlink,  Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "read",       14, op_read,      Read,    0,  0,  0.0,  0,   0,  { 0, }},
 { "wrcache",     0, op_nosys,     Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "write",       7, op_write,     Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "create",      1, op_create,    Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "remove",      1, op_remove,    Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "rename",      0, op_rename,    Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "link",        0, op_link,      Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "symlink",     0, op_symlink,   Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "mkdir",       0, op_mkdir,     Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "rmdir",       0, op_rmdir,     Write,   0,  0,  0.0,  0,   0,  { 0, }},
 { "readdir",     6, op_readdir,   Read,    0,  0,  0.0,  0,   0,  { 0, }},
 { "fsstat",      1, op_fsstat,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "access",	  0, op_nosys,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "commit",	  0, op_nosys,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "fsinfo",	  0, op_nosys,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "mknod",	  0, op_nosys,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "pathconf",	  0, op_nosys,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "readdirplus", 0, op_nosys,	   Lookup,  0,  0,  0.0,  0,   0,  { 0, }},
 { "TOTAL",     100, 0,            Lookup,  0,  0,  0.0,  0,   0,  { 0, }}
};

sfs_op_type *Ops;

/*
 * --------------------  RPC routines for NFS protocol --------------------
 */

void
init_ops(void)
{ 
    Ops = nfsv2_Ops;
    nfs_version = NFS_VERSION;
} 

/*
 * The routines below attempt to do over-the-wire operations.
 * Each op tries to cause one or more of a particular
 * NFS operation to go over the wire.  OPs return the success
 * of their NFS call(s).  Each OP records how many calls it
 * actually made in global data.
 *
 * An array of file information is kept for files existing in
 * the test directory.  File handles, attributes, names, etc
 * are stored in this array.
 *
 */

static int
op_nosys(void)
{
    /*
     * This is a generic catcher for operations that either don't
     * exist or were never implemented.  We will be
     * kind and simply mark it as a bad call.
     */
    Ops[TOTAL].results.bad_calls++;
    return(0);

} /* op_nosys */


static int
op_null(void)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int 		ret;		/* ret val == call success */

    op_ptr = &Ops[NULLCALL];
    ret = 0;

    /* make the call */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_NULL,
			xdr_void, (char *)0,
			xdr_void, (char *)0,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr,
		    "%s: null_op call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_null */


static int
op_getattr(void)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    fhandle_t		fh;		/* fh to do op on */
    attrstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;		/* ret val == call success */

    op_ptr = &Ops[GETATTR];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &fh, (char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);

    /* make the call */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_GETATTR,
			xdr_getattr, (char *) &fh,
			xdr_getattr, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {
	    Cur_file_ptr->attributes2 = reply.attrstat_u.attributes;
	    Cur_file_ptr->size = fh_size(Cur_file_ptr);
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: getattr call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr,
		    "%s: getattr call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_getattr */


/*
 * perform an RPC setattr operation.  If 'truncate_size' is non-negative,
 * truncate the file to that size.
 */
static int
op_setattr(
    int		truncate_size)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    sattrargs		args;
    attrstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;		/* ret val == call success */

    op_ptr = &Ops[SETATTR];
    ret = 0;

    /* set up the arguments */
    args.attributes.mode = 0666;
    args.attributes.uid = (unsigned int) -1;
    args.attributes.gid = (unsigned int) -1;
    args.attributes.size = (unsigned int) -1;
    args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
    args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
    (void) memmove((char *) &args.file, (char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);

    /* handle file truncations */
    if (truncate_size >= 0) {
	if (truncate_size > Cur_file_ptr->attributes2.size)
	    args.attributes.size = (unsigned int) 0;
	else
	    args.attributes.size = (unsigned int) Cur_file_ptr->attributes2.size
				   - truncate_size;
    }

    /* make the call */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_SETATTR,
			xdr_setattr, (char *) &args,
			xdr_setattr, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);

    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {
	    Cur_file_ptr->attributes2 = reply.attrstat_u.attributes;
	    Cur_file_ptr->size = fh_size(Cur_file_ptr);
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: setattr call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr,
		    "%s: setattr call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_setattr */


static int
op_lookup(void)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    diropargs		args;
    diropres		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;		/* ret val == call success */

    op_ptr = &Ops[LOOKUP];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &args.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    args.name = Cur_filename;

    /* make the call */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_LOOKUP,
			xdr_lookup, (char *) &args,
			xdr_lookup, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {
	    Cur_file_ptr->state = Exists;
	    (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file,
			NFS_FHSIZE);
	    (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
	    Cur_file_ptr->attributes2 = reply.diropres_u.diropres.attributes;
	    Cur_file_ptr->size = fh_size(Cur_file_ptr);
	} else {
            /* We do lookup Nonexistent and this is not an error */
            if (reply.status != NFSERR_NOENT ||
                        Cur_file_ptr->state != Nonexistent) {
	        if (DEBUG_CHILD_ERROR) {
		     (void) fprintf(stderr,
			"%s: lookup call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	        }
            }

	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: lookup call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_lookup */


static int
op_readlink(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    fhandle_t		fh;			/* fh to do op on */
    readlinkres		reply;			/* the reply */
    char		sym_data[NFS_MAXPATHLEN];
    int			len;			/* length of symlink data */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */

    op_ptr = &Ops[READLINK];
    ret = 0;

    /* set up the arguments */
    /*
     * Note: this fh may be bogus because SYMLINK does
     * not return a fh ... only a status.  So unless we have
     * done a LOOKUP on this guy, the fh will probably be bad.
     * If it is bad it shows up as a symlink error in the results.
     */
    (void) memmove((char *) &fh, (char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);

    /* Have lower layers fill in the data directly. */
    reply.readlinkres_u.data = sym_data;
    len = 0;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_READLINK,
			xdr_readlink, (char *) &fh,
			xdr_readlink, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {
	    if (DEBUG_CHILD_RPC) {
		len = reply.readlinkres_u.len;
		sym_data[len] = '\0';
		(void) fprintf(stderr, "%s: READLINK on %s returned %s\n",
				    sfs_Myname, Cur_filename, sym_data);
		(void) fflush(stderr);
	    }
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: readlink call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr,
		    "%s: readlink call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_readlink */


/*
 * perform an RPC read operation of length 'xfer_size'
 */
static int
op_read(
    int 			xfer_size)
{
    sfs_op_type			*op_ptr;	/* per operation info */
    int				cur_cnt;
    int				max_cnt;	/* packet ctrs */
    char			buf[DEFAULT_MAX_BUFSIZE];/* data buffer */
    readargs			args;
    readres			reply;		/* the reply */
    enum clnt_stat		rpc_stat;	/* result from RPC call */
    struct ladtime		start;
    struct ladtime		stop;
    int				size;
    int				j;
    int				ret;		/* ret val == call success */

    op_ptr = &Ops[READ];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &args.file, (char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);

    /*
     * Don't allow a read of less than one block size
     */
    if (xfer_size < Bytes_per_block)
	xfer_size = Bytes_per_block;

    /*
     * randomly choose an offset that is a multiple of the block size
     * and constrained by making the transfer fit within the file
     */
    if (Cur_file_ptr->attributes2.size > xfer_size) {
	args.offset = Bytes_per_block * (sfs_random() %
			(((Cur_file_ptr->attributes2.size - xfer_size)
			/ Bytes_per_block) + 1));
    } else
	args.offset = 0;

    /* first read the whole buffers, then the fragment */
    for (j = 0; j < 2; j++) {

	if (j == 0) {
	    size = Bytes_per_block;
	    max_cnt = xfer_size / Bytes_per_block;
	} else {
	    /* 1KB - (Kb_per_block -1) KB fragment */
	    size = xfer_size % Bytes_per_block;
	    max_cnt = 1;
	}
	if (size == 0)
	    continue;

	/* check our stats to see if this would overflow */
	if (!Timed_run) {
	    if (op_ptr->target_calls > 0) {
		if ((op_ptr->results.good_calls + max_cnt)
		     > op_ptr->target_calls) {
		    max_cnt = op_ptr->target_calls - op_ptr->results.good_calls;
		}
	    }
	}

	args.count = size;
	args.totalcount = size;		/* unused */

	/* Have lower layers fill in the data directly.  */
	reply.readres_u.reply.data.data_val = buf;

	if (DEBUG_CHILD_RPC) {
	    (void) fprintf(stderr, "read: %d buffers\n", max_cnt);
	    (void) fflush(stderr);
	}

	/* make the call(s) now */
	for (cur_cnt = 0; cur_cnt < max_cnt; cur_cnt++) {

	    /* capture length for possible dump */
	    Dump_length = fh_size(Cur_file_ptr);

	    sfs_gettime(&start);
	    rpc_stat = clnt_call(NFS_client, NFSPROC_READ,
				xdr_read, (char *) &args,
				xdr_read, (char *) &reply,
				(Current_test_phase < Warmup_phase)
				     ? Nfs_timers[Init]
				     : Nfs_timers[op_ptr->call_class]);
	    sfs_gettime(&stop);
	    Cur_time = stop;

	    /* capture count and offset for possible dump */
	    Dump_count = (rpc_stat == RPC_SUCCESS && reply.status == NFS_OK)
			    ? reply.readres_u.reply.data.data_len : 0;
	    Dump_offset = args.offset;

	    if (rpc_stat == RPC_SUCCESS) {
		if (reply.status == NFS_OK) {
		    Cur_file_ptr->state = Exists;
		    Cur_file_ptr->attributes2 =
					reply.readres_u.reply.attributes;
		    Cur_file_ptr->size = fh_size(Cur_file_ptr);
		    size = reply.readres_u.reply.data.data_len;

		    if (DEBUG_CHILD_RPC) {
			(void) fprintf(stderr, "%s: READ %s %d bytes\n",
					   sfs_Myname, Cur_filename, size);
			(void) fflush(stderr);
		    }
		    args.offset += size;
		} else {
		    if (DEBUG_CHILD_ERROR) {
			 (void) fprintf(stderr,
				"%s: read call NFS error %s on file %d\n",
				sfs_Myname, nfs2_strerror(reply.status),
				Cur_file_ptr->unique_num);
		    }
		}
		sfs_elapsedtime(op_ptr, &start, &stop);
		op_ptr->results.good_calls++;
		Ops[TOTAL].results.good_calls++;
		ret++;
	    } else {
		if (DEBUG_CHILD_ERROR) {
		     (void) fprintf(stderr,
			    "%s: read call RPC error %d on file %d\n",
			    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
		}
		op_ptr->results.bad_calls++;
		Ops[TOTAL].results.bad_calls++;
	    }
	} /* for reading max_cnt packets */
    } /* for buffers and fragments */
    return(ret);

} /* op_read */


char *
init_write_buffer(
    void)
{
    uint32_t *bp;
    static uint32_t write_buf[DEFAULT_MAX_BUFSIZE / sizeof(uint32_t)];
    uint32_t *be  = write_buf + (sizeof(write_buf) /
						sizeof(uint32_t));

    if (write_buf[0] != (uint32_t)0xdeadbeef) {
        for (bp = write_buf; bp < be; bp++)
            *bp = (uint32_t)0xdeadbeef;
    }
    return (char *)write_buf;
}

/*
 * Perform and RPC write operation of length 'xfer_size'.  If 'append_flag'
 * is true, then write the data to the end of the file.
 */
/* ARGSUSED2 */
static int
op_write(
    int			xfer_size,
    int			append_flag,
    int			stable)
{
    sfs_op_type			*op_ptr;	/* per operation info */
    static char			*buf = NULL;	/* the data buffer */
    int				size;		/* size of data write */
    int				cur_cnt;	/* controls # NFS calls */
    int				max_cnt;
    writeargs			args;
    attrstat			reply;		/* the reply */
    enum clnt_stat		rpc_stat;	/* result from RPC call */
    struct ladtime		start;
    struct ladtime		stop;
    int				j;
    int				ret;		/* ret val == call success */

    /*
     * Initialize write buffer to known value
     */
    if (buf == NULL) {
	buf = init_write_buffer();
    }
    op_ptr = &Ops[WRITE];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &args.file, (char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);
    args.beginoffset = 0;	/* unused */

    if (append_flag == 1) {
	args.offset = Cur_file_ptr->attributes2.size;
    } else {
	/*
	 * randomly choose an offset that is a multiple of the block size
	 * and constrained by making the transfer fit within the file
	 */
	if (Cur_file_ptr->attributes2.size > xfer_size) {
	    args.offset = Bytes_per_block * (sfs_random() %
			    (((Cur_file_ptr->attributes2.size - xfer_size)
			    / Bytes_per_block) + 1));
	} else
	    args.offset = 0;
    }

    /* first write the whole buffers, then the fragment */
    for (j = 0; j < 2; j++) {

	if (j == 0) {
	    size = Bytes_per_block;
	    max_cnt = xfer_size / Bytes_per_block;
	} else {
	    /* 1KB - (Kb_per_block - 1) KB fragment */
	    size = xfer_size % Bytes_per_block;
	    max_cnt = 1;
	}
	if (size == 0)
	    continue;

	args.totalcount = size;	/* unused */
	args.data.data_len = size;
	args.data.data_val = buf;

	/* check our stats to see if this would overflow */
	if (!Timed_run) {
	    if (op_ptr->target_calls > 0) {
		if ((op_ptr->results.good_calls + max_cnt)
		     > op_ptr->target_calls) {
		    max_cnt = op_ptr->target_calls - op_ptr->results.good_calls;
		}
	    }
	}

	if (DEBUG_CHILD_RPC) {
	    (void) fprintf(stderr, "write: %d buffers\n", max_cnt);
	    (void) fflush(stderr);
	}

	/* make the call(s) now */
	for (cur_cnt = 0; cur_cnt < max_cnt; cur_cnt++) {

	    /* capture length for possible dump */
	    Dump_length = fh_size(Cur_file_ptr);

	    sfs_gettime(&start);
	    rpc_stat = clnt_call(NFS_client, NFSPROC_WRITE,
				xdr_write, (char *) &args,
				xdr_write, (char *) &reply,
				(Current_test_phase < Warmup_phase)
				     ? Nfs_timers[Init]
				     : Nfs_timers[op_ptr->call_class]);
	    sfs_gettime(&stop);
	    Cur_time = stop;

	    /* capture count and offset for possible dump */
	    Dump_count = args.data.data_len;
	    Dump_offset = args.offset;

	    if (rpc_stat == RPC_SUCCESS) {
		if (reply.status == NFS_OK) {
		    Cur_file_ptr->state = Exists;
		    Cur_file_ptr->attributes2 = reply.attrstat_u.attributes;
		    Cur_file_ptr->size = fh_size(Cur_file_ptr);
		    args.offset += size;

		    if (DEBUG_CHILD_RPC) {
			(void) fprintf(stderr, "%s: WRITE %s %d bytes\n",
					   sfs_Myname, Cur_filename, size);
			(void) fflush(stderr);
		    }
		} else {
		    if (DEBUG_CHILD_ERROR) {
			 (void) fprintf(stderr,
				"%s: write call NFS error %s on file %d\n",
				sfs_Myname, nfs2_strerror(reply.status),
				Cur_file_ptr->unique_num);
		    }
		}
		sfs_elapsedtime(op_ptr, &start, &stop);
		op_ptr->results.good_calls++;
		Ops[TOTAL].results.good_calls++;
		ret++;
	    } else {
		if (DEBUG_CHILD_ERROR) {
		     (void) fprintf(stderr,
			    "%s: write call RPC error %d on file %d\n",
			    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
		}
		op_ptr->results.bad_calls++;
		Ops[TOTAL].results.bad_calls++;
	    }
	} /* for writing max_cnt packets */
    } /* for buffers and fragments */
    return(ret);

} /* op_write */


static int
op_create(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    createargs		args;
    diropres		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */

    op_ptr = &Ops[CREATE];
    ret = 0;

    /* set up the arguments */
    args.attributes.mode = (0100000 | 0666);	/* 666 NFREG file */
    args.attributes.uid = Cur_uid;
    args.attributes.gid = Cur_gid;
    args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
    args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
    args.attributes.size = 0;
    (void) memmove((char *) &args.where.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    args.where.name = Cur_filename;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_CREATE,
			xdr_create, (char *) &args,
			xdr_create, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {
	    Cur_file_ptr->state = Exists;
	    (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file,
			NFS_FHSIZE);
	    (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
	    Cur_file_ptr->attributes2 = reply.diropres_u.diropres.attributes;
	    Cur_file_ptr->size = fh_size(Cur_file_ptr);
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: create call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: create call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_create */


static int
op_remove(void)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    diropargs		args;
    nfsstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;		/* ret val == call success */

    op_ptr = &Ops[REMOVE];
    ret = 0;

    /* set up the arguments */
    args.name = Cur_filename;
    (void) memmove((char *) &args.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_REMOVE,
			xdr_remove, (char *) &args,
			xdr_remove, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply == NFS_OK) {
	    Cur_file_ptr->state = Nonexistent;
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: remove call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: remove call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_remove */


static int
op_rename(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    sfs_fh_type		*target_fileinfo_ptr;	/* target name */
    renameargs		args;
    nfsstat		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */

    op_ptr = &Ops[RENAME];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &args.from.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    (void) memmove((char *) &args.to.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);

    target_fileinfo_ptr = randfh(RENAME, 0, 0, Nonexistent,
				 Sfs_non_io_file);

    args.from.name = Cur_file_ptr->file_name;
    (void) sprintf(target_fileinfo_ptr->file_name, Filespec,
		   target_fileinfo_ptr->unique_num);
    args.to.name = target_fileinfo_ptr->file_name;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_RENAME,
			xdr_rename, (char *) &args,
			xdr_rename, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply == NFS_OK) {
	    target_fileinfo_ptr->state = Exists;
	    (void) memmove((char *) &target_fileinfo_ptr->fh2,
			(char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);
	    target_fileinfo_ptr->attributes2 = Cur_file_ptr->attributes2;
	    target_fileinfo_ptr->size = Cur_file_ptr->size;
	    Cur_file_ptr->state = Nonexistent;
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: rename call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: rename call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_rename */


static int
op_link(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    sfs_fh_type		*target_fileinfo_ptr;	/* target */
    linkargs		args;
    nfsstat		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */

    op_ptr = &Ops[LINK];
    ret = 0;

    /* set up the arguments */
    target_fileinfo_ptr = randfh(LINK, 0, 0, Exists, Sfs_non_io_file);
    (void) memmove((char *) &args.from, (char *) &target_fileinfo_ptr->fh2,
			NFS_FHSIZE);
    (void) memmove((char *) &args.to.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    args.to.name = Cur_filename;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_LINK,
			xdr_link, (char *) &args,
			xdr_link, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply == NFS_OK) {
	    Cur_file_ptr->state = Exists;
	    (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &target_fileinfo_ptr->fh2, NFS_FHSIZE);
	    (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
	    target_fileinfo_ptr->attributes2.nlink++;
	    Cur_file_ptr->attributes2 = target_fileinfo_ptr->attributes2;
	    Cur_file_ptr->size = fh_size(Cur_file_ptr);
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: link call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: link call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_link */


static int
op_symlink(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    sfs_fh_type		*target_fileinfo_ptr;	/* target file */
    symlinkargs		args;
    nfsstat		reply;			/* the reply */
    char		sym_data[NFS_MAXPATHLEN];	/* symlink data */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */

    op_ptr = &Ops[SYMLINK];
    ret = 0;

    /* set up the arguments */
    target_fileinfo_ptr = randfh(SYMLINK, 0, 0, Exists, Sfs_non_io_file);
    (void) memmove((char *) &args.from.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    args.from.name = Cur_filename;

    (void) strcpy(sym_data, "./");
    (void) strcat(sym_data, target_fileinfo_ptr->file_name);
    args.attributes.size = strlen(sym_data);
    args.to = sym_data;

    args.attributes.mode = (0120000 | 0777);
    args.attributes.uid = Cur_uid;
    args.attributes.gid = Cur_gid;
    args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
    args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_SYMLINK,
			xdr_symlink, (char *) &args,
			xdr_symlink, (char *) &reply,
			((int)Current_test_phase < (int)Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply == NFS_OK) {
	    /*
	     * SYMLINK doesn't return a fh. If we try to
	     * access this symlink (eg, remove(), readlink())
	     * before we do a lookup, we won't have a fh to use.
	     * So, we do a lookup call here.
	     * If it fails, we fill in what we can.
	     */
	    Cur_file_ptr->state = Exists;
	    if (op_lookup() == 0) {
		(void) strcpy(Cur_file_ptr->file_name, Cur_filename);
		Cur_file_ptr->attributes2.type = NFLNK;
		Cur_file_ptr->attributes2.mode = (0120000|0777);
		Cur_file_ptr->attributes2.uid = Cur_uid;
		Cur_file_ptr->attributes2.gid = Cur_gid;
		Cur_file_ptr->attributes2.atime.seconds =(unsigned int)Cur_time.esec;
		Cur_file_ptr->attributes2.atime.useconds=(unsigned int)Cur_time.usec;
		Cur_file_ptr->attributes2.mtime = Cur_file_ptr->attributes2.atime;
	    } else
		ret++;
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: symlink call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: symlink call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_symlink */


static int
op_mkdir(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    mkdirargs		args;
    diropres		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */

    op_ptr = &Ops[MKDIR];
    ret = 0;

    /* set up the arguments */
    args.attributes.mode = (NFSMODE_DIR | 0777);
    args.attributes.uid = Cur_uid;
    args.attributes.gid = Cur_gid;
    args.attributes.size = (unsigned int) 512;
    args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
    args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
    args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
    (void) memmove((char *) &args.where.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    args.where.name = Cur_filename;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_MKDIR,
			xdr_mkdir, (char *) &args,
			xdr_mkdir, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {
	    Cur_file_ptr->state = Empty_dir;
	    (void) memmove((char *) &Cur_file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file,
			NFS_FHSIZE);
	    (void) strcpy(Cur_file_ptr->file_name, Cur_filename);
	    Cur_file_ptr->attributes2 = reply.diropres_u.diropres.attributes;
	    Cur_file_ptr->size = fh_size(Cur_file_ptr);
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: mkdir call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: mkdir call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_mkdir */


static int
op_rmdir(void)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    diropargs		args;
    nfsstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;		/* ret val == call success */

    op_ptr = &Ops[RMDIR];
    ret = 0;

    if (Cur_file_ptr->state != Empty_dir) {
	     (void) fprintf(stderr, "%s: Attempting to remove non-Empty_dir %d\n",
		    sfs_Myname, Cur_file_ptr->unique_num);
    }

    /* set up the arguments */
    (void) memmove((char *) &args.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    args.name = Cur_file_ptr->file_name;

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_RMDIR,
			xdr_rmdir, (char *) &args,
			xdr_rmdir, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply == NFS_OK) {
	    Cur_file_ptr->state = Nonexistent;
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: rmdir call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: rmdir call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_rmdir */


static int
op_readdir(void)
{
    sfs_op_type		*op_ptr;		/* per operation info */
    readdirargs		args;
    readdirres		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    uint_t		i;
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;			/* ret val == call success */
    nfscookie		cookie;
    bool_t		hit_eof;
	/* arbitrary fixed ceiling */
    int			entry_cnt = SFS_MAXDIRENTS;
	/* array of entries */
    entry		entry_stream[SFS_MAXDIRENTS];
    entry		*entry_ptr;		/* ptr to the dir entry */

    char	name[SFS_MAXNAMLEN];
	/* array of dir names */
    char	name_stream[SFS_MAXDIRENTS * SFS_MAXNAMLEN];


    /*
     * 1) need some measure of how many entries are in a directory
     * currently, we assume SFS_MAXDIRENTS - it should be random
     * from 0 to MAX for a large MAX we should pre-allocate a buffer for the
     * returned directory names.
     * 2) need some measure of how many directory entries to read
     * during each readdir() call.  Again, we assume SFS_MAXDIRENTS.
     */

    op_ptr = &Ops[READDIR];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &args.dir, (char *) &Cur_file_ptr->dir->fh2,
			NFS_FHSIZE);
    (void) memset((char *) args.cookie, '\0', NFS_COOKIESIZE);
    args.count = DEFAULT_MAX_BUFSIZE;

    /* Have lower layers fill in the data directly.  */
    reply.readdirres_u.reply.max_entries = entry_cnt;
    reply.readdirres_u.reply.entries = entry_stream;
    for (i = 0; i < entry_cnt; i++) {
	entry_stream[i].name = &name_stream[i * SFS_MAXNAMLEN];
    }

    /* make the call now */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_READDIR,
			xdr_readdir, (char *) &args,
			xdr_readdir, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	if (reply.status == NFS_OK) {

	    if (DEBUG_CHILD_RPC) {
		hit_eof = reply.readdirres_u.reply.eof;
		entry_cnt = reply.readdirres_u.reply.max_entries;
		entry_ptr = reply.readdirres_u.reply.entries;
		for (i = 0; i < entry_cnt; i++) {
		    entry_ptr->name[entry_ptr->name_len] ='\0';
		    (void) strcpy(name, entry_ptr->name);
		    (void) memmove((char *) cookie,
					(char *) entry_ptr->cookie,
					NFS_COOKIESIZE);
		    (void) fprintf(stderr, "%s:READDIR (eof=%d) entry %s\n",
					sfs_Myname, hit_eof, name);
		    entry_ptr++;
		}
		(void) fflush(stderr);
	    }
	} else {
	    if (DEBUG_CHILD_ERROR) {
		 (void) fprintf(stderr,
			"%s: readdir call NFS error %s on file %d\n",
			sfs_Myname, nfs2_strerror(reply.status),
			Cur_file_ptr->unique_num);
	    }
	}
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: readdir call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_readdir */


/* Beware - op_statfs() collides w/ some other name, use op_fsstat() */
static int
op_fsstat(void)
{
    sfs_op_type		*op_ptr;	/* per operation info */
    fhandle_t		fh;
    statfsres		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    struct ladtime	start;
    struct ladtime	stop;
    int			ret;		/* ret val == call success */

    op_ptr = &Ops[FSSTAT];
    ret = 0;

    /* set up the arguments */
    (void) memmove((char *) &fh, (char *) &Cur_file_ptr->fh2,
			NFS_FHSIZE);

    /* make the call */
    sfs_gettime(&start);
    rpc_stat = clnt_call(NFS_client, NFSPROC_STATFS,
			xdr_statfs, (char *) &fh,
			xdr_statfs, (char *) &reply,
			(Current_test_phase < Warmup_phase)
				 ? Nfs_timers[Init]
				 : Nfs_timers[op_ptr->call_class]);
    sfs_gettime(&stop);
    Cur_time = stop;

    if (rpc_stat == RPC_SUCCESS) {
	sfs_elapsedtime(op_ptr, &start, &stop);
	op_ptr->results.good_calls++;
	Ops[TOTAL].results.good_calls++;
	ret++;
    } else {
	if (DEBUG_CHILD_ERROR) {
	     (void) fprintf(stderr, "%s: fsstat call RPC error %d on file %d\n",
		    sfs_Myname, rpc_stat, Cur_file_ptr->unique_num);
	}
	op_ptr->results.bad_calls++;
	Ops[TOTAL].results.bad_calls++;
    }
    return(ret);

} /* op_fsstat */


/*
 * These are a set of reliable functions used by the initialization code.
 */

#define LAD_RETRIABLE(stat) (((stat) == RPC_TIMEDOUT) || ((stat) == RPC_CANTDECODERES))

/*
 * Reliably lookup a file in the current directory
 * Return:
 *	-1	RPC error
 *	1	File doesn't exist
 *	0	File exists
 */
int
lad_lookup(sfs_fh_type *file_ptr, char *name)
{
    diropargs		args;
    diropres		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_lookup: %lx[%lx] %s\n", sfs_Myname,
			(int32_t) file_ptr, (int32_t) file_ptr->dir, name);
	(void) fflush(stderr);
    }

    /* CONSTCOND */
    while (1) {
	/* set up the arguments */
	(void) memmove((char *) &args.dir, (char *) &file_ptr->dir->fh2,
			NFS_FHSIZE);
	args.name = name;

	/* make the call */
	rpc_stat = clnt_call(NFS_client, NFSPROC_LOOKUP,
			xdr_lookup, (char *) &args,
			xdr_lookup, (char *) &reply,
			Nfs_timers[Init]);

	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_lookup(%s) RPC call failed : %s\n",
				name, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
    }

    if (reply.status == NFSERR_NOENT) {
	return(1);
    }

    if (reply.status != NFS_OK) {
	(void) fprintf(stderr, "lad_lookup(%s) NFS call failed : %s\n",
			name, nfs2_strerror(reply.status));
	return(-1);
    }

    file_ptr->state = Exists;
    (void) memmove((char *) &file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file, NFS_FHSIZE);
    (void) strcpy(file_ptr->file_name, name);
    file_ptr->attributes2 = reply.diropres_u.diropres.attributes;
    file_ptr->size = fh_size(file_ptr);

    return(0);
}

/*
 * Reliably remove a file in the current directory
 */
int
lad_remove(sfs_fh_type *file_ptr, char *name)
{
    diropargs		args;
    nfsstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    int			retried = 0;

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_remove: %lx[%lx] %s\n", sfs_Myname,
			(int32_t) file_ptr, (int32_t) file_ptr->dir, name);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name does exist
     */
    if (file_ptr->attributes2.type == NFDIR)
	return (lad_rmdir(file_ptr, name));

    /* CONSTCOND */
    while (1) {
	/* set up the arguments */
	args.name = name;
	(void) memmove((char *) &args.dir, (char *) &file_ptr->dir->fh2,
			NFS_FHSIZE);

	/* make the call now */
	rpc_stat = clnt_call(NFS_client, NFSPROC_REMOVE,
			xdr_remove, (char *) &args,
			xdr_remove, (char *) &reply,
			Nfs_timers[Init]);

	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_remove(%s) RPC call failed : %s\n",
				name, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
	retried++;
    }

    if (reply != NFS_OK) {
	if (reply != NFSERR_NOENT || !retried) {
	    (void) fprintf(stderr, "lad_remove(%s) NFS call failed : %s\n",
			name, nfs2_strerror(reply));
	    return(-1);
	}
    }

    file_ptr->state = Nonexistent;

    return(0);
}

/*
 * Reliably remove a directory in the current directory
 */
int
lad_rmdir(sfs_fh_type *file_ptr, char *name)
{
    diropargs		args;
    nfsstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */
    int			retried = 0;

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s: lad_rmdir: %lx[%lx] %s\n", sfs_Myname,
			(int32_t) file_ptr, (int32_t) file_ptr->dir, name);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name does exist and the directory
     * is empty.
     */
    if (file_ptr->attributes2.type != NFDIR)
	return (lad_remove(file_ptr, name));

    /* CONSTCOND */
    while (1) {
	/* set up the arguments */
	args.name = name;
	(void) memmove((char *) &args.dir, (char *) &file_ptr->dir->fh2,
			NFS_FHSIZE);

	/* make the call now */
	rpc_stat = clnt_call(NFS_client, NFSPROC_RMDIR,
			xdr_remove, (char *) &args,
			xdr_remove, (char *) &reply,
			Nfs_timers[Init]);

	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_rmdir(%s) RPC call failed : %s\n",
				name, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
	retried++;
    }

    if (reply != NFS_OK) {
	if (reply != NFSERR_NOENT || !retried) {
	    (void) fprintf(stderr, "lad_rmdir(%s) NFS call failed : %s\n",
			name, nfs2_strerror(reply));
	    return(-1);
	}
    }

    file_ptr->state = Nonexistent;

    return(0);
}

/*
 * Reliably create a symlink in the current directory
 */
int
lad_symlink(sfs_fh_type *file_ptr, char *target, char *name)
{
    symlinkargs		args;
    nfsstat		reply;			/* the reply */
    char		sym_data[NFS_MAXPATHLEN];	/* symlink data */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    int			retried = 0;

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_symlink: %lx %s -> %s\n", sfs_Myname,
			(int32_t) file_ptr, name, target);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name does not already exist
     */
    /* CONSTCOND */
    while (1) {
	/* set up the arguments */
	(void) memmove((char *) &args.from.dir, (char *) &file_ptr->dir->fh2,
			NFS_FHSIZE);
	args.from.name = name;

	(void) strcpy(sym_data, "./");
	(void) strcat(sym_data, target);
	args.attributes.size = strlen(sym_data);
	args.to = sym_data;

	args.attributes.mode = (0120000 | 0777);
	args.attributes.uid = Cur_uid;
	args.attributes.gid = Cur_gid;
	args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
	args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;

	/* make the call now */
	rpc_stat = clnt_call(NFS_client, NFSPROC_SYMLINK,
			xdr_symlink, (char *) &args,
			xdr_symlink, (char *) &reply,
		 	Nfs_timers[Init]);
	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_symlink(%s) RPC call failed : %s\n",
				name, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
	retried++;
    }

    if (reply != NFS_OK) {
	if (reply != NFSERR_EXIST || !retried) {
	    (void) fprintf(stderr, "lad_symlink(%s, %s) NFS call failed : %s\n",
			target, name, nfs2_strerror(reply));
	    return(-1);
	}
    }

    /*
     * SYMLINK doesn't return a fh. If we try to
     * access this symlink (eg, remove(), readlink())
     * before we do a lookup, we won't have a fh to use.
     * So, we do a lookup call here.
     * If it fails, we fill in what we can.
     */
    return (lad_lookup(file_ptr, name));
}

/*
 * Reliably create a directory in the current directory
 */
int
lad_mkdir(sfs_fh_type *file_ptr, char *name)
{
    mkdirargs		args;
    diropres		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    int			retried = 0;

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_mkdir: %lx[%lx] %s\n", sfs_Myname,
			(int32_t) file_ptr, (int32_t) file_ptr->dir, name);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name does not already exist
     */
    /* CONSTCOND */
    while (1) {
	/* set up the arguments */
	args.attributes.mode = (NFSMODE_DIR | 0777);
	args.attributes.uid = Cur_uid;
	args.attributes.gid = Cur_gid;
	args.attributes.size = (unsigned int) 512;
	args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
	args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
	(void) memmove((char *) &args.where.dir, (char *) &file_ptr->dir->fh2,
			NFS_FHSIZE);
	args.where.name = name;

	/* make the call now */
	rpc_stat = clnt_call(NFS_client, NFSPROC_MKDIR,
			xdr_mkdir, (char *) &args,
			xdr_mkdir, (char *) &reply,
			Nfs_timers[Init]);

	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_mkdir(%s) RPC call failed : %s\n",
				name, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
	retried++;
    }

    if (!retried && reply.status == NFSERR_EXIST)
	return(1);

    if (reply.status != NFS_OK) {
	if (reply.status != NFSERR_EXIST || !retried) {
	    (void) fprintf(stderr, "lad_mkdir(%s) NFS call failed : %s\n",
			name, nfs2_strerror(reply.status));
	    return(-1);
	}
	/*
	 * If the first mkdir suceeded but the reply as dropped and
	 * was retransmitted, we still need to lookup the attributes
	 */
	if (lad_lookup(file_ptr, name))
	   return (-1);
    } else {
	(void) memmove((char *) &file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file,
			NFS_FHSIZE);
	(void) strcpy(file_ptr->file_name, name);
	file_ptr->attributes2 = reply.diropres_u.diropres.attributes;
	file_ptr->size = fh_size(file_ptr);
    }
    file_ptr->state = Empty_dir;

    return(0);
}

/*
 * Reliably write a file in the current directory
 */
int
lad_write(sfs_fh_type *file_ptr, int32_t offset, int32_t length)
{
    static char			*buf = NULL;	/* the data buffer */
    int32_t			size;		/* size of data write */
    int32_t			cur_cnt;
    writeargs			args;
    attrstat			reply;		/* the reply */
    enum clnt_stat		rpc_stat;	/* result from RPC call */

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_write: %lx[%lx] %ld %ld\n",
			sfs_Myname, (int32_t) file_ptr, (int32_t) file_ptr->dir,
			offset, length);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name does exist
     * Initialize write buffer to known value
     */
    if (buf == NULL) {
	buf = init_write_buffer();
    }

    /* set up the arguments */
    (void) memmove((char *) &args.file, (char *) &file_ptr->fh2,
			NFS_FHSIZE);
    args.beginoffset = 0;	/* unused */
    args.offset = offset;

    size = Bytes_per_block;
    for (cur_cnt = 0; cur_cnt < length; cur_cnt += size) {
	if ((cur_cnt + size) > length)
		size = length - cur_cnt;

	if (size == 0)
	    break;

	args.totalcount = size;	/* unused */
	args.data.data_len = size;
	args.data.data_val = buf;

	/* make the call now */
        /* CONSTCOND */
	while (1) {
	    rpc_stat = clnt_call(NFS_client, NFSPROC_WRITE,
				xdr_write, (char *) &args,
				xdr_write, (char *) &reply,
				Nfs_timers[Init]);

	    if (rpc_stat == RPC_SUCCESS) 
		break;
	    if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_write() RPC call failed : %s\n",
				clnt_sperrno(rpc_stat));
	    }
	    if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	    }
	}
	if (reply.status != NFS_OK) {
	    (void) fprintf(stderr, "lad_write() NFS call failed : %s\n",
			nfs2_strerror(reply.status));
	    return(-1);
	}
	file_ptr->state = Exists;
	file_ptr->attributes2 = reply.attrstat_u.attributes;
	file_ptr->size = fh_size(file_ptr);
	args.offset += size;
    }
    return(0);
}

/*
 * Reliably create a file in the current directory
 */
int
lad_create(sfs_fh_type *file_ptr, char *name)
{
    createargs		args;
    diropres		reply;			/* the reply */
    enum clnt_stat	rpc_stat;		/* result from RPC call */
    int			retried = 0;

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_create: %lx[%lx] %s\n", sfs_Myname,
			(int32_t) file_ptr, (int32_t) file_ptr->dir, name);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name does not already exist
     */
    /* CONSTCOND */
    while (1) {
	/* set up the arguments */
	args.attributes.mode = (0100000 | 0666);	/* 666 NFREG file */
	args.attributes.uid = Cur_uid;
	args.attributes.gid = Cur_gid;
	args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
	args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
	args.attributes.size = 0;
	(void) memmove((char *) &args.where.dir, (char *) &file_ptr->dir->fh2,
			NFS_FHSIZE);
	args.where.name = name;

	/* make the call now */
	rpc_stat = clnt_call(NFS_client, NFSPROC_CREATE,
			xdr_create, (char *) &args,
			xdr_create, (char *) &reply,
			Nfs_timers[Init]);

	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr, "lad_create(%s) RPC call failed : %s\n",
				name, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
	retried++;
    }

    if (!retried && reply.status == NFSERR_EXIST) {
	return(1);
    }

    if (reply.status != NFS_OK) {
	if (reply.status != NFSERR_EXIST || !retried) {
	    (void) fprintf(stderr, "lad_create(%s) NFS call failed : %s\n",
			name, nfs2_strerror(reply.status));
	    return(-1);
	}
	/*
	 * If the first create suceeded but the reply as dropped and
	 * was retransmitted, we still need to lookup the attributes
	 */
	if (lad_lookup(file_ptr, name))
	   return (-1);
    } else {
	(void) memmove((char *) &file_ptr->fh2,
			(char *) &reply.diropres_u.diropres.file, NFS_FHSIZE);
	(void) strcpy(file_ptr->file_name, name);
	file_ptr->attributes2 = reply.diropres_u.diropres.attributes;
	file_ptr->size = fh_size(file_ptr);
    }

    file_ptr->state = Exists;
    /*
     * Directories are created as Empty_dir, when a file is created it
     * becomes an Exists.
     */
    file_ptr->dir->state = Exists;

    return(0);
}

/*
 * Reliably set the size of a file in the current directory
 */
int
lad_truncate(sfs_fh_type *file_ptr, int32_t size)
{
    sattrargs		args;
    attrstat		reply;		/* the reply */
    enum clnt_stat	rpc_stat;	/* result from RPC call */

    if (DEBUG_CHILD_RPC) {
	(void) fprintf(stderr, "%s:lad_truncate: %lx[%lx] %ld\n", sfs_Myname,
			(int32_t) file_ptr, (int32_t) file_ptr->dir, size);
	(void) fflush(stderr);
    }

    /*
     * This function presumes that the file name already exists
     */
    /* CONSTCOND */
    while (1) {
	/*
	 * set up the arguments
	 * Set the mode and times as well
	 */
	args.attributes.mode = 0666;
	args.attributes.uid = (unsigned int) -1;
	args.attributes.gid = (unsigned int) -1;
	args.attributes.size = (unsigned int) -1;
	args.attributes.atime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.atime.useconds = (unsigned int) Cur_time.usec;
	args.attributes.mtime.seconds = (unsigned int) Cur_time.esec;
	args.attributes.mtime.useconds = (unsigned int) Cur_time.usec;
	(void) memmove((char *) &args.file, (char *) &file_ptr->fh2,
			NFS_FHSIZE);
	args.attributes.size = (unsigned int) size;

	/* make the call */
	rpc_stat = clnt_call(NFS_client, NFSPROC_SETATTR,
			xdr_setattr, (char *) &args,
			xdr_setattr, (char *) &reply,
			Nfs_timers[Init]);

	if (rpc_stat == RPC_SUCCESS) 
		break;
	if (rpc_stat != RPC_TIMEDOUT) {
		(void) fprintf(stderr,
				"lad_truncate(%ld) RPC call failed : %s\n",
				size, clnt_sperrno(rpc_stat));
	}
	if (!LAD_RETRIABLE(rpc_stat)) {
		return(-1);
	}
    }

    if (reply.status != NFS_OK) {
	(void) fprintf(stderr, "lad_truncate(%ld) NFS call failed : %s\n",
			size, nfs2_strerror(reply.status));
	return(-1);
    }
    file_ptr->attributes2 = reply.attrstat_u.attributes;
    file_ptr->size = fh_size(file_ptr);

    return(0);
}

static char *
nfs2_strerror(int status)
{
    static char str[40];
    switch (status) {
	case NFS_OK:
	    (void) strcpy(str, "no error");
	    break;
	case NFSERR_PERM:
	    (void) strcpy(str, "Not owner");
	    break;
	case NFSERR_NOENT:
	    (void) strcpy(str, "No such file or directory");
	    break;
	case NFSERR_IO:
	    (void) strcpy(str, "I/O error");
	    break;
	case NFSERR_NXIO:
	    (void) strcpy(str, "No such device or address");
	    break;
	case NFSERR_ACCES:
	    (void) strcpy(str, "Permission denied");
	    break;
	case NFSERR_EXIST:
	    (void) strcpy(str, "File exists");
	    break;
	case NFSERR_XDEV:
	    (void) strcpy(str, "Cross-device link");
	    break;
	case NFSERR_NODEV:
	    (void) strcpy(str, "No such device");
	    break;
	case NFSERR_NOTDIR:
	    (void) strcpy(str, "Not a directory");
	    break;
	case NFSERR_ISDIR:
	    (void) strcpy(str, "Is a directory");
	    break;
	case NFSERR_INVAL:
	    (void) strcpy(str, "Invalid argument");
	    break;
	case NFSERR_FBIG:
	    (void) strcpy(str, "File too large");
	    break;
	case NFSERR_NOSPC:
	    (void) strcpy(str, "No space left on device");
	    break;
	case NFSERR_ROFS:
	    (void) strcpy(str, "Read-only file system");
	    break;
	case NFSERR_OPNOTSUPP:
	    (void) strcpy(str, "Operation not supported");
	    break;
	case NFSERR_NAMETOOLONG:
	    (void) strcpy(str, "File name too long");
	    break;
	case NFSERR_NOTEMPTY:
	    (void) strcpy(str, "Directory not empty");
	    break;
	case NFSERR_DQUOT:
	    (void) strcpy(str, "Disc quota exceeded");
	    break;
	case NFSERR_STALE:
	    (void) strcpy(str, "Stale NFS file handle");
	    break;
	case NFSERR_REMOTE:
	    (void) strcpy(str, "Object is remote");
	    break;
	case NFSERR_WFLUSH:
	    (void) strcpy(str, "write cache flushed");
	    break;
	default:
	    (void) sprintf(str, "Unknown status %d", status);
	    break;
    }
    return (str);
}
/* sfs_c_ops.c */
