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

#define _GNU_SOURCE
#define _ATFILE_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "bluesky-private.h"

/* The logging layer for BlueSky.  This is used to write filesystem changes
 * durably to disk so that they can be recovered in the event of a system
 * crash. */

/* The logging layer takes care out writing out a sequence of log records to
 * disk.  On disk, each record consists of a header, a data payload, and a
 * footer.  The footer contains a checksum of the record, meant to help with
 * identifying corrupt log records (we would assume because the log record was
 * only incompletely written out before a crash, which should only happen for
 * log records that were not considered committed). */

#define HEADER_MAGIC 0x676f4c0a
#define FOOTER_MAGIC 0x2e435243

static size_t readbuf(int fd, char *buf, size_t len)
{
    size_t total_bytes = 0;
    while (len > 0) {
        ssize_t bytes = read(fd, buf, len);
        if (bytes < 0 && errno == EINTR)
            continue;
        g_assert(bytes >= 0);
        if (bytes == 0)
            break;
        buf += bytes;
        len -= bytes;
    }
    return total_bytes;
}

static void writebuf(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t written;
        written = write(fd, buf, len);
        if (written < 0 && errno == EINTR)
            continue;
        g_assert(written >= 0);
        buf += written;
        len -= written;
    }
}

static void log_commit(BlueSkyLog *log)
{
    int batchsize = 0;

    if (log->fd < 0)
        return;

    fdatasync(log->fd);

    /* Update disk-space usage statistics for the journal file. */
    g_atomic_int_add(&log->disk_used, -log->current_log->disk_used);
    struct stat statbuf;
    if (fstat(log->fd, &statbuf) >= 0) {
        /* Convert from 512-byte blocks to 1-kB units */
        log->current_log->disk_used = (statbuf.st_blocks + 1) / 2;
    }
    g_atomic_int_add(&log->disk_used, log->current_log->disk_used);

    while (log->committed != NULL) {
        BlueSkyCloudLog *item = (BlueSkyCloudLog *)log->committed->data;
        g_mutex_lock(item->lock);
        bluesky_cloudlog_stats_update(item, -1);
        item->pending_write &= ~CLOUDLOG_JOURNAL;
        item->location_flags
            = (item->location_flags & ~CLOUDLOG_UNCOMMITTED) | CLOUDLOG_JOURNAL;
        bluesky_cloudlog_stats_update(item, 1);
        g_cond_signal(item->cond);
        g_mutex_unlock(item->lock);
        log->committed = g_slist_delete_link(log->committed, log->committed);
        bluesky_cloudlog_unref(item);
        batchsize++;
    }

    if (bluesky_verbose && batchsize > 1)
        g_print("Log batch size: %d\n", batchsize);
}

static gboolean log_open(BlueSkyLog *log)
{
    char logname[64];

    if (log->fd >= 0) {
        log_commit(log);
        close(log->fd);
        log->seq_num++;
        log->fd = -1;
    }

    if (log->current_log != NULL) {
        bluesky_cachefile_unref(log->current_log);
        log->current_log = NULL;
    }

    while (log->fd < 0) {
        g_snprintf(logname, sizeof(logname), "journal-%08d", log->seq_num);
        log->fd = openat(log->dirfd, logname, O_CREAT|O_WRONLY|O_EXCL, 0600);
        if (log->fd < 0 && errno == EEXIST) {
            fprintf(stderr, "Log file %s already exists...\n", logname);
            log->seq_num++;
            continue;
        } else if (log->fd < 0) {
            fprintf(stderr, "Error opening logfile %s: %m\n", logname);
            return FALSE;
        }
    }

    log->current_log = bluesky_cachefile_lookup(log->fs, -1, log->seq_num,
                                                FALSE);
    g_assert(log->current_log != NULL);
    g_mutex_unlock(log->current_log->lock);

    if (ftruncate(log->fd, LOG_SEGMENT_SIZE) < 0) {
        fprintf(stderr, "Unable to truncate logfile %s: %m\n", logname);
    }
    fsync(log->fd);
    fsync(log->dirfd);
    return TRUE;
}

/* All log writes (at least for a single log) are made by one thread, so we
 * don't need to worry about concurrent access to the log file.  Log items to
 * write are pulled off a queue (and so may be posted by any thread).
 * fdatasync() is used to ensure the log items are stable on disk.
 *
 * The log is broken up into separate files, roughly of size LOG_SEGMENT_SIZE
 * each.  If a log segment is not currently open (log->fd is negative), a new
 * one is created.  Log segment filenames are assigned sequentially.
 *
 * Log replay ought to be implemented later, and ought to set the initial
 * sequence number appropriately.
 */
