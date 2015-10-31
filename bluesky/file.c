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
#include <inttypes.h>

#include "bluesky-private.h"

/* Core filesystem: handling of regular files and caching of file data. */

/* Mark a given block dirty and make sure that data is faulted in so that it
 * can be written to.
 *
 * If preserve is set to false, this is a hint that the block is about to be
 * entirely overwritten.  In this case, a dirty block is made available but any
 * prior contents might be lost.  A value of preserve = TRUE is always safe. */
void bluesky_block_touch(BlueSkyInode *inode, uint64_t i, gboolean preserve)
{
    g_return_if_fail(i < inode->blocks->len);
    BlueSkyBlock *block = &g_array_index(inode->blocks, BlueSkyBlock, i);

    gsize block_len;
    if (i < inode->blocks->len - 1) {
        block_len = BLUESKY_BLOCK_SIZE;
    } else {
        block_len = inode->size - i * BLUESKY_BLOCK_SIZE;
    }

    switch (block->type) {
    case BLUESKY_BLOCK_ZERO:
        block->dirty = bluesky_string_new(g_malloc0(block_len), block_len);
        break;
    case BLUESKY_BLOCK_REF:
        if (preserve) {
            // FIXME: locking on the cloudlog?
            bluesky_block_fetch(inode, block, NULL);
            bluesky_string_ref(block->ref->data);
            block->dirty = bluesky_string_dup(block->ref->data);
        } else {
            block->dirty = bluesky_string_new(g_malloc0(block_len), block_len);
        }
        break;
    case BLUESKY_BLOCK_DIRTY:
        block->dirty = bluesky_string_dup(block->dirty);
        break;
    }

    if (block->type != BLUESKY_BLOCK_DIRTY)
        g_atomic_int_add(&inode->fs->cache_dirty, 1);

    block->type = BLUESKY_BLOCK_DIRTY;
    bluesky_cloudlog_unref(block->ref);
    block->ref = NULL;
}

/* Set the size of a file.  This will truncate or extend the file as needed.
 * Newly-allocated bytes are zeroed. */
// FIXME
void bluesky_file_truncate(BlueSkyInode *inode, uint64_t size)
{
    g_return_if_fail(size <= BLUESKY_MAX_FILE_SIZE);

    if (size == inode->size)
        return;

    if (bluesky_verbose) {
        g_log("bluesky/file", G_LOG_LEVEL_DEBUG,
              "Truncating file to %"PRIi64" bytes", size);
    }

    uint64_t blocks = (size + BLUESKY_BLOCK_SIZE - 1) / BLUESKY_BLOCK_SIZE;

    /* Calculate number of bytes in the last block of the file */
    int lastblock_old, lastblock_new;
    lastblock_old = inode->size % BLUESKY_BLOCK_SIZE;
    if (lastblock_old == 0 && inode->size > 0)
        lastblock_old = BLUESKY_BLOCK_SIZE;
    lastblock_new = size % BLUESKY_BLOCK_SIZE;
    if (lastblock_new == 0 && size > 0)
        lastblock_new = BLUESKY_BLOCK_SIZE;

    if (blocks > inode->blocks->len) {
        /* Need to add new blocks to the end of a file.  New block structures
         * are automatically zeroed, which initializes them to be pointers to
         * zero blocks so we don't need to do any more work.  If the
         * previously-last block in the file is smaller than
         * BLUESKY_BLOCK_SIZE, extend it to full size. */
        if (inode->blocks->len > 0) {
            BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock,
                                             inode->blocks->len - 1);

            if (b->type != BLUESKY_BLOCK_ZERO
                    && lastblock_old < BLUESKY_BLOCK_SIZE) {
                bluesky_block_touch(inode, inode->blocks->len - 1, TRUE);
                gsize old_size = b->dirty->len;
                if (lastblock_old != old_size) {
                    fprintf(stderr,
                            "Warning: last block size = %zd, expected %d\n",
                            old_size, lastblock_old);
                }
                bluesky_string_resize(b->dirty, BLUESKY_BLOCK_SIZE);
                memset(&b->dirty->data[old_size], 0,
                       BLUESKY_BLOCK_SIZE - old_size);
            }
        }

        g_array_set_size(inode->blocks, blocks);
    } else if (blocks < inode->blocks->len) {
        /* Delete blocks from a file.  Must reclaim memory. */
        for (guint i = blocks; i < inode->blocks->len; i++) {
            BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock, i);
            if (b->type == BLUESKY_BLOCK_DIRTY)
                g_atomic_int_add(&inode->fs->cache_dirty, -1);
            bluesky_string_unref(b->dirty);
            bluesky_cloudlog_unref(b->ref);
        }
        g_array_set_size(inode->blocks, blocks);
    }

    /* Ensure the new last block of the file is properly sized.  If the block
     * is extended, newly-added bytes must be zeroed. */
    if (blocks > 0) {
        BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock,
                                         blocks - 1);

        gboolean need_resize = TRUE;
        if (b->type == BLUESKY_BLOCK_ZERO)
            need_resize = FALSE;
        else if (size < inode->size && lastblock_new == BLUESKY_BLOCK_SIZE)
            need_resize = FALSE;

        if (need_resize) {
            bluesky_block_touch(inode, blocks - 1, TRUE);
            gsize old_size = b->dirty->len;
            gsize new_size = size - (blocks - 1) * BLUESKY_BLOCK_SIZE;

            bluesky_string_resize(b->dirty, new_size);

            if (new_size > old_size) {
                memset(&b->dirty->data[old_size], 0, new_size - old_size);
            }
        }
    }

    inode->size = size;
    bluesky_inode_update_ctime(inode, 1);
}

