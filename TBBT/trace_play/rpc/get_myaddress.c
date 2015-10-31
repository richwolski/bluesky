#ifndef lint
static char sfs_get_myaddress_id[] = "@(#)get_myaddress.c     2.1     97/10/23";
#endif
/* @(#)get_myaddress.c	2.1 88/07/29 4.0 RPCSRC */
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
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)get_myaddress.c 1.4 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * get_myaddress.c
 *
 * Get client's IP address via ioctl.  This avoids using the yellowpages.
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#ifndef FreeBSD
#include <stropts.h>	
#endif /* ndef FreeBSD */
#include <string.h>
#include "rpc/rpc.h"
#include "rpc/pmap_prot.h"
#include "rpc/osdep.h"
#include <sys/utsname.h>

#ifdef FreeBSD
#include <netdb.h>	
#define MY_NAMELEN 256
static char myhostname[MY_NAMELEN];
#endif /* def FreeBSD */

void
get_myaddress(
	struct sockaddr_in *addr)
{
	int s;
	char buf[BUFSIZ];
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	int len; 	
	struct sockaddr_in tmp_addr;
#ifdef _AIX
	char	*cp, *cplim;
#endif

#ifdef FreeBSD
        static struct in_addr *my_addr;
        struct hostent *ent;     

	/* In FreeBSD, SIOCGIFCONF provides AF_LINK information, not AF_INET. */
        gethostname(myhostname, MY_NAMELEN);
        ent = gethostbyname(myhostname);
        if (ent == NULL) {
            fprintf(stderr, "lookup on server's name failed\n");
            exit(1);
        }
        bzero(addr, sizeof(struct sockaddr_in));
        my_addr = (struct in_addr *)(*(ent->h_addr_list));
        bcopy(my_addr, &(addr->sin_addr.s_addr), sizeof(struct in_addr));
        addr->sin_family = AF_INET;
        addr->sin_port = htons(PMAPPORT);
        return;                
#endif /* def FreeBSD */

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    perror("get_myaddress: socket");
	    exit(1);
	}
	ifc.ifc_len = sizeof (buf);
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
		perror("get_myaddress: ioctl (get interface configuration)");
		exit(1);
	}
	ifr = ifc.ifc_req;
#ifdef _AIX
	cplim = buf + ifc.ifc_len;
	for (cp = buf; cp < cplim;
	     cp += MAX(sizeof(struct ifreq),
		       (sizeof (ifr->ifr_name) +
			MAX((ifr->ifr_addr).sa_len, sizeof(ifr->ifr_addr))))) {
  		ifr = (struct ifreq *)cp;
#else
	for (len = ifc.ifc_len; len; len -= sizeof(ifreq), ifr++) {
#endif
		ifreq = *ifr;
                /* Save address, since SIOCGIFFLAGS may scribble over *ifr. */
                tmp_addr = *((struct sockaddr_in *)&ifr->ifr_addr);
		if (ioctl(s, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			perror("get_myaddress: ioctl");
			exit(1);
		}
		if ((ifreq.ifr_flags & IFF_UP) &&
		    ifr->ifr_addr.sa_family == AF_INET) {
			*addr = tmp_addr;
			addr->sin_port = htons(PMAPPORT);
			break;
		}
	}
	(void) close(s);
}

/*
 * A generic gethostname
 */
int
getmyhostname(char *name, int namelen)
{
#if !defined(HAS_GETHOSTNAME)
        struct utsname utsname;
	int ret;

        ret = uname(&utsname);
	if (ret == -1)
		return (-1);
        (void) strncpy(name, utsname.nodename, namelen);
	return (0);
#else
	return(gethostname(name, namelen));
#endif
}
