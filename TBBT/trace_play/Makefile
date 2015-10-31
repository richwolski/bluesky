#
#	@(#)Makefile	2.1	97/10/23
#
# Makefile to build SFS benchmark suite
#
#
#    Copyright (c) 1992-1997,2001 by Standard Performance Evaluation Corporation
#	All rights reserved.
#		Standard Performance Evaluation Corporation (SPEC)
#		6585 Merchant Place, Suite 100
#		Warrenton, VA 20187
#
#	This tape contains benchmarks acquired from several sources who
#	understand and agree with SPEC's goal of creating fair and objective
#	benchmarks to measure computer performance.
#
#	This copyright notice is placed here only to protect SPEC in the
#	event the source is misused in any manner that is contrary to the
#	spirit, the goals and the intent of SPEC.
#
#	The source code is provided to the user or company under the license
#	agreement for the SPEC Benchmark Suite for this tape.
#
#
# *****************************************************************
# *								  *
# *	Copyright 1991,1992  Legato Systems, Inc.                 *
# *	Copyright 1991,1992  Auspex Systems, Inc.                 *
# *	Copyright 1991,1992  Data General Corporation             *
# *	Copyright 1991,1992  Digital Equipment Corporation        *
# *	Copyright 1991,1992  Interphase Corporation               *
# *	Copyright 1991,1992  Sun Microsystems, Inc.               *
# *                                                               *
# *****************************************************************

#
# -----------------------  How to use this makefile  --------------------
#
# The makefile is divided into 10 sections:
#  1) basic information
#  2) default compiler definitions
#  3) vendor specific definitions
#  4) OS feature specific definitions
#  5) final compiler definitions (includes specific vendor and feature defs)
#  6) source file definitions
#  7) object file definitions
#  8) rules and targets for building sfs
#  9) lint, install, clean, misc rules


# ----------------------  default compiler definitions  -----------------
#
# For Solaris 2.X (SunOS 5.X) or later
MACHID = solaris2
CC=cc
OPT = -O
CFLAGS = -v -Xc -D__sparc -D_POSIX_SOURCE
LDFLAGS =
EXTRA_CFLAGS = -DPORTMAP
EXTRA_LDFLAGS =
EXTRA_LINTFLAGS =
EXTRA_LIBS =
RESVPORT_MOUNT=
OSTYPE = -DSVR4 -DSOLARIS2
INCDIR = -I.
RPCLIB = rpc/librpclib.a
LIBS = -lm -lsocket -lnsl

#
# ----------------------  OS feature specific definitions  ------------------
#

#
# A UNIX derivative that only supports the getfsent(3) family of calls
# should have the USEFSTAB variable set.
# A UNIX derivative that supports the getmntent(3) family of calls
# should have the line commented out.
#FSTAB = -DUSEFSTAB

# SFS uses the setsid(2) system call to form a new proces group
# This prevents 'synchronization' signals from causing parent shell (sh)
# to exit. SETPGRP_CALL allows you to specify the alternative use
# of the setpgrp(2) and which variation you are using. 

# If you are linking with BSD libraries then uncomment the next line.
# SETPGRP_CALL = -DSETPGRP_BSD

# If you are linking with SYSV type libraries then uncomment the next line.
#SETPGRP_CALL = -DSETPGRP_SYSV

# if your client is failing the mount even when run as root then you should
# uncomment the RESVPORT_MOUNT to explicitly bind to a reserved port.
RESVPORT_MOUNT = -DRESVPORT

#
# ----------------------  final compiler definitions  -----------------------
#

DEBUG = -g
CFLAGS = 
EXTRA_CFLAGS =

LDFLAGS =  $(OSTYPE)

MACHID= linux
CC= /usr/bin/gcc
#OPT= -O
OPT =
EXTRA_CFLAGS= -DNO_T_TYPES -DUSE_INTTYPES
LIBS= -lm
OSTYPE=


#
# ----------------------  source file definitions  ------------------
#

MAKEFILE =	Makefile

DOC =		sfs.1 README sfs.1

SHELLFILES =	sfs_mgr sfs_mcr sfs_rc 

V2_HDR =	sfs_c_def.h sfs_c_nfs.h

