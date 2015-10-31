#ifndef lint
static char sfs_c_mntSid[] = "@(#)sfs_c_mnt.c	2.1	97/10/23";
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
 * ---------------------- sfs_c_mnt.c ---------------------
 *
 *      The sfs child.  Routines to handle mount points.
 *
 *.Exported_Routines
 *	void init_mount_point(int, char *, CLIENT *)
 *
 *.Local_Routines
 *	int pseudo_mount(char *, int, char *, CLIENT *)
 *
 *.Revision_History
 *	2-Jul-92	Teelucksingh    Added code for OSF/1
 *					use of getmntinfo().
 *	16-Dec-91	Wittle 		Created.
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
#include <time.h>
#include <signal.h>
 
#include <sys/types.h>
#include <sys/stat.h> 

#include <fcntl.h>

#include <unistd.h>

#include "sfs_c_def.h"

struct hostent   *Server_hostent;

/*
 * -------------------------  Constants  -------------------------
 */

/*
 * Number of times a load generating process will retry amount.
 * Each load generating process also picks a client shifted
 * mount start time, and executes a backoff on retry time on
 * failure.
 */
#define NUMBER_MOUNT_RETRIES	10

/*
 * -------------------------  External Definitions  -------------------------
 */

/* forward definitions for local routines */
CLIENT * lad_getmnt_hand(char *);
static int pseudo_mount(char *, int, char *, CLIENT *);

/*
 * Mounts are retried when an RPC timeout occurs, in the mainline
 * code. They are not retried by the RPC clnt_call routine, as the
 * timeout values are set now.
 */
static struct timeval Mount_timer = {  10,  0 };


/*
 * -------------------------  Mount Point Routines  -------------------------
 */


/*
 * mount the testdir 'dirnum' under the parent directory 'parentdir'.
 */
