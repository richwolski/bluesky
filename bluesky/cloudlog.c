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

#include <stdio.h>
#include <stdint.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

// Rough size limit for a log segment.  This is not a firm limit and there are
// no absolute guarantees on the size of a log segment.
#define CLOUDLOG_SEGMENT_SIZE (4 << 20)

// Maximum number of segments to attempt to upload concurrently
int cloudlog_concurrent_uploads = 32;

BlueSkyCloudID bluesky_cloudlog_new_id()
{
    BlueSkyCloudID id;
    bluesky_crypt_random_bytes((uint8_t *)&id.bytes, sizeof(id));
    return id;
}

gchar *bluesky_cloudlog_id_to_string(BlueSkyCloudID id)
{
    char buf[sizeof(BlueSkyCloudID) * 2 + 1];
    buf[0] = '\0';

    for (int i = 0; i < sizeof(BlueSkyCloudID); i++) {
        sprintf(&buf[2*i], "%02x", (uint8_t)(id.bytes[i]));
    }

    return g_strdup(buf);
}

BlueSkyCloudID bluesky_cloudlog_id_from_string(const gchar *idstr)
{
    BlueSkyCloudID id;
    memset(&id, 0, sizeof(id));
    for (int i = 0; i < 2*sizeof(BlueSkyCloudID); i++) {
        char c = idstr[i];
        if (c == '\0') {
            g_warning("Short cloud id: %s\n", idstr);
            break;
        }
        int val = 0;
        if (c >= '0' && c <= '9')
            val = c - '0';
        else if (c >= 'a' && c <= 'f')
            val = c - 'a' + 10;
        else
            g_warning("Bad character in cloud id: %s\n", idstr);
        id.bytes[i / 2] += val << (i % 2 ? 0 : 4);
    }
    return id;
}

gboolean bluesky_cloudlog_equal(gconstpointer a, gconstpointer b)
{
    BlueSkyCloudID *id1 = (BlueSkyCloudID *)a, *id2 = (BlueSkyCloudID *)b;

    return memcmp(id1, id2, sizeof(BlueSkyCloudID)) == 0;
}

guint bluesky_cloudlog_hash(gconstpointer a)
{
    BlueSkyCloudID *id = (BlueSkyCloudID *)a;

    // Assume that bits in the ID are randomly chosen so that any subset of the
    // bits can be used as a hash key.
    return *(guint *)(&id->bytes);
}

/* Formatting of cloud log segments.  This handles grouping items together
 * before writing a batch to the cloud, handling indirection through items like
 * the inode map, etc. */

BlueSkyCloudLog *bluesky_cloudlog_new(BlueSkyFS *fs, const BlueSkyCloudID *id)
{
    BlueSkyCloudLog *log = g_new0(BlueSkyCloudLog, 1);

    log->lock = g_mutex_new();
    log->cond = g_cond_new();
    log->fs = fs;
    log->type = LOGTYPE_UNKNOWN;
    if (id != NULL)
        memcpy(&log->id, id, sizeof(BlueSkyCloudID));
    else
        log->id = bluesky_cloudlog_new_id();
    log->links = g_array_new(FALSE, TRUE, sizeof(BlueSkyCloudLog *));
    g_atomic_int_set(&log->refcount, 1);

    return log;
}

/* Helper function for updating memory usage statistics for a filesystem (the
 * cache_log_* variables).  This will increment (type=1) or decrement (type=-1)
 * the counter associated with the current state of the cloud log item.  The
 * item should be locked or otherwise protected from concurrent access. */
void bluesky_cloudlog_stats_update(BlueSkyCloudLog *log, int type)
{
    BlueSkyFS *fs = log->fs;

    if (log->location_flags & CLOUDLOG_CLOUD) {
        g_atomic_int_add(&fs->cache_log_cloud, type);
    } else if (log->location_flags & CLOUDLOG_JOURNAL) {
        g_atomic_int_add(&fs->cache_log_journal, type);
    } else if (log->pending_write & CLOUDLOG_JOURNAL) {
        g_atomic_int_add(&fs->cache_log_journal, type);
    } else if (log->data != NULL) {
        g_atomic_int_add(&fs->cache_log_dirty, type);
    }
}