V3_HDR =	sfs_c_def.h sfs_c_nfs.h

V2_SRC =	sfs_2_ops.c sfs_2_vld.c

#V3_SRC =	rfs_3_ops.c sfs_3_vld.c
V3_SRC =	rfs_3_ops.c generic_hash.c profile.c

#C_SRC =		sfs_c_clk.c sfs_c_dat.c rfs_c_dat.c sfs_c_dmp.c sfs_c_man.c 
#		sfs_c_pnt.c sfs_c_rnd.c sfs_c_sig.c sfs_c_bio.c 
C_SRC =		sfs_c_clk.c sfs_c_dat.c rfs_c_dat.c sfs_c_dmp.c \
		sfs_c_rnd.c sfs_c_sig.c sfs_c_bio.c \
		sfs_2_xdr.c sfs_3_xdr.c sfs_c_mnt.c sfs_c_chd.c sem.c\
		sfs_c_clnt.c

C_COMMON_SRC =	sfs_c_sub.c

M_HDR =		sfs_m_def.h

M_PRIME_SRC =	sfs_m_prm.c

M_SYNCD_SRC =	sfs_m_snc.c

M_MSG_SRC =	sfs_m_msg.c

M_COMMON_SRC =	sfs_m_xdr.c

#
# ----------------------  object file definitions  -------------------
#

V2_OBJ =	sfs_2_ops.o sfs_2_vld.o

#V3_OBJ =	rfs_3_ops.o sfs_3_vld.o 
V3_OBJ =	rfs_3_ops.o generic_hash.o profile.o

C_OBJ =		sfs_c_clk.o sfs_c_dat.o rfs_c_dat.o sfs_c_dmp.o \
		sfs_c_rnd.o sfs_c_sig.o sfs_c_bio.o \
		sfs_2_xdr.o sfs_3_xdr.o sfs_c_mnt.o sfs_c_chd.o sem.o \
		sfs_c_clnt.o

C_COMMON_OBJ =	sfs_c_sub.o


M_PRIME_OBJ =	sfs_m_prm.o

M_SYNCD_OBJ =	sfs_m_snc.o

M_MSG_OBJ =	sfs_m_msg.o

M_COMMON_OBJ =	sfs_m_xdr.o

#
# ----------------------  rules and targets for building sfs  ---------
#

BENCH = /home/ningning/rfs/trace_src
all:		fix_perm install agefs

compile:	fix_perm install

fix_perm:
		chmod 755 $(BENCH)/src
		chmod 755 $(BENCH)/src/rpc
		chmod 755 $(BENCH)/bin

sfs:		$(V2_HDR) $(M_HDR) $(V2_OBJ) $(C_OBJ) \
		$(C_COMMON_OBJ) $(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB)
		$(CC) -o $@ $(LDFLAGS) $(V2_OBJ) $(C_OBJ) $(C_COMMON_OBJ) \
			$(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB) $(LIBS) \
			$(EXTRA_LIBS) 

sfs3:	$(V3_HDR) $(M_HDR) $(V3_OBJ) $(C_OBJ) \
		$(C_COMMON_OBJ) $(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB)
		$(CC) -lpthread -o $@ $(LDFLAGS) $(V3_OBJ) $(C_OBJ) $(C_COMMON_OBJ) \
			$(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB) $(LIBS) \
			$(EXTRA_LIBS) 
		chmod 755 sfs3

sfs3.full_speed:	$(V3_HDR) $(M_HDR) $(V3_OBJ) $(C_OBJ) \
		$(C_COMMON_OBJ) $(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB)
		$(CC) -o $@ $(LDFLAGS) $(V3_OBJ) $(C_OBJ) $(C_COMMON_OBJ) \
			$(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB) $(LIBS) \
			$(EXTRA_LIBS) 



sfs_prime:	$(V2_HDR) $(M_HDR) $(M_PRIME_OBJ) \
		$(C_COMMON_OBJ) $(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB)
		$(CC) -o $@ $(LDFLAGS) $(M_PRIME_OBJ) $(C_COMMON_OBJ) \
			$(M_COMMON_OBJ) $(M_MSG_OBJ) $(RPCLIB) $(LIBS) \
			$(EXTRA_LIBS) 

