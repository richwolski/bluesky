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

#ifndef _BLUESKY_H
#define _BLUESKY_H

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Various options to tweak for performance benchmarking purposes. */
typedef struct {
    /* OBSOLETE: Perform all get/put operations synchronously. */
    int synchronous_stores;

    /* OBSOLETE: Write data in cache immediately after file is modified. */
    int writethrough_cache;

    /* Can inodes be fetched asynchronously?  (Inode object is initially
     * created in a pending state, and not unlocked until the data is actually
     * available.) */
    int sync_inode_fetches;

    /* Should frontends handle requests serially or allow operations to proceed
     * in parallel? */
    int sync_frontends;

    /* Target size of the disk cache at the proxy, in kilobytes. */
    int cache_size;

    /* Full segment fetches (1) or use range requests (0) for fetching segments
     * from cloud? */
    int full_segment_fetches;

    /* Disable aggregating of data into log segments.  Each object will be
     * stored in a separate segment. */
    int disable_aggregation;

    /* Disable cryptography.  This is for benchmarking purposes. */
    int disable_crypto;

    /* Disable aggregation of read requests.  Fetch items individually. */
    int disable_read_aggregation;
} BlueSkyOptions;

extern BlueSkyOptions bluesky_options;

/* Maximum number of threads to use in any particular thread pool, or -1 for no
 * limit */
extern int bluesky_max_threads;

/* A general-purpose counter for gathering run-time statistics. */
struct bluesky_stats {
    const char *name;
    int64_t count;
    int64_t sum;
};
struct bluesky_stats *bluesky_stats_new(const char *name);
void bluesky_stats_add(struct bluesky_stats *stats, int64_t value);
void bluesky_stats_dump_all();
void bluesky_stats_run_periodic_dump(FILE *f);

/* BlueSky status and error codes.  Various frontends should translate these to
 * the appropriate error code for whatever protocol they implement. */
typedef enum {
    BSTATUS_OK = 0,             /* No error */
    BSTATUS_IOERR,              /* I/O error of some form */
    BSTATUS_NOENT,              /* File does not exist */
} BlueSkyStatus;

void bluesky_init(void);

gchar *bluesky_lowercase(const gchar *s);

struct BlueSkyCacheFile;
typedef struct BlueSkyCacheFile BlueSkyCacheFile;

typedef struct {
    gint refcount;
    BlueSkyCacheFile *mmap;
    gchar *data;
    gsize len;
} BlueSkyRCStr;

BlueSkyRCStr *bluesky_string_new(gpointer data, gsize len);
BlueSkyRCStr *bluesky_string_new_from_gstring(GString *s);
BlueSkyRCStr *bluesky_string_new_from_mmap(BlueSkyCacheFile *mmap,
                                           int offset, gsize len);
void bluesky_string_ref(BlueSkyRCStr *string);
void bluesky_string_unref(BlueSkyRCStr *string);
BlueSkyRCStr *bluesky_string_dup(BlueSkyRCStr *string);
void bluesky_string_resize(BlueSkyRCStr *string, gsize len);

struct BlueSkyRangeset;
typedef struct BlueSkyRangeset BlueSkyRangeset;
typedef struct {
    uint64_t start, length;
    gpointer data;
} BlueSkyRangesetItem;

BlueSkyRangeset *bluesky_rangeset_new();
void bluesky_rangeset_free(BlueSkyRangeset *rangeset);
gboolean bluesky_rangeset_insert(BlueSkyRangeset *rangeset,
                                 uint64_t start, uint64_t length,
                                 gpointer data);
const BlueSkyRangesetItem *bluesky_rangeset_lookup(BlueSkyRangeset *rangeset,
                                                   uint64_t offset);
const BlueSkyRangesetItem *bluesky_rangeset_lookup_next(BlueSkyRangeset *rangeset, uint64_t offset);
void bluesky_rangeset_get_extents(BlueSkyRangeset *rangeset,
                                  uint64_t *start, uint64_t *length);