void
init_mount_point(
    int		dirnum,
    char *	parentdir,
    CLIENT *	mount_client_ptr)
{
    char	pnt_dir[SFS_MAXPATHLEN];      /* test dir component name */
    char	testdirname[SFS_MAXPATHLEN];  /* test dir component name */
    char	export_fsname[SFS_MAXPATHLEN]; /* "host:path" exported fs */
    sfs_fh_type file_handle;
    char	*fh_ptr;
    char	*cp;
    int		ret;
    sfs_fh_data *fh_datap, *Ex_fh_datap;

    fh_datap= calloc(1,sizeof(sfs_fh_data));
    (void) memset((char *)fh_datap, 0, sizeof(sfs_fh_data));
    (void) memset((char *)&file_handle, 0, sizeof(file_handle));
    file_handle.fh_data = fh_datap;
    file_handle.dir = &Export_dir;

    Ex_fh_datap = (sfs_fh_data *) calloc(1,sizeof(sfs_fh_data));
    Export_dir.fh_data = Ex_fh_datap;

    (void) strcpy(pnt_dir, parentdir);

    cp = strchr(pnt_dir, ':');
    if (cp == NULL) {
	(void) fprintf(stderr, "%s: malformed fsname %s\n",
			sfs_Myname, parentdir);
	if (!Validate)
	    (void) generic_kill(0, SIGINT);
	exit(86);
    }

    *cp++ = '\0';

    /*
     * Now we have the parent directory in the form:
     *		host:host_path
     *
     * First we get the file handle for parent directory
     *
     * Verify that the server is running the correct version of
     * the NFS protocol specification and then proceed to get
     * the exported fh from the server.
     */
    (void) strcpy(testdirname, pnt_dir);
    (void) strcat(testdirname, ":");
    (void) strcat(testdirname, cp);
    (void) strcpy(export_fsname, testdirname);

    if (nfs_version == NFS_VERSION) {
	(void) memset((char *) &Export_dir.fh2, '\0', sizeof (Export_dir.fh2));
	fh_ptr = (char *)&Export_dir.fh2;
    } else if (nfs_version == NFS_V3) {
	(void) memset((char *) &Export_dir.fh3, '\0', sizeof (Export_dir.fh3));
	fh_ptr = (char *)&Export_dir.fh3;
    }

    ret = pseudo_mount(export_fsname, nfs_version,
					fh_ptr, mount_client_ptr);
    if (ret < 0) {
	if (ret == -2)  {
	    (void) fprintf(stderr,
		"%s: NFS Protocol Version %lu verification failed.\n",
		sfs_Myname, (uint32_t)nfs_version);
	}
	else  {
	    (void) fprintf(stderr, "%s: can't pseudo mount %s\n",
			sfs_Myname, export_fsname);
	}
	if (!Validate)
	    (void) generic_kill(0, SIGINT);
	exit(87);
    }

    /*
     * Setup initial state of export directory
     */
    Export_dir.state = Exists;
    (void) strcpy(Export_dir.file_name, testdirname);
    Export_dir.dir = &Export_dir;

#ifndef RFS
    /*
     * Check for and create the client directory. Stat it first, if not
     * there then mkdir, if that fails with EEXIST we lost the race but
     * that's OK.
     */
    if (Validate) {
	(void) sprintf(testdirname, "%s", "validatedir");
    } else {
	(void) sprintf(testdirname, "CL%d", Client_num);
    }

    if ((ret = lad_lookup(&file_handle, testdirname)) == -1) {
	if (!Validate)
	    (void) generic_kill(0, SIGINT);
	exit(88);
    }

    if (ret == 1) {
	/*
	 * Directory doesn't exist so create it
	 * if it already exists thats OK
	 */
        if ((ret = lad_mkdir(&file_handle, testdirname)) == -1) {
	    if (!Validate)
	        (void) generic_kill(0, SIGINT);
	    exit(89);
        }
	/*
	 * If someone else created this out from underneath us simply
	 * lookup the result and continue on.
	 */
        if (ret != 0 && (ret = lad_lookup(&file_handle, testdirname)) == -1) {
	    if (!Validate)
	        (void) generic_kill(0, SIGINT);
	    exit(90);
        }
    }

    /* testdirname now exists, verify it is a directory and writeable */
    if (!fh_isdir(&file_handle) ||
		(check_fh_access(&file_handle) == -1)) {
	(void) fprintf(stderr,
		"%s: %s is either not a directory or not accessible\n",
			sfs_Myname, testdirname);
	if (!Validate)
	    (void) generic_kill(0, SIGINT);
	exit(91);
    }

    /*
     * logically chdir into CL directory
     */
   /*  Export_dir = file_handle; Implied bcopy here */
    (void) memmove(&Export_dir,&file_handle,sizeof(sfs_fh_type));
    Export_dir.fh_data = Ex_fh_datap;
    (void ) memmove(Export_dir.fh_data, file_handle.fh_data, 
			sizeof(sfs_fh_data));
    (void) memset((char *)&file_handle, 0, sizeof(file_handle));
    (void) memset((char *)fh_datap, 0, sizeof(sfs_fh_data));
    file_handle.fh_data = fh_datap;
    file_handle.dir = &Export_dir;

    /*
     * Validation only occurs one directory deep so we can exit early
     */
    if (Validate)
        return;

    (void) sprintf(testdirname, "testdir%d", dirnum);

    if ((ret = lad_lookup(&file_handle, testdirname)) == -1) {
	(void) generic_kill(0, SIGINT);
	exit(92);
    }

    if (ret == 1) {
	/*
	 * Directory doesn't exist so create it
	 */
        if (lad_mkdir(&file_handle, testdirname) != 0) {
	    (void) fprintf(stderr, "%s: Unable to create %s\n",
			sfs_Myname, testdirname);
	    (void) generic_kill(0, SIGINT);
	    exit(93);
        }
    }

    /* testdirname now exists, verify it is a directory and writeable */
    if (!fh_isdir(&file_handle) ||
		(check_fh_access(&file_handle) == -1)) {
	(void) fprintf(stderr,
		"%s: %s is either not a directory or not accessible\n",
			sfs_Myname, testdirname);
	(void) generic_kill(0, SIGINT);
	exit(94);
    }

    /*
     * logically chdir into testdir directory
     */
    /* Export_dir = file_handle;*/
    (void) memmove(&Export_dir, &file_handle, sizeof(struct sfs_fh_type));
    Export_dir.fh_data = Ex_fh_datap; /* Put pointer back */
    (void) memmove(Export_dir.fh_data, file_handle.fh_data, sizeof
			(sfs_fh_data));
#endif
} /* init_mount_point */

/*
 * Get the filehandle for 'mount_fsname', and return it
 * Returns NULL for error ... not mounted || no NFS client.
 *
 * Children should only call this routine 1 time.
 */
