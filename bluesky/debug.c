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
#include <inttypes.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

/* Debugging support for BlueSky. */

static void inode_dump(gpointer key, gpointer value, gpointer user_data)
{
    BlueSkyInode *inode = (BlueSkyInode *)value;

    g_print("Inode %"PRIu64":\n", inode->inum);

    gboolean locked = TRUE;
    if (g_mutex_trylock(inode->lock)) {
        locked = FALSE;
        g_mutex_unlock(inode->lock);
    }
    g_print("    Locked: %c   Refcount: %d\n",
            locked ? 'T' : 'F', inode->refcount);

    g_print("    Type: %d   Mode: %o\n", inode->type, inode->mode);
    g_print("    change_count = %"PRIu64", change_commit = %"PRIu64", "
            "change_cloud = %"PRIu64"\n",
            inode->change_count, inode->change_commit, inode->change_cloud);
}

static void cloudlog_dump(gpointer key, gpointer value, gpointer user_data)
{
    BlueSkyCloudLog *log = (BlueSkyCloudLog *)value;

    for (int i = 0; i < sizeof(BlueSkyCloudID); i++) {
        g_print("%02x", (uint8_t)(log->id.bytes[i]));
    }
    g_print(": refs=%d ty=%d inode=%"PRIu64" locs=%x log@(%d,%d) cloud@(%d,%d,%d)\n",
            log->refcount,
            log->type, log->inum,
            log->location_flags | (log->data != NULL ? 0x100 : 0),
            log->log_seq, log->log_offset, log->location.directory,
            log->location.sequence, log->location.offset);
}

static void cache_dump(gpointer key, gpointer value, gpointer user_data)
{
    BlueSkyCacheFile *cache = (BlueSkyCacheFile *)value;

    int64_t age = bluesky_get_current_time() - cache->atime;
    g_print("%s addr=%p mapcount=%d refcount=%d atime_age=%f",
            cache->filename, cache->addr, cache->mapcount, cache->refcount,
            age / 1e6);
    if (cache->fetching)
        g_print(" (fetching)");
    g_print("\n");
}


void inode_map_dump(GSequence *inode_map)
{
    GSequenceIter *i, *j;

    g_print("\nInode map dump:\n");
    for (i = g_sequence_get_begin_iter(inode_map);
         !g_sequence_iter_is_end(i); i = g_sequence_iter_next(i))
    {
        InodeMapRange *range = (InodeMapRange *)g_sequence_get(i);

        g_print("  Range [%"PRIu64", %"PRIu64"]\n", range->start, range->end);

        for (j = g_sequence_get_begin_iter(range->map_entries);
             !g_sequence_iter_is_end(j); j = g_sequence_iter_next(j))
        {
            InodeMapEntry *entry = (InodeMapEntry *)g_sequence_get(j);
            BlueSkyCloudLog *item = entry->item;
            if (item != NULL) {
                char *id = bluesky_cloudlog_id_to_string(item->id);
                g_print("    Entry %"PRIu64" id=%s\n", entry->inum, id);
                g_free(id);
            } else {
                g_print("    Entry %"PRIu64" not available\n", entry->inum);
            }
        }
    }
}
/* Dump a summary of filesystem state as it is cached in memory. */
void bluesky_debug_dump(BlueSkyFS *fs)
{
    g_print("\n*** DEBUG DUMP FOR FILESYSTEM %s ***\n", fs->name);
    g_print("Dirty blocks: %d\n", g_atomic_int_get(&fs->cache_dirty));
    g_print("Cached inodes: %u\tNext inode: %"PRIu64"\n",
            g_hash_table_size(fs->inodes), fs->next_inum);
    g_print("Cloudlog cache: %d dirty, %d writeback, %d journal, %d cloud\n",
            g_atomic_int_get(&fs->cache_log_dirty),
            g_atomic_int_get(&fs->cache_log_writeback),
            g_atomic_int_get(&fs->cache_log_journal),
            g_atomic_int_get(&fs->cache_log_cloud));

    GList *item;
    g_print("Unsynced inode list:");
    for (item = fs->unlogged_list.next; item != NULL; item = item->next) {
        g_print(" %"PRIu64";", ((BlueSkyInode *)item->data)->inum);
    }
    g_print("\n");
    g_print("Dirty inode LRU list:");
    for (item = fs->dirty_list.next; item != NULL; item = item->next) {
        g_print(" %"PRIu64";", ((BlueSkyInode *)item->data)->inum);
    }
    g_print("\n");
    g_print("Accessed inode LRU list:");
    for (item = fs->accessed_list.next; item != NULL; item = item->next) {
        g_print(" %"PRIu64";", ((BlueSkyInode *)item->data)->inum);
    }
    g_print("\n");

    g_hash_table_foreach(fs->inodes, inode_dump, fs);

    g_print("\nLog Objects:\n");
    g_hash_table_foreach(fs->locations, cloudlog_dump, fs);

    g_print("\nJournal/Cache Files:\n");
    g_hash_table_foreach(fs->log->mmap_cache, cache_dump, fs);
    g_print("\n");

    g_mutex_lock(fs->lock);
    inode_map_dump(fs->inode_map);
    g_mutex_unlock(fs->lock);
}

