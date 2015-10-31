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

#define _GNU_SOURCE
#define _ATFILE_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "bluesky-private.h"

#define WRITEBACK_DELAY (20 * 1000000)
#define CACHE_DROP_DELAY (20 * 1000000)

/* Filesystem caching and cache coherency.  There are actually a couple of
 * different tasks that are performed here:
 *   - Forcing data to the log if needed to reclaim memory or simply if the
 *     data has been dirty in memory long enough.
 *   - Writing batches of data to the cloud.
 */

static void flushd_dirty_inode(BlueSkyInode *inode)
{
    BlueSkyFS *fs = inode->fs;

    g_mutex_lock(fs->lock);
    bluesky_list_unlink(&fs->unlogged_list, inode->unlogged_list);
    inode->unlogged_list = NULL;
    g_mutex_unlock(fs->lock);

    /* Inode is clean; nothing to do. */
    if (inode->change_count == inode->change_commit)
        return;

    if (bluesky_verbose) {
        g_log("bluesky/flushd", G_LOG_LEVEL_DEBUG,
            "Starting flush of inode %"PRIu64, inode->inum);
    }

    bluesky_inode_start_sync(inode);
}

/* Check whether memory usage may have dropped below critical thresholds for
 * waking up waiting threads. */
void flushd_check_wakeup(BlueSkyFS *fs)
{
    int dirty = g_atomic_int_get(&fs->cache_dirty);
    dirty += g_atomic_int_get(&fs->cache_log_dirty);

    if (dirty <= bluesky_watermark_high_dirty)
        g_cond_broadcast(fs->flushd_cond);
}

/* Try to flush dirty data to disk, either due to memory pressure or due to
 * timeouts. */
static void flushd_dirty(BlueSkyFS *fs)
{
    int64_t start_time = bluesky_get_current_time();
    g_mutex_lock(fs->lock);

    while (1) {
        BlueSkyInode *inode;
        if (fs->unlogged_list.prev == NULL)
            break;
        inode = fs->unlogged_list.prev->data;

        if (bluesky_verbose) {
            g_log("bluesky/flushd", G_LOG_LEVEL_DEBUG,
                  "Considering flushing inode %"PRIu64, inode->inum);
        }

        /* Stop processing dirty inodes if we both have enough memory available
         * and the oldest inode is sufficiently new that it need not be flushed
         * out. */
        uint64_t elapsed = bluesky_get_current_time() - inode->change_time;
        if (g_atomic_int_get(&fs->cache_dirty) < bluesky_watermark_low_dirty
                && elapsed < WRITEBACK_DELAY)
            break;
        if (inode->change_time > start_time)
            break;

        bluesky_inode_ref(inode);

        g_mutex_unlock(fs->lock);

        g_mutex_lock(inode->lock);
        flushd_dirty_inode(inode);
        g_mutex_unlock(inode->lock);
        bluesky_inode_unref(inode);

        g_mutex_lock(fs->lock);
        flushd_check_wakeup(fs);
    }

    g_cond_broadcast(fs->flushd_cond);

    g_mutex_unlock(fs->lock);
}

/* Try to flush dirty data to the cloud.  This will take a snapshot of the
 * entire filesystem (though only point-in-time consistent for isolated inodes
 * and not the filesystem as a whole) and ensure all data is written to the
 * cloud.  When the write completes, we will allow old journal segments (those
 * that were fully written _before_ the snapshot process started) to be garbage
 * collected.  Newer journal segments can't be collected yet since they may
 * still contain data which has not been written persistently to the cloud.
 *
 * Note that some of this code relies on the fact that only this thread of
 * control (running flushd_cloud) is manipulating the inode map, and so
 * concurrent updates to the inode map are prevented even without the
 * filesystem lock held.  Take great care if allowing multi-threaded access to
 * the inode map... */