/* Storage interface.  This presents a key-value store abstraction, and can
 * have multiple implementations: in-memory, on-disk, in-cloud. */
struct BlueSkyStore;
typedef struct BlueSkyStore BlueSkyStore;

struct BlueSkyLog;
typedef struct BlueSkyLog BlueSkyLog;

struct BlueSkyCloudLogState;
typedef struct BlueSkyCloudLogState BlueSkyCloudLogState;

struct BlueSkyCloudLog;
typedef struct BlueSkyCloudLog BlueSkyCloudLog;

void bluesky_store_init();
BlueSkyStore *bluesky_store_new(const gchar *type);
void bluesky_store_free(BlueSkyStore *store);
BlueSkyRCStr *bluesky_store_get(BlueSkyStore *store, const gchar *key);
void bluesky_store_put(BlueSkyStore *store,
                       const gchar *key, BlueSkyRCStr *val);

/* File types.  The numeric values are chosen to match with those used in
 * NFSv3. */
typedef enum {
    BLUESKY_REGULAR = 1,
    BLUESKY_DIRECTORY = 2,
    BLUESKY_BLOCK = 3,
    BLUESKY_CHARACTER = 4,
    BLUESKY_SYMLINK = 5,
    BLUESKY_SOCKET = 6,
    BLUESKY_FIFO = 7,

    /* Special types used only internally. */
    BLUESKY_PENDING = 0,    /* Inode being loaded; type not yet determined */
    BLUESKY_INVALID = -1,   /* Inode is invalid (failed to load) */
} BlueSkyFileType;

/* Filesystem state.  Each filesystem which is exported is represented by a
 * single bluesky_fs structure in memory. */
struct BlueSkyCryptKeys;

typedef struct {
    GMutex *lock;

    gchar *name;                /* Descriptive name for the filesystem */
    GHashTable *inodes;         /* Cached inodes */
    uint64_t next_inum;         /* Next available inode for allocation */

    BlueSkyStore *store;
    BlueSkyLog *log;
    BlueSkyCloudLogState *log_state;

    /* Filesystem crypto keys */
    char *master_key;
    struct BlueSkyCryptKeys *keys;

    /* Accounting for memory used for caches.  Space is measured in blocks, not
     * bytes.  Updates to these variables must be made atomically. */
    gint cache_dirty;

    /* Like above, but tracking data stored in the cloudlog entries
     * specifically:
     *  - cache_log_dirty: data uncommitted to journal and cloud
     *  - cache_log_writeback: data being written to journal
     *  - cache_log_journal: data committed to journal
     *  - cache_log_cloud: data written to cloud as well
     * Log entries should progress from the top state to the bottom, and are
     * only ever counted in one category at a time. */
    gint cache_log_dirty, cache_log_writeback,
         cache_log_journal, cache_log_cloud;

    /* Linked list of inodes, sorted by access/modification times for cache
     * management.  Editing these lists is protected by the filesystem lock; to
     * avoid deadlock do not attempt to take any other locks while the FS lock
     * is held for list editing purposes.  Items at the head of the list are
     * most recently accessed/modified. */
    GList unlogged_list;        // Changes not yet synced to journal
    GList dirty_list;           // Not yet written to cloud storage
    GList accessed_list;        // All in-memory inodes

    /* Mutex for the flush daemon, to prevent concurrent execution. */
    GMutex *flushd_lock;

    /* Used to wait for the cache daemon to free up space */
    GCond *flushd_cond;

    /* Mapping of object identifiers (blocks, inodes) to physical location (in
     * the local cache or in the logs in the cloud). */
    GHashTable *locations;

    /* The inode map, which maps inode numbers to the location of the most
     * recent version. */
    GSequence *inode_map;

    /* Queue for asynchronous cloudlog unrefs, where needed. */
    GAsyncQueue *unref_queue;

    /* Thread pool for asynchronous inode fetches */
    GThreadPool *inode_fetch_thread_pool;
} BlueSkyFS;

