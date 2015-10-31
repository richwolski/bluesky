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

/* Declarations internal to the BlueSky library.  This header file should not
 * be included by any users of the library (such as any filesystem
 * proxy)--external users should only include bluesky.h. */

#ifndef _BLUESKY_PRIVATE_H
#define _BLUESKY_PRIVATE_H

#include "bluesky.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int bluesky_verbose;

/* Target cache size levels. */
extern int bluesky_watermark_low_dirty;
extern int bluesky_watermark_medium_dirty;
extern int bluesky_watermark_med2_dirty;
extern int bluesky_watermark_high_dirty;

extern int bluesky_watermark_low_total;
extern int bluesky_watermark_medium_total;
extern int bluesky_watermark_med2_total;
extern int bluesky_watermark_high_total;

/* TODO: Make this go away entirely. */
BlueSkyFS *bluesky_new_fs(gchar *name);

void bluesky_inode_free_resources(BlueSkyInode *inode);

/* Linked list update functions for LRU lists. */
void bluesky_list_unlink(GList *head, GList *item);
GList *bluesky_list_prepend(GList *head, BlueSkyInode *inode);
GList *bluesky_list_append(GList *head, BlueSkyInode *inode);
BlueSkyInode *bluesky_list_head(GList *head);
BlueSkyInode *bluesky_list_tail(GList *head);

/* Serialization and deserialization of filesystem data for storing to
 * persistent storage. */
void bluesky_serialize_superblock(GString *out, BlueSkyFS *fs);
BlueSkyFS *bluesky_deserialize_superblock(const gchar *buf);
BlueSkyCloudLog *bluesky_serialize_inode(BlueSkyInode *inode);
gboolean bluesky_deserialize_inode(BlueSkyInode *inode, BlueSkyCloudLog *item);

void bluesky_deserialize_cloudlog(BlueSkyCloudLog *item,
                                  const char *data,
                                  size_t len);

void bluesky_serialize_cloudlog(BlueSkyCloudLog *log,
                                GString *encrypted,
                                GString *authenticated,
                                GString *writable);

/* Cryptographic operations. */
#define CRYPTO_BLOCK_SIZE 16        /* 128-bit AES */
#define CRYPTO_KEY_SIZE   16
#define CRYPTO_HASH_SIZE  32        /* SHA-256 */

typedef struct BlueSkyCryptKeys {
    uint8_t encryption_key[CRYPTO_KEY_SIZE];
    uint8_t authentication_key[CRYPTO_HASH_SIZE];
} BlueSkyCryptKeys;

void bluesky_crypt_init();
void bluesky_crypt_hash_key(const char *keystr, uint8_t *out);
void bluesky_crypt_random_bytes(guchar *buf, gint len);
void bluesky_crypt_derive_keys(BlueSkyCryptKeys *keys, const gchar *master);
BlueSkyRCStr *bluesky_crypt_encrypt(BlueSkyRCStr *in, const uint8_t *key);
BlueSkyRCStr *bluesky_crypt_decrypt(BlueSkyRCStr *in, const uint8_t *key);

void bluesky_crypt_block_encrypt(gchar *cloud_block, size_t len,
                                 BlueSkyCryptKeys *keys);
gboolean bluesky_crypt_block_decrypt(gchar *cloud_block, size_t len,
                                     BlueSkyCryptKeys *keys,
                                     gboolean allow_unauth);
void bluesky_cloudlog_encrypt(GString *segment, BlueSkyCryptKeys *keys);
void bluesky_cloudlog_decrypt(char *segment, size_t len,
                              BlueSkyCryptKeys *keys,
                              BlueSkyRangeset *items,
                              gboolean allow_unauth);

/* Storage layer.  Requests can be performed asynchronously, so these objects
 * help keep track of operations in progress. */
typedef enum {
    STORE_OP_NONE,
    STORE_OP_GET,
    STORE_OP_PUT,
    STORE_OP_DELETE,
    STORE_OP_BARRIER,       // Waits for other selected operations to complete
} BlueSkyStoreOp;

