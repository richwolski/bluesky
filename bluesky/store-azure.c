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
#include <gcrypt.h>
#include <curl/curl.h>

#include "bluesky-private.h"
#include "libs3.h"

#define AZURE_API_VERSION "2009-09-19"

/* Fixed headers that are used in calculating the request signature for Azure,
 * in the order that they are included. */
static const char *signature_headers[] = {
    "Content-Encoding", "Content-Language", "Content-Length", "Content-MD5",
    "Content-Type", "Date", "If-Modified-Since", "If-Match", "If-None-Match",
    "If-Unmodified-Since", "Range", NULL
};

/* Prototype Windows Azure backend for BlueSky.  This is intended to be
 * minimally functional, but could use additional work for production use. */

#define MAX_IDLE_CONNECTIONS 8

typedef struct {
    GThreadPool *thread_pool;
    char *account, *container;
    uint8_t *key;
    size_t key_len;

    /* A pool of available idle connections that could be used. */
    GQueue *curl_pool;
    GMutex *curl_pool_lock;
} AzureStore;

static CURL *get_connection(AzureStore *store)
{
    CURL *curl = NULL;

    g_mutex_lock(store->curl_pool_lock);
    if (!g_queue_is_empty(store->curl_pool)) {
        curl = (CURL *)(g_queue_pop_head(store->curl_pool));
    }
    g_mutex_unlock(store->curl_pool_lock);

    if (curl == NULL)
        curl = curl_easy_init();

    return curl;
}

static void put_connection(AzureStore *store, CURL *curl)
{
    g_mutex_lock(store->curl_pool_lock);
    g_queue_push_head(store->curl_pool, curl);
    while (g_queue_get_length(store->curl_pool) > MAX_IDLE_CONNECTIONS) {
        curl = (CURL *)(g_queue_pop_tail(store->curl_pool));
        curl_easy_cleanup(curl);
    }
    g_mutex_unlock(store->curl_pool_lock);
}

static void get_extra_headers(gchar *key, gchar *value, GList **headers)
{
    key = g_ascii_strdown(key, strlen(key));
    if (strncmp(key, "x-ms-", strlen("x-ms-")) == 0) {
        *headers = g_list_prepend(*headers,
                                  g_strdup_printf("%s:%s\n", key, value));
    }
    g_free(key);
}

static void get_curl_headers(gchar *key, gchar *value,
                             struct curl_slist **curl_headers)
{
    char *line = g_strdup_printf("%s: %s", key, value);
    *curl_headers = curl_slist_append(*curl_headers, line);
    g_free(line);
}

struct curlbuf {
    /* For reading */
    const char *readbuf;
    size_t readsize;
};

static size_t curl_readfunc(void *ptr, size_t size, size_t nmemb,
                            void *userdata)
{
    struct curlbuf *buf = (struct curlbuf *)userdata;

    if (buf == NULL)
        return 0;

    size_t copied = size * nmemb;
    if (copied > buf->readsize)
        copied = buf->readsize;

    memcpy(ptr, buf->readbuf, copied);
    buf->readbuf += copied;
    buf->readsize -= copied;

    return copied;
}

static size_t curl_writefunc(void *ptr, size_t size, size_t nmemb,
                             void *userdata)
{
    GString *buf = (GString *)userdata;
    if (buf != NULL)
        g_string_append_len(buf, ptr, size * nmemb);
    return size * nmemb;
}

