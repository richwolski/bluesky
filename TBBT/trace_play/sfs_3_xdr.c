#ifndef lint
static char sfs_3_xdrSid[] = "@(#)sfs_3_xdr.c 2.1     97/10/23";
#endif

/*
 *   Copyright (c) 1992-1997,2001 by Standard Performance Evaluation Corporation
 *	All rights reserved.
 *		Standard Performance Evaluation Corporation (SPEC)
 *              6585 Merchant Place, Suite 100
 *              Warrenton, VA 20187
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
 * -------------------------- sfs_3_xdr.c --------------------------
 *
 *      XDR routines for the nfs protocol.
 *
 *.Exported_routines
 *	xdr_GETATTR3args(XDR *, GETATTR3args *)
 *	xdr_GETATTR3res(XDR *, GETATTR3res *)
 *	xdr_SETATTR3args(XDR *, SETATTR3args *)
 *	xdr_SETATTR3res(XDR *, SETATTR3res *)
 *	xdr_LOOKUP3args(XDR *, LOOKUP3args *)
 *	xdr_LOOKUP3res(XDR *, LOOKUP3res *)
 *	xdr_ACCESS3args(XDR *, ACCESS3args *)
 *	xdr_ACCESS3res(XDR *, ACCESS3res *)
 *	xdr_READLINK3args(XDR *, READLINK3args *)
 *	xdr_READLINK3res(XDR *, READLINK3res *)
 *	xdr_READ3args(XDR *, READ3args *)
 *	xdr_READ3res(XDR *, READ3res *)
 *	xdr_WRITE3args(XDR *, WRITE3args *)
 *	xdr_WRITE3res(XDR *, WRITE3res *)
 *	xdr_CREATE3args(XDR *, CREATE3args *)
 *	xdr_CREATE3res(XDR *, CREATE3res *)
 *	xdr_MKDIR3args(XDR *, MKDIR3args *)
 *	xdr_MKDIR3res(XDR *, MKDIR3res *)
 *	xdr_SYMLINK3args(XDR *, SYMLINK3args *)
 *	xdr_SYMLINK3res(XDR *, SYMLINK3res *)
 *	xdr_MKNOD3args(XDR *, MKNOD3args *)
 *	xdr_MKNOD3res(XDR *, MKNOD3res *)
 *	xdr_REMOVE3args(XDR *, REMOVE3args *)
 *	xdr_REMOVE3res(XDR *, REMOVE3res *)
 *	xdr_RMDIR3args(XDR *, RMDIR3args *)
 *	xdr_RMDIR3res(XDR *, RMDIR3res *)
 *	xdr_RENAME3args(XDR *, RENAME3args *)
 *	xdr_RENAME3res(XDR *, RENAME3res *)
 *	xdr_LINK3args(XDR *, LINK3args *)
 *	xdr_LINK3res(XDR *, LINK3res *)
 *	xdr_READDIR3args(XDR *, READDIR3args *)
 *	xdr_READDIR3res(XDR *, READDIR3res *)
 *	xdr_READDIRPLUS3args(XDR *, READDIRPLUS3args *)
 *	xdr_READDIRPLUS3res(XDR *, READDIRPLUS3res *)
 *	xdr_FSSTAT3args(XDR *, FSSTAT3args *)
 *	xdr_FSSTAT3res(XDR *, FSSTAT3res *)
 *	xdr_FSINFO3args(XDR *, FSINFO3args *)
 *	xdr_FSINFO3res(XDR *, FSINFO3res *)
 *	xdr_PATHCONF3args(XDR *, PATHCONF3args *)
 *	xdr_PATHCONF3res(XDR *, PATHCONF3res *)
 *	xdr_COMMIT3args(XDR *, COMMIT3args *)
 *	xdr_COMMIT3res(XDR *, COMMIT3res *)
 *	xdr_mntres3(XDR *, mountres3 *)
 *
 *.Local_routines
 *	xdr_string3(XDR *, char **, unsigned int)
 *	xdr_filename3(XDR *, filename3 *)
 *	xdr_nfspath3(XDR *, nfspath3 *)
 *	xdr_nfs_uint64_t(XDR *, nfs_uint64_t *)
 *	xdr_cookieverf3(XDR *, cookieverf3)
 *	xdr_createverf3(XDR *, createverf3)
 *	xdr_writeverf3(XDR *, writeverf3)
 *	xdr_nfs_fh3(XDR *, nfs_fh3 *)
 *	xdr_diropargs3(XDR *, diropargs3 *)
 *	xdr_nfstime3(XDR *, nfstime3 *)
 *	xdr_specdata3(XDR *, specdata3 *)
 *	xdr_nfsstat3(XDR *, nfsstat3 *)
 *	xdr_ftype3(XDR *, ftype3 *)
 *	xdr_fattr3(XDR *, fattr3 *)
 *	xdr_post_op_attr(XDR *, post_op_attr *)
 *	xdr_wcc_attr(XDR *, wcc_attr *)
 *	xdr_pre_op_attr(XDR *, pre_op_attr *)
 *	xdr_wcc_data(XDR *, wcc_data *)
 *	xdr_post_op_fh3(XDR *, post_op_fh3 *)
 *	xdr_time_how(XDR *, time_how *)
 *	xdr_set_mode3(XDR *, set_mode3 *)
 *	xdr_set_uid3(XDR *, set_uid3 *)
 *	xdr_set_gid3(XDR *, set_gid3 *)
 *	xdr_set_size3(XDR *, set_size3 *)
 *	xdr_set_atime(XDR *, set_atime *)
 *	xdr_set_mtime(XDR *, set_mtime *)
 *	xdr_sattr3(XDR *, sattr3 *)
 *	xdr_GETATTR3resok(XDR *, GETATTR3resok *)
 *	xdr_sattrguard3(XDR *, sattrguard3 *)
 *	xdr_SETATTR3resok(XDR *, SETATTR3resok *)
 *	xdr_SETATTR3resfail(XDR *, SETATTR3resfail *)
 *	xdr_LOOKUP3resok(XDR *, LOOKUP3resok *)
 *	xdr_LOOKUP3resfail(XDR *, LOOKUP3resfail *)
 *	xdr_ACCESS3resok(XDR *, ACCESS3resok *)
 *	xdr_ACCESS3resfail(XDR *, ACCESS3resfail *)
 *	xdr_READLINK3resok(XDR *, READLINK3resok *)
 *	xdr_READLINK3resfail(XDR *, READLINK3resfail *)
 *	xdr_READ3resok(XDR *, READ3resok *)
 *	xdr_READ3resfail(XDR *, READ3resfail *)
 *	xdr_stable_how(XDR *, stable_how *)
 *	xdr_WRITE3resok(XDR *, WRITE3resok *)
 *	xdr_WRITE3resfail(XDR *, WRITE3resfail *)
 *	xdr_createmode3(XDR *, createmode3 *)
 *	xdr_createhow3(XDR *, createhow3 *)
 *	xdr_CREATE3resok(XDR *, CREATE3resok *)
 *	xdr_CREATE3resfail(XDR *, CREATE3resfail *)
 *	xdr_MKDIR3resok(XDR *, MKDIR3resok *)
 *	xdr_MKDIR3resfail(XDR *, MKDIR3resfail *)
 *	xdr_symlinkdata3(XDR *, symlinkdata3 *)
 *	xdr_SYMLINK3resok(XDR *, SYMLINK3resok *)
 *	xdr_SYMLINK3resfail(XDR *, SYMLINK3resfail *)
 *	xdr_devicedata3(XDR *, devicedata3 *)
 *	xdr_mknoddata3(XDR *, mknoddata3 *)
 *	xdr_MKNOD3resok(XDR *, MKNOD3resok *)
 *	xdr_MKNOD3resfail(XDR *, MKNOD3resfail *)
 *	xdr_REMOVE3resok(XDR *, REMOVE3resok *)
 *	xdr_REMOVE3resfail(XDR *, REMOVE3resfail *)
 *	xdr_RMDIR3resok(XDR *, RMDIR3resok *)
 *	xdr_RMDIR3resfail(XDR *, RMDIR3resfail *)
 *	xdr_RENAME3resok(XDR *, RENAME3resok *)
 *	xdr_RENAME3resfail(XDR *, RENAME3resfail *)
 *	xdr_LINK3resok(XDR *, LINK3resok *)
 *	xdr_LINK3resfail(XDR *, LINK3resfail *)
 *	xdr_getdirlist(XDR *, READDIR3resok *)
 *	xdr_READDIR3resok(XDR *, READDIR3resok *)
 *	xdr_READDIR3resfail(XDR *, READDIR3resfail *)
 *	xdr_getdirpluslist(XDR *, READDIRPLUS3resok *)
 *	xdr_READDIRPLUS3resok(XDR *, READDIRPLUS3resok *)
 *	xdr_READDIRPLUS3resfail(XDR *, READDIRPLUS3resfail *)
 *	xdr_FSSTAT3resok(XDR *, FSSTAT3resok *)
 *	xdr_FSSTAT3resfail(XDR *, FSSTAT3resfail *)
 *	xdr_FSINFO3resok(XDR *, FSINFO3resok *)
 *	xdr_FSINFO3resfail(XDR *, FSINFO3resfail *)
 *	xdr_PATHCONF3resok(XDR *, PATHCONF3resok *)
 *	xdr_PATHCONF3resfail(XDR *, PATHCONF3resfail *)
 *	xdr_COMMIT3resok(XDR *, COMMIT3resok *)
 *	xdr_COMMIT3resfail(XDR *, COMMIT3resfail *)
 *
 *.Revision_History
 *      28-Jun-94	ChakChung Ng	Created.
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
 * -----------------------  SFS XDR Routines  -------------------------
 */