typedef enum {
    ASYNC_NEW,              // Operation not yet submitted to storage layer
    ASYNC_PENDING,          // Submitted to storage layer
    ASYNC_RUNNING,          // Operation is in progress
    ASYNC_COMPLETE,         // Operation finished, results available
} BlueSkyAsyncStatus;

struct BlueSkyNotifierList;
typedef struct BlueSkyStoreAsync BlueSkyStoreAsync;
struct BlueSkyStoreAsync {
    BlueSkyStore *store;

    GMutex *lock;
    GCond *completion_cond;     /* Used to wait for operation to complete. */

    gint refcount;              /* Reference count for destruction. */

    BlueSkyAsyncStatus status;

    BlueSkyStoreOp op;
    gchar *key;                 /* Key to read/write */
    BlueSkyRCStr *data;         /* Data read/to write */

    /* For range requests on reads: starting byte offset and length; len 0
     * implies reading to the end of the object.  At completion, the backend
     * should set range_done if a range read was made; if not set the entire
     * object was read and the storage layer will select out just the
     * appropriate bytes. */
    size_t start, len;
    gboolean range_done;

    int result;                 /* Result code; 0 for success. */
    struct BlueSkyNotifierList *notifiers;
    gint notifier_count;

    /* The barrier waiting on this operation.  Support for more than one
     * barrier for a single async is not well-supported and should be avoided
     * if possible. */
    BlueSkyStoreAsync *barrier;

    bluesky_time_hires start_time;  /* Time operation was submitted. */
    bluesky_time_hires exec_time;   /* Time processing started on operation. */

    gpointer store_private;     /* For use by the storage implementation */

    /* If storage operations should be charged to any particular profile, which
     * one? */
    BlueSkyProfile *profile;
};

/* Support for notification lists.  These are lists of one-shot functions which
 * can be called when certain events--primarily, competed storage
 * events--occur.  Multiple notifiers can be added, but no particular order is
 * guaranteed for the notification functions to be called. */
struct BlueSkyNotifierList {
    struct BlueSkyNotifierList *next;
    GFunc func;
    BlueSkyStoreAsync *async;
    gpointer user_data;     // Passed to the function when called
};

/* The abstraction layer for storage, allowing multiple implementations. */
typedef struct {
    /* Create a new store instance and return a handle to it. */
    gpointer (*create)(const gchar *path);

    /* Clean up any resources used by this store. */
    void (*destroy)(gpointer store);

    /* Submit an operation (get/put/delete) to the storage layer to be
     * performed asynchronously. */
    void (*submit)(gpointer store, BlueSkyStoreAsync *async);

    /* Clean up any implementation-private data in a BlueSkyStoreAsync. */
    void (*cleanup)(gpointer store, BlueSkyStoreAsync *async);

    /* Find the lexicographically-largest file starting with the specified
     * prefix. */
    char * (*lookup_last)(gpointer store, const gchar *prefix);
} BlueSkyStoreImplementation;

void bluesky_store_register(const BlueSkyStoreImplementation *impl,
                            const gchar *name);

char *bluesky_store_lookup_last(BlueSkyStore *store, const char *prefix);
BlueSkyStoreAsync *bluesky_store_async_new(BlueSkyStore *store);
gpointer bluesky_store_async_get_handle(BlueSkyStoreAsync *async);
void bluesky_store_async_ref(BlueSkyStoreAsync *async);
void bluesky_store_async_unref(BlueSkyStoreAsync *async);
void bluesky_store_async_wait(BlueSkyStoreAsync *async);
void bluesky_store_async_add_notifier(BlueSkyStoreAsync *async,
                                      GFunc func, gpointer user_data);
void bluesky_store_async_mark_complete(BlueSkyStoreAsync *async);
void bluesky_store_async_submit(BlueSkyStoreAsync *async);
void bluesky_store_sync(BlueSkyStore *store);

void bluesky_store_add_barrier(BlueSkyStoreAsync *barrier,
                               BlueSkyStoreAsync *async);

void bluesky_inode_start_sync(BlueSkyInode *inode);