/* The reference held by the hash table does not count towards the reference
 * count.  When a new object is created, it initially has a reference count of
 * 1 for the creator, and similarly fetching an item from the hash table will
 * also create a reference.  If the reference count drops to zero,
 * bluesky_cloudlog_unref attempts to remove the object from the hash
 * table--but there is a potential race since another thread might read the
 * object from the hash table at the same time.  So an object with a reference
 * count of zero may still be resurrected, in which case we need to abort the
 * destruction.  Once the object is gone from the hash table, and if the
 * reference count is still zero, it can actually be deleted. */
void bluesky_cloudlog_ref(BlueSkyCloudLog *log)
{
    if (log == NULL)
        return;

    g_atomic_int_inc(&log->refcount);
}

void bluesky_cloudlog_unref(BlueSkyCloudLog *log)
{
    if (log == NULL)
        return;

    if (g_atomic_int_dec_and_test(&log->refcount)) {
        BlueSkyFS *fs = log->fs;

        g_mutex_lock(fs->lock);
        if (g_atomic_int_get(&log->refcount) > 0) {
            g_mutex_unlock(fs->lock);
            return;
        }

        if (!g_hash_table_remove(fs->locations, &log->id)) {
            if (bluesky_verbose)
                g_warning("Could not find and remove cloud log item from hash table!");
        }
        g_mutex_unlock(fs->lock);

        bluesky_cloudlog_stats_update(log, -1);
        log->type = LOGTYPE_INVALID;
        g_mutex_free(log->lock);
        g_cond_free(log->cond);
        for (int i = 0; i < log->links->len; i++) {
            BlueSkyCloudLog *c = g_array_index(log->links,
                                               BlueSkyCloudLog *, i);
            bluesky_cloudlog_unref(c);
        }
        g_array_unref(log->links);
        bluesky_string_unref(log->data);
        g_free(log);
    }
}

/* For locking reasons cloudlog unrefs may sometimes need to be performed in
 * the future.  We launch a thread for handling these delayed unreference
 * requests. */
static gpointer cloudlog_unref_thread(gpointer q)
{
    GAsyncQueue *queue = (GAsyncQueue *)q;

    while (TRUE) {
        BlueSkyCloudLog *item = (BlueSkyCloudLog *)g_async_queue_pop(queue);
        bluesky_cloudlog_unref(item);
    }

    return NULL;
}

void bluesky_cloudlog_unref_delayed(BlueSkyCloudLog *log)
{
    if (log != NULL)
        g_async_queue_push(log->fs->unref_queue, log);
}

/* Erase the information contained within the in-memory cloud log
 * representation.  This does not free up the item itself, but frees the data
 * and references to other log items and resets the type back to unknown.  If
 * the object was written out to persistent storage, all state about it can be
 * recovered by loading the object back in.  The object must be locked before
 * calling this function. */
void bluesky_cloudlog_erase(BlueSkyCloudLog *log)
{
    g_assert(log->data_lock_count == 0);

    if (log->type == LOGTYPE_UNKNOWN)
        return;

    log->type = LOGTYPE_UNKNOWN;
    log->data_size = 0;
    bluesky_string_unref(log->data);
    log->data = NULL;
    log->data_lock_count = 0;

    for (int i = 0; i < log->links->len; i++) {
        BlueSkyCloudLog *c = g_array_index(log->links,
                                           BlueSkyCloudLog *, i);
        bluesky_cloudlog_unref(c);
    }
    g_array_unref(log->links);
    log->links = g_array_new(FALSE, TRUE, sizeof(BlueSkyCloudLog *));
}

/* Start a write of the object to the local log. */
void bluesky_cloudlog_sync(BlueSkyCloudLog *log)
{
    bluesky_log_item_submit(log, log->fs->log);
}

/* Add the given entry to the global hash table containing cloud log entries.
 * Takes ownership of the caller's reference. */
void bluesky_cloudlog_insert_locked(BlueSkyCloudLog *log)
{
    g_hash_table_insert(log->fs->locations, &log->id, log);
}