static gpointer log_thread(gpointer d)
{
    BlueSkyLog *log = (BlueSkyLog *)d;

    while (TRUE) {
        if (log->fd < 0) {
            if (!log_open(log)) {
                return NULL;
            }
        }

        BlueSkyCloudLog *item
            = (BlueSkyCloudLog *)g_async_queue_pop(log->queue);
        g_mutex_lock(item->lock);
        g_assert(item->data != NULL);

        /* The item may have already been written to the journal... */
        if ((item->location_flags | item->pending_write) & CLOUDLOG_JOURNAL) {
            g_mutex_unlock(item->lock);
            bluesky_cloudlog_unref(item);
            g_atomic_int_add(&item->data_lock_count, -1);
            continue;
        }

        bluesky_cloudlog_stats_update(item, -1);
        item->pending_write |= CLOUDLOG_JOURNAL;
        bluesky_cloudlog_stats_update(item, 1);

        GString *data1 = g_string_new("");
        GString *data2 = g_string_new("");
        GString *data3 = g_string_new("");
        bluesky_serialize_cloudlog(item, data1, data2, data3);

        struct log_header header;
        struct log_footer footer;
        size_t size = sizeof(header) + sizeof(footer);
        size += data1->len + data2->len + data3->len;
        off_t offset = 0;
        if (log->fd >= 0)
            offset = lseek(log->fd, 0, SEEK_CUR);

        /* Check whether the item would overflow the allocated journal size.
         * If so, start a new log segment.  We only allow oversized log
         * segments if they contain a single log entry. */
        if (offset + size >= LOG_SEGMENT_SIZE && offset > 0) {
            log_open(log);
            offset = 0;
        }

        header.magic = GUINT32_TO_LE(HEADER_MAGIC);
        header.offset = GUINT32_TO_LE(offset);
        header.size1 = GUINT32_TO_LE(data1->len);
        header.size2 = GUINT32_TO_LE(data2->len);
        header.size3 = GUINT32_TO_LE(data3->len);
        header.type = item->type + '0';
        header.id = item->id;
        header.inum = GUINT64_TO_LE(item->inum);
        footer.magic = GUINT32_TO_LE(FOOTER_MAGIC);

        uint32_t crc = BLUESKY_CRC32C_SEED;

        writebuf(log->fd, (const char *)&header, sizeof(header));
        crc = crc32c(crc, (const char *)&header, sizeof(header));

        writebuf(log->fd, data1->str, data1->len);
        crc = crc32c(crc, data1->str, data1->len);
        writebuf(log->fd, data2->str, data2->len);
        crc = crc32c(crc, data2->str, data2->len);
        writebuf(log->fd, data3->str, data3->len);
        crc = crc32c(crc, data3->str, data3->len);

        crc = crc32c(crc, (const char *)&footer,
                     sizeof(footer) - sizeof(uint32_t));
        footer.crc = crc32c_finalize(crc);
        writebuf(log->fd, (const char *)&footer, sizeof(footer));

        item->log_seq = log->seq_num;
        item->log_offset = offset;
        item->log_size = size;
        item->data_size = item->data->len;

        offset += size;

        g_string_free(data1, TRUE);
        g_string_free(data2, TRUE);
        g_string_free(data3, TRUE);

        /* Replace the log item's string data with a memory-mapped copy of the
         * data, now that it has been written to the log file.  (Even if it
         * isn't yet on disk, it should at least be in the page cache and so
         * available to memory map.) */
        bluesky_string_unref(item->data);
        item->data = NULL;
        bluesky_cloudlog_fetch(item);

        log->committed = g_slist_prepend(log->committed, item);
        g_atomic_int_add(&item->data_lock_count, -1);
        g_mutex_unlock(item->lock);

        /* Force an if there are no other log items currently waiting to be
         * written. */
        if (g_async_queue_length(log->queue) <= 0)
            log_commit(log);
    }

    return NULL;
}

BlueSkyLog *bluesky_log_new(const char *log_directory)
{
    BlueSkyLog *log = g_new0(BlueSkyLog, 1);

    log->log_directory = g_strdup(log_directory);
    log->fd = -1;
    log->seq_num = 0;
    log->queue = g_async_queue_new();
    log->mmap_lock = g_mutex_new();
    log->mmap_cache = g_hash_table_new(g_str_hash, g_str_equal);

    /* Determine the highest-numbered log file, so that we can start writing
     * out new journal entries at the next sequence number. */
    GDir *dir = g_dir_open(log_directory, 0, NULL);
    if (dir != NULL) {
        const gchar *file;
        while ((file = g_dir_read_name(dir)) != NULL) {
            if (strncmp(file, "journal-", 8) == 0) {
                log->seq_num = MAX(log->seq_num, atoi(&file[8]) + 1);
            }
        }
        g_dir_close(dir);
        g_print("Starting journal at sequence number %d\n", log->seq_num);
    }

    log->dirfd = open(log->log_directory, O_DIRECTORY);
    if (log->dirfd < 0) {
        fprintf(stderr, "Unable to open logging directory: %m\n");
        return NULL;
    }

    g_thread_create(log_thread, log, FALSE, NULL);

    return log;
}

