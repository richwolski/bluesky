#ifndef lint
static char sfs_xdr_c_id[] = "@(#)xdr.c     2.1     97/10/23";
#endif
/* @(#)xdr.c	2.1 88/07/29 4.0 RPCSRC */
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
static char sccsid[] = "@(#)xdr.c 1.35 87/08/12";
#endif

/*
 * xdr.c, Generic XDR routines implementation.
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */
#ifndef lint
static char sfs_clnt_id[] = "@(#)xdr.c     2.1     97/10/23";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpc/types.h"
#include "rpc/xdr.h"

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE	((int32_t) 0)
#define XDR_TRUE	((int32_t) 1)
#define LASTUNSIGNED	((uint_t) 0-1)

/*
 * for unit alignment
 */
static char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

/*
 * Free a data structure using XDR
 * Not a filter, but a convenient utility nonetheless
 */
void
xdr_free(
	xdrproc_t proc,
	char *objp)
{
	XDR x;
	
	x.x_op = XDR_FREE;
	(*proc)(&x, objp);
}

/*
 * XDR nothing
 */
bool_t
xdr_void(void)
{

	return (TRUE);
}

/*
 * XDR integers
 */
bool_t
xdr_int(
	XDR *xdrs,
	int *ip)
{

#ifdef lint
	(void) (xdr_int16_t(xdrs, (int16_t *)ip));
	return (xdr_int32_t(xdrs, (int32_t *)ip));
#else
	if (sizeof (int) == sizeof (int32_t)) {
		return (xdr_int32_t(xdrs, (int32_t *)ip));
	} else {
		return (xdr_int16_t(xdrs, (int16_t *)ip));
	}
#endif
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(
	XDR *xdrs,
	uint_t *up)
{

#ifdef lint
	(void) (xdr_int16_t(xdrs, (int16_t *)up));
	return (xdr_uint32_t(xdrs, (uint32_t *)up));
#else
	if (sizeof (uint_t) == sizeof (uint32_t)) {
		return (xdr_uint32_t(xdrs, (uint32_t *)up));
	} else {
		return (xdr_int16_t(xdrs, (int16_t *)up));
	}
#endif
}

/*
 * XDR long integers
 * same as xdr_u_long - open coded to save a proc call!
 */
bool_t
xdr_long(
	XDR *xdrs,
	int32_t *lp)
{

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTLONG(xdrs, lp));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, lp));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

bool_t
xdr_int32_t(
	XDR *xdrs,
	int32_t *lp)
{

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTLONG(xdrs, lp));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, lp));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_u_long(
	XDR *xdrs,
	uint32_t *ulp)
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, (int32_t *)ulp));
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTLONG(xdrs, (int32_t *)ulp));
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	return (FALSE);
}

bool_t
xdr_uint32_t(
	XDR *xdrs,
	uint32_t *ulp)
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, (int32_t *)ulp));
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTLONG(xdrs, (int32_t *)ulp));
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	return (FALSE);
}

/*
 * XDR short integers
 */
