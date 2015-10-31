#ifndef lint
static char sfs_c_xdrSid[] = "@(#)sfs_2_xdr.c	2.1	97/10/23";
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

/*
 * -------------------------- sfs_c_xdr.c --------------------------
 *
 *      XDR routines for the nfs protocol.
 *
 *.Exported_routines
 *	bool_t xdr_fhstatus(XDR *, struct fhstatus *)
 *	bool_t xdr_path(XDR *, char **)
 *	bool_t xdr_fattr(XDR *, fattr *)
 *	bool_t xdr_sattr(XDR *, sattr *)
 *	bool_t xdr_null(void)
 *	bool_t xdr_getattr(XDR *, char *)
 *	bool_t xdr_setattr(XDR *, char *)
 *	bool_t xdr_root(void)
 *	bool_t xdr_lookup(XDR *, char *)
 *	bool_t xdr_readlink(XDR *, char *)
 *	bool_t xdr_read(XDR *, char *)
 *	bool_t xdr_write(XDR *, char *)
 *	bool_t xdr_create(XDR *, char *)
 *	bool_t xdr_remove(XDR *, char *)
 *	bool_t xdr_rename(XDR *, char *)
 *	bool_t xdr_link(XDR *, char *)
 *	bool_t xdr_symlink(XDR *, char *)
 *	bool_t xdr_mkdir(XDR *, char *)
 *	bool_t xdr_rmdir(XDR *, char *)
 *	bool_t xdr_readdir(XDR *, char *)
 *	bool_t xdr_statfs(XDR *, char *)
 *
 *.Local_routines
 *	bool_t xdr_timeval(XDR *, nfstime *)
 *	bool_t xdr_nfsstat(XDR *, nfsstat *)
 *	bool_t xdr_ftype(XDR *, ftype *)
 *	bool_t xdr_diropargs(XDR *, diropargs *)
 *	bool_t xdr_diropres(XDR *, diropres *)
 *	bool_t xdr_attrstat(XDR *, attrstat *)
 *
 *.Revision_History
 *      28-NOv-91	Teelucksingh	ANSI C
 *	25-Jun-91	Santa Wiryaman	Changed return values to TRUE when
 *					status != NFS_OK.  This way we can
 *					decode NFS error messages.
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

#include "sfs_c_def.h"

/*
 * -----------------------  Forward Definitions  -----------------------
 */

static bool_t xdr_f_handle(XDR *, fhandle_t *);
static bool_t xdr_fattr(XDR *, fattr *);
static bool_t xdr_sattr(XDR *, sattr *);
static bool_t xdr_timeval(XDR *, nfstime *);
static bool_t xdr_nfsstat(XDR *, nfsstat *);
static bool_t xdr_ftype(XDR *, ftype *);
static bool_t xdr_diropargs(XDR *, diropargs *);
static bool_t xdr_diropres(XDR *, diropres *);
static bool_t xdr_attrstat(XDR *, attrstat *);

/*
 * -----------------------  SFS XDR Routines  -------------------------
 */


static bool_t
xdr_f_handle(
    XDR *		xdrs,
    fhandle_t *		fhandle_ptr)
{
    return(xdr_opaque(xdrs, (char *)fhandle_ptr, NFS_FHSIZE));
}


static bool_t
xdr_timeval(
    XDR *		xdrs,
    nfstime *		timeval_ptr)
{
    return(xdr_int32_t(xdrs, (int32_t *) &timeval_ptr->seconds) &&
	   xdr_int32_t(xdrs, (int32_t *) &timeval_ptr->useconds));
}


static bool_t
xdr_nfsstat(
    XDR *		xdrs,
    nfsstat *		nfsstat_ptr)
{
    return(xdr_enum(xdrs, (enum_t *) nfsstat_ptr));
}


static bool_t
xdr_ftype(
    XDR *		xdrs,
    ftype *		ftype_ptr)
{
    return(xdr_enum(xdrs, (enum_t *) ftype_ptr));
}