void bluesky_log_item_submit(BlueSkyCloudLog *item, BlueSkyLog *log)
{
    if (!(item->location_flags & CLOUDLOG_JOURNAL)) {
        bluesky_cloudlog_ref(item);
        item->location_flags |= CLOUDLOG_UNCOMMITTED;
        g_atomic_int_add(&item->data_lock_count, 1);
        g_async_queue_push(log->queue, item);
    }
}

void bluesky_log_finish_all(GList *log_items)
{
    while (log_items != NULL) {
        BlueSkyCloudLog *item = (BlueSkyCloudLog *)log_items->data;

        g_mutex_lock(item->lock);
        while ((item->location_flags & CLOUDLOG_UNCOMMITTED))
            g_cond_wait(item->cond, item->lock);
        g_mutex_unlock(item->lock);
        bluesky_cloudlog_unref(item);

        log_items = g_list_delete_link(log_items, log_items);
    }
}

/* Return a committed cloud log record that can be used as a watermark for how
 * much of the journal has been written. */
BlueSkyCloudLog *bluesky_log_get_commit_point(BlueSkyFS *fs)
{
    BlueSkyCloudLog *marker = bluesky_cloudlog_new(fs, NULL);
    marker->type = LOGTYPE_JOURNAL_MARKER;
    marker->data = bluesky_string_new(g_strdup(""), 0);
    bluesky_cloudlog_stats_update(marker, 1);
    bluesky_cloudlog_sync(marker);

    g_mutex_lock(marker->lock);
    while ((marker->pending_write & CLOUDLOG_JOURNAL))
        g_cond_wait(marker->cond, marker->lock);
    g_mutex_unlock(marker->lock);

    return marker;
}

void bluesky_log_write_commit_point(BlueSkyFS *fs, BlueSkyCloudLog *marker)
{
    BlueSkyCloudLog *commit = bluesky_cloudlog_new(fs, NULL);
    commit->type = LOGTYPE_JOURNAL_CHECKPOINT;

    uint32_t seq, offset;
    seq = GUINT32_TO_LE(marker->log_seq);
    offset = GUINT32_TO_LE(marker->log_offset);
    GString *loc = g_string_new("");
    g_string_append_len(loc, (const gchar *)&seq, sizeof(seq));
    g_string_append_len(loc, (const gchar *)&offset, sizeof(offset));
    commit->data = bluesky_string_new_from_gstring(loc);
    bluesky_cloudlog_stats_update(commit, 1);
    bluesky_cloudlog_sync(commit);

    g_mutex_lock(commit->lock);
    while ((commit->location_flags & CLOUDLOG_UNCOMMITTED))
        g_cond_wait(commit->cond, commit->lock);
    g_mutex_unlock(commit->lock);

    bluesky_cloudlog_unref(marker);
    bluesky_cloudlog_unref(commit);
}

/* Memory-map the given log object into memory (read-only) and return a pointer
 * to it. */
static int page_size = 0;

void bluesky_cachefile_unref(BlueSkyCacheFile *cachefile)
{
    g_atomic_int_add(&cachefile->refcount, -1);
}

static void cloudlog_fetch_start(BlueSkyCacheFile *cachefile);

/* Find the BlueSkyCacheFile object for the given journal or cloud log segment.
 * Returns the object in the locked state and with a reference taken. */
