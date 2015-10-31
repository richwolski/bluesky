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
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

/* Interaction with cloud storage.  We expose very simple GET/PUT style
 * interface, which different backends can implement.  Available backends
 * (will) include Amazon S3 and a simple local store for testing purposes.
 * Operations may be performed asynchronously. */

struct BlueSkyStore {
    const BlueSkyStoreImplementation *impl;
    gpointer handle;

    GMutex *lock;
    GCond *cond_idle;
    int pending;                /* Count of operations not yet complete. */

    struct bluesky_stats *stats_get, *stats_put;
};

GHashTable *store_implementations;

/* Thread pool for calling notifier functions when an operation completes.
 * These are called in a separate thread for locking reasons: we want to call
 * the notifiers without the lock on the async object held, but completion
 * occurs when the lock is held--so we need some way to defer the call.  This
 * isn't really optimal from a cache-locality standpoint. */
static GThreadPool *notifier_thread_pool;

void bluesky_store_register(const BlueSkyStoreImplementation *impl,
                            const gchar *name)
{
    g_hash_table_insert(store_implementations, g_strdup(name), (gpointer)impl);
}

BlueSkyStore *bluesky_store_new(const gchar *type)
{
    const BlueSkyStoreImplementation *impl;

    gchar *scheme, *path;
    scheme = g_strdup(type);
    path = strchr(scheme, ':');
    if (path != NULL) {
        *path = '\0';
        path++;
    }

    impl = g_hash_table_lookup(store_implementations, scheme);
    if (impl == NULL) {
        g_free(scheme);
        return NULL;
    }

    gpointer handle = impl->create(path);
    if (handle == NULL) {
        g_free(scheme);
        return NULL;
    }

    BlueSkyStore *store = g_new(BlueSkyStore, 1);
    store->impl = impl;
    store->handle = handle;
    store->lock = g_mutex_new();
    store->cond_idle = g_cond_new();
    store->pending = 0;
    store->stats_get = bluesky_stats_new(g_strdup_printf("Store[%s]: GETS",
                                                         type));
    store->stats_put = bluesky_stats_new(g_strdup_printf("Store[%s]: PUTS",
                                                         type));
    g_free(scheme);
    return store;
}

void bluesky_store_free(BlueSkyStore *store)
{
    store->impl->destroy(store->handle);
    g_free(store);
}

char *bluesky_store_lookup_last(BlueSkyStore *store, const char *prefix)
{
    return store->impl->lookup_last(store->handle, prefix);
}

BlueSkyStoreAsync *bluesky_store_async_new(BlueSkyStore *store)
{
    BlueSkyStoreAsync *async;

    async = g_new(BlueSkyStoreAsync, 1);
    async->store = store;
    async->lock = g_mutex_new();
    async->completion_cond = g_cond_new();
    async->refcount = 1;
    async->status = ASYNC_NEW;
    async->op = STORE_OP_NONE;
    async->key = NULL;
    async->data = NULL;
    async->start = async->len = 0;
    async->range_done = FALSE;
    async->result = -1;
    async->notifiers = NULL;
    async->notifier_count = 0;
    async->barrier = NULL;
    async->store_private = NULL;
    async->profile = NULL;

    return async;
}

gpointer bluesky_store_async_get_handle(BlueSkyStoreAsync *async)
{
    return async->store->handle;
}

void bluesky_store_async_ref(BlueSkyStoreAsync *async)
{
    if (async == NULL)
        return;

    g_return_if_fail(g_atomic_int_get(&async->refcount) > 0);

    g_atomic_int_inc(&async->refcount);
}

void bluesky_store_async_unref(BlueSkyStoreAsync *async)
{
    if (async == NULL)
        return;

    if (g_atomic_int_dec_and_test(&async->refcount)) {
        async->store->impl->cleanup(async->store->handle, async);
        g_mutex_free(async->lock);
        g_cond_free(async->completion_cond);
        g_free(async->key);
        bluesky_string_unref(async->data);
        g_free(async);
    }
}

/* Block until the given operation has completed. */
void bluesky_store_async_wait(BlueSkyStoreAsync *async)
{
    g_return_if_fail(async != NULL);
    g_mutex_lock(async->lock);

    if (async->status == ASYNC_NEW) {
        g_error("bluesky_store_async_wait on a new async object!\n");
        g_mutex_unlock(async->lock);
        return;
    }

    while (async->status != ASYNC_COMPLETE
           || g_atomic_int_get(&async->notifier_count) > 0) {
        g_cond_wait(async->completion_cond, async->lock);
    }

    g_mutex_unlock(async->lock);
}