/* Compute the signature for a request to Azure and add it to the headers. */
static void azure_compute_signature(AzureStore *store,
                                    GHashTable *headers,
                                    const char *method, const char *path)
{
    if (g_hash_table_lookup(headers, "Date") == NULL) {
        time_t t;
        struct tm now;
        char timebuf[4096];
        time(&t);
        gmtime_r(&t, &now);
        strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", &now);
        g_hash_table_insert(headers, g_strdup("Date"), g_strdup(timebuf));
    }

    g_hash_table_insert(headers, g_strdup("x-ms-version"),
                        g_strdup(AZURE_API_VERSION));

    GString *to_sign = g_string_new("");
    g_string_append_printf(to_sign, "%s\n", method);
    for (const char **h = signature_headers; *h != NULL; h++) {
        const char *val = g_hash_table_lookup(headers, *h);
        g_string_append_printf(to_sign, "%s\n", val ? val : "");
    }

    GList *extra_headers = NULL;
    g_hash_table_foreach(headers, (GHFunc)get_extra_headers, &extra_headers);
    extra_headers = g_list_sort(extra_headers, (GCompareFunc)g_strcmp0);
    while (extra_headers != NULL) {
        g_string_append(to_sign, extra_headers->data);
        g_free(extra_headers->data);
        extra_headers = g_list_delete_link(extra_headers, extra_headers);
    }

    /* FIXME: Doesn't handle query parameters (after '?') */
    g_string_append_printf(to_sign, "/%s/%s/%s",
                           store->account, store->container, path);

    /* Compute an HMAC-SHA-256 of the encoded parameters */
    gcry_error_t status;
    gcry_md_hd_t handle;
    status = gcry_md_open(&handle, GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC);
    g_assert(status == 0);
    status = gcry_md_setkey(handle, store->key, store->key_len);
    g_assert(status == 0);
    gcry_md_write(handle, to_sign->str, to_sign->len);
    unsigned char *digest = gcry_md_read(handle, GCRY_MD_SHA256);
    gchar *signature = g_base64_encode(digest,
                                       gcry_md_get_algo_dlen(GCRY_MD_SHA256));
    g_hash_table_insert(headers, g_strdup("Authorization"),
                        g_strdup_printf("SharedKey %s:%s",
                                        store->account, signature));
    g_free(signature);
    gcry_md_close(handle);
    g_string_free(to_sign, TRUE);
}

/* Submit an HTTP request using CURL.  Takes as input the Azure storage backend
 * we are acting for, as well as the method (GET, PUT, etc.), HTTP path, other
 * HTTP headers, and an optional body.  If body is not NULL, an empty body is
 * sent.  This will compute an Azure authentication signature before sending
 * the request. */