BlueSkyCacheFile *bluesky_cachefile_lookup(BlueSkyFS *fs,
                                           int clouddir, int log_seq,
                                           gboolean start_fetch)
{
    if (page_size == 0) {
        page_size = getpagesize();
    }

    BlueSkyLog *log = fs->log;

    struct stat statbuf;
    char logname[64];
    int type;

    // A request for a local log file
    if (clouddir < 0) {
        sprintf(logname, "journal-%08d", log_seq);
        type = CLOUDLOG_JOURNAL;
    } else {
        sprintf(logname, "log-%08d-%08d", clouddir, log_seq);
        type = CLOUDLOG_CLOUD;
    }

    BlueSkyCacheFile *map;
    g_mutex_lock(log->mmap_lock);
    map = g_hash_table_lookup(log->mmap_cache, logname);

    if (map == NULL
        && type == CLOUDLOG_JOURNAL
        && fstatat(log->dirfd, logname, &statbuf, 0) < 0) {
        /* A stale reference to a journal file which doesn't exist any longer
         * because it was reclaimed.  Return NULL. */
    } else if (map == NULL) {
        if (bluesky_verbose)
            g_print("Adding cache file %s\n", logname);

        map = g_new0(BlueSkyCacheFile, 1);
        map->fs = fs;
        map->type = type;
        map->lock = g_mutex_new();
        map->type = type;
        g_mutex_lock(map->lock);
        map->cond = g_cond_new();
        map->filename = g_strdup(logname);
        map->log_dir = clouddir;
        map->log_seq = log_seq;
        map->log = log;
        g_atomic_int_set(&map->mapcount, 0);
        g_atomic_int_set(&map->refcount, 0);
        map->items = bluesky_rangeset_new();

        g_hash_table_insert(log->mmap_cache, map->filename, map);

        int fd = openat(log->dirfd, logname, O_WRONLY | O_CREAT, 0600);
        if (fd >= 0) {
            ftruncate(fd, 5 << 20);     // FIXME
            close(fd);
        }
    } else {
        g_mutex_lock(map->lock);
    }


    /* If the log file is stored in the cloud and has not been fully fetched,
     * we may need to initiate a fetch now. */
    if (clouddir >= 0 && start_fetch && !map->complete && !map->fetching)
        cloudlog_fetch_start(map);

    g_mutex_unlock(log->mmap_lock);
    if (map != NULL)
        g_atomic_int_inc(&map->refcount);
    return map;
}

static void robust_pwrite(int fd, const char *buf, ssize_t count, off_t offset)
{
    while (count > 0) {
        ssize_t written = pwrite(fd, buf, count, offset);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            g_warning("pwrite failure: %m");
            return;
        }
        buf += written;
        count -= written;
        offset += written;
    }
}

static void cloudlog_partial_fetch_complete(BlueSkyStoreAsync *async,
                                            BlueSkyCacheFile *cachefile);

static void cloudlog_partial_fetch_start(BlueSkyCacheFile *cachefile,
                                         size_t offset, size_t length)
{
    g_atomic_int_inc(&cachefile->refcount);
    if (bluesky_verbose)
        g_print("Starting partial fetch of %s from cloud (%zd + %zd)\n",
                cachefile->filename, offset, length);
    BlueSkyStoreAsync *async = bluesky_store_async_new(cachefile->fs->store);
    async->op = STORE_OP_GET;
    async->key = g_strdup(cachefile->filename);
    async->start = offset;
    async->len = length;
    async->profile = bluesky_profile_get();
    bluesky_store_async_add_notifier(async,
                                     (GFunc)cloudlog_partial_fetch_complete,
                                     cachefile);
    bluesky_store_async_submit(async);
    bluesky_store_async_unref(async);
}

static void cloudlog_partial_fetch_complete(BlueSkyStoreAsync *async,
                                            BlueSkyCacheFile *cachefile)
{
    if (bluesky_verbose || async->result != 0)
        g_print("Fetch of %s from cloud complete, status = %d\n",
                async->key, async->result);

    g_mutex_lock(cachefile->lock);
    if (async->result >= 0) {
        if (async->len == 0) {
            if (bluesky_verbose)
                g_print("Complete object was fetched.\n");
            cachefile->complete = TRUE;
        }

        /* Descrypt items fetched and write valid items out to the local log,
         * but only if they do not overlap existing objects.  This will protect
         * against an attack by the cloud provider where one valid object is
         * moved to another offset and used to overwrite data that we already
         * have fetched. */
        BlueSkyRangeset *items = bluesky_rangeset_new();
        int fd = openat(cachefile->log->dirfd, cachefile->filename, O_WRONLY);
        if (fd >= 0) {
            gboolean allow_unauth;
            async->data = bluesky_string_dup(async->data);
            allow_unauth = cachefile->log_dir == BLUESKY_CLOUD_DIR_CLEANER;
            bluesky_cloudlog_decrypt(async->data->data, async->data->len,
                                     cachefile->fs->keys, items, allow_unauth);
            uint64_t item_offset = 0;
            while (TRUE) {
                const BlueSkyRangesetItem *item;
                item = bluesky_rangeset_lookup_next(items, item_offset);
                if (item == NULL)
                    break;
                if (bluesky_verbose) {
                    g_print("  item offset from range request: %d\n",
                            (int)(item->start + async->start));
                }
                if (bluesky_rangeset_insert(cachefile->items,
                                            async->start + item->start,
                                            item->length, item->data))
                {
                    robust_pwrite(fd, async->data->data + item->start,
                                  item->length, async->start + item->start);
                } else {
                    g_print("    item overlaps existing data!\n");
                }
                item_offset = item->start + 1;
            }
            /* TODO: Iterate over items and merge into cached file. */
            close(fd);
        } else {
            g_warning("Unable to open and write to cache file %s: %m",
                      cachefile->filename);
        }

        bluesky_rangeset_free(items);
    } else {
        g_print("Error fetching %s from cloud, retrying...\n", async->key);
        cloudlog_partial_fetch_start(cachefile, async->start, async->len);
    }

    /* Update disk-space usage statistics, since the writes above may have
     * consumed more space. */
    g_atomic_int_add(&cachefile->log->disk_used, -cachefile->disk_used);
    struct stat statbuf;
    if (fstatat(cachefile->log->dirfd, cachefile->filename, &statbuf, 0) >= 0) {
        /* Convert from 512-byte blocks to 1-kB units */
        cachefile->disk_used = (statbuf.st_blocks + 1) / 2;
    }
    g_atomic_int_add(&cachefile->log->disk_used, cachefile->disk_used);

    bluesky_cachefile_unref(cachefile);
    g_cond_broadcast(cachefile->cond);
    g_mutex_unlock(cachefile->lock);
}

