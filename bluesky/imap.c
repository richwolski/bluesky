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
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

/* Magic number at the start of the checkpoint record, to check for version
 * mismatches. */
#define CHECKPOINT_MAGIC 0x7ad7dafb42a498b4ULL

/* Inode maps.  There is both an in-memory representation as well as the
 * serialized form in the cloud.
 *
 * The inode map is broken up into pieces by partitioning in the inode number
 * space.  The checkpoint region will contain pointers to each of these map
 * ranges. */

/* Roughly how many inodes to try to put in each inode map range. */
static int target_imap_size = 4096;

/* Comparison function for lookuping up inode map entries.  Reads a 64-bit
 * value from the pointed-to address to do the comparison, so we require that
 * the InodeMapEntry and InodeMapRange structures start with the 64-bit value
 * they are sorted by. */
static gint compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
    uint64_t x = *(uint64_t *)a;
    uint64_t y = *(uint64_t *)b;

    if (x < y)
        return -1;
    else if (x > y)
        return 1;
    else
        return 0;
}

/* Look up the inode map entry for the given inode number.  If action is +1,
 * create a new entry if it does not already exist; if it is -1 then delete the
 * entry instead.  If 0, return the entry if it is already present.  A non-zero
 * action value will mark the inode map range as dirty. */
InodeMapEntry *bluesky_inode_map_lookup(GSequence *inode_map, uint64_t inum,
                                        int action)
{
    GSequenceIter *i;

    /* First, scan through to find the inode map section that covers the
     * appropriate range.  Create one if it does not exist but we were asked to
     * create an entry. */
    InodeMapRange *range = NULL;

    i = g_sequence_search(inode_map, &inum, compare, NULL);
    i = g_sequence_iter_prev(i);
    if (!g_sequence_iter_is_end(i))
        range = (InodeMapRange *)g_sequence_get(i);
    if (range == NULL || inum < range->start || inum > range->end) {
        if (action <= 0)
            return NULL;

        /* Create a new range.  First, determine bounds on the range enpoints
         * based on neighboring ranges.  Then, shrink the size of the range to
         * a reasonable size that covers the needed inode number. */
        range = g_new0(InodeMapRange, 1);
        range->start = 0;
        range->end = G_MAXUINT64;
        range->map_entries = g_sequence_new(NULL);
        range->serialized = NULL;

        g_print("Creating inode map range, 1: start=%"PRIu64" end=%"PRIu64"\n",
                range->start, range->end);

        if (!g_sequence_iter_is_begin(i) && !g_sequence_iter_is_end(i))
            range->start = ((InodeMapRange *)g_sequence_get(i))->end + 1;
        i = g_sequence_iter_next(i);
        if (!g_sequence_iter_is_end(i))
            range->end = ((InodeMapRange *)g_sequence_get(i))->start - 1;

        g_print("Creating inode map range, 2: start=%"PRIu64" end=%"PRIu64"\n",
                range->start, range->end);
        g_assert(inum >= range->start && inum <= range->end);

        range->start = MAX(range->start,
                           inum & ~(uint64_t)(target_imap_size - 1));
        range->end = MIN(range->end, range->start + target_imap_size - 1);

        g_print("Creating inode map range, 3: start=%"PRIu64" end=%"PRIu64"\n",
                range->start, range->end);

        g_sequence_insert_sorted(inode_map, range, compare, NULL);
    }

    /* Next, try to find the entry within this range of the inode map. */
    InodeMapEntry *entry = NULL;
    i = g_sequence_search(range->map_entries, &inum, compare, NULL);
    i = g_sequence_iter_prev(i);
    if (!g_sequence_iter_is_end(i))
        entry = (InodeMapEntry *)g_sequence_get(i);
    if (entry == NULL || inum != entry->inum) {
        if (action <= 0)
            return NULL;

        entry = g_new0(InodeMapEntry, 1);
        entry->inum = inum;
        g_sequence_insert_sorted(range->map_entries, entry, compare, NULL);

        if (bluesky_verbose)
            g_print("Created inode map entry for %"PRIu64"\n", inum);
    }

    if (action != 0) {
        bluesky_cloudlog_unref_delayed(range->serialized);
        range->serialized = NULL;
    }

    /* Were we requested to delete the item? */
    if (action < 0) {
        g_sequence_remove(i);
        entry = NULL;
    }

    return entry;
}

/* Convert a section of the inode map to serialized form, in preparation for
 * writing it out to the cloud. */
static void bluesky_inode_map_serialize_section(BlueSkyFS *fs,
                                                InodeMapRange *range)
{
    if (range->serialized != NULL)
        return;

    GString *buf = g_string_new("");
    BlueSkyCloudLog *log = bluesky_cloudlog_new(fs, NULL);
    log->type = LOGTYPE_INODE_MAP;
    log->inum = 0;

    GSequenceIter *i = g_sequence_get_begin_iter(range->map_entries);
    while (!g_sequence_iter_is_end(i)) {
        InodeMapEntry *entry = (InodeMapEntry *)g_sequence_get(i);
        uint64_t inum = GUINT64_TO_LE(entry->inum);
        g_string_append_len(buf, (const char *)&inum, sizeof(inum));
        bluesky_cloudlog_ref(entry->item);
        g_array_append_val(log->links, entry->item);
        i = g_sequence_iter_next(i);
    }

    log->data = bluesky_string_new_from_gstring(buf);
    bluesky_cloudlog_unref(range->serialized);
    range->serialized = log;
    bluesky_cloudlog_stats_update(log, 1);
}