/* Add a notifier function to be called when the operation completes. */
void bluesky_store_async_add_notifier(BlueSkyStoreAsync *async,
                                      GFunc func, gpointer user_data)
{
    struct BlueSkyNotifierList *nl = g_new(struct BlueSkyNotifierList, 1);
    g_mutex_lock(async->lock);
    nl->next = async->notifiers;
    nl->func = func;
    nl->async = async; bluesky_store_async_ref(async);
    nl->user_data = user_data;
    g_atomic_int_inc(&async->notifier_count);
    if (async->status == ASYNC_COMPLETE) {
        g_thread_pool_push(notifier_thread_pool, nl, NULL);
    } else {
        async->notifiers = nl;
    }
    g_mutex_unlock(async->lock);
}

static void op_complete(gpointer a, gpointer b)
{
    BlueSkyStoreAsync *barrier = (BlueSkyStoreAsync *)b;

    bluesky_store_async_ref(barrier);
    g_mutex_lock(barrier->lock);
    barrier->store_private
        = GINT_TO_POINTER(GPOINTER_TO_INT(barrier->store_private) - 1);
    if (GPOINTER_TO_INT(barrier->store_private) == 0
            && barrier->status != ASYNC_NEW) {
        bluesky_store_async_mark_complete(barrier);
    }
    g_mutex_unlock(barrier->lock);
    bluesky_store_async_unref(barrier);
}

/* Mark an asynchronous operation as complete.  This should only be called by
 * the store implementations.  The lock should be held when calling this
 * function.  Any notifier functions will be called, but in a separate thread
 * and without the lock held. */
void bluesky_store_async_mark_complete(BlueSkyStoreAsync *async)
{
    g_return_if_fail(async->status != ASYNC_COMPLETE);

    bluesky_time_hires elapsed = bluesky_now_hires() - async->start_time;
    bluesky_time_hires latency = bluesky_now_hires() - async->exec_time;

    if (async->op != STORE_OP_BARRIER) {
        g_mutex_lock(async->store->lock);
        async->store->pending--;
        if (async->store->pending == 0)
            g_cond_broadcast(async->store->cond_idle);
        g_mutex_unlock(async->store->lock);
    }

    /* If the request was a range request but the backend read the entire
     * object, select out the appropriate bytes. */
    if (async->op == STORE_OP_GET
            && !async->range_done
            && async->result == 0
            && async->data != NULL) {
        if (async->start != 0 || async->len != 0) {
            /* If the caller requesteda read outside the object, return an
             * error. */
            if (async->start + async->len > async->data->len) {
                g_warning("Range request outside object boundaries!\n");
                async->result = -1;
            } else {
                if (async->len == 0)
                    async->len = async->data->len - async->start;
                BlueSkyRCStr *newstr = bluesky_string_new(g_memdup(&async->data->data[async->start], async->len), async->len);
                bluesky_string_unref(async->data);
                async->data = newstr;
                async->range_done = TRUE;
            }
        }
    }

    async->status = ASYNC_COMPLETE;
    g_cond_broadcast(async->completion_cond);

    if (async->barrier != NULL && async->notifiers == NULL)
        op_complete(async, async->barrier);

    while (async->notifiers != NULL) {
        struct BlueSkyNotifierList *nl = async->notifiers;
        async->notifiers = nl->next;
        g_thread_pool_push(notifier_thread_pool, nl, NULL);
    }

    if (async->profile) {
        bluesky_profile_add_event(
            async->profile,
            g_strdup_printf("%s for %s complete",
                            async->op == STORE_OP_GET ? "GET"
                            : async->op == STORE_OP_PUT ? "PUT"
                            : async->op == STORE_OP_DELETE ? "DELETE"
                            : async->op == STORE_OP_BARRIER ? "BARRIER" : "???",
                            async->key)
        );
    }

    if (bluesky_verbose) {
        g_log("bluesky/store", G_LOG_LEVEL_DEBUG,
              "[%p] complete: elapsed = %"PRIi64" ns, latency = %"PRIi64" ns",
              async, elapsed, latency);
    }

    if (async->data) {
        if (async->op == STORE_OP_GET) {
            bluesky_stats_add(async->store->stats_get, async->data->len);
        } else if (async->op == STORE_OP_PUT) {
            bluesky_stats_add(async->store->stats_put, async->data->len);
        }
    }
}