static void cloudlog_fetch_start(BlueSkyCacheFile *cachefile)
{
    g_atomic_int_inc(&cachefile->refcount);
    cachefile->fetching = TRUE;
    if (bluesky_verbose)
        g_print("Starting fetch of %s from cloud\n", cachefile->filename);
    BlueSkyStoreAsync *async = bluesky_store_async_new(cachefile->fs->store);
    async->op = STORE_OP_GET;
    async->key = g_strdup(cachefile->filename);
    async->profile = bluesky_profile_get();
    bluesky_store_async_add_notifier(async,
                                     (GFunc)cloudlog_partial_fetch_complete,
                                     cachefile);
    bluesky_store_async_submit(async);
    bluesky_store_async_unref(async);
}

/* Map and return a read-only version of a byte range from a cached file.  The
 * CacheFile object must be locked. */
BlueSkyRCStr *bluesky_cachefile_map_raw(BlueSkyCacheFile *cachefile,
                                        off_t offset, size_t size)
{
    cachefile->atime = bluesky_get_current_time();

    /* Easy case: the needed data is already in memory */
    if (cachefile->addr != NULL && offset + size <= cachefile->len)
        return bluesky_string_new_from_mmap(cachefile, offset, size);

    int fd = openat(cachefile->log->dirfd, cachefile->filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening logfile %s: %m\n",
                cachefile->filename);
        return NULL;
    }

    off_t length = lseek(fd, 0, SEEK_END);
    if (offset + size > length) {
        close(fd);
        return NULL;
    }

    /* File is not mapped in memory.  Map the entire file in, then return a
     * pointer to just the required data. */
    if (cachefile->addr == NULL) {
        cachefile->addr = (const char *)mmap(NULL, length, PROT_READ,
                                             MAP_SHARED, fd, 0);
        cachefile->len = length;
        g_atomic_int_inc(&cachefile->refcount);

        close(fd);
        return bluesky_string_new_from_mmap(cachefile, offset, size);
    }

    /* Otherwise, the file was mapped in but doesn't cover the data we need.
     * This shouldn't happen much, if at all, but if it does just read the data
     * we need directly from the file.  We lose memory-management benefits of
     * using mmapped data, but otherwise this works. */
    char *buf = g_malloc(size);
    size_t actual_size = readbuf(fd, buf, size);
    close(fd);
    if (actual_size != size) {
        g_free(buf);
        return NULL;
    } else {
        return bluesky_string_new(buf, size);
    }
}

/* The arguments are mostly straightforward.  log_dir is -1 for access from the
 * journal, and non-negative for access to a cloud log segment.  map_data
 * should be TRUE for the case that are mapping just the data of an item where
 * we have already parsed the item headers; this surpresses the error when the
 * access is not to the first bytes of the item. */