/*
 * xdr_string3 deals with "C strings" - arrays of bytes that are terminated by
 * a NULL character.  The parameter cpp references a pointer to storage. If the
 * pointer is null, then necessary storage is allocated.  The last parameter is
 * the max allowed length of the string as allowed by the system.  The NFS
 * Version 3 protocol does not place limits on strings, but the implementation
 * needs to place a reasonable limit to avoid problems.
 */

static bool_t
xdr_string3(
    XDR *		xdrs,
    char **		cpp,
    unsigned int	maxsize)
{
	char *sp;
	unsigned int size, nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	sp = *cpp;
	switch (xdrs->x_op) {
	case XDR_FREE:
		if (sp == NULL || sp == nfs3nametoolong)
			return(TRUE);  /* already free */
		/* FALLTHROUGH */
	case XDR_ENCODE:
		size = strlen(sp);
		break;
	}

	if (!xdr_u_int(xdrs, &size))
		return(FALSE);

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {
	case XDR_DECODE:
		if (size > maxsize) {
			int xskp = (((((int)size) + 3) / 4) * 4);

			*cpp = nfs3nametoolong;
			if ((xdrs->x_handy -= xskp) < 0)
				return(FALSE);
			xdrs->x_private += xskp;
			return(TRUE);
		}
		nodesize = size + 1;
		if (nodesize == 0)
			return(TRUE);
		if (sp == NULL) {
			sp = (char *)malloc(nodesize);
			*cpp = sp;
			if (sp == NULL)
				return(FALSE);
		}
		sp[size] = 0;	/* 0 through size-1 are original string */
		/* FALLTHROUGH */
	case XDR_ENCODE:
		return(xdr_opaque(xdrs, sp, size));
	case XDR_FREE:
		nodesize = size + 1;
		mem_free((caddr_t)sp, nodesize);
		*cpp = NULL;
		return(TRUE);
	}

	return(FALSE);
}