CLIENT *
lad_getmnt_hand(
    char *		mount_point)
{
    char 		mnt_pnt[SFS_MAXPATHLEN];    /* working buffer */
    char 		host[SFS_MAXPATHLEN];     /* host with exported fs */
    static struct hostent 	hp;
    struct hostent	*thp;
    CLIENT		*mount_client_ptr;	/* Mount client handle */
    char		*cp;
    int			rpc_result; 		/* rpc call result */
    uint32_t		mount_vers = 0;

    /*
     * If the mount point is of the form host:path just use the explicit
     * name instead of grovelling through the mount table.
     */
    (void) strcpy(mnt_pnt, mount_point);
    cp = strchr(mnt_pnt, ':');
    if (cp == NULL) {
	(void) fprintf(stderr, "%s: malformed fsname %s\n",
			sfs_Myname, mount_point);
	return(NULL);
    }

    *cp++ = '\0';
    (void) strcpy(host, mnt_pnt);

    /* Verify NFS Version */
    rpc_result = callrpc(host,
			(uint32_t) NFS_PROGRAM,
			(uint32_t) nfs_version,
			(uint32_t) NFSPROC_NULL, (xdrproc_t) xdr_void,
			(char *) NULL, (xdrproc_t) xdr_void, (char *) NULL);
    if (rpc_result != 0) {
	 clnt_perrno((enum clnt_stat)rpc_result);
	 (void) fprintf(stderr,
"\nUnable to contact NFS server %s.\n", host);
	 (void) fprintf(stderr,
"Verify NFS server daemon supporting version %u is running and\n",
		(uint32_t)nfs_version);
	 (void) fprintf(stderr, "registered with the portmapper.\n");
	return(NULL);
    }

    /* Get host's address */
    if ((thp = gethostbyname(host)) == NULL) {
	/* Failure may be due to yellow pages, try again */
	if ((thp = gethostbyname(host)) == NULL) {
	    (void) fprintf(stderr, "%s: %s not in hosts database\n",
			    sfs_Myname, host);
	    return(NULL);
	}
    }

    hp = *thp;
    Server_hostent = &hp;

    if (nfs_version == NFS_VERSION)
	mount_vers = MOUNTVERS;
    if (nfs_version == NFS_V3)
	mount_vers = MOUNTVER3;

    mount_client_ptr = lad_clnt_create(0, Server_hostent,
                                        (uint32_t) MOUNTPROG,
                                        mount_vers,
                                        RPC_ANYSOCK, &Mount_timer);
                

    if (mount_client_ptr == ((CLIENT*) NULL)) {
	(void) fprintf(stderr,
			"%s: portmap/mountd %s server not responding",
			sfs_Myname, mount_point);
	return(NULL);
    }

    mount_client_ptr->cl_auth = authunix_create_default();
    return (mount_client_ptr);

} /* lad_getmnt_hand */


/*
 * Get the filehandle for 'mount_fsname', and return it in 'fh_ptr'.
 * Returns 0 for OK, -1 for error ... not mounted || no NFS client.
 *
 * Children should only call this routine 1 time.
 */