BlueSkyRCStr *bluesky_log_map_object(BlueSkyCloudLog *item, gboolean map_data)
{
    BlueSkyFS *fs = item->fs;
    BlueSkyCacheFile *map = NULL;
    BlueSkyRCStr *str = NULL;
    int location = 0;
    size_t file_offset = 0, file_size = 0;
    gboolean range_request = bluesky_options.full_segment_fetches
                              ? FALSE : TRUE;

    if (page_size == 0) {
        page_size = getpagesize();
    }

    bluesky_cloudlog_stats_update(item, -1);

    /* First, check to see if the journal still contains a copy of the item and
     * if so use that. */
    if ((item->location_flags | item->pending_write) & CLOUDLOG_JOURNAL) {
        map = bluesky_cachefile_lookup(fs, -1, item->log_seq, TRUE);
        if (map != NULL) {
            location = CLOUDLOG_JOURNAL;
            file_offset = item->log_offset;
            file_size = item->log_size;
        }
    }

    if (location == 0 && (item->location_flags & CLOUDLOG_CLOUD)) {
        item->location_flags &= ~CLOUDLOG_JOURNAL;
        map = bluesky_cachefile_lookup(fs,
                                       item->location.directory,
                                       item->location.sequence,
                                       !range_request);
        if (map == NULL) {
            g_warning("Unable to remap cloud log segment!");
            goto exit1;
        }
        location = CLOUDLOG_CLOUD;
        file_offset = item->location.offset;
        file_size = item->location.size;
    }

    /* Log segments fetched from the cloud might only be partially-fetched.
     * Check whether the object we are interested in is available. */
    if (location == CLOUDLOG_CLOUD) {
        while (TRUE) {
            const BlueSkyRangesetItem *rangeitem;
            rangeitem = bluesky_rangeset_lookup(map->items, file_offset);
            if (rangeitem != NULL && (rangeitem->start != file_offset
                                      || rangeitem->length != file_size)) {
                g_warning("log-%d: Item offset %zd seems to be invalid!",
                          (int)item->location.sequence, file_offset);
                goto exit2;
            }
            if (rangeitem == NULL) {
                if (bluesky_verbose) {
                    g_print("Item at offset 0x%zx not available, need to fetch.\n",
                            file_offset);
                }
                if (range_request) {
                    uint64_t start = file_offset, length = file_size, end;
                    if (map->prefetches != NULL)
                        bluesky_rangeset_get_extents(map->prefetches,
                                                     &start, &length);
                    start = MIN(start, file_offset);
                    end = MAX(start + length, file_offset + file_size);
                    length = end - start;
                    cloudlog_partial_fetch_start(map, start, length);
                    if (map->prefetches != NULL) {
                        bluesky_rangeset_free(map->prefetches);
                        map->prefetches = NULL;
                    }
                }
                g_cond_wait(map->cond, map->lock);
            } else if (rangeitem->start == file_offset
                       && rangeitem->length == file_size) {
                if (bluesky_verbose)
                    g_print("Item %zd now available.\n", file_offset);
                break;
            }
        }
    }

    if (map_data) {
        if (location == CLOUDLOG_JOURNAL)
            file_offset += sizeof(struct log_header);
        else
            file_offset += sizeof(struct cloudlog_header);

        file_size = item->data_size;
    }
    str = bluesky_cachefile_map_raw(map, file_offset, file_size);

exit2:
    bluesky_cachefile_unref(map);
    g_mutex_unlock(map->lock);
exit1:
    bluesky_cloudlog_stats_update(item, 1);
    return str;
}

void bluesky_mmap_unref(BlueSkyCacheFile *mmap)
{
    if (mmap == NULL)
        return;

    if (g_atomic_int_dec_and_test(&mmap->mapcount)) {
        g_mutex_lock(mmap->lock);
        if (mmap->addr != NULL && g_atomic_int_get(&mmap->mapcount) == 0) {
            if (bluesky_verbose)
                g_print("Unmapped log segment %d...\n", mmap->log_seq);
            munmap((void *)mmap->addr, mmap->len);
            mmap->addr = NULL;
            g_atomic_int_add(&mmap->refcount, -1);
        }
        g_mutex_unlock(mmap->lock);
    }
    g_assert(g_atomic_int_get(&mmap->mapcount) >= 0);
}

/******************************* JOURNAL REPLAY *******************************
 * The journal replay code is used to recover filesystem state after a
 * filesystem restart.  We first look for the most recent commit record in the
 * journal, which indicates the point before which all data in the journal has
 * also been committed to the cloud.  Then, we read in all data in the log past
 * that point.
 */
static GList *directory_contents(const char *dirname)
{
    GList *contents = NULL;
    GDir *dir = g_dir_open(dirname, 0, NULL);
    if (dir == NULL) {
        g_warning("Unable to open journal directory: %s", dirname);
        return NULL;
    }

    const gchar *file;
    while ((file = g_dir_read_name(dir)) != NULL) {
        if (strncmp(file, "journal-", 8) == 0)
            contents = g_list_prepend(contents, g_strdup(file));
    }
    g_dir_close(dir);

    contents = g_list_sort(contents, (GCompareFunc)strcmp);

    return contents;
}

