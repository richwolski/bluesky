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
#include <inttypes.h>
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

/* Serialization of in-memory filesystem data structures to bytestrings which
 * can be written to persistent storage.  All data is stored in little-endian
 * format. */

/* Magic signature and structure of serialized superblocks. */

#define SUPERBLOCK_MAGIC 0x65ca91e91b124234ULL

struct serialized_superblock {
    uint64_t signature;         /* SUPERBLOCK_MAGIC */
    uint64_t next_inum;
} __attribute__((packed));

/* Magic signature for serialized inodes. */

#define INODE_MAGIC 0xa6832100943d71e6ULL

struct serialized_inode {
    uint64_t signature;         /* INODE_MAGIC */
    int32_t type;
    uint32_t mode;
    uint32_t uid, gid;
    uint32_t nlink;
    uint64_t inum;
    uint64_t change_count;
    int64_t atime;
    int64_t ctime;
    int64_t mtime;
    int64_t ntime;
} __attribute__((packed));

void bluesky_serialize_superblock(GString *out, BlueSkyFS *fs)
{
    struct serialized_superblock buf;

    buf.signature = GUINT64_TO_LE(SUPERBLOCK_MAGIC);
    buf.next_inum = GUINT64_TO_LE(fs->next_inum);

    g_string_append_len(out, (gchar *)&buf, sizeof(buf));
}

BlueSkyFS *bluesky_deserialize_superblock(const gchar *buf)
{
    struct serialized_superblock *raw = (struct serialized_superblock *)buf;

    if (GUINT64_FROM_LE(raw->signature) != SUPERBLOCK_MAGIC)
        return NULL;

    BlueSkyFS *fs = bluesky_new_fs("deserialized");
    fs->next_inum = GUINT64_FROM_LE(raw->next_inum);

    return fs;
}

BlueSkyCloudLog *bluesky_serialize_inode(BlueSkyInode *inode)
{
    BlueSkyFS *fs = inode->fs;
    GString *out = g_string_new("");
    struct serialized_inode buf;

    BlueSkyCloudLog *cloudlog = bluesky_cloudlog_new(fs, NULL);
    cloudlog->type = LOGTYPE_INODE;
    cloudlog->inum = inode->inum;

    buf.signature = GUINT64_TO_LE(INODE_MAGIC);
    buf.type = GUINT32_TO_LE(inode->type);
    buf.mode = GUINT32_TO_LE(inode->mode);
    buf.uid = GUINT32_TO_LE(inode->uid);
    buf.gid = GUINT32_TO_LE(inode->gid);
    buf.nlink = GUINT32_TO_LE(inode->nlink);
    buf.inum = GUINT64_TO_LE(inode->inum);
    buf.change_count = GUINT64_TO_LE(inode->change_count);
    buf.atime = GINT64_TO_LE(inode->atime);
    buf.ctime = GINT64_TO_LE(inode->ctime);
    buf.mtime = GINT64_TO_LE(inode->mtime);
    buf.ntime = GINT64_TO_LE(inode->ntime);

    g_string_append_len(out, (gchar *)&buf, sizeof(buf));

    switch (inode->type) {
    case BLUESKY_REGULAR:
    {
        uint64_t size = GUINT64_TO_LE(inode->size);
        g_string_append_len(out, (gchar *)&size, sizeof(uint64_t));
        for (int i = 0; i < inode->blocks->len; i++) {
            BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock, i);
            BlueSkyCloudLog *ref = (b->type == BLUESKY_BLOCK_REF ? b->ref : NULL);
            bluesky_cloudlog_ref(ref);
            g_array_append_val(cloudlog->links, ref);
        }
        break;
    }

    case BLUESKY_DIRECTORY:
    {
        uint32_t seq;
        uint64_t inum;
        GSequenceIter *i = g_sequence_get_begin_iter(inode->dirents);

        while (!g_sequence_iter_is_end(i)) {
            BlueSkyDirent *d = g_sequence_get(i);

            seq = GUINT32_TO_LE(d->cookie);
            inum = GUINT64_TO_LE(d->inum);
            g_string_append_len(out, (gchar *)&seq, sizeof(uint32_t));
            g_string_append_len(out, (gchar *)&inum, sizeof(uint64_t));
            g_string_append(out, d->name);
            g_string_append_c(out, '\0');

            i = g_sequence_iter_next(i);
        }

        seq = GUINT32_TO_LE(0);
        g_string_append_len(out, (gchar *)&seq, sizeof(uint32_t));

        break;
    }

    case BLUESKY_SYMLINK:
    {
        g_string_append(out, inode->symlink_contents);
        g_string_append_c(out, '\0');
        break;
    }

    default:
        g_warning("Serialization for inode type %d not implemented!\n",
                  inode->type);
    }

    cloudlog->data = bluesky_string_new_from_gstring(out);
    bluesky_cloudlog_insert(cloudlog);
    bluesky_cloudlog_stats_update(cloudlog, 1);

    return cloudlog;
}