sfs_syncd:	$(V2_HDR) $(M_HDR) $(M_SYNCD_OBJ) \
		$(C_COMMON_OBJ) $(M_COMMON_OBJ) $(RPCLIB)
		$(CC) -o $@ $(LDFLAGS) $(M_SYNCD_OBJ) $(C_COMMON_OBJ) \
			$(M_COMMON_OBJ) $(RPCLIB) $(LIBS) $(EXTRA_LIBS) 

.c.o:
		$(CC) -c $(INCDIR) $(CFLAGS) $(EXTRA_CFLAGS) $(DEBUG) \
			$(SETPGRP_CALL) $(FSTAB) $(OSTYPE) $(OPT) \
			$(RESVPORT_MOUNT) $*.c

${RPCLIB}: FRC
	@cd rpc; make MACHID="${MACHID}"\
		CC="${CC}" CFLAGS="${CFLAGS}" DEBUG="${DEBUG}"\
		LDFLAGS="${LDFLAGS}" EXTRA_CFLAGS="${EXTRA_CFLAGS}" \
		EXTRA_LDFLAGS="${EXTRA_LDFLAGS}" EXTRA_LIBS="${EXTRA_LIBS}" \
		LIBS="${LIBS}" OSTYPE="${OSTYPE}" OPT="${OPT}" \
		EXTRA_LINTFLAGS="${EXTRA_LINTFLAGS}" \
		RESVPORT_MOUNT="${RESVPORT_MOUNT}"\
		librpclib.a

generic_hash.o:	rfs_c_def.h generic_hash.h generic_hash.c

profile.o:	rfs_c_def.h profile.h profile.c

rfs_3_ops.o:	sfs_c_nfs.h sfs_c_def.h rfs_c_def.h rfs_3_ops.c

sfs_3_vld.o:	sfs_c_nfs.h sfs_c_def.h sfs_3_vld.c

sfs_3_xdr.o:	sfs_c_nfs.h sfs_c_def.h sfs_3_xdr.c

sfs_c_bio.o:	sfs_c_nfs.h sfs_c_def.h rfs_c_def.h sfs_c_bio.c

sfs_c_chd.o:	sfs_c_nfs.h sfs_c_def.h rfs_c_def.h sfs_m_def.h generic_hash.h profile.h sfs_c_chd.c

sem.o:	sem.c

sfs_c_clk.o:	sfs_c_nfs.h sfs_c_def.h sfs_c_clk.c

sfs_c_clnt.o:	sfs_c_nfs.h sfs_c_def.h sfs_c_clnt.c

sfs_c_dat.o:	sfs_c_def.h sfs_m_def.h sfs_c_nfs.h sfs_c_dat.c
rfs_c_dat.o:	sfs_c_def.h rfs_c_def.h sfs_c_nfs.h rfs_c_dat.c

sfs_c_dmp.o:	sfs_c_nfs.h sfs_c_def.h sfs_c_dmp.c

sfs_c_man.o:	sfs_c_nfs.h sfs_c_def.h \
		sfs_m_def.h sfs_c_man.c

sfs_c_mnt.o:	sfs_c_nfs.h sfs_c_def.h rfs_c_def.h sfs_c_mnt.c

sfs_2_ops.o:	sfs_c_nfs.h sfs_c_def.h sfs_2_ops.c

sfs_c_pnt.o:	sfs_c_nfs.h sfs_c_def.h \
		sfs_m_def.h sfs_c_pnt.c

sfs_c_rnd.o:	sfs_c_rnd.c

sfs_c_sig.o:	sfs_c_nfs.h sfs_c_def.h rfs_c_def.h sfs_c_sig.c

sfs_c_sub.o:	sfs_c_nfs.h sfs_c_def.h sfs_c_sub.c

sfs_2_vld.o:	sfs_c_nfs.h sfs_c_def.h sfs_2_vld.c

sfs_2_xdr.o:	sfs_c_nfs.h sfs_c_def.h sfs_2_xdr.c

sfs_m_msg.o:	sfs_c_nfs.h sfs_c_def.h \
		sfs_m_def.h sfs_m_msg.c

sfs_m_prm.o:	sfs_c_nfs.h sfs_c_def.h \
		sfs_m_def.h sfs_m_prm.c

