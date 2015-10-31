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
#include <stdio.h>
#include <stdint.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "bluesky-private.h"

/* Miscellaneous useful functions that don't really fit anywhere else. */

bluesky_time_hires bluesky_now_hires()
{
    struct timespec time;

    if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
        perror("clock_gettime");
        return 0;
    }

    return (int64_t)(time.tv_sec) * 1000000000 + time.tv_nsec;
}

/* Convert a UTF-8 string to lowercase.  This can be used to implement
 * case-insensitive lookups and comparisons, by normalizing all values to
 * lowercase first.  Returns a newly-allocated string as a result. */
gchar *bluesky_lowercase(const gchar *s)
{
    /* TODO: Unicode handling; for now just do ASCII. */
    return g_ascii_strdown(s, -1);
}

gboolean bluesky_inode_is_ready(BlueSkyInode *inode)
{
    if (inode == NULL)
        return FALSE;

    g_mutex_lock(inode->lock);
    gboolean valid = (inode->type != BLUESKY_PENDING
                      && inode->type != BLUESKY_INVALID);

    g_mutex_unlock(inode->lock);

    return valid;
}

/**** Reference-counted strings. ****/

/* Create and return a new reference-counted string.  The reference count is
 * initially one.  The newly-returned string takes ownership of the memory
 * pointed at by data, and will call g_free on it when the reference count
 * drops to zero. */
BlueSkyRCStr *bluesky_string_new(gpointer data, gsize len)
{
    BlueSkyRCStr *string = g_new(BlueSkyRCStr, 1);
    string->mmap = NULL;
    string->data = data;
    string->len = len;
    g_atomic_int_set(&string->refcount, 1);
    return string;
}

/* Create a new BlueSkyRCStr from a GString.  The GString is destroyed. */
BlueSkyRCStr *bluesky_string_new_from_gstring(GString *s)
{
    gsize len = s->len;
    return bluesky_string_new(g_string_free(s, FALSE), len);
}

/* Create a new BlueSkyRCStr from a memory-mapped buffer. */
BlueSkyRCStr *bluesky_string_new_from_mmap(BlueSkyCacheFile *mmap,
                                           int offset, gsize len)
{
    g_assert(offset + len <= mmap->len);
    g_assert(mmap->addr != NULL);

    BlueSkyRCStr *string = g_new(BlueSkyRCStr, 1);
    string->mmap = mmap;
    g_atomic_int_inc(&mmap->mapcount);
    string->data = (char *)mmap->addr + offset;
    string->len = len;
    g_atomic_int_set(&string->refcount, 1);
    return string;
}

void bluesky_string_ref(BlueSkyRCStr *string)
{
    if (string == NULL)
        return;

    g_atomic_int_inc(&string->refcount);
}

void bluesky_string_unref(BlueSkyRCStr *string)
{
    if (string == NULL)
        return;

    if (g_atomic_int_dec_and_test(&string->refcount)) {
        if (string->mmap == NULL) {
            g_free(string->data);
        } else {
            bluesky_mmap_unref(string->mmap);
        }
        g_free(string);
    }
}

/* Duplicate and return a new reference-counted string, containing a copy of
 * the original data, with a reference count of 1.  As an optimization, if the
 * passed-in string already has a reference count of 1, the original is
 * returned.   Can be used to make a mutable copy of a shared string.  For this
 * to truly be safe, it is probably needed that there be some type of lock
 * protecting access to the string. */
BlueSkyRCStr *bluesky_string_dup(BlueSkyRCStr *string)
{
    if (string == NULL)
        return NULL;

    if (string->mmap != NULL) {
        BlueSkyRCStr *s;
        s = bluesky_string_new(g_memdup(string->data, string->len),
                               string->len);
        bluesky_string_unref(string);
        return s;
    }

    if (g_atomic_int_dec_and_test(&string->refcount)) {
        /* There are no other shared copies, so return this one. */
        g_atomic_int_inc(&string->refcount);
        return string;
    } else {
        return bluesky_string_new(g_memdup(string->data, string->len),
                                  string->len);
    }
}

/* Resize the data block used by a BlueSkyRCStr.  The data pointer might change
 * after making this call, so it should not be cached across calls to this
 * function.  To avoid confusing any other users, the caller probably ought to
 * hold the only reference to the string (by calling bluesky_string_dup first
 * if needed). */