static bool_t
xdr_filename3(
    XDR *		xdrs,
    filename3 *		objp)
{

	return(xdr_string3(xdrs, objp, NFS_MAXNAMLEN));
}

static bool_t
xdr_nfspath3(
    XDR *		xdrs,
    nfspath3 *		objp)
{

	return(xdr_string3(xdrs, objp, NFS_MAXPATHLEN));
}

static bool_t
xdr_nfs_uint64_t(
    XDR			*xdrs,
    nfs_uint64_t *	objp)
{
	return(xdr_int(xdrs, (int *)&objp->_p._u) &&
					xdr_int(xdrs, (int *)&objp->_p._l));
}

static bool_t
xdr_cookieverf3(
    XDR *		xdrs,
    cookieverf3		objp)
{
	return(xdr_opaque(xdrs, objp, NFS3_COOKIEVERFSIZE));
}

static bool_t
xdr_createverf3(
    XDR *		xdrs,
    createverf3		objp)
{
	return(xdr_opaque(xdrs, objp, NFS3_CREATEVERFSIZE));
}

static bool_t
xdr_writeverf3(
    XDR *		xdrs,
    writeverf3		objp)
{
	return(xdr_opaque(xdrs, objp, NFS3_WRITEVERFSIZE));
}

static bool_t
xdr_nfs_fh3(
    XDR *		xdrs,
    nfs_fh3 *		objp)
{
	if (!xdr_u_int(xdrs, &objp->fh3_length))
		return(FALSE);

	if (objp->fh3_length > NFS3_FHSIZE)
		return(FALSE);
	if (xdrs->x_op == XDR_DECODE || xdrs->x_op == XDR_ENCODE)
		return(xdr_opaque(xdrs, objp->fh3_u.data, objp->fh3_length));

	if (xdrs->x_op == XDR_FREE)
		return(TRUE);

	return(FALSE);
}

static bool_t
xdr_diropargs3(
    XDR *		xdrs,
    diropargs3 *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->dir))
		return(xdr_filename3(xdrs, &objp->name));
	return(FALSE);
}

static bool_t
xdr_nfstime3(
    XDR *		xdrs,
    nfstime3 *		objp)
{
	if (xdr_uint32_t(xdrs, &objp->seconds))
		return(xdr_uint32_t(xdrs, &objp->nseconds));
	return(FALSE);
}

static bool_t
xdr_specdata3(
    XDR *		xdrs,
    specdata3 *		objp)
{
	if (xdr_uint32_t(xdrs, &objp->specdata1))
		return(xdr_uint32_t(xdrs, &objp->specdata2));
	return(FALSE);
}

static bool_t
xdr_nfsstat3(
    XDR *		xdrs,
    nfsstat3 *		objp)
{
	return(xdr_enum(xdrs, (enum_t *)objp));
}

static bool_t
xdr_ftype3(
    XDR *		xdrs,
    ftype3 *		objp)
{
	return(xdr_enum(xdrs, (enum_t *)objp));
}

static bool_t
xdr_fattr3(
    XDR *		xdrs,
    fattr3 *		objp)
{
	if (!xdr_ftype3(xdrs, &objp->type))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->mode))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->nlink))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->uid))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->gid))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->size))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->used))
		return(FALSE);
	if (!xdr_specdata3(xdrs, &objp->rdev))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->fsid))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->fileid))
		return(FALSE);
	if (!xdr_nfstime3(xdrs, &objp->atime))
		return(FALSE);
	if (!xdr_nfstime3(xdrs, &objp->mtime))
		return(FALSE);
	if (!xdr_nfstime3(xdrs, &objp->ctime))
		return(FALSE);
	return(TRUE);
}
 
static bool_t
xdr_post_op_attr(
    XDR *		xdrs,
    post_op_attr *	objp)
{
	if (!xdr_bool(xdrs, &objp->attributes))
		return(FALSE);
	switch (objp->attributes) {
	case TRUE:	return(xdr_fattr3(xdrs, &objp->attr));
	case FALSE:	return(TRUE);
	default:	return(FALSE);
	}
}

static bool_t
xdr_wcc_attr(
    XDR *		xdrs,
    wcc_attr *		objp)
{
	if (xdr_nfs_uint64_t(xdrs, &objp->size) &&
					xdr_nfstime3(xdrs, &objp->mtime))
		return(xdr_nfstime3(xdrs, &objp->ctime));
	return(FALSE);
}

static bool_t
xdr_pre_op_attr(
    XDR *		xdrs,
    pre_op_attr *	objp)
{
	if (!xdr_bool(xdrs, &objp->attributes))
		return(FALSE);
	switch (objp->attributes) {
	case TRUE:	return(xdr_wcc_attr(xdrs, &objp->attr));
	case FALSE:	return(TRUE);
	default:	return(FALSE);
	}
}
 
static bool_t
xdr_wcc_data(
    XDR *		xdrs,
    wcc_data *		objp)
{
	if (xdr_pre_op_attr(xdrs, &objp->before))
		return(xdr_post_op_attr(xdrs, &objp->after));
	return(FALSE);
}
 
static bool_t
xdr_post_op_fh3(
    XDR *		xdrs,
    post_op_fh3 *	objp)
{
	if (!xdr_bool(xdrs, &objp->handle_follows))
		return(FALSE);
	switch (objp->handle_follows) {
	case TRUE:	return(xdr_nfs_fh3(xdrs, &objp->handle));
	case FALSE:	return(TRUE);
	default:  	return(FALSE);
	}
}	
 
static bool_t
xdr_time_how(
    XDR *		xdrs,
    time_how *		objp)
{
	return(xdr_enum(xdrs, (enum_t *)objp));
}

static bool_t
xdr_set_mode3(
    XDR *		xdrs,
    set_mode3 *		objp)
{
	if (!xdr_bool(xdrs, &objp->set_it))
		return(FALSE);
	if (objp->set_it == TRUE)
		return(xdr_uint32_t(xdrs, &objp->mode));
	return(TRUE);
}	
 