static BlueSkyRCStr *submit_request(AzureStore *store,
                                    CURL *curl,
                                    const char *method,
                                    const char *path,
                                    GHashTable *headers,
                                    BlueSkyRCStr *body)
{
    BlueSkyRCStr *result = NULL;

    g_hash_table_insert(headers,
                        g_strdup("Content-Length"),
                        g_strdup_printf("%zd", body != NULL ? body->len : 0));

    if (body != 0 && body->len > 0) {
        GChecksum *csum = g_checksum_new(G_CHECKSUM_MD5);
        g_checksum_update(csum, (uint8_t *)body->data, body->len);
        uint8_t md5[16];
        gsize md5_len = sizeof(md5);
        g_checksum_get_digest(csum, md5, &md5_len);
        g_hash_table_insert(headers,
                            g_strdup("Content-MD5"),
                            g_base64_encode(md5, md5_len));
        g_checksum_free(csum);
    }

    azure_compute_signature(store, headers, method, path);

    CURLcode status;

#define curl_easy_setopt_safe(opt, val)                                     \
    if ((status = curl_easy_setopt(curl, (opt), (val))) != CURLE_OK) {      \
        fprintf(stderr, "CURL error: %s!\n", curl_easy_strerror(status));   \
        goto cleanup;                                                       \
    }

    curl_easy_setopt_safe(CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt_safe(CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt_safe(CURLOPT_NETRC, CURL_NETRC_IGNORED);
    curl_easy_setopt_safe(CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt_safe(CURLOPT_MAXREDIRS, 10);

    curl_easy_setopt_safe(CURLOPT_HEADERFUNCTION, curl_writefunc);
    curl_easy_setopt_safe(CURLOPT_HEADERDATA, NULL);

    struct curlbuf readbuf;
    if (body != NULL) {
        readbuf.readbuf = body->data;
        readbuf.readsize = body->len;
    }
    curl_easy_setopt_safe(CURLOPT_READFUNCTION, curl_readfunc);
    curl_easy_setopt_safe(CURLOPT_READDATA, body ? &readbuf : NULL);

    GString *result_body = g_string_new("");
    curl_easy_setopt_safe(CURLOPT_WRITEFUNCTION, curl_writefunc);
    curl_easy_setopt_safe(CURLOPT_WRITEDATA, result_body);

    struct curl_slist *curl_headers = NULL;
    g_hash_table_foreach(headers, (GHFunc)get_curl_headers, &curl_headers);
    curl_easy_setopt_safe(CURLOPT_HTTPHEADER, curl_headers);

    char *uri = g_strdup_printf("http://%s.blob.core.windows.net/%s/%s",
                                store->account, store->container, path);
    printf("URI: %s\n", uri);
    curl_easy_setopt_safe(CURLOPT_URL, uri);

    if (strcmp(method, "GET") == 0) {
        /* nothing special needed */
    } else if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt_safe(CURLOPT_UPLOAD, 1);
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt_safe(CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    status = curl_easy_perform(curl);
    if (status != 0) {
        fprintf(stderr, "CURL error: %s!\n", curl_easy_strerror(status));
        goto cleanup;
    }

    long response_code = 0;
    status = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (status != 0) {
        fprintf(stderr, "CURL error: %s!\n", curl_easy_strerror(status));
        goto cleanup;
    }

    if (response_code / 100 == 2) {
        result = bluesky_string_new_from_gstring(result_body);
        result_body = NULL;
    } else {
        fprintf(stderr, "HTTP response code: %ld!\n", response_code);
        goto cleanup;
    }

cleanup:
    if (result != NULL && result_body != NULL)
        g_string_free(result_body, TRUE);
    curl_easy_reset(curl);
    curl_slist_free_all(curl_headers);
    g_free(uri);

    return result;
}

static void azurestore_task(gpointer a, gpointer s)
{
    BlueSkyStoreAsync *async = (BlueSkyStoreAsync *)a;
    AzureStore *store = (AzureStore *)s;

    async->status = ASYNC_RUNNING;
    async->exec_time = bluesky_now_hires();

    GHashTable *headers = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                g_free, g_free);

    BlueSkyRCStr *result = NULL;
    CURL *curl = get_connection(store);

    if (async->op == STORE_OP_GET) {
        /* FIXME: We ought to check that the response returned the requested
         * byte range. */
        if (async->start != 0 && async->len != 0) {
            g_hash_table_insert(headers,
                                g_strdup("Range"),
                                g_strdup_printf("bytes=%zd-%zd", async->start,
                                                async->start + async->len));
            async->range_done = TRUE;
        } else if (async->start != 0) {
            g_hash_table_insert(headers,
                                g_strdup("Range"),
                                g_strdup_printf("bytes=%zd-", async->start));
            async->range_done = TRUE;
        }
        result = submit_request(store, curl, "GET", async->key, headers, NULL);
        if (result != NULL) {
            async->data = result;
            async->result = 0;
        }
    } else if (async->op == STORE_OP_PUT) {
        g_hash_table_insert(headers,
                            g_strdup("x-ms-blob-type"),
                            g_strdup("BlockBlob"));
        g_hash_table_insert(headers,
                            g_strdup("Transfer-Encoding"),
                            g_strdup(""));
        result = submit_request(store, curl, "PUT", async->key,
                                headers, async->data);
        if (result != NULL) {
            async->result = 0;
        }
        bluesky_string_unref(result);
    }

    bluesky_store_async_mark_complete(async);
    bluesky_store_async_unref(async);
    g_hash_table_unref(headers);
    put_connection(store, curl);
}

static gpointer azurestore_new(const gchar *path)
{
    AzureStore *store = g_new(AzureStore, 1);
    store->thread_pool = g_thread_pool_new(azurestore_task, store, -1, FALSE,
                                           NULL);
    if (path == NULL || strlen(path) == 0)
        store->container = g_strdup("bluesky");
    else
        store->container = g_strdup(path);

    store->account = g_strdup(getenv("AZURE_ACCOUNT_NAME"));

    const char *key = getenv("AZURE_SECRET_KEY");
    store->key = g_base64_decode(key, &store->key_len);

    g_print("Initializing Azure with account %s, container %s\n",
            store->account, store->container);

    store->curl_pool = g_queue_new();
    store->curl_pool_lock = g_mutex_new();

    return store;
}

static void azurestore_destroy(gpointer store)
{
    /* TODO: Clean up resources */
}

static void azurestore_submit(gpointer s, BlueSkyStoreAsync *async)
{
    AzureStore *store = (AzureStore *)s;
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
        g_warning("Uknown operation type for AzureStore: %d\n", async->op);
        bluesky_store_async_mark_complete(async);
        break;
    }
}

static void azurestore_cleanup(gpointer store, BlueSkyStoreAsync *async)
{
}

static char *azurestore_lookup_last(gpointer s, const char *prefix)
{
    return NULL;
}

static BlueSkyStoreImplementation store_impl = {
    .create = azurestore_new,
    .destroy = azurestore_destroy,
    .submit = azurestore_submit,
    .cleanup = azurestore_cleanup,
    .lookup_last = azurestore_lookup_last,
};

void bluesky_store_init_azure(void)
{
    bluesky_store_register(&store_impl, "azure");
}