/* Statistics counters: for operation counts, bytes transferred, etc. */
static GStaticMutex stats_lock = G_STATIC_MUTEX_INIT;
static GList *stats_list = NULL;

struct bluesky_stats *bluesky_stats_new(const char *name)
{
    struct bluesky_stats *stats = g_new0(struct bluesky_stats, 1);
    stats->name = name;
    g_static_mutex_lock(&stats_lock);
    stats_list = g_list_append(stats_list, stats);
    g_static_mutex_unlock(&stats_lock);
    return stats;
}

void bluesky_stats_add(struct bluesky_stats *stats, int64_t value)
{
    __sync_fetch_and_add(&stats->count, (int64_t)1);
    __sync_fetch_and_add(&stats->sum, value);
}

void bluesky_stats_dump_all()
{
    g_static_mutex_lock(&stats_lock);
    for (GList *item = stats_list; item != NULL; item = item->next) {
        struct bluesky_stats *stats = (struct bluesky_stats *)item->data;
        g_print("%s: count=%"PRIi64" sum=%"PRIi64"\n",
                stats->name, stats->count, stats->sum);
    }
    g_static_mutex_unlock(&stats_lock);
}

static void periodic_stats_dumper(FILE *f)
{
    const int64_t interval = 10 * 1000000000ULL;
    bluesky_time_hires timestamp = bluesky_now_hires();
    bluesky_time_hires next_timestamp = timestamp;

    while (TRUE) {
        g_static_mutex_lock(&stats_lock);
        timestamp = bluesky_now_hires();
        fprintf(f, "********\ntime=%f\n\n", timestamp / 1e9);
        for (GList *item = stats_list; item != NULL; item = item->next) {
            struct bluesky_stats *stats = (struct bluesky_stats *)item->data;
            fprintf(f, "%s: count=%"PRIi64" sum=%"PRIi64"\n",
                    stats->name, stats->count, stats->sum);
        }
        g_static_mutex_unlock(&stats_lock);
        fflush(f);

        /* Wait until ten seconds from the last timestamp, with a few extra
         * checks for sanity (always try to wait some amount of time in the
         * range [0, 20] seconds). */
        timestamp = bluesky_now_hires();
        next_timestamp += interval;
        if (next_timestamp < timestamp) {
            next_timestamp = timestamp;
            continue;
        }
        if (next_timestamp - timestamp > 2 * interval) {
            next_timestamp = timestamp + interval;
        }

        struct timespec delay;
        delay.tv_sec = (next_timestamp - timestamp) / 1000000000;
        delay.tv_nsec = (next_timestamp - timestamp) % 1000000000;
        nanosleep(&delay, NULL);
    }
}

/* Start a background thread that will periodically dump statistics counters to
 * he specified file every ten seconds. */
void bluesky_stats_run_periodic_dump(FILE *f)
{
    g_thread_create((GThreadFunc)periodic_stats_dumper, f, FALSE, NULL);
}