bool_t
xdr_fhstatus(
    XDR *		xdrs,
    struct fhstatus *	fhsp)
{
    if (!xdr_nfsstat(xdrs, (nfsstat *) &fhsp->fhs_status)) {
	return(FALSE);
    }
    if (fhsp->fhs_status != (int)NFS_OK) {
	return(TRUE);
    }
    return(xdr_f_handle(xdrs, &fhsp->fhs_fh));
}


bool_t
xdr_path(
    XDR *		xdrs,
    char **		pathp)
{
    return(xdr_string(xdrs, pathp, NFS_MAXPATHLEN));
}


static bool_t
xdr_fattr(
    XDR *		xdrs,
    fattr *		fattr_ptr)
{
    return(xdr_ftype(xdrs, &fattr_ptr->type) &&
	   xdr_u_int(xdrs, &fattr_ptr->mode) &&
	   xdr_u_int(xdrs, &fattr_ptr->nlink) &&
	   xdr_u_int(xdrs, &fattr_ptr->uid) &&
	   xdr_u_int(xdrs, &fattr_ptr->gid) &&
	   xdr_u_int(xdrs, &fattr_ptr->size) &&
	   xdr_u_int(xdrs, &fattr_ptr->blocksize) &&
	   xdr_u_int(xdrs, &fattr_ptr->rdev) &&
	   xdr_u_int(xdrs, &fattr_ptr->blocks) &&
	   xdr_u_int(xdrs, &fattr_ptr->fsid) &&
	   xdr_u_int(xdrs, &fattr_ptr->fileid) &&
	   xdr_timeval(xdrs, &fattr_ptr->atime) &&
	   xdr_timeval(xdrs, &fattr_ptr->mtime) &&
	   xdr_timeval(xdrs, &fattr_ptr->ctime));
}


static bool_t
xdr_sattr(
    XDR *		xdrs,
    sattr *		sattr_ptr)
{
    return(xdr_u_int(xdrs, &sattr_ptr->mode) &&
	   xdr_u_int(xdrs, &sattr_ptr->uid) &&
	   xdr_u_int(xdrs, &sattr_ptr->gid) &&
	   xdr_u_int(xdrs, &sattr_ptr->size) &&
	   xdr_timeval(xdrs, &sattr_ptr->atime) &&
	   xdr_timeval(xdrs, &sattr_ptr->mtime));
}


static bool_t
xdr_diropargs(
    XDR *		xdrs,
    diropargs *		dir_args_ptr)
{
    return(xdr_f_handle(xdrs, &dir_args_ptr->dir) &&
	   xdr_path(xdrs, &dir_args_ptr->name));
}


static bool_t
xdr_diropres(
    XDR *		xdrs,
    diropres *		dir_res_ptr)
{
    if (!xdr_nfsstat(xdrs, &dir_res_ptr->status)) {
	return(FALSE);
    }

    if (dir_res_ptr->status == NFS_OK) {
	return(xdr_f_handle(xdrs, &dir_res_ptr->diropres_u.diropres.file) &&
	       xdr_fattr(xdrs, &dir_res_ptr->diropres_u.diropres.attributes));
    }
    return(TRUE);
}


static bool_t
xdr_attrstat(
    XDR *		xdrs,
    attrstat *		attrstat_ptr)
{
    if (!xdr_nfsstat(xdrs, &attrstat_ptr->status)) {
	return(FALSE);
    }

    if (attrstat_ptr->status == NFS_OK) {
	return(xdr_fattr(xdrs, &attrstat_ptr->attrstat_u.attributes));
    }
    return(TRUE);
}