static void flushd_cloud(BlueSkyFS *fs)
{
    g_mutex_lock(fs->lock);

    /* TODO: Locking?  Since we're reading a single variable this is probably
     * atomic but a lock could be safer. */
    BlueSkyCloudLog *marker = bluesky_log_get_commit_point(fs);
    int journal_seq_start = fs->log->seq_num;

    while (1) {
        BlueSkyInode *inode;
        if (fs->dirty_list.prev == NULL)
            break;
        inode = fs->dirty_list.prev->data;

        if (bluesky_verbose) {
            g_log("bluesky/flushd", G_LOG_LEVEL_DEBUG,
                  "Flushing inode %"PRIu64" to cloud", inode->inum);
        }

        bluesky_inode_ref(inode);

        g_mutex_unlock(fs->lock);

        g_mutex_lock(inode->lock);
        g_assert(inode->change_cloud == inode->change_commit);
        g_mutex_lock(fs->lock);
        bluesky_list_unlink(&fs->dirty_list, inode->dirty_list);
        inode->dirty_list = NULL;
        g_mutex_unlock(fs->lock);

        BlueSkyCloudLog *log = inode->committed_item;
        inode->committed_item = NULL;
        g_mutex_unlock(inode->lock);

        if (log != NULL)
            bluesky_cloudlog_serialize(log, fs);
        bluesky_inode_unref(inode);
        bluesky_cloudlog_unref(log);

        g_mutex_lock(fs->lock);
    }
    g_mutex_unlock(fs->lock);

    /* Write out any updated inode map entries, so that all inodes just written
     * can be located, and then a final commit record. */
    BlueSkyCloudLog *commit_record = bluesky_inode_map_serialize(fs);
    if (commit_record != NULL) {
        bluesky_cloudlog_serialize(commit_record, fs);
    } else {
        g_print("No need for a checkpoint record...\n");
    }

    bluesky_cloudlog_flush(fs);

    /* Wait until all segments have been written to the cloud, so that it
     * becomes safe to free up journal segments. */
    while (fs->log_state->pending_segments != NULL) {
        SerializedRecord *segment
            = (SerializedRecord *)fs->log_state->pending_segments->data;
        g_mutex_lock(segment->lock);
        while (!segment->complete)
            g_cond_wait(segment->cond, segment->lock);
        g_mutex_unlock(segment->lock);

        g_mutex_free(segment->lock);
        g_cond_free(segment->cond);
        g_free(segment);

        fs->log_state->pending_segments
            = g_list_delete_link(fs->log_state->pending_segments,
                                 fs->log_state->pending_segments);
    }

    bluesky_log_write_commit_point(fs, marker);
    bluesky_cloudlog_unref(commit_record);

    g_print("All segments have been flushed, journal < %d is clean\n",
            journal_seq_start);

    fs->log->journal_watermark = journal_seq_start;

    bluesky_inode_map_minimize(fs);
}

/* Drop cached data for a given inode, if it is clean.  inode must be locked. */
static void drop_caches(BlueSkyInode *inode)
{
    if (inode->type == BLUESKY_REGULAR)
        bluesky_file_drop_cached(inode);

    BlueSkyCloudLog *log = inode->committed_item;
    if (log != NULL) {
        g_mutex_lock(log->lock);
        if (log->data != NULL
            && g_atomic_int_get(&log->data_lock_count) == 0
            && (log->location_flags != 0))
        {
            bluesky_cloudlog_stats_update(log, -1);
            bluesky_string_unref(log->data);
            log->data = NULL;
            bluesky_cloudlog_stats_update(log, 1);
        }
        g_mutex_unlock(log->lock);
    }
}

/* Drop clean data from the cache if needed.  Clean data should generally be
 * memory-mapped from log file or similar, so the kernel can drop this clean
 * data from memory for us and hence memory management isn't too important.
 * Mainly, we'll want to drop references to data that hasn't been accessed in a
 * while so that it is possible to reclaim log segments on disk.
 *
 * If aggressive is set, try much harder to drop data from the caches to free
 * up space. */
static void flushd_clean(BlueSkyFS *fs, int aggressive)
{
    g_mutex_lock(fs->lock);

    size_t inode_count = g_hash_table_size(fs->inodes);
    if (!inode_count)
        inode_count = 1;

    while (inode_count-- > 0) {
        BlueSkyInode *inode;
        if (fs->accessed_list.prev == NULL)
            break;
        inode = fs->accessed_list.prev->data;

        uint64_t elapsed = bluesky_get_current_time() - inode->access_time;
        if (elapsed < CACHE_DROP_DELAY && !aggressive)
            break;

        if (bluesky_verbose) {
            g_log("bluesky/flushd", G_LOG_LEVEL_DEBUG,
                  "Considering dropping cached data for inode %"PRIu64,
                  inode->inum);
        }

        bluesky_inode_ref(inode);

        g_mutex_unlock(fs->lock);

        g_mutex_lock(inode->lock);

        g_mutex_lock(fs->lock);
        bluesky_list_unlink(&fs->accessed_list, inode->accessed_list);
        inode->accessed_list = bluesky_list_prepend(&fs->accessed_list, inode);
        g_mutex_unlock(fs->lock);

        drop_caches(inode);

        g_mutex_unlock(inode->lock);
        bluesky_inode_unref(inode);

        g_mutex_lock(fs->lock);
    }

    g_mutex_unlock(fs->lock);
}