/* Inode number of the root directory. */
#define BLUESKY_ROOT_INUM 1

/* Timestamp, measured in microseconds since the Unix epoch. */
typedef int64_t bluesky_time;

/* High-resolution timer, measured in nanoseconds. */
typedef int64_t bluesky_time_hires;
bluesky_time_hires bluesky_now_hires();

/* In-memory representation of an inode within a Blue Sky server.  This
 * corresponds roughly with information that is committed to persistent
 * storage.  Locking/refcounting rules:
 *   - To access or modify any data fields, the lock must be held.  This
 *     includes file blocks.
 *   - One reference is held by the BlueSkyFS inode hash table.  If that is the
 *     only reference (and the inode is unlocked), the inode is subject to
 *     dropping from the cache.
 *   - Any pending operations should hold extra references to the inode as
 *     appropriate to keep it available until the operation completes.
 *   - Locking dependency order is, when multiple locks are to be acquired, to
 *     acquire locks on parents in the filesystem tree before children.
 *     (TODO: What about rename when we acquire locks in unrelated parts of the
 *     filesystem?)
 *   - An inode should not be locked while the filesystem lock is already held,
 *     since some code may do an inode lookup (which acquires the filesystem
 *     lock) while a different inode is locked.
 * */
typedef struct {
    GMutex *lock;
    gint refcount;

    BlueSkyFS *fs;

    BlueSkyFileType type;
    uint32_t mode;
    uint32_t uid, gid;
    uint32_t nlink;

    /* Rather than track an inode number and generation number, we will simply
     * never re-use a fileid after a file is deleted.  64 bits should be enough
     * that we don't exhaust the identifier space. */
    uint64_t inum;

    /* change_count is increased with every operation which modifies the inode,
     * and can be used to determine if cached data is still valid.
     * change_commit is the value of change_count when the inode was last
     * committed to stable storage (the log).
     * change_cloud tracks which version was last commited to cloud storage. */
    uint64_t change_count, change_commit, change_cloud;

    /* Timestamp for controlling when modified data is flushed to stable
     * storage.  When an inode is first modified from a clean state, this is
     * set to the current time.  If the inode is clean, it is set to zero. */
    int64_t change_time;

    /* Last access time to this inode, for controlling cache evictions. */
    int64_t access_time;

    /* Version of the object last serialized and committed to storage. */
    BlueSkyCloudLog *committed_item;

    /* Pointers to the linked-list elements for this inode in the accessed and
     * dirty linked lists.  We re-use the GList structure, using ->next to
     * point to the head of the list and ->prev to point to the tail.  The data
     * element is unused. */
    GList *unlogged_list, *accessed_list, *dirty_list;

    int64_t atime;              /* Microseconds since the Unix epoch */
    int64_t ctime;
    int64_t mtime;
    int64_t ntime;              /* "new time": time object was created */

    /* File-specific fields */
    uint64_t size;
    GArray *blocks;

    /* Directory-specific fields */
    GSequence *dirents;         /* List of entries for READDIR */
    GHashTable *dirhash;        /* Hash table by name for LOOKUP */
    GHashTable *dirhash_folded; /* As above, but case-folded */
    uint64_t parent_inum;       /* inode for ".."; 0 if the root directory */

    /* Symlink-specific fields */
    gchar *symlink_contents;

    /* A field for short-term use internally while the lock is held. */
    gpointer private_data;
} BlueSkyInode;

/* A directory entry.  The name is UTF-8 and is a freshly-allocated string.
 * Each directory entry is listed in two indices: dirents is indexed by cookie
 * and dirhash by name.  The cookie is a randomly-assigned 32-bit value, unique
 * within the directory, that remains unchanged until the entry is deleted.  It
 * is used to provide a stable key for restarting a READDIR call. */
typedef struct {
    gchar *name;
    gchar *name_folded;         /* Name, folded for case-insensitive lookup */
    uint32_t cookie;
    uint64_t inum;
} BlueSkyDirent;