void bluesky_cloudlog_insert(BlueSkyCloudLog *log)
{
    g_mutex_lock(log->fs->lock);
    bluesky_cloudlog_insert_locked(log);
    g_mutex_unlock(log->fs->lock);
}

/* Look up the cloud log entry for the given ID.  If create is TRUE and the
 * item does not exist, create a special pending entry that can later be filled
 * in when the real item is loaded.  The returned item has a reference held.
 * As a special case, if a null ID is provided then NULL is returned. */
BlueSkyCloudLog *bluesky_cloudlog_get(BlueSkyFS *fs, BlueSkyCloudID id)
{
    static BlueSkyCloudID id0 = {{0}};

    if (memcmp(&id, &id0, sizeof(BlueSkyCloudID)) == 0)
        return NULL;

    g_mutex_lock(fs->lock);
    BlueSkyCloudLog *item;
    item = g_hash_table_lookup(fs->locations, &id);
    if (item == NULL) {
        item = bluesky_cloudlog_new(fs, &id);
        bluesky_cloudlog_stats_update(item, 1);
        bluesky_cloudlog_insert_locked(item);
    } else {
        bluesky_cloudlog_ref(item);
    }
    g_mutex_unlock(fs->lock);
    return item;
}

/* Work to fetch a cloudlog item in a background thread.  The item will be
 * locked while the fetch is in progress and unlocked when it completes. */
static GThreadPool *fetch_pool;

static void background_fetch_task(gpointer p, gpointer unused)
{
    BlueSkyCloudLog *item = (BlueSkyCloudLog *)p;

    g_mutex_lock(item->lock);
    g_mutex_unlock(item->lock);
    bluesky_cloudlog_unref(item);
}

void bluesky_cloudlog_background_fetch(BlueSkyCloudLog *item)
{
    bluesky_cloudlog_ref(item);
    g_thread_pool_push(fetch_pool, item, NULL);
}

/* Attempt to prefetch a cloud log item.  This does not guarantee that it will
 * be made available, but does make it more likely that a future call to
 * bluesky_cloudlog_fetch will complete quickly.  Item must be locked? */
void bluesky_cloudlog_prefetch(BlueSkyCloudLog *item)
{
    if (item->data != NULL)
        return;

    /* When operating in a non log-structured mode, simply start a background
     * fetch immediately when asked to prefetch. */
    if (bluesky_options.disable_aggregation
        || bluesky_options.disable_read_aggregation) {
        bluesky_cloudlog_background_fetch(item);
        return;
    }

    /* TODO: Some of the code here is duplicated with bluesky_log_map_object.
     * Refactor to fix that. */
    BlueSkyFS *fs = item->fs;
    BlueSkyCacheFile *map = NULL;

    /* First, check to see if the journal still contains a copy of the item and
     * if so update the atime on the journal so it is likely to be kept around
     * until we need it. */
    if ((item->location_flags | item->pending_write) & CLOUDLOG_JOURNAL) {
        map = bluesky_cachefile_lookup(fs, -1, item->log_seq, TRUE);
        if (map != NULL) {
            map->atime = bluesky_get_current_time();
            bluesky_cachefile_unref(map);
            g_mutex_unlock(map->lock);
            return;
        }
    }

    item->location_flags &= ~CLOUDLOG_JOURNAL;
    if (!(item->location_flags & CLOUDLOG_CLOUD))
        return;

    map = bluesky_cachefile_lookup(fs,
                                   item->location.directory,
                                   item->location.sequence,
                                   FALSE);
    if (map == NULL)
        return;

    /* At this point, we have information about the log segment containing the
     * item we need.  If our item is already fetched, we have nothing to do
     * except update the atime.  If not, queue up a fetch of our object. */
    const BlueSkyRangesetItem *rangeitem;
    rangeitem = bluesky_rangeset_lookup(map->items,
                                        item->location.offset);
    if (rangeitem == NULL) {
        if (map->prefetches == NULL)
            map->prefetches = bluesky_rangeset_new();

        gchar *id = bluesky_cloudlog_id_to_string(item->id);
        if (bluesky_verbose)
            g_print("Need to prefetch %s\n", id);
        g_free(id);

        bluesky_rangeset_insert(map->prefetches,
                                item->location.offset,
                                item->location.size, NULL);

        uint64_t start, length;
        bluesky_rangeset_get_extents(map->prefetches, &start, &length);
        if (bluesky_verbose)
            g_print("Range to prefetch: %"PRIu64" + %"PRIu64"\n",
                    start, length);
    }

    bluesky_cachefile_unref(map);
    g_mutex_unlock(map->lock);
}