BlueSkyCloudLog *bluesky_inode_map_serialize(BlueSkyFS *fs)
{
    gboolean updated = FALSE;
    GString *buf = g_string_new("");
    BlueSkyCloudLog *log = bluesky_cloudlog_new(fs, NULL);
    log->type = LOGTYPE_CHECKPOINT;
    log->inum = 0;

    /* The checkpoint record starts with a magic number, followed by the
     * version vector which lists the latest sequence number of all other logs
     * (currently, only the cleaner) which have been seen. */
    uint64_t magic = GUINT64_TO_LE(CHECKPOINT_MAGIC);
    g_string_append_len(buf, (const char *)&magic, sizeof(magic));
    uint32_t versions;
    versions = GUINT32_TO_LE(fs->log_state->latest_cleaner_seq_seen >= 0);
    g_string_append_len(buf, (const char *)&versions, sizeof(versions));
    if (fs->log_state->latest_cleaner_seq_seen >= 0) {
        versions = GUINT32_TO_LE(BLUESKY_CLOUD_DIR_CLEANER);
        g_string_append_len(buf, (const char *)&versions, sizeof(versions));
        versions = GUINT32_TO_LE(fs->log_state->latest_cleaner_seq_seen);
        g_string_append_len(buf, (const char *)&versions, sizeof(versions));
    }

    GSequenceIter *i = g_sequence_get_begin_iter(fs->inode_map);
    while (!g_sequence_iter_is_end(i)) {
        InodeMapRange *range = (InodeMapRange *)g_sequence_get(i);
        uint64_t inum = GUINT64_TO_LE(range->start);
        g_string_append_len(buf, (const char *)&inum, sizeof(inum));
        inum = GUINT64_TO_LE(range->end);
        g_string_append_len(buf, (const char *)&inum, sizeof(inum));

        if (range->serialized == NULL) {
            bluesky_inode_map_serialize_section(fs, range);
            updated = TRUE;
        }
        bluesky_cloudlog_ref(range->serialized);
        g_array_append_val(log->links, range->serialized);
        i = g_sequence_iter_next(i);
    }

    log->data = bluesky_string_new_from_gstring(buf);
    bluesky_cloudlog_stats_update(log, 1);

    if (updated) {
        return log;
    } else {
        bluesky_cloudlog_unref(log);
        return NULL;
    }
}

/* Minimize resources consumed the inode map.  This should only be called once
 * an updated inode map has been serialized to the cloud, and will replace
 * cloud log objects with skeletal versions that just reference the data
 * location in the cloud (rather than pinning all object data in memory). */
void bluesky_inode_map_minimize(BlueSkyFS *fs)
{
    GSequenceIter *i = g_sequence_get_begin_iter(fs->inode_map);
    while (!g_sequence_iter_is_end(i)) {
        InodeMapRange *range = (InodeMapRange *)g_sequence_get(i);

        if (range->serialized != NULL)
            bluesky_cloudlog_erase(range->serialized);

        GSequenceIter *j;
        for (j = g_sequence_get_begin_iter(range->map_entries);
             !g_sequence_iter_is_end(j); j = g_sequence_iter_next(j))
        {
            InodeMapEntry *entry = (InodeMapEntry *)g_sequence_get(j);
            BlueSkyCloudLog *item = entry->item;
            if (item != NULL) {
                g_mutex_lock(item->lock);
                if (g_atomic_int_get(&item->refcount) == 1) {
                    bluesky_cloudlog_erase(item);
                }
                g_mutex_unlock(item->lock);
            } else {
                g_warning("Null item for inode map entry %"PRIu64"!",
                          entry->inum);
            }
        }

        i = g_sequence_iter_next(i);
    }
}