void bluesky_string_resize(BlueSkyRCStr *string, gsize len)
{
    g_assert(string->mmap == NULL);

    if (string->len == len)
        return;

    g_warn_if_fail(string->refcount == 1);

    string->data = g_realloc(string->data, len);
    string->len = len;
}

/* Cache LRU list management functions.  These manage the doubly-linked list of
 * inodes sorted by accessed/modified time.  The FS lock should be held while
 * calling these.
 *
 * _remove will unlink an inode from the linked list.
 *
 * _prepend and _append insert an inode at the head or tail of the linked list,
 * and return a pointer to the linked list element (which should be stored in
 * the inode); the inode should not already be in the list.
 *
 * _head and _tail simply return the first or last item inode in the list. */
void bluesky_list_unlink(GList *head, GList *item)
{
    if (item == NULL)
        return;

    if (head->prev == item)
        head->prev = item->prev;
    head->next = g_list_delete_link(head->next, item);
}

GList *bluesky_list_prepend(GList *head, BlueSkyInode *inode)
{
    head->next = g_list_prepend(head->next, inode);
    if (head->prev == NULL)
        head->prev = g_list_last(head->next);
    return head->next;
}

GList *bluesky_list_append(GList *head, BlueSkyInode *inode)
{
    if (head->next == NULL)
        return bluesky_list_prepend(head, inode);

    g_assert(head->prev != NULL && head->prev->next == NULL);

    GList *link = g_list_alloc();
    link->data = inode;
    link->next = NULL;
    link->prev = head->prev;
    head->prev->next = link;
    head->prev = link;
    return link;
}

BlueSkyInode *bluesky_list_head(GList *head)
{
    if (head->next == NULL)
        return NULL;
    else
        return (BlueSkyInode *)head->next->data;
}

BlueSkyInode *bluesky_list_tail(GList *head)
{
    if (head->prev == NULL)
        return NULL;
    else
        return (BlueSkyInode *)head->prev->data;
}

/**** Range sets. ****/

/* These are a data structure which can track a set of discontiguous integer
 * ranges--such as the partitioning of the inode number space or the bytes in a
 * log file into objects.  This current prototype implementation just tracks
 * the starting offset with a hash table and doesn't track the length, but
 * should be extended later to track properly. */

struct BlueSkyRangeset {
    GSequence *seq;
};

static gint compare64(gconstpointer a, gconstpointer b, gpointer user_data)
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

BlueSkyRangeset *bluesky_rangeset_new()
{
    BlueSkyRangeset *rangeset = g_new(BlueSkyRangeset, 1);
    rangeset->seq = g_sequence_new(g_free);
    return rangeset;
}

void bluesky_rangeset_free(BlueSkyRangeset *rangeset)
{
    g_sequence_free(rangeset->seq);
    g_free(rangeset);
}

gboolean bluesky_rangeset_insert(BlueSkyRangeset *rangeset,
                                 uint64_t start, uint64_t length,
                                 gpointer data)
{
    GSequenceIter *i;
    i = g_sequence_search(rangeset->seq, &start, compare64, NULL);
    i = g_sequence_iter_prev(i);

    /* TODO: Checks that no other item overlaps. */

    BlueSkyRangesetItem *item = g_new(BlueSkyRangesetItem, 1);
    item->start = start;
    item->length = length;
    item->data = data;
    g_sequence_insert_sorted(rangeset->seq, item, compare64, NULL);

    return TRUE;
}

const BlueSkyRangesetItem *bluesky_rangeset_lookup(BlueSkyRangeset *rangeset,
                                                   uint64_t offset)
{
    GSequenceIter *i;
    i = g_sequence_search(rangeset->seq, &offset, compare64, NULL);
    i = g_sequence_iter_prev(i);
    if (g_sequence_iter_is_end(i))
        return NULL;

    BlueSkyRangesetItem *item = (BlueSkyRangesetItem *)g_sequence_get(i);
    if (offset >= item->start && offset < item->start + item->length)
        return item;
    else
        return NULL;
}

/* Look up the first rangeset item starting at or following the given address.
 * Can be used to iterate through a rangeset. */
const BlueSkyRangesetItem *bluesky_rangeset_lookup_next(BlueSkyRangeset *rangeset, uint64_t offset)
{
    GSequenceIter *i;
    i = g_sequence_search(rangeset->seq, &offset, compare64, NULL);
    i = g_sequence_iter_prev(i);
    if (g_sequence_iter_is_end(i))
        return NULL;
    BlueSkyRangesetItem *item = (BlueSkyRangesetItem *)g_sequence_get(i);
    if (item->start < offset) {
        i = g_sequence_iter_next(i);
        if (g_sequence_iter_is_end(i))
            item = NULL;
        else
            item = (BlueSkyRangesetItem *)g_sequence_get(i);
    }
    return item;
}