static gboolean validate_journal_item(const char *buf, size_t len, off_t offset)
{
    const struct log_header *header;
    const struct log_footer *footer;

    if (offset + sizeof(struct log_header) + sizeof(struct log_footer) > len)
        return FALSE;

    header = (const struct log_header *)(buf + offset);
    if (GUINT32_FROM_LE(header->magic) != HEADER_MAGIC)
        return FALSE;
    if (GUINT32_FROM_LE(header->offset) != offset)
        return FALSE;
    size_t size = GUINT32_FROM_LE(header->size1)
                   + GUINT32_FROM_LE(header->size2)
                   + GUINT32_FROM_LE(header->size3);

    off_t footer_offset = offset + sizeof(struct log_header) + size;
    if (footer_offset + sizeof(struct log_footer) > len)
        return FALSE;
    footer = (const struct log_footer *)(buf + footer_offset);

    if (GUINT32_FROM_LE(footer->magic) != FOOTER_MAGIC)
        return FALSE;

    uint32_t crc = crc32c(BLUESKY_CRC32C_SEED, buf + offset,
                          sizeof(struct log_header) + sizeof(struct log_footer)
                          + size);
    if (crc != BLUESKY_CRC32C_VALIDATOR) {
        g_warning("Journal entry failed to validate: CRC %08x != %08x",
                  crc, BLUESKY_CRC32C_VALIDATOR);
        return FALSE;
    }

    return TRUE;
}

/* Scan through a journal segment to extract correctly-written items (those
 * that pass sanity checks and have a valid checksum). */
static void bluesky_replay_scan_journal(const char *buf, size_t len,
                                        uint32_t *seq, uint32_t *start_offset)
{
    const struct log_header *header;
    off_t offset = 0;

    while (validate_journal_item(buf, len, offset)) {
        header = (const struct log_header *)(buf + offset);
        size_t size = GUINT32_FROM_LE(header->size1)
                       + GUINT32_FROM_LE(header->size2)
                       + GUINT32_FROM_LE(header->size3);

        if (header->type - '0' == LOGTYPE_JOURNAL_CHECKPOINT) {
            const uint32_t *data = (const uint32_t *)((const char *)header + sizeof(struct log_header));
            *seq = GUINT32_FROM_LE(data[0]);
            *start_offset = GUINT32_FROM_LE(data[1]);
        }

        offset += sizeof(struct log_header) + size + sizeof(struct log_footer);
    }
}

static void reload_item(BlueSkyCloudLog *log_item,
                        const char *data,
                        size_t len1, size_t len2, size_t len3)
{
    BlueSkyFS *fs = log_item->fs;
    /*const char *data1 = data;*/
    const BlueSkyCloudID *data2
        = (const BlueSkyCloudID *)(data + len1);
    /*const BlueSkyCloudPointer *data3
        = (const BlueSkyCloudPointer *)(data + len1 + len2);*/

    bluesky_cloudlog_stats_update(log_item, -1);
    bluesky_string_unref(log_item->data);
    log_item->data = NULL;
    log_item->location_flags = CLOUDLOG_JOURNAL;
    bluesky_cloudlog_stats_update(log_item, 1);

    BlueSkyCloudID id0;
    memset(&id0, 0, sizeof(id0));

    int link_count = len2 / sizeof(BlueSkyCloudID);
    GArray *new_links = g_array_new(FALSE, TRUE, sizeof(BlueSkyCloudLog *));
    for (int i = 0; i < link_count; i++) {
        BlueSkyCloudID id = data2[i];
        BlueSkyCloudLog *ref = NULL;
        if (memcmp(&id, &id0, sizeof(BlueSkyCloudID)) != 0) {
            g_mutex_lock(fs->lock);
            ref = g_hash_table_lookup(fs->locations, &id);
            if (ref != NULL) {
                bluesky_cloudlog_ref(ref);
            }
            g_mutex_unlock(fs->lock);
        }
        g_array_append_val(new_links, ref);
    }

    for (int i = 0; i < log_item->links->len; i++) {
        BlueSkyCloudLog *c = g_array_index(log_item->links,
                                           BlueSkyCloudLog *, i);
        bluesky_cloudlog_unref(c);
    }
    g_array_unref(log_item->links);
    log_item->links = new_links;
}