void bluesky_store_async_submit(BlueSkyStoreAsync *async)
{
    BlueSkyStore *store = async->store;

    async->start_time = bluesky_now_hires();

    // Backends should fill this in with a better estimate of the actual time
    // processing was started, if there could be a delay from submission time.
    async->exec_time = bluesky_now_hires();

    if (async->profile) {
        bluesky_profile_add_event(
            async->profile,
            g_strdup_printf("Start %s for %s",
                            async->op == STORE_OP_GET ? "GET"
                            : async->op == STORE_OP_PUT ? "PUT"
                            : async->op == STORE_OP_DELETE ? "DELETE"
                            : async->op == STORE_OP_BARRIER ? "BARRIER" : "???",
                            async->key)
        );
    }

    if (bluesky_verbose) {
        g_log("bluesky/store", G_LOG_LEVEL_DEBUG, "[%p] submit: %s %s",
              async,
              async->op == STORE_OP_GET ? "GET"
                : async->op == STORE_OP_PUT ? "PUT"
                : async->op == STORE_OP_DELETE ? "DELETE"
                : async->op == STORE_OP_BARRIER ? "BARRIER" : "???",
              async->key);
    }

    /* Barriers are handled specially, and not handed down the storage
     * implementation layer. */
    if (async->op == STORE_OP_BARRIER) {
        async->status = ASYNC_RUNNING;
        if (GPOINTER_TO_INT(async->store_private) == 0)
            bluesky_store_async_mark_complete(async);
        return;
    }

    g_mutex_lock(async->store->lock);
    async->store->pending++;
    g_mutex_unlock(async->store->lock);

    store->impl->submit(store->handle, async);

    if (bluesky_options.synchronous_stores)
        bluesky_store_async_wait(async);
}

/* Add the given operation to the barrier.  The barrier will not complete until
 * all operations added to it have completed. */
void bluesky_store_add_barrier(BlueSkyStoreAsync *barrier,
                               BlueSkyStoreAsync *async)
{
    g_return_if_fail(barrier->op == STORE_OP_BARRIER);

    g_mutex_lock(barrier->lock);
    barrier->store_private
        = GINT_TO_POINTER(GPOINTER_TO_INT(barrier->store_private) + 1);
    g_mutex_unlock(barrier->lock);

    g_mutex_lock(async->lock);
    if (async->barrier == NULL && async->status != ASYNC_COMPLETE) {
        async->barrier = barrier;
        g_mutex_unlock(async->lock);
    } else {
        if (async->barrier != NULL)
            g_warning("Adding async to more than one barrier!\n");
        g_mutex_unlock(async->lock);
        bluesky_store_async_add_notifier(async, op_complete, barrier);
    }
}

static void notifier_task(gpointer n, gpointer s)
{
    struct BlueSkyNotifierList *notifier = (struct BlueSkyNotifierList *)n;

    notifier->func(notifier->async, notifier->user_data);
    if (g_atomic_int_dec_and_test(&notifier->async->notifier_count)) {
        g_mutex_lock(notifier->async->lock);
        if (notifier->async->barrier != NULL)
            op_complete(notifier->async, notifier->async->barrier);
        g_cond_broadcast(notifier->async->completion_cond);
        g_mutex_unlock(notifier->async->lock);
    }
    bluesky_store_async_unref(notifier->async);
    g_free(notifier);
}

void bluesky_store_sync(BlueSkyStore *store)
{
    g_mutex_lock(store->lock);
    if (bluesky_verbose) {
        g_log("bluesky/store", G_LOG_LEVEL_DEBUG,
              "Waiting for pending store operations to complete...");
    }
    while (store->pending > 0) {
        g_cond_wait(store->cond_idle, store->lock);
    }
    g_mutex_unlock(store->lock);
    if (bluesky_verbose) {
        g_log("bluesky/store", G_LOG_LEVEL_DEBUG, "Operations are complete.");
    }
}

/* Convenience wrappers that perform a single operation synchronously. */
BlueSkyRCStr *bluesky_store_get(BlueSkyStore *store, const gchar *key)
{
    BlueSkyStoreAsync *async = bluesky_store_async_new(store);
    async->op = STORE_OP_GET;
    async->key = g_strdup(key);
    bluesky_store_async_submit(async);

    bluesky_store_async_wait(async);

    BlueSkyRCStr *data = async->data;
    bluesky_string_ref(data);
    bluesky_store_async_unref(async);
    return data;
}

void bluesky_store_put(BlueSkyStore *store,
                       const gchar *key, BlueSkyRCStr *val)
{
    BlueSkyStoreAsync *async = bluesky_store_async_new(store);
    async->op = STORE_OP_PUT;
    async->key = g_strdup(key);
    bluesky_string_ref(val);
    async->data = val;
    bluesky_store_async_submit(async);

    bluesky_store_async_wait(async);
    bluesky_store_async_unref(async);
}