void bluesky_block_touch(BlueSkyInode *inode, uint64_t i, gboolean preserve);
void bluesky_block_fetch(BlueSkyInode *inode, BlueSkyBlock *block,
                         BlueSkyStoreAsync *barrier);
void bluesky_block_flush(BlueSkyInode *inode, BlueSkyBlock *block,
                         GList **log_items);
void bluesky_file_flush(BlueSkyInode *inode, GList **log_items);
void bluesky_file_drop_cached(BlueSkyInode *inode);

/* Writing of data to the cloud in log segments and tracking the location of
 * various pieces of data (both where in the cloud and where cached locally).
 * */

/* Eventually we'll want to support multiple writers.  But for now, hard-code
 * separate namespaces in the cloud for the proxy and the cleaner to write to.
 * */
#define BLUESKY_CLOUD_DIR_PRIMARY 0
#define BLUESKY_CLOUD_DIR_CLEANER 1

typedef struct {
    char bytes[16];
} BlueSkyCloudID;

typedef struct {
    uint32_t directory;
    uint32_t sequence;
    uint32_t offset;
    uint32_t size;
} BlueSkyCloudPointer;

typedef enum {
    LOGTYPE_INVALID = -1,
    LOGTYPE_UNKNOWN = 0,
    LOGTYPE_DATA = 1,
    LOGTYPE_INODE = 2,
    LOGTYPE_INODE_MAP = 3,
    LOGTYPE_CHECKPOINT = 4,

    /* Used only as metadata in the local journal, not loaded as a
     * BlueSkyCloudLogState nor stored in the cloud */
    LOGTYPE_JOURNAL_MARKER = 16,
    LOGTYPE_JOURNAL_CHECKPOINT = 17,
} BlueSkyCloudLogType;

/* Headers that go on items in local log segments and cloud log segments. */
struct log_header {
    uint32_t magic;             // HEADER_MAGIC
    uint8_t type;               // Object type + '0'
    uint32_t offset;            // Starting byte offset of the log header
    uint32_t size1;             // Size of the data item (bytes)
    uint32_t size2;             //
    uint32_t size3;             //
    uint64_t inum;              // Inode which owns this data, if any
    BlueSkyCloudID id;          // Object identifier
} __attribute__((packed));

struct log_footer {
    uint32_t magic;             // FOOTER_MAGIC
    uint32_t crc;               // Computed from log_header to log_footer.magic
} __attribute__((packed));

struct cloudlog_header {
    char magic[4];
    uint8_t crypt_auth[CRYPTO_HASH_SIZE];
    uint8_t crypt_iv[CRYPTO_BLOCK_SIZE];
    uint8_t type;
    BlueSkyCloudID id;
    uint64_t inum;
    uint32_t size1, size2, size3;
} __attribute__((packed));

// Rough size limit for a log segment.  This is not a firm limit and there are
// no absolute guarantees on the size of a log segment.
#define LOG_SEGMENT_SIZE (1 << 22)

#define JOURNAL_MAGIC "\nLog"
#define CLOUDLOG_MAGIC "AgI-"
#define CLOUDLOG_MAGIC_ENCRYPTED "AgI="     // CLOUDLOG_MAGIC[3] ^= 0x10

/* A record which tracks an object which has been written to a local log,
 * cached, locally, and/or written to the cloud. */
#define CLOUDLOG_JOURNAL    0x01
#define CLOUDLOG_CLOUD      0x02
#define CLOUDLOG_CACHE      0x04
#define CLOUDLOG_UNCOMMITTED 0x10
struct BlueSkyCloudLog {
    gint refcount;
    GMutex *lock;
    GCond *cond;

    BlueSkyFS *fs;

    BlueSkyCloudLogType type;

    // Bitmask of CLOUDLOG_* flags indicating where the object exists.
    int location_flags;
    int pending_read, pending_write;

    // A stable identifier for the object (only changes when authenticated data
    // is written out, but stays the same when the in-cloud cleaner relocates
    // the object).
    BlueSkyCloudID id;

    // The inode which owns this data, if any, and an offset.
    uint64_t inum;
    int32_t inum_offset;