bool_t
xdr_getattr(
    XDR *		xdrs,
    char *		params_ptr)
{
    fhandle_t *		fhandle_ptr;
    attrstat *		attrstat_ptr;

    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    fhandle_ptr = (fhandle_t *) params_ptr;
	    return(xdr_f_handle(xdrs, fhandle_ptr));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    attrstat_ptr = (attrstat *) params_ptr;
	    return(xdr_attrstat(xdrs, attrstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_setattr(
    XDR *		xdrs,
    char *		params_ptr)
{
    sattrargs *		sattrargs_ptr;
    attrstat *		attrstat_ptr;

    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    sattrargs_ptr = (sattrargs *) params_ptr;
	    return(xdr_f_handle(xdrs, &sattrargs_ptr->file) &&
		   xdr_sattr(xdrs, &sattrargs_ptr->attributes));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    attrstat_ptr = (attrstat *) params_ptr;
	    return(xdr_attrstat(xdrs, attrstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_lookup(
    XDR *		xdrs,
    char *		params_ptr)
{
    diropargs *		diropargs_ptr;
    diropres *		diropres_ptr;


    switch(xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    diropargs_ptr = (diropargs *) params_ptr;
	    return(xdr_f_handle(xdrs, &diropargs_ptr->dir) &&
		   xdr_path(xdrs, &diropargs_ptr->name));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    diropres_ptr = (diropres *) params_ptr;
	    return(xdr_diropres(xdrs, diropres_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}

bool_t
xdr_readlink(
    XDR *		xdrs,
    char *		params_ptr)
{
    fhandle_t *		fhandle_ptr;
    readlinkres *	readlinkres_ptr;

    switch(xdrs->x_op) {
	case XDR_ENCODE:
	    fhandle_ptr = (fhandle_t *) params_ptr;
	    return(xdr_f_handle(xdrs, fhandle_ptr));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    readlinkres_ptr = (readlinkres *) params_ptr;
	    if (!xdr_nfsstat(xdrs, &readlinkres_ptr->status)) {
		return(FALSE);
	    }
	    if (readlinkres_ptr->status != NFS_OK) {
		return(TRUE);
	    }
	    return(xdr_bytes(xdrs, &readlinkres_ptr->readlinkres_u.data,
			     (unsigned int *)
			     &readlinkres_ptr->readlinkres_u.len,
			     (unsigned int) NFS_MAXPATHLEN));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_read(
    XDR *		xdrs,
    char *		params_ptr)
{
    readargs *		readargs_ptr;
    readres *		readres_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    readargs_ptr = (readargs *) params_ptr;
	    return(xdr_f_handle(xdrs, &readargs_ptr->file) &&
		   xdr_u_int(xdrs, &readargs_ptr->offset) &&
		   xdr_u_int(xdrs, &readargs_ptr->count) &&
		   xdr_u_int(xdrs, &readargs_ptr->totalcount));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    readres_ptr = (readres *) params_ptr;
	    if (!xdr_nfsstat(xdrs, &readres_ptr->status)) {
		return(FALSE);
	    }
	    if (readres_ptr->status != NFS_OK) {
		return(TRUE);
	    }
	    return(xdr_fattr(xdrs, &readres_ptr->readres_u.reply.attributes) &&
		   xdr_bytes(xdrs, &readres_ptr->readres_u.reply.data.data_val,
			     &readres_ptr->readres_u.reply.data.data_len,
			     (unsigned int) NFS_MAXDATA));

	default:
	    return(FALSE);
    } /* switch on operation */
}

bool_t
xdr_write(
    XDR *		xdrs,
    char *		params_ptr)
{
    writeargs *		writeargs_ptr;
    attrstat *		attrstat_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    writeargs_ptr = (writeargs *) params_ptr;
	    return(xdr_f_handle(xdrs, &writeargs_ptr->file) &&
		   xdr_u_int(xdrs, &writeargs_ptr->beginoffset) &&
		   xdr_u_int(xdrs, &writeargs_ptr->offset) &&
		   xdr_u_int(xdrs, &writeargs_ptr->totalcount) &&
		   xdr_bytes(xdrs, &writeargs_ptr->data.data_val,
				   &writeargs_ptr->data.data_len,
				   (unsigned int) NFS_MAXDATA));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    attrstat_ptr = (attrstat *) params_ptr;
	    return(xdr_attrstat(xdrs, attrstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_create(
    XDR *		xdrs,
    char *		params_ptr)
{
    createargs *	createargs_ptr;
    diropres *		diropres_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    createargs_ptr = (createargs *) params_ptr;
	    return(xdr_diropargs(xdrs, &createargs_ptr->where) &&
		   xdr_sattr(xdrs, &createargs_ptr->attributes));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    diropres_ptr = (diropres *) params_ptr;
	    return(xdr_diropres(xdrs, diropres_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_remove(
    XDR *		xdrs,
    char *		params_ptr)
{
    diropargs *		diropargs_ptr;
    nfsstat *		nfsstat_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    diropargs_ptr = (diropargs *) params_ptr;
	    return(xdr_diropargs (xdrs, diropargs_ptr));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    nfsstat_ptr = (nfsstat *) params_ptr;
	    return(xdr_nfsstat(xdrs, nfsstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_rename(
    XDR *		xdrs,
    char *		params_ptr)
{
    renameargs *	renameargs_ptr;
    nfsstat *		nfsstat_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    renameargs_ptr = (renameargs *) params_ptr;
	    return(xdr_diropargs(xdrs, &renameargs_ptr->from) &&
		   xdr_diropargs(xdrs, &renameargs_ptr->to));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    nfsstat_ptr = (nfsstat *) params_ptr;
	    return(xdr_nfsstat(xdrs, nfsstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_link(
    XDR *		xdrs,
    char *		params_ptr)
{
    linkargs *		linkargs_ptr;
    nfsstat *		nfsstat_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    linkargs_ptr = (linkargs *) params_ptr;
	    return(xdr_f_handle(xdrs, &linkargs_ptr->from) &&
		   xdr_diropargs(xdrs, &linkargs_ptr->to));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    nfsstat_ptr = (nfsstat *) params_ptr;
	    return(xdr_nfsstat(xdrs, nfsstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_symlink(
    XDR *		xdrs,
    char *		params_ptr)
{
    symlinkargs *	symlinkargs_ptr;
    nfsstat *		nfsstat_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    symlinkargs_ptr = (symlinkargs *) params_ptr;
	    return(xdr_diropargs(xdrs, &symlinkargs_ptr->from) &&
		   xdr_path(xdrs, &symlinkargs_ptr->to) &&
		   xdr_sattr(xdrs, &symlinkargs_ptr->attributes));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    nfsstat_ptr = (nfsstat *) params_ptr;
	    return(xdr_nfsstat(xdrs, nfsstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_mkdir(
    XDR *		xdrs,
    char *		params_ptr)
{
    mkdirargs *		mkdirargs_ptr;
    diropres *		diropres_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    mkdirargs_ptr = (mkdirargs *) params_ptr;
	    return(xdr_diropargs(xdrs, &mkdirargs_ptr->where) &&
		   xdr_sattr(xdrs, &mkdirargs_ptr->attributes));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    diropres_ptr = (diropres *) params_ptr;
	    return(xdr_diropres(xdrs, diropres_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_rmdir(
    XDR *		xdrs,
    char *		params_ptr)
{
    diropargs *		diropargs_ptr;
    nfsstat *		nfsstat_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    diropargs_ptr = (diropargs *) params_ptr;
	    return(xdr_diropargs(xdrs, diropargs_ptr));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    nfsstat_ptr = (nfsstat *) params_ptr;
	    return(xdr_nfsstat(xdrs, nfsstat_ptr));

	default:
	    return(FALSE);
    } /* switch on operation */
}


bool_t
xdr_readdir(
    XDR *		xdrs,
    char *		params_ptr)
{
    readdirargs *	readdirargs_ptr;
    readdirres *	readdirres_ptr;
    entry *		entry_ptr;
    int			n;		/* entry ctr */


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    /* LINTED pointer cast */
	    readdirargs_ptr = (readdirargs *) params_ptr;
	    return(xdr_f_handle(xdrs, &readdirargs_ptr->dir) &&
		   xdr_opaque(xdrs, (char *) readdirargs_ptr->cookie,
				    NFS_COOKIESIZE) &&
		   xdr_u_int(xdrs, &readdirargs_ptr->count));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    readdirres_ptr = (readdirres *) params_ptr;
	    if (!xdr_nfsstat(xdrs, &readdirres_ptr->status)) {
		return(FALSE);
	    }
	    if (readdirres_ptr->status != NFS_OK) {
		return(TRUE);
	    }

	    /*
	     * go thru the stream of entries until hit an invalid one
	     * or have gotten all the user asked for.
	     *
	     * max_entries is read to obtain a maximum.  it is written
	     * to return how many entries were decoded.
	     */
	    entry_ptr = readdirres_ptr->readdirres_u.reply.entries;

	    n = 0;
	    while (n < readdirres_ptr->readdirres_u.reply.max_entries) {
		if (!xdr_bool(xdrs, &entry_ptr->valid)) {
		    return(FALSE);
		}

		if (!entry_ptr->valid) {
		    break;
		}

		if (!xdr_u_int(xdrs, &entry_ptr->fileid)) {
		    return(FALSE);
		}

		if (!xdr_uint16_t(xdrs, &entry_ptr->name_len)) {
		    return(FALSE);
		}

		if (!xdr_opaque(xdrs, entry_ptr->name, entry_ptr->name_len)) {
		    return(FALSE);
		}

		if (!xdr_opaque(xdrs, entry_ptr->cookie, NFS_COOKIESIZE)) {
		    return(FALSE);
		}

		n++;
		entry_ptr++;
	    } /* while extracting entries */

	    /* If we are at the user's data buffer limit, stop right now.  */
	    if (n == readdirres_ptr->readdirres_u.reply.max_entries) {
		return(TRUE);
	    }

	    /* Return how many entries were gotten for the dirlist */
	    readdirres_ptr->readdirres_u.reply.max_entries = n;

	    /* check the EOF flag for the dirlist */
	    if(!xdr_bool(xdrs, &readdirres_ptr->readdirres_u.reply.eof)) {
		return(FALSE);
	    }

	    return(TRUE);

	default:
	    return(FALSE);
    } /* switch on operation */
}

bool_t
xdr_statfs(
    XDR *		xdrs,
    char *		params_ptr)
{
    fhandle_t *		fhandle_ptr;
    statfsres *		statfsres_ptr;


    switch (xdrs->x_op) {
	case XDR_ENCODE:
	    fhandle_ptr = (fhandle_t *) params_ptr;
	    return(xdr_f_handle(xdrs, fhandle_ptr));

	case XDR_DECODE:
	    /* LINTED pointer cast */
	    statfsres_ptr = (statfsres *) params_ptr;
	    if (!xdr_nfsstat(xdrs, &statfsres_ptr->status)) {
		return(FALSE);
	    }
	    if (statfsres_ptr->status != NFS_OK) {
		return(TRUE);
	    }
	    return(xdr_u_int(xdrs, &statfsres_ptr->statfsres_u.reply.tsize) &&
		   xdr_u_int(xdrs, &statfsres_ptr->statfsres_u.reply.bsize) &&
		   xdr_u_int(xdrs, &statfsres_ptr->statfsres_u.reply.blocks) &&
		   xdr_u_int(xdrs, &statfsres_ptr->statfsres_u.reply.bfree) &&
		   xdr_u_int(xdrs, &statfsres_ptr->statfsres_u.reply.bavail));

	default:
	    return(FALSE);
    } /* switch on operation */
}


/* sfs_c_xdr.c */