/* Deserialize an inode into an in-memory representation.  Returns a boolean
 * indicating whether the deserialization was successful. */
gboolean bluesky_deserialize_inode(BlueSkyInode *inode, BlueSkyCloudLog *item)
{
    g_assert(item->data != NULL);
    const char *buf = item->data->data;

    if (bluesky_verbose) {
        g_log("bluesky/serialize", G_LOG_LEVEL_DEBUG,
              "Deserializing inode %lld...", (long long)inode->inum);
    }

    struct serialized_inode *raw = (struct serialized_inode *)buf;

    if (GUINT64_FROM_LE(raw->signature) != INODE_MAGIC)
        return FALSE;

    if (inode->inum != GUINT64_FROM_LE(raw->inum))
        return FALSE;

    bluesky_init_inode(inode, GUINT32_FROM_LE(raw->type));

    inode->mode = GUINT32_FROM_LE(raw->mode);
    inode->uid = GUINT32_FROM_LE(raw->uid);
    inode->gid = GUINT32_FROM_LE(raw->gid);
    inode->nlink = GUINT32_FROM_LE(raw->nlink);
    inode->change_count = GUINT64_FROM_LE(raw->change_count);
    inode->change_commit = inode->change_count;
    inode->atime = GINT64_FROM_LE(raw->atime);
    inode->ctime = GINT64_FROM_LE(raw->ctime);
    inode->mtime = GINT64_FROM_LE(raw->mtime);
    inode->ntime = GINT64_FROM_LE(raw->ntime);

    buf += sizeof(struct serialized_inode);

    /* TODO: Bounds checking */
    switch (inode->type) {
    case BLUESKY_REGULAR:
        inode->size = GINT64_FROM_LE(*(uint64_t *)buf);
        buf += sizeof(uint64_t);
        g_array_set_size(inode->blocks,
                         (inode->size + BLUESKY_BLOCK_SIZE - 1)
                          / BLUESKY_BLOCK_SIZE);
        g_assert(inode->blocks->len == item->links->len);
        for (int i = 0; i < inode->blocks->len; i++) {
            BlueSkyBlock *b = &g_array_index(inode->blocks, BlueSkyBlock, i);
            b->type = BLUESKY_BLOCK_REF;
            b->ref = g_array_index(item->links, BlueSkyCloudLog *, i);
            bluesky_cloudlog_ref(b->ref);
            b->dirty = NULL;
        }
        break;

    case BLUESKY_DIRECTORY:
    {
        struct serialized_dirent {
            uint32_t seq;
            uint64_t inum;
            gchar name[0];
        } __attribute__((packed));

        struct serialized_dirent *d = (struct serialized_dirent *)buf;
        while (GUINT32_FROM_LE(d->seq) != 0) {
            BlueSkyDirent *dirent = g_new(BlueSkyDirent, 1);
            dirent->cookie = GUINT64_FROM_LE(d->seq);
            dirent->inum = GUINT64_FROM_LE(d->inum);
            dirent->name = g_strdup(d->name);
            dirent->name_folded = bluesky_lowercase(d->name);

            g_sequence_insert_sorted(inode->dirents, dirent,
                                     bluesky_dirent_compare, NULL);
            g_hash_table_insert(inode->dirhash, dirent->name, dirent);
            g_hash_table_insert(inode->dirhash_folded, dirent->name_folded,
                                dirent);

            buf = strchr(d->name, '\0') + 1;
            d = (struct serialized_dirent *)buf;
        }
        break;
    }

    case BLUESKY_SYMLINK:
    {
        inode->symlink_contents = g_strdup(buf);
        break;
    }

    default:
        g_warning("Deserialization for inode type %d not implemented!\n",
                  inode->type);
    }

    return TRUE;
}

/* Convert an in-memory cloud log item to a more serialized form, suitable
 * either for writing to the local journal or the the cloud. */