static bool_t
xdr_set_uid3(
    XDR *		xdrs,
    set_uid3 *		objp)
{
	if (!xdr_bool(xdrs, &objp->set_it))
		return(FALSE);
	if (objp->set_it == TRUE)
		return(xdr_uint32_t(xdrs, &objp->uid));
	return(TRUE);
}	
 
static bool_t
xdr_set_gid3(
    XDR *		xdrs,
    set_gid3 *		objp)
{
	if (!xdr_bool(xdrs, &objp->set_it))
		return(FALSE);
	if (objp->set_it == TRUE)
		return(xdr_uint32_t(xdrs, &objp->gid));
	return(TRUE);
}

static bool_t
xdr_set_size3(
    XDR *		xdrs,
    set_size3 *		objp)
{
	if (!xdr_bool(xdrs, &objp->set_it))
		return(FALSE);
	if (objp->set_it == TRUE)
		return(xdr_nfs_uint64_t(xdrs, &objp->size));
	return(TRUE);
}

static bool_t
xdr_set_atime(
    XDR *		xdrs,
    set_atime *		objp)
{
	if (!xdr_time_how(xdrs, &objp->set_it))
		return(FALSE);
	if (objp->set_it == SET_TO_CLIENT_TIME)
		return(xdr_nfstime3(xdrs, &objp->atime));
	return(TRUE);
}

static bool_t
xdr_set_mtime(
    XDR *		xdrs,
    set_mtime *		objp)
{
	if (!xdr_time_how(xdrs, &objp->set_it))
		return(FALSE);
	if (objp->set_it == SET_TO_CLIENT_TIME)
		return(xdr_nfstime3(xdrs, &objp->mtime));
	return(TRUE);
}

static bool_t
xdr_sattr3(
    XDR *		xdrs,
    sattr3 *		objp)
{
	if (xdr_set_mode3(xdrs, &objp->mode) &&
					xdr_set_uid3(xdrs, &objp->uid) &&
					xdr_set_gid3(xdrs, &objp->gid) &&
					xdr_set_size3(xdrs, &objp->size) &&
					xdr_set_atime(xdrs, &objp->atime))
		return(xdr_set_mtime(xdrs, &objp->mtime));
	return(FALSE);
}


bool_t
xdr_GETATTR3args(
    XDR *		xdrs,
    GETATTR3args *	objp)
{
	return(xdr_nfs_fh3(xdrs, &objp->object));
}

static bool_t
xdr_GETATTR3resok(
    XDR *		xdrs,
    GETATTR3resok *	objp)
{
	return(xdr_fattr3(xdrs, &objp->obj_attributes));
}

bool_t
xdr_GETATTR3res(
    XDR *		xdrs,
    GETATTR3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_GETATTR3resok(xdrs, &objp->resok));
	return(TRUE);
}


static bool_t
xdr_sattrguard3(
    XDR *		xdrs,
    sattrguard3 *	objp)
{
	if (!xdr_bool(xdrs, &objp->check))
		return(FALSE);
	switch (objp->check) {
	case TRUE:	return(xdr_nfstime3(xdrs, &objp->obj_ctime));
	case FALSE:	return(TRUE);
	default:	return(FALSE);
	}
}

bool_t
xdr_SETATTR3args(
    XDR *		xdrs,
    SETATTR3args *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->object) &&
				xdr_sattr3(xdrs, &objp->new_attributes))
		return(xdr_sattrguard3(xdrs, &objp->guard));
	return(FALSE);
}

static bool_t
xdr_SETATTR3resok(
    XDR *		xdrs,
    SETATTR3resok *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->obj_wcc));
}

static bool_t
xdr_SETATTR3resfail(
    XDR *		xdrs,
    SETATTR3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->obj_wcc));
}

bool_t
xdr_SETATTR3res(
    XDR *		xdrs,
    SETATTR3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_SETATTR3resok(xdrs, &objp->resok));
	return(xdr_SETATTR3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_LOOKUP3args(
    XDR *		xdrs,
    LOOKUP3args *	objp)
{
	return(xdr_diropargs3(xdrs, &objp->what));
}
 
static bool_t
xdr_LOOKUP3resok(
    XDR *		xdrs,
    LOOKUP3resok *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->object) &&
				xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(xdr_post_op_attr(xdrs, &objp->dir_attributes));
	return(FALSE);
}
 
static bool_t
xdr_LOOKUP3resfail(
    XDR *		xdrs,
    LOOKUP3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->dir_attributes));
}
 
bool_t
xdr_LOOKUP3res(
    XDR *		xdrs,
    LOOKUP3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_LOOKUP3resok(xdrs, &objp->resok));
	return(xdr_LOOKUP3resfail(xdrs, &objp->resfail));
}	
 

bool_t
xdr_ACCESS3args(
    XDR *		xdrs,
    ACCESS3args *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->object))
		return(xdr_uint32_t(xdrs, &objp->access));
	return(FALSE);
}

static bool_t
xdr_ACCESS3resok(
    XDR *		xdrs,
    ACCESS3resok *	objp)
{
	if (xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(xdr_uint32_t(xdrs, &objp->access));
	return(FALSE);
}

static bool_t
xdr_ACCESS3resfail(
    XDR *		xdrs,
    ACCESS3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->obj_attributes));
}

bool_t
xdr_ACCESS3res(
    XDR *		xdrs,
    ACCESS3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_ACCESS3resok(xdrs, &objp->resok));
	return(xdr_ACCESS3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_READLINK3args(
    XDR *		xdrs,
    READLINK3args *	objp)
{
	return(xdr_nfs_fh3(xdrs, &objp->symlink));
}

static bool_t
xdr_READLINK3resok(
    XDR *		xdrs,
    READLINK3resok *	objp)
{
	if (xdr_post_op_attr(xdrs, &objp->symlink_attributes))
		return(xdr_nfspath3(xdrs, &objp->data));
	return(FALSE);
}

static bool_t
xdr_READLINK3resfail(
    XDR *		xdrs,
    READLINK3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->symlink_attributes));
}
 