sfs_m_snc.o:	sfs_c_nfs.h sfs_c_def.h \
		sfs_m_def.h sfs_m_snc.c

sfs_m_xdr.o:	sfs_m_def.h sfs_m_xdr.c

#
# ----------------------  lint, install, clean, misc rules  -----------------
#

lint:		lint_sfs lint_sfs3 lint_prime lint_syncd

lint_sfs:	$(V2_HDR) $(M_HDR) $(V2_SRC) $(C_SRC) \
		$(C_COMMON_SRC) $(M_COMMON_SRC) $(M_MSG_SRC)
		lint -Dlint $(INCDIR) $(CFLAGS) $(EXTRA_CFLAGS) $(DEBUG) \
			$(EXTRA_LINTFLAGS) \
			$(SETPGRP_CALL) $(FSTAB) $(OSTYPE) \
			$(RESVPORT_MOUNT) \
			$(V2_SRC) $(C_SRC) $(C_COMMON_SRC) \
			$(M_COMMON_SRC) $(M_MSG_SRC) \
			$(LIBS) $(EXTRA_LIBS) \
			> sfs.lint

lint_prime:	$(V2_HDR) $(M_HDR) $(M_PRIME_SRC) \
		$(C_COMMON_SRC) $(M_COMMON_SRC) $(M_MSG_SRC)
		lint -Dlint $(INCDIR) $(CFLAGS) $(EXTRA_CFLAGS) $(DEBUG) \
			$(EXTRA_LINTFLAGS) \
			$(SETPGRP_CALL) $(FSTAB) $(OSTYPE) \
			$(RESVPORT_MOUNT) \
			$(M_PRIME_SRC) $(C_COMMON_SRC) \
			$(M_COMMON_SRC) $(M_MSG_SRC) \
			$(LIBS) $(EXTRA_LIBS) \
			> sfs_prime.lint

lint_syncd:	$(V2_HDR) $(M_HDR) $(M_SYNCD_SRC) \
		$(C_COMMON_SRC) $(M_COMMON_SRC)
		lint -Dlint $(INCDIR) $(CFLAGS) $(EXTRA_CFLAGS) $(DEBUG) \
			$(EXTRA_LINTFLAGS) \
			$(SETPGRP_CALL) $(FSTAB) $(OSTYPE) \
			$(RESVPORT_MOUNT) \
			$(M_SYNCD_SRC) $(C_COMMON_SRC) $(M_COMMON_SRC) \
			$(LIBS) $(EXTRA_LIBS) \
			> sfs_syncd.lint

lint_sfs3:	$(V3_HDR) $(M_HDR) $(V3_SRC) $(C_SRC) \
		$(C_COMMON_SRC) $(M_COMMON_SRC) $(M_MSG_SRC)
		lint -Dlint $(INCDIR) $(CFLAGS) $(EXTRA_CFLAGS) $(DEBUG) \
			$(EXTRA_LINTFLAGS) \
			$(SETPGRP_CALL) $(FSTAB) $(OSTYPE) \
			$(RESVPORT_MOUNT) \
			$(V3_SRC) $(C_SRC) $(C_COMMON_SRC) \
			$(M_COMMON_SRC) $(M_MSG_SRC) \
			$(LIBS) $(EXTRA_LIBS) \
			> sfs3.lint

install:	sfs sfs3 sfs_syncd sfs_prime
		chmod +x sfs_syncd sfs_prime \
			sfs_mgr sfs_mcr sfs_ext_mon \
			sfs_suchown sfs sfs3
		./sfs_suchown $(RESVPORT_MOUNT) sfs sfs3

clean clobber:
		rm -f *.o sfs sfs_syncd sfs_prime \
			sfs3 sfs_prime \
			lint.out sfs_*.shr1 core *.lint
		cd rpc; make clean
		./sfs_suchown clobber

FRC:
agefs: rfs_c_age.c profile.c 
	gcc -g -o agefs rfs_c_age.c profile.c
frag_count: frag_count.c
	gcc -g -o frag_count frag_count.c
frag_collect:frag_collect
generate_xmgr: generate_xmgr.c
	gcc -g -o generate_xmgr generate_xmgr.c

