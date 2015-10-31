/*
 * @(#)types.h     2.1     97/10/23
 */
/* @(#)types.h	2.3 88/08/15 4.0 RPCSRC */
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
/*      @(#)types.h 1.18 87/07/24 SMI      */

/*
 * Rpc additions to <sys/types.h>
 */
#ifndef __TYPES_RPC_HEADER__
#define __TYPES_RPC_HEADER__

#include <sys/types.h>
#include <sys/time.h>

#if defined(__INTTYPES_INCLUDED) || defined(_SYS_INT_TYPES_H)
#define HAVE_INTTYPES
#endif

#if defined(USE_INTTYPES)
#include <inttypes.h>
#else /* USE_INTTYPES */
#if !defined(HAVE_INTTYPES)
#define HAVE_INTTYPES

typedef signed char		int8_t;
typedef short			int16_t;
typedef int			int32_t;

typedef unsigned char           uint8_t;
typedef unsigned short          uint16_t;
typedef unsigned int            uint32_t;

#endif /* !HAVE_INTTYPES */

#endif /* USE_INTTYPES */

#define	bool_t	int32_t
#define	enum_t	int32_t
#ifndef FALSE
#define	FALSE	(0)
#endif
#ifndef TRUE
#define	TRUE	(1)
#endif
#define __dontcare__	-1
#ifndef NULL
#define NULL 0
#endif

#define mem_alloc(bsize)	malloc(bsize)
#define mem_free(ptr, bsize)	free(ptr)

#if defined(NO_T_TYPES)
typedef unsigned int	uint_t;
typedef unsigned char	uchar_t;
#endif

#ifdef notdef
#ifndef INADDR_LOOPBACK
#define       INADDR_LOOPBACK         (uint32_t)0x7F000001
#endif
#endif

#endif /* ndef __TYPES_RPC_HEADER__ */