bool_t
xdr_READLINK3res(
    XDR *		xdrs,
    READLINK3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_READLINK3resok(xdrs, &objp->resok));
	return(xdr_READLINK3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_READ3args(
    XDR *		xdrs,
    READ3args *		objp)
{
 
	if (xdr_nfs_fh3(xdrs, &objp->file) &&
					xdr_nfs_uint64_t(xdrs, &objp->offset))
		return(xdr_uint32_t(xdrs, &objp->count));
	return(FALSE);
}

static bool_t
xdr_READ3resok(
    XDR *		xdrs,
    READ3resok *	objp)
{
	if (xdr_post_op_attr(xdrs, &objp->file_attributes) &&
					xdr_uint32_t(xdrs, &objp->count) &&
					xdr_bool(xdrs, &objp->eof))
		return(xdr_bytes(xdrs, (char **)&objp->data.data_val,
					(unsigned int *)&objp->data.data_len,
					~0));
	return(FALSE);
}

static bool_t
xdr_READ3resfail(
    XDR *		xdrs,
    READ3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->file_attributes));
}

bool_t
xdr_READ3res(
    XDR *		xdrs,
    READ3res *		objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_READ3resok(xdrs, &objp->resok));
	return(xdr_READ3resfail(xdrs, &objp->resfail));
}


static bool_t
xdr_stable_how(
    XDR *		xdrs,
    stable_how *	objp)
{
	return(xdr_enum(xdrs, (enum_t *)objp));
}

bool_t
xdr_WRITE3args(
    XDR *		xdrs,
    WRITE3args *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->file) &&
					xdr_nfs_uint64_t(xdrs, &objp->offset) &&
					xdr_uint32_t(xdrs, &objp->count) &&
					xdr_stable_how(xdrs, &objp->stable))
		return(xdr_bytes(xdrs, (char **)&objp->data.data_val,
					(unsigned int *)&objp->data.data_len,
					~0));
	return(FALSE);
}

static bool_t
xdr_WRITE3resok(
    XDR *		xdrs,
    WRITE3resok *	objp)
{
	if (xdr_wcc_data(xdrs, &objp->file_wcc) &&
					xdr_uint32_t(xdrs, &objp->count) &&
					xdr_stable_how(xdrs, &objp->committed))
		return(xdr_writeverf3(xdrs, objp->verf));
	return(FALSE);
}

static bool_t
xdr_WRITE3resfail(
    XDR *		xdrs,
    WRITE3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->file_wcc));
}

bool_t
xdr_WRITE3res(
    XDR *		xdrs,
    WRITE3res *		objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_WRITE3resok(xdrs, &objp->resok));
	return(xdr_WRITE3resfail(xdrs, &objp->resfail));
}


static bool_t
xdr_createmode3(
    XDR *		xdrs,
    createmode3 *	objp)
{
	return(xdr_enum(xdrs, (enum_t *)objp));
}

static bool_t
xdr_createhow3(
    XDR *		xdrs,
    createhow3 *	objp)
{
	if (!xdr_createmode3(xdrs, &objp->mode))
		return(FALSE);
	switch (objp->mode) {
	case UNCHECKED:
	case GUARDED:
		return(xdr_sattr3(xdrs, &objp->createhow3_u.obj_attributes));
	case EXCLUSIVE:
		return(xdr_createverf3(xdrs, objp->createhow3_u.verf));
	default:
		return(FALSE);
	}
}

bool_t
xdr_CREATE3args(
    XDR *		xdrs,
    CREATE3args *	objp)
{
	if (xdr_diropargs3(xdrs, &objp->where))
		return(xdr_createhow3(xdrs, &objp->how));
	return(FALSE);
}

