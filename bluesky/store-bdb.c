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

#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <db.h>
#include <errno.h>

#include "bluesky-private.h"
#include "libs3.h"

/* A storage layer that writes to Berkeley DB locally. */

typedef struct {
    DB_ENV *env;
    DB *db;
    GAsyncQueue *operations;
} BDBStore;

static gpointer bdbstore_thread(gpointer data)
{
    BDBStore *store = (BDBStore *)data;
    DB_TXN *txn = NULL;

    // Number of operations in the current transaction
    int transaction_size = 0;

    while (TRUE) {
        int res;
        BlueSkyStoreAsync *async;

        if (txn == NULL) {
            res = store->env->txn_begin(store->env, NULL, &txn, 0);
            if (res != 0) {
                fprintf(stderr, "Unable to begin transaction!\n");
                return NULL;
            }
        }

        async = (BlueSkyStoreAsync *)g_async_queue_pop(store->operations);
        async->status = ASYNC_RUNNING;
        async->exec_time = bluesky_now_hires();

        DBT key;
        memset(&key, 0, sizeof(key));

        key.data = async->key;
        key.size = strlen(async->key);

        DBT value;
        memset(&value, 0, sizeof(value));

        if (async->op == STORE_OP_GET) {
            value.flags = DB_DBT_MALLOC;

            res = store->db->get(store->db, txn, &key, &value, 0);

            async->result = res;
            async->data = NULL;

            if (res != 0) {
                fprintf(stderr, "BDB read failure: %s\n", db_strerror(res));
            } else {
                async->data = bluesky_string_new(value.data, value.size);
                async->result = 0;
            }

        } else if (async->op == STORE_OP_PUT) {
            value.data = async->data->data;
            value.size = async->data->len;

            res = store->db->put(store->db, txn, &key, &value, 0);

            if (res != 0) {
                fprintf(stderr, "BDB write failure: %s\n", db_strerror(res));
            }

            async->result = 0;
        }

        bluesky_store_async_mark_complete(async);
        bluesky_store_async_unref(async);
        transaction_size++;

        if (transaction_size >= 64) {
            txn->commit(txn, 0);
            txn = NULL;
            transaction_size = 0;
        }
    }

    return NULL;
}

static gpointer bdbstore_new(const gchar *path)
{
    int res;
    BDBStore *store = g_new0(BDBStore, 1);

    res = db_env_create(&store->env, 0);

    if (res != 0) {
        fprintf(stderr, "db_env_create failure: %s\n", db_strerror(res));
        return NULL;
    }

    res = store->env->open(store->env, path,
                           DB_CREATE | DB_RECOVER | DB_INIT_LOCK | DB_INIT_LOG
                            | DB_INIT_MPOOL | DB_INIT_TXN | DB_THREAD,
                           0644);

    if (res != 0) {
        fprintf(stderr, "BDB open failure: %s\n",
                db_strerror(res));
        return NULL;
    }

    res = db_create(&store->db, store->env, 0);

    if (res != 0) {
        fprintf(stderr, "DB create failed: %s\n", db_strerror(res));
        return NULL;
    }

    uint32_t flags = DB_CREATE | DB_THREAD | DB_AUTO_COMMIT;

    res = store->db->open(store->db,
                          NULL, /* TXN */
                          "store.db",
                          "store",
                          DB_BTREE,
                          flags,
                          0644);

    if (res != 0) {
        fprintf(stderr, "DB open failed: %s\n",
                db_strerror(res));
    }

    store->operations = g_async_queue_new();
    if (g_thread_create(bdbstore_thread, store, FALSE, NULL) == NULL) {
        fprintf(stderr, "Creating BDB thread failed!\n");
        return NULL;
    }

    return store;
}

static void bdbstore_destroy(gpointer s)
{
    BDBStore *store = (BDBStore *)store;

    if (store->db) {
        store->db->close(store->db, 0);
    }

    if (store->env) {
        store->env->close(store->env, 0);
    }

    g_free(store);
}

static void bdbstore_submit(gpointer s, BlueSkyStoreAsync *async)
{
    BDBStore *store = (BDBStore *)s;
    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
    case STORE_OP_PUT:
        async->status = ASYNC_PENDING;
        bluesky_store_async_ref(async);
        g_async_queue_push(store->operations, async);
        break;

    default:
        g_warning("Uknown operation type for BDBStore: %d\n", async->op);
        bluesky_store_async_mark_complete(async);
        break;
    }
}

static void bdbstore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static BlueSkyStoreImplementation store_impl = {
    .create = bdbstore_new,
    .destroy = bdbstore_destroy,
    .submit = bdbstore_submit,
    .cleanup = bdbstore_cleanup,
};

void bluesky_store_init_bdb(void)
{
    bluesky_store_register(&store_impl, "bdb");
}