bool_t
xdr_short(
	XDR *xdrs,
	int16_t *sp)
{
	int32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (int32_t) *sp;
		return (XDR_PUTLONG(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (int16_t) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_int16_t(
	XDR *xdrs,
	int16_t *sp)
{
	int32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (int32_t) *sp;
		return (XDR_PUTLONG(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (int16_t) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR unsigned short integers
 */
bool_t
xdr_u_short(
	XDR *xdrs,
	uint16_t *usp)
{
	uint32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (uint32_t) *usp;
		return (XDR_PUTLONG(xdrs, (int32_t *)&l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, (int32_t *)&l)) {
			return (FALSE);
		}
		*usp = (uint16_t) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

bool_t
xdr_uint16_t(
	XDR *xdrs,
	uint16_t *usp)
{
	uint32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (uint32_t) *usp;
		return (XDR_PUTLONG(xdrs, (int32_t *)&l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, (int32_t *)&l)) {
			return (FALSE);
		}
		*usp = (uint16_t) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR a char
 */
bool_t
xdr_char(
	XDR *xdrs,
	char *cp)
{
	int i;

	i = (*cp);
	if (!xdr_int(xdrs, &i)) {
		return (FALSE);
	}
	*cp = i;
	return (TRUE);
}

/*
 * XDR an unsigned char
 */
bool_t
xdr_u_char(
	XDR *xdrs,
	uchar_t *cp)
{
	uint_t u;

	u = (*cp);
	if (!xdr_u_int(xdrs, &u)) {
		return (FALSE);
	}
	*cp = u;
	return (TRUE);
}

/*
 * XDR booleans
 */
bool_t
xdr_bool(
	XDR *xdrs,
	bool_t *bp)
{
	int32_t lb;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		lb = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTLONG(xdrs, &lb));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &lb)) {
			return (FALSE);
		}
		*bp = (lb == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR enumerations
 */
bool_t
xdr_enum(
	XDR *xdrs,
	enum_t *ep)
{
#ifndef lint
	enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

	/*
	 * enums are treated as ints
	 */
	if (sizeof (enum sizecheck) == sizeof (int32_t)) {
		return (xdr_int32_t(xdrs, (int32_t *)ep));
	} else if (sizeof (enum sizecheck) == sizeof (int16_t)) {
		return (xdr_int16_t(xdrs, (int16_t *)ep));
	} else {
		return (FALSE);
	}
#else
	(void) (xdr_int16_t(xdrs, (int16_t *)ep));
	return (xdr_int32_t(xdrs, (int32_t *)ep));
#endif
}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t
xdr_opaque(
	XDR *xdrs,
	void *cp,
	uint_t cnt)
{
	uint_t rndup;
	static crud[BYTES_PER_XDR_UNIT];

	/*
	 * if no data we are done
	 */
	if (cnt == 0)
		return (TRUE);

	/*
	 * round byte count to full xdr units
	 */
	rndup = cnt % BYTES_PER_XDR_UNIT;
	if (rndup > 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE) {
		return (TRUE);
	}

	return (FALSE);
}

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t
xdr_bytes(
	XDR *xdrs,
	char **cpp,
	uint_t *sizep,
	uint_t maxsize)
{
	char *sp = *cpp;  /* sp is the actual string pointer */
	uint_t nodesize;

	/*
	 * first deal with the length since xdr bytes are counted
	 */
	if (! xdr_u_int(xdrs, sizep)) {
		return (FALSE);
	}
	nodesize = *sizep;
	if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
		return (FALSE);
	}

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (nodesize == 0) {
			return (TRUE);
		}
		if (sp == NULL) {
			*cpp = sp = (char *)mem_alloc(nodesize);
		}
		if (sp == NULL) {
			(void) fprintf(stderr, "xdr_bytes: out of memory\n");
			return (FALSE);
		}
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, nodesize));

	case XDR_FREE:
		if (sp != NULL) {
			mem_free(sp, nodesize);
			*cpp = NULL;
		}
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Implemented here due to commonality of the object.
 */
bool_t
xdr_netobj(
	XDR *xdrs,
	struct netobj *np)
{

	return (xdr_bytes(xdrs, &np->n_bytes, &np->n_len, MAX_NETOBJ_SZ));
}

/*
 * XDR a descriminated union
 * Support routine for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * an entry with a null procedure pointer.  The routine gets
 * the discriminant value and then searches the array of xdrdiscrims
 * looking for that value.  It calls the procedure given in the xdrdiscrim
 * to handle the discriminant.  If there is no specific routine a default
 * routine may be called.
 * If there is no specific or default routine an error is returned.
 */
bool_t
xdr_union(
	XDR *xdrs,
	enum_t *dscmp,		/* enum to decide which arm to work on */
	char *unp,		/* the union itself */
	struct xdr_discrim *choices,	/* [value, xdr proc] for each arm */
	xdrproc_t dfault)	/* default xdr routine */
{
	enum_t dscm;

	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (! xdr_enum(xdrs, dscmp)) {
		return (FALSE);
	}
	dscm = *dscmp;

	/*
	 * search choices for a value that matches the discriminator.
	 * if we find one, execute the xdr routine for that value.
	 */
	for (; choices->proc != NULL_xdrproc_t; choices++) {
		if (choices->value == dscm)
			return ((*(choices->proc))(xdrs, unp, LASTUNSIGNED));
	}

	/*
	 * no match - execute the default xdr routine if there is one
	 */
	return ((dfault == NULL_xdrproc_t) ? FALSE :
	    (*dfault)(xdrs, unp, LASTUNSIGNED));
}


/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */


/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t
xdr_string(
	XDR *xdrs,
	char **cpp,
	uint_t maxsize)
{
	char *sp = *cpp;  /* sp is the actual string pointer */
	uint_t size;
	uint_t nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	switch (xdrs->x_op) {
	case XDR_FREE:
		if (sp == NULL) {
			return(TRUE);	/* already free */
		}
		/* fall through... */
	case XDR_ENCODE:
		size = strlen(sp);
		break;
	}
	if (! xdr_u_int(xdrs, &size)) {
		return (FALSE);
	}
	if (size > maxsize) {
		return (FALSE);
	}
	nodesize = size + 1;

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (nodesize == 0) {
			return (TRUE);
		}
		if (sp == NULL)
			*cpp = sp = (char *)mem_alloc(nodesize);
		if (sp == NULL) {
			(void) fprintf(stderr, "xdr_string: out of memory\n");
			return (FALSE);
		}
		sp[size] = 0;
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
		mem_free(sp, nodesize);
		*cpp = NULL;
		return (TRUE);
	}
	return (FALSE);
}

/* 
 * Wrapper for xdr_string that can be called directly from 
 * routines like clnt_call
 */
bool_t
xdr_wrapstring(
	XDR *xdrs,
	char **cpp)
{
	if (xdr_string(xdrs, cpp, LASTUNSIGNED)) {
		return (TRUE);
	}
	return (FALSE);
}