static bool_t
xdr_CREATE3resok(
    XDR *		xdrs,
    CREATE3resok *	objp)
{
	if (xdr_post_op_fh3(xdrs, &objp->obj) &&
				xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(xdr_wcc_data(xdrs, &objp->dir_wcc));
	return(FALSE);
}

static bool_t
xdr_CREATE3resfail(
    XDR *		xdrs,
    CREATE3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

bool_t
xdr_CREATE3res(
    XDR *		xdrs,
    CREATE3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_CREATE3resok(xdrs, &objp->resok));
	return(xdr_CREATE3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_MKDIR3args(
    XDR *		xdrs,
    MKDIR3args *	objp)
{
	if (xdr_diropargs3(xdrs, &objp->where))
		return(xdr_sattr3(xdrs, &objp->attributes));
	return(FALSE);
}

static bool_t
xdr_MKDIR3resok(
    XDR *		xdrs,
    MKDIR3resok *	objp)
{
	if (xdr_post_op_fh3(xdrs, &objp->obj) &&
				xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(xdr_wcc_data(xdrs, &objp->dir_wcc));
	return(FALSE);
}

static bool_t
xdr_MKDIR3resfail(
    XDR *		xdrs,
    MKDIR3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

bool_t
xdr_MKDIR3res(
    XDR *		xdrs,
    MKDIR3res *		objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_MKDIR3resok(xdrs, &objp->resok));
	return(xdr_MKDIR3resfail(xdrs, &objp->resfail));
}


static bool_t
xdr_symlinkdata3(
    XDR *		xdrs,
    symlinkdata3 *	objp)
{
	if (xdr_sattr3(xdrs, &objp->symlink_attributes))
		return(xdr_nfspath3(xdrs, &objp->symlink_data));
	return(FALSE);
}

bool_t
xdr_SYMLINK3args(
    XDR *		xdrs,
    SYMLINK3args *	objp)
{
	if (xdr_diropargs3(xdrs, &objp->where))
		return(xdr_symlinkdata3(xdrs, &objp->symlink));
	return(FALSE);
}

static bool_t
xdr_SYMLINK3resok(
    XDR *		xdrs,
    SYMLINK3resok *	objp)
{
	if (xdr_post_op_fh3(xdrs, &objp->obj) &&
				xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(xdr_wcc_data(xdrs, &objp->dir_wcc));
	return(FALSE);
}

static bool_t
xdr_SYMLINK3resfail(
    XDR *		xdrs,
    SYMLINK3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

bool_t
xdr_SYMLINK3res(
    XDR *		xdrs,
    SYMLINK3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_SYMLINK3resok(xdrs, &objp->resok));
	return(xdr_SYMLINK3resfail(xdrs, &objp->resfail));
}


static bool_t
xdr_devicedata3(
    XDR *		xdrs,
    devicedata3 *	objp)
{
	if (xdr_sattr3(xdrs, &objp->dev_attributes))
		return(xdr_specdata3(xdrs, &objp->spec));
	return(FALSE);
}

static bool_t
xdr_mknoddata3(
    XDR *		xdrs,
    mknoddata3 *	objp)
{
	if (!xdr_ftype3(xdrs, &objp->type))
		return(FALSE);
	switch (objp->type) {
	case NF3CHR:
	case NF3BLK:
		if (!xdr_devicedata3(xdrs, &objp->mknoddata3_u.device))
			return(FALSE);
		break;
	case NF3SOCK:
	case NF3FIFO:
		if (!xdr_sattr3(xdrs, &objp->mknoddata3_u.pipe_attributes))
			return(FALSE);
		break;
	}
	return(TRUE);
}

bool_t
xdr_MKNOD3args(
    XDR *		xdrs,
    MKNOD3args *	objp)
{
	if (xdr_diropargs3(xdrs, &objp->where))
		return(xdr_mknoddata3(xdrs, &objp->what));
	return(FALSE);
}

static bool_t
xdr_MKNOD3resok(
    XDR *		xdrs,
    MKNOD3resok *	objp)
{
	if (xdr_post_op_fh3(xdrs, &objp->obj) &&
				xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(xdr_wcc_data(xdrs, &objp->dir_wcc));
	return(FALSE);
}

static bool_t
xdr_MKNOD3resfail(
    XDR *		xdrs,
    MKNOD3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

bool_t
xdr_MKNOD3res(
    XDR *		xdrs,
    MKNOD3res *		objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_MKNOD3resok(xdrs, &objp->resok));
	return(xdr_MKNOD3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_REMOVE3args(
    XDR *		xdrs,
    REMOVE3args *	objp)
{
	return(xdr_diropargs3(xdrs, &objp->object));
}

static bool_t
xdr_REMOVE3resok(
    XDR *		xdrs,
    REMOVE3resok *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

static bool_t
xdr_REMOVE3resfail(
    XDR *		xdrs,
    REMOVE3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

bool_t
xdr_REMOVE3res(
    XDR *		xdrs,
    REMOVE3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_REMOVE3resok(xdrs, &objp->resok));
	return(xdr_REMOVE3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_RMDIR3args(
    XDR *		xdrs,
    RMDIR3args *	objp)
{
	return(xdr_diropargs3(xdrs, &objp->object));
}

static bool_t
xdr_RMDIR3resok(
    XDR *		xdrs,
    RMDIR3resok *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

static bool_t
xdr_RMDIR3resfail(
    XDR *		xdrs,
    RMDIR3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->dir_wcc));
}

bool_t
xdr_RMDIR3res(
    XDR *		xdrs,
    RMDIR3res *		objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_RMDIR3resok(xdrs, &objp->resok));
	return(xdr_RMDIR3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_RENAME3args(
    XDR *		xdrs,
    RENAME3args *	objp)
{
	if (xdr_diropargs3(xdrs, &objp->from))
		return(xdr_diropargs3(xdrs, &objp->to));
	return(FALSE);
}

static bool_t
xdr_RENAME3resok(
    XDR *		xdrs,
    RENAME3resok *	objp)
{
	if (xdr_wcc_data(xdrs, &objp->fromdir_wcc))
		return(xdr_wcc_data(xdrs, &objp->todir_wcc));
	return(FALSE);
}

static bool_t
xdr_RENAME3resfail(
    XDR *		xdrs,
    RENAME3resfail *	objp)
{
	if (xdr_wcc_data(xdrs, &objp->fromdir_wcc))
		return(xdr_wcc_data(xdrs, &objp->todir_wcc));
	return(FALSE);
}

bool_t
xdr_RENAME3res(
    XDR *		xdrs,
    RENAME3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_RENAME3resok(xdrs, &objp->resok));
	return(xdr_RENAME3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_LINK3args(
    XDR *		xdrs,
    LINK3args *		objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->file))
		return(xdr_diropargs3(xdrs, &objp->link));
	return(FALSE);
}

static bool_t
xdr_LINK3resok(
    XDR *		xdrs,
    LINK3resok *	objp)
{
	if (xdr_post_op_attr(xdrs, &objp->file_attributes))
		return(xdr_wcc_data(xdrs, &objp->linkdir_wcc));
	return(FALSE);
}

static bool_t
xdr_LINK3resfail(
    XDR *		xdrs,
    LINK3resfail *	objp)
{
	if (xdr_post_op_attr(xdrs, &objp->file_attributes))
		return(xdr_wcc_data(xdrs, &objp->linkdir_wcc));
	return(FALSE);
}

bool_t
xdr_LINK3res(
    XDR *		xdrs,
    LINK3res *		objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_LINK3resok(xdrs, &objp->resok));
	return(xdr_LINK3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_READDIR3args(
    XDR *		xdrs,
    READDIR3args *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->dir) &&
					xdr_nfs_uint64_t(xdrs, &objp->cookie) &&
					xdr_cookieverf3(xdrs, objp->cookieverf))
		return(xdr_uint32_t(xdrs, &objp->count));
	return(FALSE);
}

#define	roundtoint(x)	(((x) + sizeof (int) - 1) & ~(sizeof (int) - 1))

/*
 * DECODE ONLY
 */
static bool_t
xdr_getdirlist(
    XDR *		xdrs,
    READDIR3resok *	objp)
{
	register int	i;
	bool_t		valid;
	unsigned int	namlen;
	char		name[SFS_MAXNAMLEN];
	nfs_uint64_t	fileid, cookie;
	entry3		*dp;

	i = 0;
	dp = objp->reply.entries;
	for (;;) {
		if (!xdr_bool(xdrs, &valid))
			return(FALSE);
		if (!valid)
			break;
		if (!xdr_nfs_uint64_t(xdrs, &fileid) ||
						!xdr_u_int(xdrs, &namlen))
			return(FALSE);
		if (namlen >= SFS_MAXNAMLEN)
			namlen = SFS_MAXNAMLEN - 1;
		if (!xdr_opaque(xdrs, name, namlen) ||
					!xdr_nfs_uint64_t(xdrs, &cookie))
			return(FALSE);
		name[namlen] = '\0';
		if (i < SFS_MAXDIRENTS) {
		    dp[i].fileid = fileid;
		    (void)memmove(dp[i].name, name, (namlen+1));
		    dp[i].cookie = cookie;
		    i++;
		}
	}
	objp->count = i;
	if (!xdr_bool(xdrs, &objp->reply.eof))
		return(FALSE);
	return(TRUE);
}

static bool_t
xdr_READDIR3resok(
    XDR *		xdrs,
    READDIR3resok *	objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return(FALSE);
	if (!xdr_cookieverf3(xdrs, objp->cookieverf))
		return(FALSE);
	if (xdrs->x_op == XDR_DECODE)
		return(xdr_getdirlist(xdrs, objp));
	return(TRUE);
}

static bool_t
xdr_READDIR3resfail(
    XDR *		xdrs,
    READDIR3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->dir_attributes));
}

bool_t
xdr_READDIR3res(
    XDR *		xdrs,
    READDIR3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_READDIR3resok(xdrs, &objp->resok));
	return(xdr_READDIR3resfail(xdrs, &objp->resfail));
}

bool_t
xdr_READDIRPLUS3args(
    XDR *		xdrs,
    READDIRPLUS3args *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->dir) &&
				xdr_nfs_uint64_t(xdrs, &objp->cookie) &&
				xdr_cookieverf3(xdrs, objp->cookieverf) &&
				xdr_uint32_t(xdrs, &objp->dircount))
		return(xdr_uint32_t(xdrs, &objp->maxcount));
	return(FALSE);
}

/*
 * copy post_op_attr from s2 to s1
 */
static void
copy_post_op_attr(post_op_attr *s1, post_op_attr *s2)
{
	s1->attributes = s2->attributes;
	(void) memmove((void *) &s1->attr, (void *) &s2->attr,
		       sizeof (fattr3));
}

/*
 * copy post_op_fh3 from s2 to s1
 */
static void
copy_post_op_fh3(post_op_fh3 *s1, post_op_fh3 *s2)
{
	s1->handle_follows = s2->handle_follows;
	(void) memmove((void *) &s1->handle, (void *) &s2->handle,
		       sizeof (nfs_fh3));
}
		
/*
 * DECODE ONLY
 */
static bool_t
xdr_getdirpluslist(
    XDR *		xdrs,
    READDIRPLUS3resok *	objp)
{
	register int	i;
	bool_t		valid;
	unsigned int	namlen;
	char		name[SFS_MAXNAMLEN];
	nfs_uint64_t	fileid, cookie;
	entryplus3	*dp;
	post_op_attr	at;
	post_op_fh3	fh;

	i = 0;
	dp = objp->reply.entries;
	for (;;) {
		if (!xdr_bool(xdrs, &valid))
			return(FALSE);
		if (!valid)
			break;
		if (!xdr_nfs_uint64_t(xdrs, &fileid) ||
					!xdr_u_int(xdrs, &namlen))
			return(FALSE);
		if (namlen >= SFS_MAXNAMLEN)
			namlen = SFS_MAXNAMLEN - 1;
		if (!xdr_opaque(xdrs, name, namlen) ||
					!xdr_nfs_uint64_t(xdrs, &cookie))
			return(FALSE);
		name[namlen] = '\0';
		if (!xdr_post_op_attr(xdrs, &at))
			return(FALSE);
		if (!xdr_post_op_fh3(xdrs, &fh))
			return(FALSE);
		if (i < SFS_MAXDIRENTS) {
		    dp[i].fileid = fileid;
		    (void)memmove(dp[i].name, name, (namlen+1));
		    dp[i].cookie = cookie;
		    copy_post_op_attr(&dp[i].name_attributes, &at);
		    copy_post_op_fh3(&dp[i].name_handle, &fh);
		    i++;
		}
	}

	objp->count = i;
	if (!xdr_bool(xdrs, &objp->reply.eof))
		return(FALSE);
	return(TRUE);
}

static bool_t
xdr_READDIRPLUS3resok(
    XDR *		xdrs,
    READDIRPLUS3resok *	objp)
{

	if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
		return(FALSE);
	if (!xdr_cookieverf3(xdrs, objp->cookieverf))
		return(FALSE);
	if (xdrs->x_op == XDR_DECODE)
		return(xdr_getdirpluslist(xdrs, objp));
	return(TRUE);
}

static bool_t
xdr_READDIRPLUS3resfail(
    XDR *			xdrs,
    READDIRPLUS3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->dir_attributes));
}

bool_t
xdr_READDIRPLUS3res(
    XDR *		xdrs,
    READDIRPLUS3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_READDIRPLUS3resok(xdrs, &objp->resok));
	return(xdr_READDIRPLUS3resfail(xdrs, &objp->resfail));
}

bool_t
xdr_FSSTAT3args(
    XDR *		xdrs,
    FSSTAT3args *	objp)
{
	return(xdr_nfs_fh3(xdrs, &objp->fsroot));
}

static bool_t
xdr_FSSTAT3resok(
    XDR *		xdrs,
    FSSTAT3resok *	objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->tbytes))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->fbytes))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->abytes))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->tfiles))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->ffiles))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->afiles))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->invarsec))
		return(FALSE);
	return(TRUE);
}