void bluesky_serialize_cloudlog(BlueSkyCloudLog *log,
                                GString *encrypted,     // Raw data payload
                                GString *authenticated, // Block links
                                GString *writable)      // Writable block links
{
    g_string_append_len(encrypted, log->data->data, log->data->len);
    for (int i = 0; i < log->links->len; i++) {
        BlueSkyCloudLog *ref = g_array_index(log->links, BlueSkyCloudLog *, i);
        if (ref != NULL) {
            g_string_append_len(authenticated,
                                (const char *)&ref->id,
                                sizeof(BlueSkyCloudID));
            // TODO: Fix endianness of output
            g_string_append_len(writable,
                                (const char *)&ref->location,
                                sizeof(ref->location));
        } else {
            BlueSkyCloudID id;
            memset(&id, 0, sizeof(id));
            g_string_append_len(authenticated, (const char *)&id, sizeof(id));
        }
    }
}

/* Deserialize data from the journal or a cloud segment back into the in-memory
 * cloud log item format. */
void bluesky_deserialize_cloudlog(BlueSkyCloudLog *item,
                                  const char *data,
                                  size_t len)
{
    const char *data1, *data2, *data3;
    size_t len1, len2, len3;
    int type;
    BlueSkyCloudID id;
    g_assert(len > 4);

    /* Auto-detect the format: either the journal or cloud log, based on the
     * magic number at the start */
    if (memcmp(data, JOURNAL_MAGIC, 4) == 0) {
        g_assert(len >= sizeof(struct log_header));
        struct log_header *header = (struct log_header *)data;
        type = header->type - '0';
        len1 = GUINT32_FROM_LE(header->size1);
        len2 = GUINT32_FROM_LE(header->size2);
        len3 = GUINT32_FROM_LE(header->size3);
        id = header->id;
        data1 = data + sizeof(struct log_header);
        data2 = data1 + len1;
        data3 = data2 + len2;
        g_assert(data3 + len3 - data <= len);
        item->type = header->type - '0';
        item->inum = GUINT64_FROM_LE(header->inum);
    } else if (memcmp(data, CLOUDLOG_MAGIC, 4) == 0) {
        g_assert(len >= sizeof(struct cloudlog_header));
        struct cloudlog_header *header = (struct cloudlog_header *)data;
        type = header->type - '0';
        len1 = GUINT32_FROM_LE(header->size1);
        len2 = GUINT32_FROM_LE(header->size2);
        len3 = GUINT32_FROM_LE(header->size3);
        id = header->id;
        data1 = data + sizeof(struct cloudlog_header);
        data2 = data1 + len1;
        data3 = data2 + len2;
        g_assert(data3 + len3 - data <= len);
        item->type = header->type - '0';
        item->inum = GUINT64_FROM_LE(header->inum);
    } else {
        g_warning("Deserializing garbage cloud log item!");
        return;
    }

    if (memcmp(&id, &item->id, sizeof(BlueSkyCloudID)) != 0) {
        g_warning("ID does not match expected value!\n");
    }

    BlueSkyFS *fs = item->fs;

    bluesky_string_unref(item->data);
    item->data = NULL;
    item->data_size = len1;

    int link_count = len2 / sizeof(BlueSkyCloudID);
    GArray *new_links = g_array_new(FALSE, TRUE, sizeof(BlueSkyCloudLog *));
    for (int i = 0; i < link_count; i++) {
        BlueSkyCloudID id;
        g_assert(len2 >= sizeof(id));
        memcpy(&id, data2, sizeof(id));
        data2 += sizeof(id); len2 -= sizeof(id);

        BlueSkyCloudLog *ref = bluesky_cloudlog_get(fs, id);
        if (ref != NULL) {
            g_mutex_lock(ref->lock);
            g_assert(len3 >= sizeof(ref->location));
            memcpy(&ref->location, data3, sizeof(ref->location));
            ref->location_flags |= CLOUDLOG_CLOUD;
            data3 += sizeof(ref->location); len3 -= sizeof(ref->location);
            g_mutex_unlock(ref->lock);
        }

        g_array_append_val(new_links, ref);
    }

    for (int i = 0; i < item->links->len; i++) {
        BlueSkyCloudLog *c = g_array_index(item->links,
                                           BlueSkyCloudLog *, i);
        bluesky_cloudlog_unref(c);
    }
    g_array_unref(item->links);
    item->links = new_links;
}