/* Determine the full extent of a rangeset--the smallest offset covered and the
 * length needed to extend to the end of the last item. */
void bluesky_rangeset_get_extents(BlueSkyRangeset *rangeset,
                                  uint64_t *start, uint64_t *length)
{
    GSequenceIter *i;
    BlueSkyRangesetItem *item;

    i = g_sequence_get_begin_iter(rangeset->seq);
    if (g_sequence_iter_is_end(i)) {
        *start = 0;
        *length = 0;
        return;
    }

    item = (BlueSkyRangesetItem *)g_sequence_get(i);
    *start = item->start;

    i = g_sequence_get_end_iter(rangeset->seq);
    i = g_sequence_iter_prev(i);
    item = (BlueSkyRangesetItem *)g_sequence_get(i);
    *length = (item->start + item->length) - *start;
}

/**** Request response-time tracking. ****/
/* TODO: Locking */
typedef struct {
    int tid;
    bluesky_time_hires timestamp;
    char *message;
} RTEvent;

/* To catch attempts to access to invalid profile structures. */
#define PROFILE_MAGIC 0x439929d8

static FILE *profiling_file = NULL;

BlueSkyProfile *bluesky_profile_new()
{
    BlueSkyProfile *profile = g_new0(BlueSkyProfile, 1);
    profile->lock = g_mutex_new();
    profile->magic = PROFILE_MAGIC;
    return profile;
}

void bluesky_profile_free(BlueSkyProfile *profile)
{
    if (profile->magic != PROFILE_MAGIC) {
        g_warning("Access to invalid BlueSkyProfile object!");
        return;
    }
    while (profile->events != NULL) {
        RTEvent *event = (RTEvent *)profile->events->data;
        g_free(event->message);
        g_free(event);
        profile->events = g_list_delete_link(profile->events, profile->events);
    }
    profile->magic = 0;
    g_mutex_free(profile->lock);
    g_free(profile->description);
    g_free(profile);
}

void bluesky_profile_add_event(BlueSkyProfile *profile, char *message)
{
    if (profiling_file == NULL)
        return;

    g_return_if_fail(profile != NULL);

    if (profile->magic != PROFILE_MAGIC) {
        g_warning("Access to invalid BlueSkyProfile object!");
        return;
    }
    g_mutex_lock(profile->lock);
    RTEvent *event = g_new(RTEvent, 1);
    event->timestamp = bluesky_now_hires();
    /* FIXME: Non-portable */
    event->tid = syscall(SYS_gettid);
    event->message = message;
    profile->events = g_list_prepend(profile->events, event);
    g_mutex_unlock(profile->lock);
}

static GStaticMutex profiling_print_lock = G_STATIC_MUTEX_INIT;

void bluesky_profile_set_output(FILE *stream)
{
    profiling_file = stream;
}

void bluesky_profile_print(BlueSkyProfile *profile)
{
    FILE *stream = profiling_file;
    if (stream == NULL)
        return;

    g_return_if_fail(profile != NULL);

    if (profile->magic != PROFILE_MAGIC) {
        g_warning("Access to invalid BlueSkyProfile object!");
        return;
    }

    g_mutex_lock(profile->lock);
    g_static_mutex_lock(&profiling_print_lock);
    fprintf(stream, "Event Timeline: %s\n", profile->description);
    GList *link = g_list_last(profile->events);
    bluesky_time_hires last_time = 0;
    while (link != NULL) {
        RTEvent *event = (RTEvent *)link->data;
        fprintf(stream, "  [%d] [%"PRIi64" ns]: %s\n",
                event->tid, event->timestamp - last_time, event->message);
        last_time = event->timestamp;
        link = link->prev;
    }
    g_static_mutex_unlock(&profiling_print_lock);
    g_mutex_unlock(profile->lock);
}

static GStaticPrivate per_thread_profile = G_STATIC_PRIVATE_INIT;

BlueSkyProfile *bluesky_profile_get()
{
    return (BlueSkyProfile *)g_static_private_get(&per_thread_profile);
}

void bluesky_profile_set(BlueSkyProfile *profile)
{
    g_static_private_set(&per_thread_profile, profile, NULL);
}