/* Simple in-memory data store for test purposes. */
typedef struct {
    GMutex *lock;

    /* TODO: A hashtable isn't optimal for list queries... */
    GHashTable *store;
} MemStore;

static gpointer memstore_create(const gchar *path)
{
    MemStore *store = g_new(MemStore, 1);
    store->lock = g_mutex_new();
    store->store = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free,
                                         (GDestroyNotify)bluesky_string_unref);

    return (gpointer)store;
}

static void memstore_destroy(gpointer store)
{
    /* TODO */
}

static BlueSkyRCStr *memstore_get(gpointer st, const gchar *key)
{
    MemStore *store = (MemStore *)st;
    BlueSkyRCStr *s = g_hash_table_lookup(store->store, key);
    if (s != NULL)
        bluesky_string_ref(s);
    return s;
}

static void memstore_put(gpointer s, const gchar *key, BlueSkyRCStr *val)
{
    MemStore *store = (MemStore *)s;
    bluesky_string_ref(val);
    g_hash_table_insert(store->store, g_strdup(key), val);
}

static void memstore_submit(gpointer s, BlueSkyStoreAsync *async)
{
    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
        async->data = memstore_get(s, async->key);
        break;

    case STORE_OP_PUT:
        memstore_put(s, async->key, async->data);
        break;

    default:
        g_warning("Uknown operation type for MemStore: %d\n", async->op);
        return;
    }

    bluesky_store_async_mark_complete(async);
}

static void memstore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static BlueSkyStoreImplementation memstore_impl = {
    .create = memstore_create,
    .destroy = memstore_destroy,
    .submit = memstore_submit,
    .cleanup = memstore_cleanup,
};

/* Store implementation which writes data as files to disk. */
static gpointer filestore_create(const gchar *path)
{
    return GINT_TO_POINTER(1);
}

static void filestore_destroy()
{
}

static BlueSkyRCStr *filestore_get(const gchar *key)
{
    gchar *contents = NULL;
    gsize length;
    GError *error = NULL;

    g_file_get_contents(key, &contents, &length, &error);
    if (contents == NULL)
        return NULL;

    return bluesky_string_new(contents, length);
}

static void filestore_put(const gchar *key, BlueSkyRCStr *val)
{
    g_file_set_contents(key, val->data, val->len, NULL);
}

static void filestore_submit(gpointer s, BlueSkyStoreAsync *async)
{
    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
        async->data = filestore_get(async->key);
        async->result = 0;
        break;

    case STORE_OP_PUT:
        filestore_put(async->key, async->data);
        async->result = 0;
        break;

    default:
        g_warning("Uknown operation type for FileStore: %d\n", async->op);
        return;
    }

    bluesky_store_async_mark_complete(async);
}

static void filestore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static char *filestore_lookup_last(gpointer store, const char *prefix)
{
    char *last = NULL;
    GDir *dir = g_dir_open(".", 0, NULL);
    if (dir == NULL) {
        g_warning("Unable to open directory for listing");
        return NULL;
    }

    const gchar *file;
    while ((file = g_dir_read_name(dir)) != NULL) {
        if (strncmp(file, prefix, strlen(prefix)) == 0) {
            if (last == NULL || strcmp(file, last) > 0) {
                g_free(last);
                last = g_strdup(file);
            }
        }
    }
    g_dir_close(dir);

    return last;
}

static BlueSkyStoreImplementation filestore_impl = {
    .create = filestore_create,
    .destroy = filestore_destroy,
    .submit = filestore_submit,
    .cleanup = filestore_cleanup,
    .lookup_last = filestore_lookup_last,
};

/* A store implementation which simply discards all data, for testing. */
static gpointer nullstore_create(const gchar *path)
{
    return (gpointer)nullstore_create;
}

static void nullstore_destroy(gpointer store)
{
}

static void nullstore_submit(gpointer s, BlueSkyStoreAsync *async)
{
    bluesky_store_async_mark_complete(async);
}

static void nullstore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static BlueSkyStoreImplementation nullstore_impl = {
    .create = nullstore_create,
    .destroy = nullstore_destroy,
    .submit = nullstore_submit,
    .cleanup = nullstore_cleanup,
};

void bluesky_store_init()
{
    store_implementations = g_hash_table_new(g_str_hash, g_str_equal);
    notifier_thread_pool = g_thread_pool_new(notifier_task, NULL,
                                             bluesky_max_threads, FALSE, NULL);
    bluesky_store_register(&memstore_impl, "mem");
    bluesky_store_register(&filestore_impl, "file");
    bluesky_store_register(&nullstore_impl, "null");
}
