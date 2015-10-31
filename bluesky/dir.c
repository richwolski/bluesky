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

#include "bluesky-private.h"

/* Core filesystem: handling of directories. */

void bluesky_dirent_destroy(gpointer data)
{
    BlueSkyDirent *dirent = (BlueSkyDirent *)data;
    g_free(dirent->name);
    g_free(dirent->name_folded);
    g_free(dirent);
}

gint bluesky_dirent_compare(gconstpointer a, gconstpointer b,
                            gpointer unused)
{
    uint32_t hash1 = ((const BlueSkyDirent *)a)->cookie;
    uint32_t hash2 = ((const BlueSkyDirent *)b)->cookie;

    if (hash1 < hash2)
        return -1;
    else if (hash1 > hash2)
        return 1;
    else
        return 0;
}

/* Perform a lookup for a file name within a directory.  Returns the inode
 * number if found, or 0 if not (0 is never a valid inode number).  Should be
 * called with the inode lock already held. */
uint64_t bluesky_directory_lookup(BlueSkyInode *inode, gchar *name)
{
    g_return_val_if_fail(inode->type == BLUESKY_DIRECTORY, 0);
    g_return_val_if_fail(inode->dirhash != NULL, 0);

    BlueSkyDirent *d = g_hash_table_lookup(inode->dirhash, name);
    if (d == NULL)
        return 0;
    else
        return d->inum;
}

/* Case-insensitive lookup. */
uint64_t bluesky_directory_ilookup(BlueSkyInode *inode, gchar *name)
{
    g_return_val_if_fail(inode->type == BLUESKY_DIRECTORY, 0);
    g_return_val_if_fail(inode->dirhash_folded != NULL, 0);

    name = bluesky_lowercase(name);
    BlueSkyDirent *d = g_hash_table_lookup(inode->dirhash_folded, name);
    g_free(name);

    if (d == NULL)
        return 0;
    else
        return d->inum;
}

/* Iterate through a directory listing.  This returns one directory entry at a
 * time, finding the first entry with a directory cookie value larger than the
 * supplied one.  Use a cookie of 0 to start reading from the start of a
 * directory. */
BlueSkyDirent *bluesky_directory_read(BlueSkyInode *dir, uint32_t cookie)
{
    BlueSkyDirent start = {NULL, NULL, cookie, 0};
    GSequenceIter *i = g_sequence_search(dir->dirents, &start,
                                         bluesky_dirent_compare, NULL);

    if (g_sequence_iter_is_end(i))
        return NULL;
    else
        return g_sequence_get(i);
}

/* Insert a new entry into a directory.  Should be called with the inode lock
 * already held. */
gboolean bluesky_directory_insert(BlueSkyInode *dir,
                                  const gchar *name, uint64_t inum)
{
    g_return_val_if_fail(dir->type == BLUESKY_DIRECTORY, FALSE);

    /* Check that no entry with the given name already exists. */
    if (g_hash_table_lookup(dir->dirhash, name) != NULL)
        return FALSE;

    BlueSkyDirent *d = g_new(BlueSkyDirent, 1);
    d->name = g_strdup(name);
    d->name_folded = bluesky_lowercase(name);
    d->inum = inum;

    GSequence *dirents = dir->dirents;

    /* Pick an unused cookie value for the directory at random.  Restrict
     * ourselves to positive 32-bit values (even if treated as signed), and
     * keep the first four slots free. */
    while (1) {
        do {
            d->cookie = g_random_int() & 0x7fffffff;
        } while (d->cookie < 4);

        /* If the directory is empty, we can't possibly have a collision, so
         * just go with the first key chosen. */
        if (g_sequence_get_length(dirents) == 0)
            break;

        /* Otherwise, try looking up the generated cookie.  If we do not find a
         * match, we can use this cookie value, otherwise we need to generate a
         * new one and try again.  Because of the check above for an empty
         * directory, we know that the lookup will return something so no need
         * to worry about NULL. */
        GSequenceIter *i = g_sequence_search(dir->dirents, d,
                                             bluesky_dirent_compare, NULL);
        i = g_sequence_iter_prev(i);
        if (((BlueSkyDirent *)g_sequence_get(i))->cookie != d->cookie)
            break;
    }

    /* Add the directory entry to both indices. */
    g_sequence_insert_sorted(dirents, d, bluesky_dirent_compare, NULL);
    g_hash_table_insert(dir->dirhash, d->name, d);
    g_hash_table_insert(dir->dirhash_folded, d->name_folded, d);

    bluesky_inode_update_ctime(dir, 1);
    //bluesky_inode_do_sync(dir);     // TODO: Needed?

    return TRUE;
}

/* Remove an entry from a directory.  Should be called with the inode lock
 * already held. */
gboolean bluesky_directory_remove(BlueSkyInode *dir, gchar *name)
{
    g_return_val_if_fail(dir->type == BLUESKY_DIRECTORY, FALSE);

    BlueSkyDirent *d = g_hash_table_lookup(dir->dirhash, name);
    if (d == NULL) {
        return FALSE;
    }

    g_hash_table_remove(dir->dirhash, d->name);
    g_hash_table_remove(dir->dirhash_folded, d->name_folded);

    GSequenceIter *i = g_sequence_search(dir->dirents, d,
                                         bluesky_dirent_compare, NULL);
    i = g_sequence_iter_prev(i);

    /* Assertion check, this ought to succeed */
    g_return_val_if_fail(g_sequence_get(i) == d, FALSE);

    g_sequence_remove(i);

    bluesky_inode_update_ctime(dir, 1);

    return TRUE;
}

/* Rename a file.  If desired (if overwrite is true) and if the target already
 * exists, it will be unlinked first. */
gboolean bluesky_rename(BlueSkyInode *dir1, gchar *name1,
                        BlueSkyInode *dir2, gchar *name2,
                        gboolean case_sensitive,
                        gboolean overwrite)
{
    g_return_val_if_fail(dir1->type == BLUESKY_DIRECTORY, FALSE);
    g_return_val_if_fail(dir2->type == BLUESKY_DIRECTORY, FALSE);

    BlueSkyDirent *d1, *d2;

    d1 = g_hash_table_lookup(case_sensitive ? dir1->dirhash
                                            : dir1->dirhash_folded, name1);
    d2 = g_hash_table_lookup(case_sensitive ? dir2->dirhash
                                            : dir2->dirhash_folded, name2);

    if (d1 == NULL)
        return FALSE;

    uint64_t inum = d1->inum;

    /* Check that this rename does not cause a directory to be moved into one
     * of its descendants, as that would create a loop of directories
     * disconnected from the root. */
    /* TODO */

    if (d2 != NULL) {
        if (!overwrite)
            return FALSE;

        bluesky_directory_remove(dir2, name2);

        // TODO: Drop inode reference
    }

    bluesky_directory_remove(dir1, name1);
    bluesky_directory_insert(dir2, name2, inum);

    return TRUE;
}

/* Dump the contents of a directory to stdout.  Debugging only. */
void bluesky_directory_dump(BlueSkyInode *dir)
{
    g_print("Directory dump:\n");

    GSequenceIter *i = g_sequence_get_begin_iter(dir->dirents);

    while (!g_sequence_iter_is_end(i)) {
        BlueSkyDirent *d = g_sequence_get(i);
        g_print("    0x%08x [inum=%"PRIu64"] %s\n",
                d->cookie, d->inum, d->name);
        i = g_sequence_iter_next(i);
    }
}