    // The size of encrypted object data, not including any headers
    int data_size;

    // The location of the object in the cloud, if available.
    BlueSkyCloudPointer location;

    // TODO: Location in journal/cache
    int log_seq, log_offset, log_size;

    // Pointers to other objects.  Each link counts towards the reference count
    // of the pointed-to object.  To avoid memory leaks there should be no
    // cycles in the reference graph.
    GArray *links;

    // Serialized data, if available in memory (otherwise NULL), and a lock
    // count which tracks if there are users that require the data to be kept
    // around.
    BlueSkyRCStr *data;
    int data_lock_count;
};

/* Serialize objects into a log segment to be written to the cloud. */
struct BlueSkyCloudLogState {
    GString *data;
    BlueSkyCloudPointer location;
    GList *inode_list;
    GSList *writeback_list;     // Items which are being serialized right now
    GList *pending_segments;    // Segments which are being uploaded now

    int uploads_pending;        // Count of uploads in progress, not completed
    GMutex *uploads_pending_lock;
    GCond *uploads_pending_cond;

    /* What is the most recent sequence number written by the cleaner which we
     * have processed and incorporated into our own log?  This gets
     * incorporated into the version vector written out with our checkpoint
     * records. */
    int latest_cleaner_seq_seen;
};

gboolean bluesky_cloudlog_equal(gconstpointer a, gconstpointer b);
guint bluesky_cloudlog_hash(gconstpointer a);
BlueSkyCloudLog *bluesky_cloudlog_new(BlueSkyFS *fs, const BlueSkyCloudID *id);
gchar *bluesky_cloudlog_id_to_string(BlueSkyCloudID id);
BlueSkyCloudID bluesky_cloudlog_id_from_string(const gchar *idstr);
void bluesky_cloudlog_threads_init(BlueSkyFS *fs);
void bluesky_cloudlog_ref(BlueSkyCloudLog *log);
void bluesky_cloudlog_unref(BlueSkyCloudLog *log);
void bluesky_cloudlog_unref_delayed(BlueSkyCloudLog *log);
void bluesky_cloudlog_erase(BlueSkyCloudLog *log);
void bluesky_cloudlog_stats_update(BlueSkyCloudLog *log, int type);
void bluesky_cloudlog_sync(BlueSkyCloudLog *log);
void bluesky_cloudlog_insert(BlueSkyCloudLog *log);
void bluesky_cloudlog_insert_locked(BlueSkyCloudLog *log);
BlueSkyCloudLog *bluesky_cloudlog_get(BlueSkyFS *fs, BlueSkyCloudID id);
void bluesky_cloudlog_prefetch(BlueSkyCloudLog *log);
void bluesky_cloudlog_fetch(BlueSkyCloudLog *log);
void bluesky_cloudlog_background_fetch(BlueSkyCloudLog *item);
BlueSkyCloudPointer bluesky_cloudlog_serialize(BlueSkyCloudLog *log,
                                               BlueSkyFS *fs);
void bluesky_cloudlog_flush(BlueSkyFS *fs);

/* Logging infrastructure for ensuring operations are persistently recorded to
 * disk. */
#define BLUESKY_CRC32C_SEED (~(uint32_t)0)
#define BLUESKY_CRC32C_VALIDATOR ((uint32_t)0xb798b438UL)
uint32_t crc32c(uint32_t crc, const char *buf, unsigned int length);
uint32_t crc32c_finalize(uint32_t crc);

struct BlueSkyLog {
    BlueSkyFS *fs;
    char *log_directory;
    GAsyncQueue *queue;
    int fd, dirfd;
    int seq_num;
    GSList *committed;

    /* The currently-open log file. */
    BlueSkyCacheFile *current_log;

    /* Cache of log segments which have been memory-mapped. */
    GMutex *mmap_lock;
    GHashTable *mmap_cache;

    /* A count of the disk space consumed (in 1024-byte units) by all files
     * tracked by mmap_cache (whether mapped or not, actually). */
    gint disk_used;