/* Ensure that a cloud log item is loaded in memory, and if not read it in.
 * TODO: Make asynchronous, and make this also fetch from the cloud.  Right now
 * we only read from the log.  Log item must be locked. */
void bluesky_cloudlog_fetch(BlueSkyCloudLog *log)
{
    if (log->data != NULL)
        return;

    BlueSkyProfile *profile = bluesky_profile_get();
    if (profile != NULL)
        bluesky_profile_add_event(profile, g_strdup_printf("Fetch log entry"));

    /* There are actually two cases: a full deserialization if we have not ever
     * read the object before, and a partial deserialization where the metadata
     * is already in memory and we just need to remap the data.  If the object
     * type has not yet been set, we'll need to read and parse the metadata.
     * Once that is done, we can fall through the case of remapping the data
     * itself. */
    if (log->type == LOGTYPE_UNKNOWN) {
        BlueSkyRCStr *raw = bluesky_log_map_object(log, FALSE);
        g_assert(raw != NULL);
        bluesky_deserialize_cloudlog(log, raw->data, raw->len);
        bluesky_string_unref(raw);
    }

    /* At this point all metadata should be available and we need only remap
     * the object data. */
    log->data = bluesky_log_map_object(log, TRUE);

    if (log->data == NULL) {
        g_error("Unable to fetch cloudlog entry!");
    }

    if (profile != NULL)
        bluesky_profile_add_event(profile, g_strdup_printf("Fetch complete"));
    g_cond_broadcast(log->cond);
}

BlueSkyCloudPointer bluesky_cloudlog_serialize(BlueSkyCloudLog *log,
                                               BlueSkyFS *fs)
{
    BlueSkyCloudLogState *state = fs->log_state;

    if ((log->location_flags | log->pending_write) & CLOUDLOG_CLOUD) {
        return log->location;
    }

    for (int i = 0; i < log->links->len; i++) {
        BlueSkyCloudLog *ref = g_array_index(log->links,
                                             BlueSkyCloudLog *, i);
        if (ref != NULL)
            bluesky_cloudlog_serialize(ref, fs);
    }

    /* FIXME: Ought lock to be taken earlier? */
    g_mutex_lock(log->lock);
    bluesky_cloudlog_fetch(log);
    g_assert(log->data != NULL);

    bluesky_cloudlog_stats_update(log, -1);

    GString *data1 = g_string_new("");
    GString *data2 = g_string_new("");
    GString *data3 = g_string_new("");
    bluesky_serialize_cloudlog(log, data1, data2, data3);

    log->location = state->location;
    log->location.offset = state->data->len;
    log->data_size = data1->len;

    struct cloudlog_header header;
    memcpy(header.magic, CLOUDLOG_MAGIC, 4);
    memset(header.crypt_auth, sizeof(header.crypt_auth), 0);
    memset(header.crypt_iv, sizeof(header.crypt_iv), 0);
    header.type = log->type + '0';
    header.size1 = GUINT32_TO_LE(data1->len);
    header.size2 = GUINT32_TO_LE(data2->len);
    header.size3 = GUINT32_TO_LE(data3->len);
    header.id = log->id;
    header.inum = GUINT64_TO_LE(log->inum);

    g_string_append_len(state->data, (const char *)&header, sizeof(header));
    g_string_append_len(state->data, data1->str, data1->len);
    g_string_append_len(state->data, data2->str, data2->len);
    g_string_append_len(state->data, data3->str, data3->len);

    log->location.size = state->data->len - log->location.offset;

    g_string_free(data1, TRUE);
    g_string_free(data2, TRUE);
    g_string_free(data3, TRUE);

    /* If the object we flushed was an inode, update the inode map. */
    if (log->type == LOGTYPE_INODE) {
        g_mutex_lock(fs->lock);
        InodeMapEntry *entry = bluesky_inode_map_lookup(fs->inode_map,
                                                        log->inum, 1);
        bluesky_cloudlog_unref_delayed(entry->item);
        entry->item = log;
        bluesky_cloudlog_ref(entry->item);
        g_mutex_unlock(fs->lock);
    }

    /* TODO: We should mark the objects as committed on the cloud until the
     * data is flushed and acknowledged. */
    log->pending_write |= CLOUDLOG_CLOUD;
    bluesky_cloudlog_stats_update(log, 1);
    state->writeback_list = g_slist_prepend(state->writeback_list, log);
    bluesky_cloudlog_ref(log);
    g_mutex_unlock(log->lock);

    if (state->data->len > CLOUDLOG_SEGMENT_SIZE
        || bluesky_options.disable_aggregation)
    {
        bluesky_cloudlog_flush(fs);
    }

    return log->location;
}

