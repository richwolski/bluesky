/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2010  The Regents of the University of California
 * Written by Michael Vrable <mvrable@cs.ucsd.edu>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Common RPC code shared between the BlueSky server and test client code. */

#include "mount_prot.h"
#include "nfs3_prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "bluesky.h"

/* Routines for XDR-encoding to a growable string. */
static bool_t xdr_string_putlong(XDR *xdrs, const long *lp)
{
    GString *str = (GString *)xdrs->x_private;
    uint32_t data = htonl(*lp);
    g_string_set_size(str, str->len + 4);
    memcpy(str->str + str->len - 4, &data, 4);
    return TRUE;
}

static bool_t xdr_string_putbytes(XDR *xdrs, const char *addr, u_int len)
{
    GString *str = (GString *)xdrs->x_private;
    g_string_set_size(str, str->len + len);
    memcpy(str->str + str->len - len, addr, len);
    return TRUE;
}

static u_int xdr_string_getpos(const XDR *xdrs)
{
    GString *str = (GString *)xdrs->x_private;
    return str->len;
}

static bool_t xdr_string_putint32(XDR *xdrs, const int32_t *ip)
{
    GString *str = (GString *)xdrs->x_private;
    uint32_t data = htonl(*ip);
    g_string_set_size(str, str->len + 4);
    memcpy(str->str + str->len - 4, &data, 4);
    return TRUE;
}

static int32_t *xdr_string_inline(XDR *xdrs, u_int len)
{
    GString *str = (GString *)xdrs->x_private;
    g_string_set_size(str, str->len + len);
    return (int32_t *)(str->str + str->len - len);
}

static void xdr_string_destroy(XDR *xdrs)
{
}

static struct xdr_ops xdr_string_ops = {
    .x_putlong = xdr_string_putlong,
    .x_putbytes = xdr_string_putbytes,
    .x_getpostn = xdr_string_getpos,
    .x_putint32 = xdr_string_putint32,
    .x_inline = xdr_string_inline,
    .x_destroy = xdr_string_destroy,
};

void xdr_string_create(XDR *xdrs, GString *string, enum xdr_op op)
{
    xdrs->x_op = op;
    xdrs->x_ops = &xdr_string_ops;
    xdrs->x_private = (char *)string;
    xdrs->x_base = NULL;
    xdrs->x_handy = 0;
}