static bool_t
xdr_FSSTAT3resfail(
    XDR *		xdrs,
    FSSTAT3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->obj_attributes));
}

bool_t
xdr_FSSTAT3res(
    XDR *		xdrs,
    FSSTAT3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_FSSTAT3resok(xdrs, &objp->resok));
	return(xdr_FSSTAT3resfail(xdrs, &objp->resfail));
}

bool_t
xdr_FSINFO3args(
    XDR *		xdrs,
    FSINFO3args *	objp)
{
	return(xdr_nfs_fh3(xdrs, &objp->fsroot));
}

static bool_t
xdr_FSINFO3resok(
    XDR *		xdrs,
    FSINFO3resok *	objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->rtmax))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->rtpref))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->rtmult))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->wtmax))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->wtpref))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->wtmult))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->dtpref))
		return(FALSE);
	if (!xdr_nfs_uint64_t(xdrs, &objp->maxfilesize))
		return(FALSE);
	if (!xdr_nfstime3(xdrs, &objp->time_delta))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->properties))
		return(FALSE);
	return(TRUE);
}

static bool_t
xdr_FSINFO3resfail(
    XDR *		xdrs,
    FSINFO3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->obj_attributes));
}

bool_t
xdr_FSINFO3res(
    XDR *		xdrs,
    FSINFO3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_FSINFO3resok(xdrs, &objp->resok));
	return(xdr_FSINFO3resfail(xdrs, &objp->resfail));
}

