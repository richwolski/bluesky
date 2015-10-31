/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2009  The Regents of the University of California
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

/* A stacked storage implementation which tries to improve performance by
 * duplicating GET requests.  Create using a name like "multi:s3" and each GET
 * request will be translated into two GET requests to the "s3" backend.  The
 * first to complete will have its results returned. */

#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

struct MultiRequest {
};

static gpointer multistore_new(const gchar *path)
{
    BlueSkyStore *base = bluesky_store_new(path);
    if (base == NULL) {
        g_warning("Unable to create base store %s for multirequest stacking.",
                  path);
    }

    return base;
}

static void multistore_destroy(gpointer store)
{
    bluesky_store_free(store);
}

static void multistore_completion_handler(BlueSkyStoreAsync *async,
                                          BlueSkyStoreAsync *top_async)
{
    g_mutex_lock(top_async->lock);

    /* This might be the second request to finish; in that case we don't do
     * anything. */
    if (top_async->status == ASYNC_RUNNING) {
        if (top_async->op == STORE_OP_GET) {
            bluesky_string_unref(top_async->data);
            top_async->data = async->data;
            bluesky_string_ref(top_async->data);
        }
        top_async->result = async->result;
        bluesky_store_async_mark_complete(top_async);
    }

    g_mutex_unlock(top_async->lock);
    bluesky_store_async_unref(top_async);
}

static void multistore_submit(gpointer store, BlueSkyStoreAsync *async)
{
    BlueSkyStore *base = (BlueSkyStore *)store;

    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
        async->status = ASYNC_RUNNING;
        async->exec_time = bluesky_now_hires();
        for (int i = 0; i < 2; i++) {
            BlueSkyStoreAsync *a = bluesky_store_async_new(base);
            a->op = STORE_OP_GET;
            a->key = g_strdup(async->key);
            bluesky_store_async_ref(async);
            bluesky_store_async_add_notifier(a, (GFunc)multistore_completion_handler, async);
            bluesky_store_async_submit(a);
            bluesky_store_async_unref(a);
        }
        break;

    case STORE_OP_PUT:
    {
        async->status = ASYNC_RUNNING;
        async->exec_time = bluesky_now_hires();

        bluesky_store_async_ref(async);
        BlueSkyStoreAsync *a = bluesky_store_async_new(base);
        a->op = STORE_OP_PUT;
        a->key = g_strdup(async->key);
        a->data = async->data;
        bluesky_string_ref(a->data);
        bluesky_store_async_add_notifier(a, (GFunc)multistore_completion_handler, async);
        bluesky_store_async_submit(a);
        bluesky_store_async_unref(a);
        break;
    }

    default:
        g_warning("Uknown operation type for multistore: %d\n", async->op);
        bluesky_store_async_mark_complete(async);
        break;
    }
}

static char *multistore_lookup_last(gpointer store, const char *prefix)
{
    BlueSkyStore *base = (BlueSkyStore *)store;
    return bluesky_store_lookup_last(base, prefix);
}

static void multistore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static BlueSkyStoreImplementation store_impl = {
    .create = multistore_new,
    .destroy = multistore_destroy,
    .submit = multistore_submit,
    .cleanup = multistore_cleanup,
    .lookup_last = multistore_lookup_last,
};

void bluesky_store_init_multi(void)
{
    bluesky_store_register(&store_impl, "multi");
}
