#
# @(#)Makefile     2.1     97/10/23
# From: @(#)Makefile	2.1 88/07/29 4.0 RPCSRC
#
#    Copyright (c) 1992-1997,2001 by Standard Performance Evaluation Corporation
#	All rights reserved.
#		Standard Performance Evaluation Corporation (SPEC)
#		6585 Merchant Place, Suite 100
#		Warrenton, VA 20187
#	This product contains benchmarks acquired from several sources who
#	understand and agree with SPEC's goal of creating fair and objective
#	benchmarks to measure computer performance.
#
#	This copyright notice is placed here only to protect SPEC in the
#	event the source is misused in any manner that is contrary to the
#	spirit, the goals and the intent of SPEC.
#
#	The source code is provided to the user or company under the license
#	agreement for the SPEC Benchmark Suite for this product.
#
# Copyright (c) 1987 by Sun Microsystems, Inc.
#
DESTDIR=

INCDIR = -I..

SRCS = sfs_cudp.c sfs_ctcp.c rpc_prot.c rpc_dtablesize.c \
	auth_none.c auth_unix.c  authunix_prot.c \
	clnt_generic.c clnt_perror.c clnt_simple.c clnt_tcp.c clnt_udp.c \
	bindresvport.c get_myaddress.c getrpcent.c getrpcport.c \
	pmap_clnt.c pmap_getmaps.c pmap_getport.c pmap_prot.c \
	pmap_prot2.c pmap_rmt.c xdr.c xdr_array.c xdr_float.c xdr_mem.c \
	xdr_rec.c xdr_reference.c xdr_stdio.c rpc_commondata.c rpc_callmsg.c \
	svc.c svc_auth.c svc_auth_unix.c svc_raw.c svc_run.c svc_simple.c \
	svc_tcp.c svc_udp.c

OBJS = sfs_cudp.o clnt_generic.o sfs_ctcp.o rpc_prot.o rpc_dtablesize.o \
	auth_none.o auth_unix.o  authunix_prot.o \
	clnt_perror.o clnt_simple.o clnt_tcp.o clnt_udp.o \
	bindresvport.o get_myaddress.o getrpcent.o getrpcport.o \
	pmap_clnt.o pmap_getmaps.o pmap_getport.o pmap_prot.o \
	pmap_prot2.o pmap_rmt.o xdr.o xdr_array.o xdr_float.o xdr_mem.o \
	xdr_rec.o xdr_reference.o xdr_stdio.o rpc_commondata.o rpc_callmsg.o \
	svc.o svc_auth.o svc_auth_unix.o svc_raw.o svc_run.o svc_simple.o \
	svc_tcp.o svc_udp.o

HDRS = auth.h auth_unix.h clnt.h netdb.h pmap_clnt.h \
	pmap_prot.h pmap_rmt.h rpc.h rpc_msg.h svc.h svc_auth.h types.h xdr.h

all:	librpclib.a

librpclib.a: ${OBJS}
	@echo "building librpclib.a"
	@ar cru librpclib.a ${OBJS}

install: $(HDRS) librpclib.a
	@echo "Creating RPC header directory"
	-mkdir ${DESTDIR}/usr/include/rpc && \
		chown bin ${DESTDIR}/usr/include/rpc && \
		chmod 755 ${DESTDIR}/usr/include/rpc
	@echo "Installing RPC header files"
	-set -x;for i in $(HDRS) ; do \
		(install -c -m 644 $$i ${DESTDIR}/usr/include/rpc) done
	@echo "Installing RPC library"
	install -c -m 644 librpclib.a ${DESTDIR}/usr/lib
	ranlib ${DESTDIR}/usr/lib/librpclib.a

lint: $(SRCS) $(HDRS)
	$(LINT.c) $(SRCS)

clobber: clean

clean:
	rm -f *.o librpclib.a

.c.o:
		$(CC) -c $(INCDIR) $(CFLAGS) $(EXTRA_CFLAGS) $(DEBUG) \
			$(SETPGRP_CALL) $(FSTAB) $(OSTYPE) $(OPT) \
			$(RESVPORT_MOUNT) $*.c