/* Reconstruct the inode map from data stored in the cloud. */
static void bluesky_inode_map_deserialize(BlueSkyFS *fs, BlueSkyCloudLog *imap)
{
    g_mutex_lock(imap->lock);
    bluesky_cloudlog_fetch(imap);
    g_assert(imap->data != NULL);
    g_assert(imap->data->len >= 12);
    uint64_t magic;
    uint32_t vector_data;
    memcpy((char *)&magic, imap->data->data, sizeof(magic));
    g_assert(GUINT64_FROM_LE(magic) == CHECKPOINT_MAGIC);
    memcpy((char *)&vector_data, imap->data->data + 8, sizeof(vector_data));
    g_assert(GUINT32_FROM_LE(vector_data) <= 2);

    int vector_size = GUINT32_FROM_LE(vector_data);
    g_assert(imap->data->len == 16 * imap->links->len + 12 + 8 * vector_size);

    for (int i = 0; i < vector_size; i++) {
        memcpy((char *)&vector_data, imap->data->data + 12 + 8*i,
               sizeof(vector_data));
        if (GUINT32_FROM_LE(vector_data) == 1) {
            memcpy((char *)&vector_data, imap->data->data + 16 + 8*i,
                   sizeof(vector_data));
            fs->log_state->latest_cleaner_seq_seen
                = GUINT32_FROM_LE(vector_data);
            g_print("Deserializing checkpoint: last cleaner sequence is %d\n",
                    GUINT32_FROM_LE(vector_data));
        }
    }

    //uint64_t *inum_range = (uint64_t *)imap->data->data;
    for (int i = 0; i < imap->links->len; i++) {
        //int64_t start = GUINT64_FROM_LE(*inum_range++);
        //int64_t end = GUINT64_FROM_LE(*inum_range++);
        BlueSkyCloudLog *section = g_array_index(imap->links,
                                                 BlueSkyCloudLog *, i);
        g_mutex_lock(section->lock);
        bluesky_cloudlog_fetch(section);
        g_print("Loaded cloudlog item (%zd bytes)\n", section->data->len);

        uint64_t *inum = (uint64_t *)section->data->data;
        for (int j = 0; j < section->links->len; j++) {
            InodeMapEntry *entry;
            entry = bluesky_inode_map_lookup(fs->inode_map, *inum, 1);
            entry->inum = GUINT64_FROM_LE(*inum);
            bluesky_cloudlog_unref_delayed(entry->item);
            entry->item = g_array_index(section->links,
                                        BlueSkyCloudLog *, j);
            bluesky_cloudlog_ref(entry->item);
            fs->next_inum = MAX(fs->next_inum, entry->inum + 1);
            inum++;
        }
        g_mutex_unlock(section->lock);
    }
    g_mutex_unlock(imap->lock);
}

/* Find the most recent checkpoint record in the cloud and reload inode map
 * data from it to initialize the filesystem.  Returns a boolean indicating
 * whether a checkpoint was found and loaded or not. */
gboolean bluesky_checkpoint_load(BlueSkyFS *fs)
{
    g_print("Claiming cloud log directory: %d\n",
            fs->log_state->location.directory);
    char *prefix = g_strdup_printf("log-%08d",
                                   fs->log_state->location.directory);
    char *last_segment = bluesky_store_lookup_last(fs->store, prefix);
    g_free(prefix);
    if (last_segment == NULL)
        return FALSE;

    g_print("Last cloud log segment: %s\n", last_segment);
    int seq = atoi(last_segment + 13);
    fs->log_state->location.sequence = seq + 1;

    BlueSkyRCStr *last = bluesky_store_get(fs->store, last_segment);
    g_free(last_segment);
    if (last == NULL) {
        g_warning("Unable to fetch last log segment from cloud!");
        return FALSE;
    }

    last = bluesky_string_dup(last);
    bluesky_cloudlog_decrypt(last->data, last->len, fs->keys, NULL, FALSE);

    /* Scan through the contents of the last log segment to find a checkpoint
     * record.  We need to do a linear scan since at this point we don't have a
     * direct pointer; once we have the last commit record then all other data
     * can be loaded by directly following pointers. */
    const char *buf = last->data;
    size_t len = last->len;
    const char *checkpoint = NULL;
    size_t checkpoint_size = 0;
    while (len > sizeof(struct cloudlog_header)) {
        struct cloudlog_header *header = (struct cloudlog_header *)buf;
        if (memcmp(header->magic, CLOUDLOG_MAGIC, 4) != 0) {
            g_warning("Could not parse cloudlog entry!");
            break;
        }
        int size = sizeof(struct cloudlog_header);
        size += GUINT32_FROM_LE(header->size1);
        size += GUINT32_FROM_LE(header->size2);
        size += GUINT32_FROM_LE(header->size3);
        if (size > len) {
            g_warning("Cloudlog entry is malformed (size too large)!");
            break;
        }
        if (header->type - '0' == LOGTYPE_CHECKPOINT) {
            checkpoint = buf;
            checkpoint_size = size;
        }
        buf += size;
        len -= size;
    }

    if (checkpoint_size == 0) {
        g_error("Unable to locate checkpoint record!\n");
    }

    g_print("Found checkpoint record at %zd (size %zd)\n",
            checkpoint - last->data, checkpoint_size);

    /* Bootstrap the loading process by manually setting the location of this
     * log item. */
    BlueSkyCloudLog *commit;
    commit = bluesky_cloudlog_get(fs,
                                  ((struct cloudlog_header *)checkpoint)->id);
    g_mutex_lock(commit->lock);
    commit->location_flags |= CLOUDLOG_CLOUD;
    commit->location.directory = 0;
    commit->location.sequence = seq;
    commit->location.offset = checkpoint - last->data;
    commit->location.size = checkpoint_size;
    g_mutex_unlock(commit->lock);
    bluesky_cloudlog_stats_update(commit, 1);

    bluesky_inode_map_deserialize(fs, commit);
    bluesky_cloudlog_unref(commit);

    return TRUE;
}