    /* The smallest journal sequence number which may still contain data that
     * must be preserved (since it it not yet in the cloud). */
    int journal_watermark;
};

/* An object for tracking log files which are stored locally--either the
 * journal for filesystem consistency or log segments which have been fetched
 * back from cloud storage. */
struct BlueSkyCacheFile {
    GMutex *lock;
    GCond *cond;
    gint refcount;
    int type;                   // Only one of CLOUDLOG_{JOURNAL,CLOUD}
    int log_dir;
    int log_seq;
    char *filename;             // Local filename, relateive to log directory
    gint mapcount;              // References to the mmaped data
    const char *addr;           // May be null if data is not mapped in memory
    size_t len;
    int disk_used;
    BlueSkyFS *fs;
    BlueSkyLog *log;
    gboolean fetching;          // Cloud data: downloading or ready for use
    gboolean complete;          // Complete file has been fetched from cloud
    int64_t atime;              // Access time, for cache management
    BlueSkyRangeset *items;     // Locations of valid items
    BlueSkyRangeset *prefetches;// Locations we have been requested to prefetch
};

BlueSkyLog *bluesky_log_new(const char *log_directory);
void bluesky_log_item_submit(BlueSkyCloudLog *item, BlueSkyLog *log);
void bluesky_log_finish_all(GList *log_items);
BlueSkyCloudLog *bluesky_log_get_commit_point(BlueSkyFS *fs);
void bluesky_log_write_commit_point(BlueSkyFS *fs, BlueSkyCloudLog *marker);

BlueSkyRCStr *bluesky_cachefile_map_raw(BlueSkyCacheFile *cachefile,
                                        off_t offset, size_t size);
BlueSkyRCStr *bluesky_log_map_object(BlueSkyCloudLog *item, gboolean map_data);
void bluesky_mmap_unref(BlueSkyCacheFile *mmap);
void bluesky_cachefile_unref(BlueSkyCacheFile *cachefile);

BlueSkyCacheFile *bluesky_cachefile_lookup(BlueSkyFS *fs,
                                           int clouddir, int log_seq,
                                           gboolean start_fetch);
void bluesky_cachefile_gc(BlueSkyFS *fs);

void bluesky_replay(BlueSkyFS *fs);

/* Used to track log segments that are being written to the cloud. */
typedef struct {
    BlueSkyFS *fs;
    char *key;                  /* File name for log segment in backend */
    GString *raw_data;          /* Data before encryption */
    BlueSkyRCStr *data;         /* Data after encryption */
    GSList *items;
    GMutex *lock;
    GCond *cond;
    gboolean complete;
} SerializedRecord;

/***** Inode map management *****/

/* Mapping information for a single inode number.  These are grouped together
 * into InodeMapRange objects. */
typedef struct {
    uint64_t inum;

    /* A pointer to the cloud log entry for this inode.  This may or may not
     * actually have data loaded (it might just contain pointers to the data
     * location, and in fact this will likely often be the case). */
    BlueSkyCloudLog *item;
} InodeMapEntry;

typedef struct {
    /* Starting and ending inode number values that fall in this section.
     * Endpoint values are inclusive. */
    uint64_t start, end;

    /* A sorted list (by inode number) of InodeMapEntry objects. */
    GSequence *map_entries;

    /* The serialized version of the inode map data. */
    BlueSkyCloudLog *serialized;

    /* Have there been changes that require writing this section out again? */
    gboolean dirty;
} InodeMapRange;

InodeMapEntry *bluesky_inode_map_lookup(GSequence *inode_map, uint64_t inum,
                                        int action);
BlueSkyCloudLog *bluesky_inode_map_serialize(BlueSkyFS *fs);
void bluesky_inode_map_minimize(BlueSkyFS *fs);

gboolean bluesky_checkpoint_load(BlueSkyFS *fs);

/* Merging of log state with the work of the cleaner. */
void bluesky_cleaner_merge(BlueSkyFS *fs);
void bluesky_cleaner_thread_launch(BlueSkyFS *fs);

#ifdef __cplusplus
}
#endif

#endif