static void cloudlog_flush_complete(BlueSkyStoreAsync *async,
                                    SerializedRecord *record)
{
    g_print("Write of %s to cloud complete, status = %d\n",
            async->key, async->result);

    g_mutex_lock(record->lock);
    if (async->result >= 0) {
        while (record->items != NULL) {
            BlueSkyCloudLog *item = (BlueSkyCloudLog *)record->items->data;
            g_mutex_lock(item->lock);
            bluesky_cloudlog_stats_update(item, -1);
            item->pending_write &= ~CLOUDLOG_CLOUD;
            item->location_flags |= CLOUDLOG_CLOUD;
            bluesky_cloudlog_stats_update(item, 1);
            g_mutex_unlock(item->lock);
            bluesky_cloudlog_unref(item);

            record->items = g_slist_delete_link(record->items, record->items);
        }

        bluesky_string_unref(record->data);
        record->data = NULL;
        g_slist_free(record->items);
        record->items = NULL;
        record->complete = TRUE;

        BlueSkyCloudLogState *state = record->fs->log_state;
        g_mutex_lock(state->uploads_pending_lock);
        state->uploads_pending--;
        g_cond_broadcast(state->uploads_pending_cond);
        g_mutex_unlock(state->uploads_pending_lock);

        g_cond_broadcast(record->cond);
    } else {
        g_print("Write should be resubmitted...\n");

        BlueSkyStoreAsync *async2 = bluesky_store_async_new(async->store);
        async2->op = STORE_OP_PUT;
        async2->key = g_strdup(async->key);
        async2->data = record->data;
        async2->profile = async->profile;
        bluesky_string_ref(record->data);
        bluesky_store_async_submit(async2);
        bluesky_store_async_add_notifier(async2,
                                         (GFunc)cloudlog_flush_complete,
                                         record);
        bluesky_store_async_unref(async2);
    }
    g_mutex_unlock(record->lock);
}

/* Finish up a partially-written cloud log segment and flush it to storage. */
static void cloud_flush_background(SerializedRecord *record)
{
    bluesky_cloudlog_encrypt(record->raw_data, record->fs->keys);
    record->data = bluesky_string_new_from_gstring(record->raw_data);
    record->raw_data = NULL;

    BlueSkyStoreAsync *async = bluesky_store_async_new(record->fs->store);
    async->op = STORE_OP_PUT;
    async->key = record->key;
    async->data = record->data;
    bluesky_string_ref(record->data);
    bluesky_store_async_submit(async);
    bluesky_store_async_add_notifier(async,
                                     (GFunc)cloudlog_flush_complete,
                                     record);
    bluesky_store_async_unref(async);
}

