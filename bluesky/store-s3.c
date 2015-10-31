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

#include "bluesky-private.h"
#include "libs3.h"

/* Interface to Amazon S3 storage. */

typedef struct {
    GThreadPool *thread_pool;
    S3BucketContext bucket;
    uint8_t encryption_key[CRYPTO_KEY_SIZE];
} S3Store;

typedef struct {
    enum { S3_GET, S3_PUT } op;
    gchar *key;
    BlueSkyRCStr *data;
} S3Op;

struct get_info {
    int success;
    GString *buf;
};

struct put_info {
    int success;
    BlueSkyRCStr *val;
    gint offset;
};

struct list_info {
    int success;
    char *last_entry;
    gboolean truncated;
};

static S3Status s3store_get_handler(int bufferSize, const char *buffer,
                                    void *callbackData)
{
    struct get_info *info = (struct get_info *)callbackData;
    g_string_append_len(info->buf, buffer, bufferSize);
    return S3StatusOK;
}

static int s3store_put_handler(int bufferSize, char *buffer,
                               void *callbackData)
{
    struct put_info *info = (struct put_info *)callbackData;
    gint bytes = MIN(bufferSize, (int)(info->val->len - info->offset));
    memcpy(buffer, (char *)info->val->data + info->offset, bytes);
    info->offset += bytes;
    return bytes;
}

static S3Status s3store_properties_callback(const S3ResponseProperties *properties,
                                     void *callbackData)
{
    return S3StatusOK;
}

static void s3store_response_callback(S3Status status,
                               const S3ErrorDetails *errorDetails,
                               void *callbackData)
{
    struct get_info *info = (struct get_info *)callbackData;

    if (status == 0) {
        info->success = 1;
    }

    if (errorDetails != NULL && errorDetails->message != NULL) {
        g_print("  Error message: %s\n", errorDetails->message);
    }
}

static void s3store_task(gpointer a, gpointer s)
{
    BlueSkyStoreAsync *async = (BlueSkyStoreAsync *)a;
    S3Store *store = (S3Store *)s;

    async->status = ASYNC_RUNNING;
    async->exec_time = bluesky_now_hires();

    if (async->op == STORE_OP_GET) {
        struct get_info info;
        info.buf = g_string_new("");
        info.success = 0;

        struct S3GetObjectHandler handler;
        handler.responseHandler.propertiesCallback = s3store_properties_callback;
        handler.responseHandler.completeCallback = s3store_response_callback;
        handler.getObjectDataCallback = s3store_get_handler;

        S3_get_object(&store->bucket, async->key, NULL,
                      async->start, async->len, NULL, &handler, &info);
        async->range_done = TRUE;

        if (info.success) {
            async->data = bluesky_string_new_from_gstring(info.buf);
            async->result = 0;
        } else {
            g_string_free(info.buf, TRUE);
        }

    } else if (async->op == STORE_OP_PUT) {
        struct put_info info;
        info.success = 0;
        info.val = async->data;
        info.offset = 0;

        struct S3PutObjectHandler handler;
        handler.responseHandler.propertiesCallback
            = s3store_properties_callback;
        handler.responseHandler.completeCallback = s3store_response_callback;
        handler.putObjectDataCallback = s3store_put_handler;

        S3_put_object(&store->bucket, async->key, async->data->len, NULL, NULL,
                      &handler, &info);

        if (info.success) {
            async->result = 0;
        } else {
            g_warning("Error completing S3 put operation; client must retry!");
        }
    }

    bluesky_store_async_mark_complete(async);
    bluesky_store_async_unref(async);
}

static S3Status s3store_list_handler(int isTruncated,
                                     const char *nextMarker,
                                     int contentsCount,
                                     const S3ListBucketContent *contents,
                                     int commonPrefixesCount,
                                     const char **commonPrefixes,
                                     void *callbackData)
{
    struct list_info *info = (struct list_info *)callbackData;
    if (contentsCount > 0) {
        g_free(info->last_entry);
        info->last_entry = g_strdup(contents[contentsCount - 1].key);
    }
    info->truncated = isTruncated;
    return S3StatusOK;
}

static char *s3store_lookup_last(gpointer s, const char *prefix)
{
    S3Store *store = (S3Store *)s;
    struct list_info info = {0, NULL, FALSE};

    struct S3ListBucketHandler handler;
    handler.responseHandler.propertiesCallback
        = s3store_properties_callback;
    handler.responseHandler.completeCallback = s3store_response_callback;
    handler.listBucketCallback = s3store_list_handler;

    char *marker = NULL;

    do {
        S3_list_bucket(&store->bucket, prefix, marker, NULL, 1024, NULL,
                       &handler, &info);
        g_free(marker);
        marker = g_strdup(info.last_entry);
        g_print("Last key: %s\n", info.last_entry);
    } while (info.truncated);

    g_free(marker);

    return info.last_entry;
}

static gpointer s3store_new(const gchar *path)
{
    S3Store *store = g_new(S3Store, 1);
    store->thread_pool = g_thread_pool_new(s3store_task, store, -1, FALSE,
                                           NULL);
    if (path == NULL || strlen(path) == 0)
        store->bucket.bucketName = "mvrable-bluesky";
    else
        store->bucket.bucketName = g_strdup(path);
    store->bucket.protocol = S3ProtocolHTTP;
    store->bucket.uriStyle = S3UriStyleVirtualHost;
    store->bucket.accessKeyId = getenv("AWS_ACCESS_KEY_ID");
    store->bucket.secretAccessKey = getenv("AWS_SECRET_ACCESS_KEY");

    const char *key = getenv("BLUESKY_KEY");
    if (key == NULL) {
        g_error("Encryption key not defined; please set BLUESKY_KEY environment variable");
        exit(1);
    }

    bluesky_crypt_hash_key(key, store->encryption_key);

    g_print("Initializing S3 with bucket %s, access key %s, encryption key %s\n",
            store->bucket.bucketName, store->bucket.accessKeyId, key);

    return store;
}

static void s3store_destroy(gpointer store)
{
    g_free(store);
}

static void s3store_submit(gpointer s, BlueSkyStoreAsync *async)
{
    S3Store *store = (S3Store *)s;
    g_return_if_fail(async->status == ASYNC_NEW);
    g_return_if_fail(async->op != STORE_OP_NONE);

    switch (async->op) {
    case STORE_OP_GET:
    case STORE_OP_PUT:
        async->status = ASYNC_PENDING;
        bluesky_store_async_ref(async);
        g_thread_pool_push(store->thread_pool, async, NULL);
        break;

    default:
        g_warning("Uknown operation type for S3Store: %d\n", async->op);
        bluesky_store_async_mark_complete(async);
        break;
    }
}

static void s3store_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
    GString *buf = (GString *)async->store_private;

    if (buf != NULL) {
        g_string_free(buf, TRUE);
        async->store_private = NULL;
    }
}

static BlueSkyStoreImplementation store_impl = {
    .create = s3store_new,
    .destroy = s3store_destroy,
    .submit = s3store_submit,
    .cleanup = s3store_cleanup,
    .lookup_last = s3store_lookup_last,
};

void bluesky_store_init_s3(void)
{
    S3_initialize(NULL, S3_INIT_ALL, NULL);
    bluesky_store_register(&store_impl, "s3");
}