/* Scan through all currently-stored files in the journal/cache and garbage
 * collect old unused ones, if needed. */
static void gather_cachefiles(gpointer key, gpointer value, gpointer user_data)
{
    GList **files = (GList **)user_data;
    *files = g_list_prepend(*files, value);
}

static gint compare_cachefiles(gconstpointer a, gconstpointer b)
{
    int64_t ta, tb;

    ta = ((BlueSkyCacheFile *)a)->atime;
    tb = ((BlueSkyCacheFile *)b)->atime;
    if (ta < tb)
        return -1;
    else if (ta > tb)
        return 1;
    else
        return 0;
}

void bluesky_cachefile_gc(BlueSkyFS *fs)
{
    GList *files = NULL;

    g_mutex_lock(fs->log->mmap_lock);
    g_hash_table_foreach(fs->log->mmap_cache, gather_cachefiles, &files);

    /* Sort based on atime.  The atime should be stable since it shouln't be
     * updated except by threads which can grab the mmap_lock, which we already
     * hold. */
    files = g_list_sort(files, compare_cachefiles);

    /* Walk the list of files, starting with the oldest, deleting files if
     * possible until enough space has been reclaimed. */
    g_print("\nScanning cache: (total size = %d kB)\n", fs->log->disk_used);
    while (files != NULL) {
        BlueSkyCacheFile *cachefile = (BlueSkyCacheFile *)files->data;
        /* Try to lock the structure, but if the lock is held by another thread
         * then we'll just skip the file on this pass. */
        if (g_mutex_trylock(cachefile->lock)) {
            int64_t age = bluesky_get_current_time() - cachefile->atime;
            if (bluesky_verbose) {
                g_print("%s addr=%p mapcount=%d refcount=%d size=%d atime_age=%f",
                        cachefile->filename, cachefile->addr, cachefile->mapcount,
                        cachefile->refcount, cachefile->disk_used, age / 1e6);
                if (cachefile->fetching)
                    g_print(" (fetching)");
                g_print("\n");
            }

            gboolean deletion_candidate = FALSE;
            if (g_atomic_int_get(&fs->log->disk_used)
                    > bluesky_options.cache_size
                && g_atomic_int_get(&cachefile->refcount) == 0
                && g_atomic_int_get(&cachefile->mapcount) == 0)
            {
                deletion_candidate = TRUE;
            }

            /* Don't allow journal files to be reclaimed until all data is
             * known to be durably stored in the cloud. */
            if (cachefile->type == CLOUDLOG_JOURNAL
                && cachefile->log_seq >= fs->log->journal_watermark)
            {
                deletion_candidate = FALSE;
            }

            if (deletion_candidate) {
                if (bluesky_verbose) {
                    g_print("   ...deleting\n");
                }
                if (unlinkat(fs->log->dirfd, cachefile->filename, 0) < 0) {
                    fprintf(stderr, "Unable to unlink journal %s: %m\n",
                            cachefile->filename);
                }

                g_atomic_int_add(&fs->log->disk_used, -cachefile->disk_used);
                g_hash_table_remove(fs->log->mmap_cache, cachefile->filename);
                bluesky_rangeset_free(cachefile->items);
                if (cachefile->prefetches != NULL)
                    bluesky_rangeset_free(cachefile->prefetches);
                g_mutex_unlock(cachefile->lock);
                g_mutex_free(cachefile->lock);
                g_cond_free(cachefile->cond);
                g_free(cachefile->filename);
                g_free(cachefile);
            } else {
                g_mutex_unlock(cachefile->lock);
            }
        }
        files = g_list_delete_link(files, files);
    }
    g_list_free(files);
    g_print("\nEnding cache size: %d kB\n", fs->log->disk_used);

    g_mutex_unlock(fs->log->mmap_lock);
}

/* Run the flush daemon for a single iteration, though if it is already
 * executing returns immediately. */
static gpointer flushd_task(BlueSkyFS *fs)
{
    if (!g_mutex_trylock(fs->flushd_lock))
        return NULL;

    g_print("\nCloudlog cache: %d dirty, %d writeback, %d journal, %d cloud\n",
            g_atomic_int_get(&fs->cache_log_dirty),
            g_atomic_int_get(&fs->cache_log_writeback),
            g_atomic_int_get(&fs->cache_log_journal),
            g_atomic_int_get(&fs->cache_log_cloud));

    flushd_dirty(fs);
    flushd_cloud(fs);
    flushd_clean(fs, 0);
    bluesky_cachefile_gc(fs);

    /* If running out of disk cache space, make another more aggressive pass to
     * free up space. */
    if (g_atomic_int_get(&fs->log->disk_used) > bluesky_options.cache_size) {
        g_print("Still short on disk space, trying again to free space...\n");
        flushd_clean(fs, 1);
        bluesky_cachefile_gc(fs);
    }

    g_mutex_unlock(fs->flushd_lock);

    return NULL;
}