void bluesky_file_write(BlueSkyInode *inode, uint64_t offset,
                        const char *data, gint len)
{
    g_return_if_fail(inode->type == BLUESKY_REGULAR);
    g_return_if_fail(offset < inode->size);
    g_return_if_fail(len <= inode->size - offset);

    if (len == 0)
        return;

    while (len > 0) {
        uint64_t block_num = offset / BLUESKY_BLOCK_SIZE;
        gint block_offset = offset % BLUESKY_BLOCK_SIZE;
        gint bytes = MIN(BLUESKY_BLOCK_SIZE - block_offset, len);

        gboolean preserve = TRUE;
        gsize block_size = BLUESKY_BLOCK_SIZE;
        if (block_num == inode->blocks->len - 1) {
            block_size = inode->size - block_num * BLUESKY_BLOCK_SIZE;
        }
        if (block_offset == 0 && bytes == block_size) {
            preserve = FALSE;
        }
        bluesky_block_touch(inode, block_num, preserve);
        BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock,
                                         block_num);
        memcpy(&b->dirty->data[block_offset], data, bytes);

        offset += bytes;
        data += bytes;
        len -= bytes;
    }

    bluesky_inode_update_ctime(inode, 1);
}

void bluesky_file_read(BlueSkyInode *inode, uint64_t offset,
                       char *buf, gint len)
{
    if (len == 0 && offset <= inode->size)
        return;

    g_return_if_fail(inode->type == BLUESKY_REGULAR);
    g_return_if_fail(offset < inode->size);
    g_return_if_fail(len <= inode->size - offset);

    BlueSkyProfile *profile = bluesky_profile_get();

    bluesky_profile_add_event(profile,
                              g_strdup_printf("Start file read prefetch"));
    uint64_t start_block, end_block;
    start_block = offset / BLUESKY_BLOCK_SIZE;
    end_block = (offset + len - 1) / BLUESKY_BLOCK_SIZE;
    for (uint64_t i = start_block; i <= end_block; i++) {
        BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock,
                                         i);
        if (b->type == BLUESKY_BLOCK_REF)
            bluesky_cloudlog_prefetch(b->ref);
    }

    bluesky_profile_add_event(profile,
                              g_strdup_printf("End file read prefetch"));

    while (len > 0) {
        uint64_t block_num = offset / BLUESKY_BLOCK_SIZE;
        gint block_offset = offset % BLUESKY_BLOCK_SIZE;
        gint bytes = MIN(BLUESKY_BLOCK_SIZE - block_offset, len);

        BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock,
                                         block_num);
        if (b->type == BLUESKY_BLOCK_ZERO) {
            memset(buf, 0, bytes);
        } else {
            BlueSkyRCStr *data = NULL;
            if (b->type == BLUESKY_BLOCK_REF) {
                bluesky_block_fetch(inode, b, NULL);
                data = b->ref->data;
            } else if (b->type == BLUESKY_BLOCK_DIRTY) {
                data = b->dirty;
            }
            memcpy(buf, &data->data[block_offset], bytes);
        }

        offset += bytes;
        buf += bytes;
        len -= bytes;
    }

    bluesky_profile_add_event(profile,
                              g_strdup_printf("BlueSky read complete"));
}