static int
pseudo_mount(
    char *		mount_fsname,
    int			version,
    char *		fh_ptr,
    CLIENT *		mount_client_ptr)
{
    char *		host_ptr;		/* host with exported fs */
    char *		path_ptr;		/* ptr to path for RPC */

    struct fhstatus 	fhs;			/* status of mountd call */
    nfs_fh3 *		fh_ptr3;
    mountres3	 	mntres3;		/* status of mountd call */
    char *		cp;
    enum clnt_stat 	rpc_stat;
    int			tries = 0;		/* Number of retries */
				/* Space by 200ms intervals. */
    int			pacesleep = Child_num * 200;

    /* Parse the fsname for host and path strings */
    cp = strchr(mount_fsname, ':');

    if (cp == NULL) {
	(void) fprintf(stderr, "%s: malformed fsname %s\n",
			sfs_Myname, mount_fsname);
	return(-1);
    }

    *cp++ = '\0';
    host_ptr = mount_fsname;
    path_ptr = cp;

    /* Check host's address */
    if (gethostbyname(host_ptr) == NULL) {
	/* Failure may be due to yellow pages, try again */
	if (gethostbyname(host_ptr) == NULL) {
	    (void) fprintf(stderr, "%s: %s not in hosts database\n",
			    sfs_Myname, host_ptr);
	    return(-1);
	}
    }

    if (DEBUG_CHILD_GENERAL) {
	(void) fprintf(stderr, "%s: mount clnt_call\n", sfs_Myname);
    }

    /* get fhandle of remote path from host's mountd */

retry_mount:
    /*
     * Many children on many clients hammer a server with
     * mounts. Crude fix is to pace them. Some run rule interpretations
     * are to have *many* children on each client. This can
     * cause problems.
     */
    (void) msec_sleep(pacesleep);

    if (version == NFS_VERSION) {
	(void) memset((char *) &fhs, '\0', sizeof (fhs));
	rpc_stat = clnt_call(mount_client_ptr, MOUNTPROC_MNT, xdr_path,
			(char *) &path_ptr,xdr_fhstatus,(char *) &fhs,
			Mount_timer);
    } else if (version == NFS_V3) {
	(void) memset((char *) &mntres3, '\0', sizeof (mntres3));
	rpc_stat = clnt_call(mount_client_ptr, MOUNTPROC_MNT, xdr_dirpath,
			(char *) &path_ptr, xdr_mntres3, (char *) &mntres3,
			Mount_timer);
    } else
	rpc_stat = RPC_PROGVERSMISMATCH;

    errno = 0;
    if (rpc_stat != RPC_SUCCESS) {

	switch (rpc_stat) {

	    case RPC_TIMEDOUT:
		errno = ETIMEDOUT;
		(void) fprintf(stderr,
		    "%s: mounting %s:%s server not responding: %s (%d)\n",
			    sfs_Myname, host_ptr, path_ptr,
			    strerror(errno), errno);
		if (tries++ < NUMBER_MOUNT_RETRIES) {
		    /* Randomize the backoff on retry */
		    pacesleep = pacesleep + (sfs_random() % 2000);
		    goto retry_mount;
		}
		break;


	    case RPC_PMAPFAILURE:
		errno = ENETDOWN;	/* reasonable error */
		(void) fprintf(stderr,
			    "%s: mounting %s portmap call failed: %s (%d)\n",
			    sfs_Myname, host_ptr, strerror(errno), errno);
		break;

	    case RPC_PROGNOTREGISTERED:
		errno = ENETDOWN;	/* reasonable error */
		(void) fprintf(stderr,
			    "%s: mounting %s nfsd not registered: %s (%d)\n",
			    sfs_Myname, host_ptr, strerror(errno), errno);
		break;

	    case RPC_AUTHERROR:
		errno = EACCES;
		(void) fprintf(stderr,
			"%s: mounting %s authentication failed: %s (%d)\n",
			sfs_Myname, host_ptr, strerror(errno), errno);
		break;

	    default:
		errno = ENETDOWN;	/* reasonable error */
		(void) fprintf(stderr,
			    "%s: mounting %s:%s failed: %s (%d)\n",
			    sfs_Myname, host_ptr, path_ptr,
			    strerror(errno), errno);
		break;
	}

	clnt_perror(mount_client_ptr, "");
	return(-1);

    } /* MOUNTPROC_MNT call failed */

    if (version == NFS_VERSION) {
	if (fhs.fhs_status != 0) {
	    if (fhs.fhs_status == EACCES) {
		(void) fprintf(stderr, "%s: mounting %s:%s - access denied\n",
			   sfs_Myname, host_ptr, path_ptr);
	    } else {
		(void) fprintf(stderr,
			    "%s: mounting %s:%s - bad fh status %d\n ",
			    sfs_Myname, host_ptr, path_ptr, fhs.fhs_status);
	    }
	    return(-1);
	} /* bad fhs status */

	/*
	 * fill in the caller's file handle
	 */
	(void) memmove(fh_ptr, (char *) &fhs.fhs_fh, NFS_FHSIZE);

    } else if (version == NFS_V3) {

	if (mntres3.fhs_status != MNT_OK) {
	    if (mntres3.fhs_status == MNT3ERR_ACCES) {
		(void) fprintf(stderr, "%s: mounting %s:%s - access denied\n",
			   sfs_Myname, host_ptr, path_ptr);
	    } else {
		(void) fprintf(stderr,
			    "%s: mounting %s:%s - bad fh status %d\n ",
		    sfs_Myname, host_ptr, path_ptr, mntres3.fhs_status);
	    }
	    return(-1);
	} /* bad fhs status */

	/*
	 * fill in the caller's file handle
	 * space pointed by fhandle3_val is allocated through xdr_mntres3
	 */
	fh_ptr3 = (nfs_fh3 *)fh_ptr;
	fh_ptr3->fh3_length = mntres3.mntres3_u.mntinfo.fhandle.fhandle3_len;
	(void) memmove((char *) fh_ptr3->fh3_u.data,
			(char *) mntres3.mntres3_u.mntinfo.fhandle.fhandle3_val,
			fh_ptr3->fh3_length);
    }

    return(0);

} /* pseudo_mount */


/* sfs_c_mnt.c */