void bluesky_flushd_invoke(BlueSkyFS *fs)
{
    g_thread_create((GThreadFunc)flushd_task, fs, FALSE, NULL);
}

/* How urgent is flushing out data?  Returns one of several values:
 *   0 - memory state is fine
 *   1 - should launch flushd if not already running
 *   2 - should delay writers while memory frees up
 *   3 - should block writers until memory frees up
 */
static int compute_pressure(BlueSkyFS *fs)
{
    /* LEVEL 3 */
    /* Too much dirty data in memory? */
    if (g_atomic_int_get(&fs->cache_dirty)
                + g_atomic_int_get(&fs->cache_log_dirty)
           > bluesky_watermark_high_dirty)
    {
        g_print("pressure: too much dirty data (3)\n");
        return 3;
    }

    /* Too much uncommitted data in the journal on disk, not yet flushed to the
     * cloud? */
    /*printf("Dirty journals: %d to %d\n",
           fs->log->journal_watermark, fs->log->seq_num);*/
    int dirty_limit;
    dirty_limit = bluesky_options.cache_size / (LOG_SEGMENT_SIZE / 1024) / 2;
    int dirty_journals = fs->log->seq_num - fs->log->journal_watermark + 1;
    if (dirty_journals > 1 && dirty_journals >= dirty_limit) {
        printf("pressure: too many dirty journals (%d >= %d) (3)\n",
               dirty_journals, dirty_limit);
        return 3;
    }

    /* LEVEL 2 */
    if (g_atomic_int_get(&fs->cache_dirty) > bluesky_watermark_med2_dirty) {
        g_print("pressure: too much dirty data (2)\n");
        return 2;
    }

    if (dirty_journals > 1 && dirty_journals > dirty_limit * 3 / 4) {
        printf("pressure: many dirty journals (%d), should start writeback (2)\n",
               dirty_journals);
        return 2;
    }

    /* LEVEL 1 */
    if (g_atomic_int_get(&fs->cache_dirty) > bluesky_watermark_medium_dirty) {
        g_print("pressure: too much dirty data (1)\n");
        return 1;
    }

    if (dirty_journals > 1 && dirty_journals > dirty_limit / 2) {
        printf("pressure: many dirty journals (%d), should start writeback (1)\n",
               dirty_journals);
        return 1;
    }

    return 0;
}

void bluesky_flushd_invoke_conditional(BlueSkyFS *fs)
{
    int pressure = compute_pressure(fs);
    if (pressure == 0)
        return;

    if (bluesky_verbose) {
        g_log("bluesky/flushd", G_LOG_LEVEL_DEBUG,
              "Too much data; invoking flushd: dirty=%d",
              g_atomic_int_get(&fs->cache_dirty));
    }

    bluesky_flushd_invoke(fs);

    /* If the system is under heavy memory pressure, actually delay execution
     * so the flush daemon can catch up. */
    while (pressure > 1) {
        g_log("bluesky/flushd", G_LOG_LEVEL_DEBUG,
              "Waiting due to memory pressure, dirty=%d + %d",
              g_atomic_int_get(&fs->cache_dirty),
              g_atomic_int_get(&fs->cache_log_dirty));
        g_mutex_lock(fs->lock);
        pressure = compute_pressure(fs);
        if (pressure > 2) {
            g_cond_wait(fs->flushd_cond, fs->lock);
        } else if (pressure > 1) {
            /* Wait for up to 10 seconds. */
            GTimeVal timeout;
            g_get_current_time(&timeout);
            g_time_val_add(&timeout, 10 * 1000 * 1000);
            g_cond_timed_wait(fs->flushd_cond, fs->lock, &timeout);
        }
        g_mutex_unlock(fs->lock);
        pressure = compute_pressure(fs);
        if (pressure == 1)
            break;  /* Do not loop indefinitely for a pressure of 1 */
    }
}

/* Start a perpetually-running thread that flushes the cache occasionally. */
static gpointer flushd_thread(BlueSkyFS *fs)
{
    while (TRUE) {
        bluesky_flushd_invoke(fs);
        struct timespec delay;
        delay.tv_sec = 2;
        delay.tv_nsec = 0;
        nanosleep(&delay, NULL);
    }

    return NULL;
}

void bluesky_flushd_thread_launch(BlueSkyFS *fs)
{
    g_thread_create((GThreadFunc)flushd_thread, fs, FALSE, NULL);
}