/* File data is divided into fixed-size blocks (except the last block which may
 * be short?).  These blocks are backed by storage in a key/value store, but
 * may also be dirty if modifications have been made in-memory that have not
 * been committed. */
#define BLUESKY_BLOCK_SIZE 32768ULL
#define BLUESKY_MAX_FILE_SIZE (BLUESKY_BLOCK_SIZE << 24)
typedef enum {
    BLUESKY_BLOCK_ZERO = 0,     /* Data is all zeroes, not explicitly stored */
    BLUESKY_BLOCK_REF = 1,      /* Reference to cloud log item, data clean */
    BLUESKY_BLOCK_DIRTY = 2,    /* Data needs to be committed to store */
} BlueSkyBlockType;

typedef struct {
    BlueSkyBlockType type;
    BlueSkyCloudLog *ref;       /* if REF: cloud log entry with data */
    BlueSkyRCStr *dirty;        /* if DIRTY: raw data in memory */
} BlueSkyBlock;

BlueSkyFS *bluesky_init_fs(gchar *name, BlueSkyStore *store,
                           const gchar *master_key);

gboolean bluesky_inode_is_ready(BlueSkyInode *inode);

int64_t bluesky_get_current_time();
void bluesky_inode_update_ctime(BlueSkyInode *inode, gboolean update_mtime);
uint64_t bluesky_fs_alloc_inode(BlueSkyFS *fs);
void bluesky_init_inode(BlueSkyInode *i, BlueSkyFileType type);
BlueSkyInode *bluesky_new_inode(uint64_t inum, BlueSkyFS *fs, BlueSkyFileType type);

BlueSkyInode *bluesky_get_inode(BlueSkyFS *fs, uint64_t inum);
void bluesky_inode_ref(BlueSkyInode *inode);
void bluesky_inode_unref(BlueSkyInode *inode);
void bluesky_insert_inode(BlueSkyFS *fs, BlueSkyInode *inode);

void bluesky_dirent_destroy(gpointer dirent);
uint64_t bluesky_directory_lookup(BlueSkyInode *inode, gchar *name);
uint64_t bluesky_directory_ilookup(BlueSkyInode *inode, gchar *name);
BlueSkyDirent *bluesky_directory_read(BlueSkyInode *dir, uint32_t cookie);
gboolean bluesky_directory_insert(BlueSkyInode *dir, const gchar *name,
                                  uint64_t inum);
void bluesky_directory_dump(BlueSkyInode *dir);

void bluesky_file_truncate(BlueSkyInode *inode, uint64_t size);
void bluesky_file_write(BlueSkyInode *inode, uint64_t offset,
                        const char *data, gint len);
void bluesky_file_read(BlueSkyInode *inode, uint64_t offset,
                       char *buf, gint len);

void bluesky_inode_fetch(BlueSkyFS *fs, uint64_t inum);
void bluesky_inode_prefetch(BlueSkyFS *fs, uint64_t inum);

gint bluesky_dirent_compare(gconstpointer a, gconstpointer b,
                            gpointer unused);

void bluesky_flushd_invoke(BlueSkyFS *fs);
void bluesky_flushd_invoke_conditional(BlueSkyFS *fs);
void bluesky_inode_do_sync(BlueSkyInode *inode);
void bluesky_flushd_thread_launch(BlueSkyFS *fs);

void bluesky_debug_dump(BlueSkyFS *fs);

/* Request response time tracking. */
typedef struct BlueSkyProfile {
    int magic;
    GMutex *lock;
    char *description;
    GList *events;
} BlueSkyProfile;

BlueSkyProfile *bluesky_profile_new();
void bluesky_profile_free(BlueSkyProfile *profile);
void bluesky_profile_add_event(BlueSkyProfile *profile, char *message);
void bluesky_profile_print(BlueSkyProfile *profile);
BlueSkyProfile *bluesky_profile_get();
void bluesky_profile_set(BlueSkyProfile *profile);
void bluesky_profile_set_output(FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