void bluesky_block_fetch(BlueSkyInode *inode, BlueSkyBlock *block,
                         BlueSkyStoreAsync *barrier)
{
    if (block->type != BLUESKY_BLOCK_REF)
        return;

    g_mutex_lock(block->ref->lock);
    bluesky_cloudlog_fetch(block->ref);
    g_mutex_unlock(block->ref->lock);
    block->type = BLUESKY_BLOCK_REF;
}

/* Write the given block to cloud-backed storage and mark it clean. */
void bluesky_block_flush(BlueSkyInode *inode, BlueSkyBlock *block,
                         GList **log_items)
{
    BlueSkyFS *fs = inode->fs;

    if (block->type != BLUESKY_BLOCK_DIRTY)
        return;

    g_assert(block->ref == NULL);

    BlueSkyCloudLog *cloudlog = bluesky_cloudlog_new(fs, NULL);
    cloudlog->type = LOGTYPE_DATA;
    cloudlog->inum = inode->inum;
    cloudlog->data = block->dirty;      // String ownership is transferred
    bluesky_cloudlog_stats_update(cloudlog, 1);
    bluesky_cloudlog_sync(cloudlog);
    bluesky_cloudlog_ref(cloudlog);     // Reference for log_items list
    *log_items = g_list_prepend(*log_items, cloudlog);
    bluesky_cloudlog_insert(cloudlog);

    block->ref = cloudlog;              // Uses initial reference from _new()

    block->type = BLUESKY_BLOCK_REF;
    block->dirty = NULL;
    g_atomic_int_add(&fs->cache_dirty, -1);
}

/* Flush all blocks in a file to stable storage. */
void bluesky_file_flush(BlueSkyInode *inode, GList **log_items)
{
    g_return_if_fail(inode->type == BLUESKY_REGULAR);

    for (int i = 0; i < inode->blocks->len; i++) {
        BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock, i);
        bluesky_block_flush(inode, b, log_items);
    }
}

/* Drop clean data blocks for a file from cache. */
void bluesky_file_drop_cached(BlueSkyInode *inode)
{
    g_return_if_fail(inode->type == BLUESKY_REGULAR);

    for (int i = 0; i < inode->blocks->len; i++) {
        BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock, i);
        if (b->type == BLUESKY_BLOCK_REF) {
            g_mutex_lock(b->ref->lock);
            if (b->ref->data != NULL
                && g_atomic_int_get(&b->ref->data_lock_count) == 0
                && (b->ref->location_flags != 0))
            {
                bluesky_cloudlog_stats_update(b->ref, -1);
                bluesky_string_unref(b->ref->data);
                b->ref->data = NULL;
                bluesky_cloudlog_stats_update(b->ref, 1);
            }
            g_mutex_unlock(b->ref->lock);
        }
    }
}
