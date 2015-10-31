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
#include <inttypes.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

static void inode_fetch_task(gpointer a, gpointer b);

/* Core filesystem.  Different proxies, such as the NFSv3 one, interface to
 * this, but the core actually tracks the data which is stored.  So far we just
 * implement an in-memory filesystem, but eventually this will be state which
 * is persisted to the cloud. */

/* Return the current time, in microseconds since the epoch. */
int64_t bluesky_get_current_time()
{
    GTimeVal t;
    g_get_current_time(&t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_usec;
}

/* Update an inode to indicate that a modification was made.  This increases
 * the change counter, updates the ctime to the current time, and optionally
 * updates the mtime.  This also makes the inode contents subject to writeback
 * to storage in the future.  inode must already be locked. */
void bluesky_inode_update_ctime(BlueSkyInode *inode, gboolean update_mtime)
{
    int64_t now = bluesky_get_current_time();
    inode->change_count++;
    inode->ctime = now;
    if (update_mtime)
        inode->mtime = now;

    if (inode->change_time == 0)
        inode->change_time = now;

#if 0
    if (bluesky_options.writethrough_cache)
        bluesky_file_flush(inode, NULL);
#endif

    g_mutex_lock(inode->fs->lock);
    bluesky_list_unlink(&inode->fs->unlogged_list, inode->unlogged_list);
    inode->unlogged_list = bluesky_list_prepend(&inode->fs->unlogged_list, inode);
    bluesky_list_unlink(&inode->fs->accessed_list, inode->accessed_list);
    inode->accessed_list = bluesky_list_prepend(&inode->fs->accessed_list, inode);
    g_mutex_unlock(inode->fs->lock);
}

/* Unfortunately a glib hash table is only guaranteed to be able to store
 * 32-bit keys if we use the key directly.  If we want 64-bit inode numbers,
 * we'll have to allocate memory to store the 64-bit inumber, and use a pointer
 * to it.  Rather than allocate the memory for the key, we'll just include a
 * pointer to the 64-bit inum stored in the inode itself, so that we don't need
 * to do any more memory management.  */
static guint bluesky_fs_key_hash_func(gconstpointer key)
{
    uint64_t inum = *(const uint64_t *)key;
    return (guint)inum;
}

static gboolean bluesky_fs_key_equal_func(gconstpointer a, gconstpointer b)
{
    uint64_t i1 = *(const uint64_t *)a;
    uint64_t i2 = *(const uint64_t *)b;
    return i1 == i2;
}

/* Filesystem-level operations.  A filesystem is like a directory tree that we
 * are willing to export. */
BlueSkyFS *bluesky_new_fs(gchar *name)
{
    BlueSkyFS *fs = g_new0(BlueSkyFS, 1);
    fs->lock = g_mutex_new();
    fs->name = g_strdup(name);
    fs->inodes = g_hash_table_new(bluesky_fs_key_hash_func,
                                  bluesky_fs_key_equal_func);
    fs->next_inum = BLUESKY_ROOT_INUM + 1;
    fs->store = bluesky_store_new("file");
    fs->flushd_lock = g_mutex_new();
    fs->flushd_cond = g_cond_new();
    fs->locations = g_hash_table_new(bluesky_cloudlog_hash,
                                     bluesky_cloudlog_equal);
    fs->inode_map = g_sequence_new(NULL);

    fs->log_state = g_new0(BlueSkyCloudLogState, 1);
    fs->log_state->data = g_string_new("");
    fs->log_state->latest_cleaner_seq_seen = -1;
    fs->log_state->uploads_pending_lock = g_mutex_new();
    fs->log_state->uploads_pending_cond = g_cond_new();

    bluesky_cloudlog_threads_init(fs);
    fs->inode_fetch_thread_pool = g_thread_pool_new(inode_fetch_task, NULL,
                                                    bluesky_max_threads,
                                                    FALSE, NULL);

    return fs;
}

BlueSkyFS *bluesky_init_fs(gchar *name, BlueSkyStore *store,
                           const gchar *master_key)
{
    BlueSkyFS *fs = bluesky_new_fs(name);
    fs->master_key = g_strdup(master_key);
    fs->keys = g_new(BlueSkyCryptKeys, 1);
    bluesky_crypt_derive_keys(fs->keys, master_key);
    fs->store = store;
    fs->log = bluesky_log_new("journal");
    fs->log->fs = fs;

    if (bluesky_checkpoint_load(fs)) {
        g_print("Filesystem checkpoint loaded, starting journal replay...\n");
        bluesky_replay(fs);
        g_print("Journal replay complete, filesystem ready.\n");
    } else {
        /* Initialize a fresh filesystem */
        g_print("Initializing new filesystem...\n");
        BlueSkyInode *root = bluesky_new_inode(BLUESKY_ROOT_INUM, fs,
                                               BLUESKY_DIRECTORY);
        root->nlink = 1;
        root->mode = 0755;
        bluesky_insert_inode(fs, root);
        bluesky_inode_update_ctime(root, TRUE);
        bluesky_inode_do_sync(root);
    }

    bluesky_cleaner_thread_launch(fs);

    return fs;
}

/* Inode reference counting. */
void bluesky_inode_ref(BlueSkyInode *inode)
{
    g_atomic_int_inc(&inode->refcount);
}

/* Free most of the resources used by an inode structure, but do not free the
 * inode itself.  Can be used if the inode data will be reloaded from
 * serialized form to clear out old information first. */
void bluesky_inode_free_resources(BlueSkyInode *inode)
{
    switch (inode->type) {
    case BLUESKY_REGULAR:
        if (inode->blocks != NULL) {
            for (int i = 0; i < inode->blocks->len; i++) {
                BlueSkyBlock *b = &g_array_index(inode->blocks,
                                                 BlueSkyBlock, i);
                if (b->type == BLUESKY_BLOCK_DIRTY) {
                    g_error("Deleting an inode with dirty file data!");
                }
                bluesky_cloudlog_unref(b->ref);
                bluesky_string_unref(b->dirty);
            }
            g_array_unref(inode->blocks);
            inode->blocks = NULL;
        }
        break;

    case BLUESKY_DIRECTORY:
        if (inode->dirhash != NULL)
            g_hash_table_destroy(inode->dirhash);
        inode->dirhash = NULL;
        if (inode->dirhash_folded != NULL)
            g_hash_table_destroy(inode->dirhash_folded);
        inode->dirhash_folded = NULL;
        if (inode->dirents != NULL)
            g_sequence_free(inode->dirents);
        inode->dirents = NULL;
        break;

    case BLUESKY_SYMLINK:
        g_free(inode->symlink_contents);
        inode->symlink_contents = NULL;
        break;

    default:
        break;
    }
}

void bluesky_inode_unref(BlueSkyInode *inode)
{
    if (g_atomic_int_dec_and_test(&inode->refcount)) {
        if (bluesky_verbose) {
            g_log("bluesky/inode", G_LOG_LEVEL_DEBUG,
                  "Reference count for inode %"PRIu64" dropped to zero.",
                  inode->inum);
        }

        /* Sanity check: Is the inode clean? */
        if (inode->change_commit < inode->change_count
                || inode->accessed_list != NULL
                || inode->unlogged_list != NULL
                || inode->dirty_list != NULL) {
            g_warning("Dropping inode which is not clean (commit %"PRIi64" < change %"PRIi64"; accessed_list = %p; dirty_list = %p)\n", inode->change_commit, inode->change_count, inode->accessed_list, inode->dirty_list);
        }

        /* These shouldn't be needed, but in case the above warning fires and
         * we delete the inode anyway, we ought to be sure the inode is not on
         * any LRU list. */
        g_mutex_lock(inode->fs->lock);
        bluesky_list_unlink(&inode->fs->accessed_list, inode->accessed_list);
        bluesky_list_unlink(&inode->fs->dirty_list, inode->dirty_list);
        bluesky_list_unlink(&inode->fs->unlogged_list, inode->unlogged_list);
        g_mutex_unlock(inode->fs->lock);

        bluesky_inode_free_resources(inode);

        g_mutex_free(inode->lock);
        g_free(inode);
    }
}

/* Allocate a fresh inode number which has not been used before within a
 * filesystem.  fs must already be locked. */
uint64_t bluesky_fs_alloc_inode(BlueSkyFS *fs)
{
    uint64_t inum;

    inum = fs->next_inum;
    fs->next_inum++;

    return inum;
}

/* Perform type-specification initialization of an inode.  Normally performed
 * in bluesky_new_inode, but can be separated if an inode is created first,
 * then deserialized. */
void bluesky_init_inode(BlueSkyInode *i, BlueSkyFileType type)
{
    i->type = type;

    switch (type) {
    case BLUESKY_REGULAR:
        i->blocks = g_array_new(FALSE, TRUE, sizeof(BlueSkyBlock));
        break;
    case BLUESKY_DIRECTORY:
        i->dirents = g_sequence_new(bluesky_dirent_destroy);
        i->dirhash = g_hash_table_new(g_str_hash, g_str_equal);
        i->dirhash_folded = g_hash_table_new(g_str_hash, g_str_equal);
        break;
    default:
        break;
    }
}

BlueSkyInode *bluesky_new_inode(uint64_t inum, BlueSkyFS *fs,
                                BlueSkyFileType type)
{
    BlueSkyInode *i = g_new0(BlueSkyInode, 1);

    i->lock = g_mutex_new();
    i->refcount = 1;
    i->fs = fs;
    i->inum = inum;
    i->change_count = 1;
    bluesky_init_inode(i, type);

    return i;
}

/* Issue a prefetch hint for an inode.  This signals that the inode may be
 * needed soon.  Does not return any useful data. */
void bluesky_inode_prefetch(BlueSkyFS *fs, uint64_t inum)
{
    BlueSkyInode *inode = NULL;

    g_mutex_lock(fs->lock);
    inode = (BlueSkyInode *)g_hash_table_lookup(fs->inodes, &inum);

    if (inode != NULL) {
        /* Inode is already available, no need for any prefetching... */
        g_mutex_unlock(fs->lock);
        return;
    }

    InodeMapEntry *entry = bluesky_inode_map_lookup(fs->inode_map, inum, 0);
    if (entry != NULL) {
        bluesky_cloudlog_prefetch(entry->item);
    }

    g_mutex_unlock(fs->lock);
    return;
}

/* Retrieve an inode from the filesystem.  Eventually this will be a cache and
 * so we might need to go fetch the inode from elsewhere; for now all
 * filesystem state is stored here.  inode is returned with a reference held
 * but not locked. */
BlueSkyInode *bluesky_get_inode(BlueSkyFS *fs, uint64_t inum)
{
    BlueSkyInode *inode = NULL;

    if (inum == 0) {
        return NULL;
    }

    g_mutex_lock(fs->lock);
    inode = (BlueSkyInode *)g_hash_table_lookup(fs->inodes, &inum);

    if (inode == NULL) {
        bluesky_inode_fetch(fs, inum);
        inode = (BlueSkyInode *)g_hash_table_lookup(fs->inodes, &inum);
    }

    if (inode != NULL) {
        bluesky_inode_ref(inode);

        /* FIXME: We assume we can atomically update the in-memory access time
         * without a lock. */
        inode->access_time = bluesky_get_current_time();
    }

    g_mutex_unlock(fs->lock);

    return inode;
}

/* Insert an inode into the filesystem inode cache.  fs should be locked. */
void bluesky_insert_inode(BlueSkyFS *fs, BlueSkyInode *inode)
{
    g_hash_table_insert(fs->inodes, &inode->inum, inode);
}

/* Start writeback of an inode and all associated data. */
void bluesky_inode_start_sync(BlueSkyInode *inode)
{
    GList *log_items = NULL;

    if (inode->type == BLUESKY_REGULAR)
        bluesky_file_flush(inode, &log_items);

    BlueSkyCloudLog *cloudlog = bluesky_serialize_inode(inode);

    bluesky_cloudlog_unref(inode->committed_item);
    inode->committed_item = cloudlog;

    bluesky_cloudlog_sync(cloudlog);
    bluesky_cloudlog_ref(cloudlog);
    log_items = g_list_prepend(log_items, cloudlog);

    /* Wait for all log items to be committed to disk. */
    bluesky_log_finish_all(log_items);

    /* Mark the inode as clean */
    inode->change_commit = inode->change_count;
    inode->change_time = 0;
    g_mutex_lock(inode->fs->lock);
    bluesky_list_unlink(&inode->fs->unlogged_list, inode->unlogged_list);
    inode->unlogged_list = NULL;

    /* Since a new version of the inode has been written to the log, also
     * schedule a future flush of the new data to cloud storage. */
    bluesky_list_unlink(&inode->fs->dirty_list, inode->dirty_list);
    inode->dirty_list = bluesky_list_prepend(&inode->fs->dirty_list, inode);
    inode->change_cloud = inode->change_count;

    g_mutex_unlock(inode->fs->lock);
}

/* Write back an inode and all associated data and wait for completion.  Inode
 * should already be locked. */
void bluesky_inode_do_sync(BlueSkyInode *inode)
{
    if (bluesky_verbose) {
        g_log("bluesky/inode", G_LOG_LEVEL_DEBUG,
            "Synchronous writeback for inode %"PRIu64"...", inode->inum);
    }
    bluesky_inode_start_sync(inode);
    if (bluesky_verbose) {
        g_log("bluesky/inode", G_LOG_LEVEL_DEBUG,
              "Writeback for inode %"PRIu64" complete", inode->inum);
    }
}

static void inode_fetch_task(gpointer a, gpointer b)
{
    BlueSkyInode *inode = (BlueSkyInode *)a;

    bluesky_profile_set((BlueSkyProfile *)inode->private_data);

    BlueSkyCloudLog *item = inode->committed_item;
    inode->committed_item = NULL;
    g_print("Completing fetch of inode %"PRIu64"...\n", inode->inum);

    g_mutex_lock(item->lock);
    bluesky_cloudlog_fetch(item);
    if (!bluesky_deserialize_inode(inode, item))
        g_print("Error deserializing inode %"PRIu64"\n", inode->inum);
    g_mutex_unlock(item->lock);

    inode->access_time = bluesky_get_current_time();
    g_mutex_lock(inode->fs->lock);
    bluesky_list_unlink(&inode->fs->accessed_list, inode->accessed_list);
    inode->accessed_list = bluesky_list_prepend(&inode->fs->accessed_list, inode);
    g_mutex_unlock(inode->fs->lock);

    g_mutex_unlock(inode->lock);
    bluesky_cloudlog_unref(item);
    bluesky_inode_unref(inode);
}

/* Fetch an inode from stable storage.  The fetch can be performed
 * asynchronously: the in-memory inode is allocated, but not filled with data
 * immediately.  It is kept locked until it has been filled in, so any users
 * should try to acquire the lock on the inode before accessing any data.  The
 * fs lock must be held. */
void bluesky_inode_fetch(BlueSkyFS *fs, uint64_t inum)
{
    InodeMapEntry *entry = bluesky_inode_map_lookup(fs->inode_map, inum, 0);
    if (entry == NULL)
        return;

    /* Non-portable behavior: We take the inode lock here, and release it in
     * the fetching thread.  This works with the default Linux pthreads
     * implementation but is not guaranteed. */

    BlueSkyInode *inode = bluesky_new_inode(inum, fs, BLUESKY_PENDING);
    inode->change_count = 0;
    bluesky_inode_ref(inode);       // Extra ref held by fetching process
    g_mutex_lock(inode->lock);

    inode->committed_item = entry->item;
    bluesky_cloudlog_ref(entry->item);
    bluesky_insert_inode(fs, inode);

    inode->private_data = bluesky_profile_get();
    g_thread_pool_push(fs->inode_fetch_thread_pool, inode, NULL);
}
