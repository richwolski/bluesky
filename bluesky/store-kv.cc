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

/* Interface to John McCullough's simple key/value store. */

#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"
#include "kvservice.h"
#include "kvclient.h"

using namespace boost;
using namespace kvstore;
using namespace std;

static GThreadPool *thread_pool = NULL;

static void kvstore_task(gpointer a, gpointer b)
{
    BlueSkyStoreAsync *async = (BlueSkyStoreAsync *)a;
    KeyValueClient *client = (KeyValueClient *)bluesky_store_async_get_handle(async);

    async->status = ASYNC_RUNNING;

    switch (async->op) {
    case STORE_OP_GET:
    {
        string value;
        if (client->Get(async->key, &value)) {
            async->data = bluesky_string_new(g_memdup(value.c_str(),
                                                    value.length()),
                                            value.length());
            async->result = 0;
        } else {
            g_warning("Failed to fetch key %s from kvstore", async->key);
        }
        break;
    }

    case STORE_OP_PUT:
    {
        string value(async->data->data, async->data->len);
        if (!client->Put(async->key, value)) {
            g_warning("Failed to store key %s to kvstore", async->key);
        }
        break;
    }

    default:
        break;
    }

    bluesky_store_async_mark_complete(async);
    bluesky_store_async_unref(async);
}

static gpointer kvstore_new(const gchar *path)
{
    /* TODO: Right now we leak this memory.  We should probably clean up in
     * kvstore_destroy, but it's not a big deal. */
    const gchar *host = "127.0.0.1", *port = "9090";
    if (path != NULL) {
        gchar **target = g_strsplit(path, ":", 0);
        if (target[0] != NULL) {
            host = target[0];
            if (target[1] != NULL) {
                port = target[1];
            }
        }
    }

    static volatile gsize once = 0;
    if (g_once_init_enter(&once)) {
        thread_pool = g_thread_pool_new(kvstore_task, NULL,
                                        bluesky_max_threads, FALSE, NULL);
        g_once_init_leave(&once, 1);
    }

    g_print("kvstore: %s port %s\n", host, port);
    KeyValueClient *client = new KeyValueClient(host, port);
    return client;
}

static void kvstore_destroy(gpointer store)
{
    KeyValueClient *client = (KeyValueClient *)store;
    delete client;
}

static void kvstore_submit(gpointer store, BlueSkyStoreAsync *async)
{
    KeyValueClient *client = (KeyValueClient *)store;

    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
    case STORE_OP_PUT:
        async->status = ASYNC_PENDING;
        bluesky_store_async_ref(async);
        g_thread_pool_push(thread_pool, async, NULL);
        break;

    default:
        g_warning("Uknown operation type for kvstore: %d\n", async->op);
        bluesky_store_async_mark_complete(async);
        break;
    }
}

static void kvstore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
    KeyValueClient *client = (KeyValueClient *)store;
}

static BlueSkyStoreImplementation store_impl = {
    kvstore_new,
    kvstore_destroy,
    kvstore_submit,
    kvstore_cleanup,
};

extern "C" void bluesky_store_init_kv(void)
{
    bluesky_store_register(&store_impl, "kv");
}