static void bluesky_replay_scan_journal2(BlueSkyFS *fs, GList **objects,
                                         int log_seq, int start_offset,
                                         const char *buf, size_t len)
{
    const struct log_header *header;
    off_t offset = start_offset;

    while (validate_journal_item(buf, len, offset)) {
        header = (const struct log_header *)(buf + offset);
        g_print("In replay found valid item at offset %zd\n", offset);
        size_t size = GUINT32_FROM_LE(header->size1)
                       + GUINT32_FROM_LE(header->size2)
                       + GUINT32_FROM_LE(header->size3);

        BlueSkyCloudLog *log_item = bluesky_cloudlog_get(fs, header->id);
        g_mutex_lock(log_item->lock);
        *objects = g_list_prepend(*objects, log_item);

        log_item->inum = GUINT64_FROM_LE(header->inum);
        reload_item(log_item, buf + offset + sizeof(struct log_header),
                    GUINT32_FROM_LE(header->size1),
                    GUINT32_FROM_LE(header->size2),
                    GUINT32_FROM_LE(header->size3));
        log_item->log_seq = log_seq;
        log_item->log_offset = offset + sizeof(struct log_header);
        log_item->log_size = header->size1;

        bluesky_string_unref(log_item->data);
        log_item->data = bluesky_string_new(g_memdup(buf + offset + sizeof(struct log_header), GUINT32_FROM_LE(header->size1)), GUINT32_FROM_LE(header->size1));

        /* For any inodes which were read from the journal, deserialize the
         * inode information, overwriting any old inode data. */
        if (header->type - '0' == LOGTYPE_INODE) {
            uint64_t inum = GUINT64_FROM_LE(header->inum);
            BlueSkyInode *inode;
            g_mutex_lock(fs->lock);
            inode = (BlueSkyInode *)g_hash_table_lookup(fs->inodes, &inum);
            if (inode == NULL) {
                inode = bluesky_new_inode(inum, fs, BLUESKY_PENDING);
                inode->change_count = 0;
                bluesky_insert_inode(fs, inode);
            }
            g_mutex_lock(inode->lock);
            bluesky_inode_free_resources(inode);
            if (!bluesky_deserialize_inode(inode, log_item))
                g_print("Error deserializing inode %"PRIu64"\n", inum);
            fs->next_inum = MAX(fs->next_inum, inum + 1);
            bluesky_list_unlink(&fs->accessed_list, inode->accessed_list);
            inode->accessed_list = bluesky_list_prepend(&fs->accessed_list, inode);
            bluesky_list_unlink(&fs->dirty_list, inode->dirty_list);
            inode->dirty_list = bluesky_list_prepend(&fs->dirty_list, inode);
            bluesky_list_unlink(&fs->unlogged_list, inode->unlogged_list);
            inode->unlogged_list = NULL;
            inode->change_cloud = inode->change_commit;
            bluesky_cloudlog_ref(log_item);
            bluesky_cloudlog_unref(inode->committed_item);
            inode->committed_item = log_item;
            g_mutex_unlock(inode->lock);
            g_mutex_unlock(fs->lock);
        }
        bluesky_string_unref(log_item->data);
        log_item->data = NULL;
        g_mutex_unlock(log_item->lock);

        offset += sizeof(struct log_header) + size + sizeof(struct log_footer);
    }
}

void bluesky_replay(BlueSkyFS *fs)
{
    BlueSkyLog *log = fs->log;
    GList *logfiles = directory_contents(log->log_directory);

    /* Scan through log files in reverse order to find the most recent commit
     * record. */
    logfiles = g_list_reverse(logfiles);
    uint32_t seq_num = 0, start_offset = 0;
    while (logfiles != NULL) {
        char *filename = g_strdup_printf("%s/%s", log->log_directory,
                                         (char *)logfiles->data);
        g_print("Scanning file %s\n", filename);
        GMappedFile *map = g_mapped_file_new(filename, FALSE, NULL);
        if (map == NULL) {
            g_warning("Mapping logfile %s failed!\n", filename);
        } else {
            bluesky_replay_scan_journal(g_mapped_file_get_contents(map),
                                        g_mapped_file_get_length(map),
                                        &seq_num, &start_offset);
            g_mapped_file_unref(map);
        }
        g_free(filename);

        g_free(logfiles->data);
        logfiles = g_list_delete_link(logfiles, logfiles);
        if (seq_num != 0 || start_offset != 0)
            break;
    }
    g_list_foreach(logfiles, (GFunc)g_free, NULL);
    g_list_free(logfiles);

    /* Now, scan forward starting from the given point in the log to
     * reconstruct all filesystem state.  As we reload objects we hold a
     * reference to each loaded object.  At the end we free all these
     * references, so that any objects which were not linked into persistent
     * filesystem data structures are freed. */
    GList *objects = NULL;
    while (TRUE) {
        char *filename = g_strdup_printf("%s/journal-%08d",
                                         log->log_directory, seq_num);
        g_print("Replaying file %s from offset %d\n", filename, start_offset);
        GMappedFile *map = g_mapped_file_new(filename, FALSE, NULL);
        g_free(filename);
        if (map == NULL) {
            g_warning("Mapping logfile failed, assuming end of journal\n");
            break;
        }

        bluesky_replay_scan_journal2(fs, &objects, seq_num, start_offset,
                                     g_mapped_file_get_contents(map),
                                     g_mapped_file_get_length(map));
        g_mapped_file_unref(map);
        seq_num++;
        start_offset = 0;
    }

    while (objects != NULL) {
        bluesky_cloudlog_unref((BlueSkyCloudLog *)objects->data);
        objects = g_list_delete_link(objects, objects);
    }
}