bool_t
xdr_PATHCONF3args(
    XDR *		xdrs,
    PATHCONF3args *	objp)
{
	return(xdr_nfs_fh3(xdrs, &objp->object));
}

static bool_t
xdr_PATHCONF3resok(
    XDR *		xdrs,
    PATHCONF3resok *	objp)
{
	if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->link_max))
		return(FALSE);
	if (!xdr_uint32_t(xdrs, &objp->name_max))
		return(FALSE);
	if (!xdr_bool(xdrs, &objp->no_trunc))
		return(FALSE);
	if (!xdr_bool(xdrs, &objp->chown_restricted))
		return(FALSE);
	if (!xdr_bool(xdrs, &objp->case_insensitive))
		return(FALSE);
	if (!xdr_bool(xdrs, &objp->case_preserving))
		return(FALSE);
	return(TRUE);
}

static bool_t
xdr_PATHCONF3resfail(
    XDR *		xdrs,
    PATHCONF3resfail *	objp)
{
	return(xdr_post_op_attr(xdrs, &objp->obj_attributes));
}

bool_t
xdr_PATHCONF3res(
    XDR *		xdrs,
    PATHCONF3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_PATHCONF3resok(xdrs, &objp->resok));
	return(xdr_PATHCONF3resfail(xdrs, &objp->resfail));
}

bool_t
xdr_COMMIT3args(
    XDR *		xdrs,
    COMMIT3args *	objp)
{
	if (xdr_nfs_fh3(xdrs, &objp->file) &&
					xdr_nfs_uint64_t(xdrs, &objp->offset))
		return(xdr_uint32_t(xdrs, &objp->count));
	return(FALSE);
}

static bool_t
xdr_COMMIT3resok(
    XDR *		xdrs,
    COMMIT3resok *	objp)
{
	if (xdr_wcc_data(xdrs, &objp->file_wcc))
		return(xdr_writeverf3(xdrs, objp->verf));
	return(FALSE);
}

static bool_t
xdr_COMMIT3resfail(
    XDR *		xdrs,
    COMMIT3resfail *	objp)
{
	return(xdr_wcc_data(xdrs, &objp->file_wcc));
}

bool_t
xdr_COMMIT3res(
    XDR *		xdrs,
    COMMIT3res *	objp)
{
	if (!xdr_nfsstat3(xdrs, &objp->status))
		return(FALSE);
	if (objp->status == NFS3_OK)
		return(xdr_COMMIT3resok(xdrs, &objp->resok));
	return(xdr_COMMIT3resfail(xdrs, &objp->resfail));
}


bool_t
xdr_dirpath(
    XDR *		xdrs,
    dirpath *		objp)
{
	return(xdr_string(xdrs, objp, MNTPATHLEN));
}

static bool_t
xdr_fhandle3(
    XDR *		xdrs,
    fhandle3 *		objp)
{
	return(xdr_bytes(xdrs, (char **) &objp->fhandle3_val,
				(unsigned int *) &objp->fhandle3_len, NFS3_FHSIZE));
}

static bool_t
xdr_mntres3_ok(
    XDR *		xdrs,
    mntres3_ok *	objp)
{
	if (xdr_fhandle3(xdrs, &objp->fhandle)) {
		return(xdr_array(xdrs,
			(void **) &objp->auth_flavors.auth_flavors_val,
			(unsigned int *) &objp->auth_flavors.auth_flavors_len,
			~0, sizeof (int), (xdrproc_t) xdr_int));
	}
	return(FALSE);
}

bool_t
xdr_mntres3(
    XDR *		xdrs,
    mountres3 *		objp)
{
	if (!xdr_enum(xdrs, (enum_t *) &objp->fhs_status))
		return(FALSE);
	if (objp->fhs_status == MNT_OK)
		return(xdr_mntres3_ok(xdrs, &objp->mntres3_u.mntinfo));
	return(TRUE);
}
/* sfs_3_xdr.c */