void bluesky_cloudlog_flush(BlueSkyFS *fs)
{
    BlueSkyCloudLogState *state = fs->log_state;
    if (state->data == NULL || state->data->len == 0)
        return;

    g_mutex_lock(state->uploads_pending_lock);
    while (state->uploads_pending > cloudlog_concurrent_uploads)
        g_cond_wait(state->uploads_pending_cond, state->uploads_pending_lock);
    state->uploads_pending++;
    g_mutex_unlock(state->uploads_pending_lock);

    /* TODO: Append some type of commit record to the log segment? */

    g_print("Serializing %zd bytes of data to cloud\n", state->data->len);
    SerializedRecord *record = g_new0(SerializedRecord, 1);
    record->fs = fs;
    record->raw_data = state->data;
    record->data = NULL;
    record->items = state->writeback_list;
    record->lock = g_mutex_new();
    record->cond = g_cond_new();
    state->writeback_list = NULL;

    record->key = g_strdup_printf("log-%08d-%08d",
                                  state->location.directory,
                                  state->location.sequence);

    state->pending_segments = g_list_prepend(state->pending_segments, record);

    /* Encryption of data and upload happen in the background, for additional
     * parallelism when uploading large amounts of data. */
    g_thread_create((GThreadFunc)cloud_flush_background, record, FALSE, NULL);

    state->location.sequence++;
    state->location.offset = 0;
    state->data = g_string_new("");
}

/* Make an encryption pass over a cloud log segment to encrypt private data in
 * it. */
void bluesky_cloudlog_encrypt(GString *segment, BlueSkyCryptKeys *keys)
{
    char *data = segment->str;
    size_t remaining_size = segment->len;

    while (remaining_size >= sizeof(struct cloudlog_header)) {
        struct cloudlog_header *header = (struct cloudlog_header *)data;
        size_t item_size = sizeof(struct cloudlog_header)
                           + GUINT32_FROM_LE(header->size1)
                           + GUINT32_FROM_LE(header->size2)
                           + GUINT32_FROM_LE(header->size3);
        if (item_size > remaining_size)
            break;
        bluesky_crypt_block_encrypt(data, item_size, keys);

        data += item_size;
        remaining_size -= item_size;
    }
}

/* Make an decryption pass over a cloud log segment to decrypt items which were
 * encrypted.  Also computes a list of all offsets which at which valid
 * cloud log items are found and adds those offsets to items (if non-NULL).
 *
 * If allow_unauth is set to true, then allow a limited set of unauthenticated
 * items that may have been rewritten by a file system cleaner.  These include
 * the checkpoint and inode map records only; other items must still pass
 * authentication. */
void bluesky_cloudlog_decrypt(char *segment, size_t len,
                              BlueSkyCryptKeys *keys,
                              BlueSkyRangeset *items,
                              gboolean allow_unauth)
{
    char *data = segment;
    size_t remaining_size = len;
    size_t offset = 0;

    while (remaining_size >= sizeof(struct cloudlog_header)) {
        struct cloudlog_header *header = (struct cloudlog_header *)data;
        size_t item_size = sizeof(struct cloudlog_header)
                           + GUINT32_FROM_LE(header->size1)
                           + GUINT32_FROM_LE(header->size2)
                           + GUINT32_FROM_LE(header->size3);
        if (item_size > remaining_size)
            break;
        if (bluesky_crypt_block_decrypt(data, item_size, keys, allow_unauth)) {
            if (items != NULL) {
                if (bluesky_verbose)
                    g_print("  data item at %zx\n", offset);
                bluesky_rangeset_insert(items, offset, item_size,
                                        GINT_TO_POINTER(TRUE));
            }
        } else {
            g_warning("Unauthenticated data at offset %zd", offset);
            if (items != NULL) {
                bluesky_rangeset_insert(items, offset, item_size,
                                        GINT_TO_POINTER(TRUE));
            }
        }

        data += item_size;
        offset += item_size;
        remaining_size -= item_size;
    }
}

void bluesky_cloudlog_threads_init(BlueSkyFS *fs)
{
    fs->unref_queue = g_async_queue_new();
    g_thread_create(cloudlog_unref_thread, fs->unref_queue, FALSE, NULL);
    fetch_pool = g_thread_pool_new(background_fetch_task, NULL, 40, FALSE,
                                   NULL);
}
