/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2010  The Regents of the University of California
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

/* A simple tool for benchmarking various logging strategies.
 *
 * We want to log a series of key/value pairs.  Approaches that we try include:
 *  - Data written directly into the filesystem.
 *  - Data is written to a Berkeley DB.
 *  - Data is appended to a log file.
 * In all cases we want to ensure that data is persistent on disk so it could
 * be used for crash recovery.  We measure how many log records we can write
 * per second to gauge performance. */

#define _GNU_SOURCE
#define _ATFILE_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <db.h>
#include <glib.h>

struct item {
    char *key;
    char *data;
    size_t len;
};

int queue_capacity = 1024;
int item_size = 1024;
int opt_threads = 1;
int opt_batchsize = 1;
int opt_writes = (1 << 12);
int opt_bdb_async = FALSE;

GAsyncQueue *queue;
int outstanding = 0;
GMutex *lock;
GCond *cond_empty, *cond_full;

int64_t get_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

struct item *get_item()
{
    return (struct item *)g_async_queue_pop(queue);
}

void finish_item(struct item *item)
{
    g_free(item->key);
    g_free(item->data);
    g_free(item);

    g_mutex_lock(lock);
    outstanding--;
    if (outstanding == 0)
        g_cond_signal(cond_empty);
    if (outstanding < queue_capacity)
        g_cond_signal(cond_full);
    g_mutex_unlock(lock);
}

void writebuf(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t written;
        written = write(fd, buf, len);
        if (written < 0 && errno == EINTR)
            continue;
        g_assert(written >= 0);
        buf += written;
        len -= written;
    }
}

/************************ Direct-to-filesystem logging ***********************/
static int dir_fd = -1;

gpointer fslog_thread(gpointer d)
{
    while (TRUE) {
        struct item *item = get_item();

        int fd = openat(dir_fd, item->key, O_CREAT|O_WRONLY|O_TRUNC, 0666);
        g_assert(fd >= 0);

        writebuf(fd, item->data, item->len);

        finish_item(item);

        fsync(fd);
        fsync(dir_fd);
        close(fd);
    }

    return NULL;
}

void launch_fslog()
{
    dir_fd = open(".", O_DIRECTORY);
    g_assert(dir_fd >= 0);

    for (int i = 0; i < 1; i++)
        g_thread_create(fslog_thread, NULL, FALSE, NULL);
}

/****************************** Single-File Log ******************************/
gpointer flatlog_thread(gpointer d)
{
    int fd = open("logfile", O_CREAT|O_WRONLY|O_TRUNC, 0666);
    g_assert(fd >= 0);

    int count = 0;

    while (TRUE) {
        struct item *item = get_item();

        writebuf(fd, item->key, strlen(item->key) + 1);
        writebuf(fd, (char *)&item->len, sizeof(item->len));
        writebuf(fd, item->data, item->len);

        count++;
        if (count % opt_batchsize == 0)
            fdatasync(fd);

        finish_item(item);
    }

    return NULL;
}

void launch_flatlog()
{
    g_thread_create(flatlog_thread, NULL, FALSE, NULL);
}

/************************* Transactional Berkeley DB *************************/
gpointer bdb_thread(gpointer d)
{
    int res;
    DB_ENV *env;
    DB *db;
    DB_TXN *txn = NULL;
    int count = 0;

    res = db_env_create(&env, 0);
    g_assert(res == 0);

    res = env->open(env, ".",
                    DB_CREATE | DB_RECOVER | DB_INIT_LOCK | DB_INIT_LOG
                     | DB_INIT_MPOOL | DB_INIT_TXN | DB_THREAD, 0644);
    g_assert(res == 0);

    if (opt_bdb_async) {
        res = env->set_flags(env, DB_TXN_WRITE_NOSYNC, 1);
        g_assert(res == 0);
    }

    res = db_create(&db, env, 0);
    g_assert(res == 0);

    res = db->open(db, NULL, "log.db", "log", DB_BTREE,
                   DB_CREATE | DB_THREAD | DB_AUTO_COMMIT, 0644);
    g_assert(res == 0);

    while (TRUE) {
        if (txn == NULL && !opt_bdb_async) {
            res = env->txn_begin(env, NULL, &txn, 0);
            g_assert(res == 0);
        }

        struct item *item = get_item();

        DBT key, value;
        memset(&key, 0, sizeof(key));
        memset(&value, 0, sizeof(value));

        key.data = item->key;
        key.size = strlen(item->key);

        value.data = item->data;
        value.size = item->len;

        res = db->put(db, opt_bdb_async ? NULL : txn, &key, &value, 0);
        g_assert(res == 0);

        count++;
        if (count % opt_batchsize == 0) {
            if (opt_bdb_async) {
                env->txn_checkpoint(env, 0, 0, 0);
            } else {
                txn->commit(txn, 0);
                txn = NULL;
            }
        }

        finish_item(item);
    }

    return NULL;
}

void launch_bdb()
{
    g_thread_create(bdb_thread, NULL, FALSE, NULL);
}

int main(int argc, char *argv[])
{
    int64_t time_start, time_end;

    g_thread_init(NULL);
    queue = g_async_queue_new();
    lock = g_mutex_new();
    cond_empty = g_cond_new();
    cond_full = g_cond_new();

    int opt;
    int backend = 0;
    while ((opt = getopt(argc, argv, "at:s:b:n:BFD")) != -1) {
        switch (opt) {
        case 'a':
            // Make BDB log writes more asynchronous
            opt_bdb_async = TRUE;
            break;
        case 't':
            // Set number of log worker threads
            opt_threads = atoi(optarg);
            break;
        case 's':
            // Set item size (in bytes)
            item_size = atoi(optarg);
            break;
        case 'b':
            // Set batch size
            opt_batchsize = atoi(optarg);
            break;
        case 'n':
            // Set object count
            opt_writes = atoi(optarg);
            break;
        case 'B':
            // Select BDB backend
            backend = 'b';
            break;
        case 'F':
            // Select flat file backend
            backend = 'f';
            break;
        case 'D':
            // Select file system directory backend
            backend = 'd';
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-t threads] {-B|-F|-D}\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    switch (backend) {
    case 'b':
        launch_bdb();
        break;
    case 'f':
        launch_flatlog();
        break;
    case 'd':
        launch_fslog();
        break;
    default:
        fprintf(stderr, "Backend not selected!\n");
        return EXIT_FAILURE;
    }

    time_start = get_ns();
    for (int i = 0; i < opt_writes; i++) {
        struct item *item = g_new(struct item, 1);
        item->key = g_strdup_printf("item-%06d", i);
        item->data = g_malloc(item_size);
        item->len = item_size;

        g_mutex_lock(lock);
        g_async_queue_push(queue, item);
        outstanding++;
        if (outstanding == opt_batchsize)
            g_cond_wait(cond_empty, lock);
        g_mutex_unlock(lock);
    }

    g_mutex_lock(lock);
    while (outstanding > 0)
        g_cond_wait(cond_empty, lock);
    g_mutex_unlock(lock);
    time_end = get_ns();

    double elapsed = (time_end - time_start) / 1e9;
    printf("Elapsed: %f s\nThroughput: %f txn/s, %f MiB/s\n",
           elapsed, opt_writes / elapsed,
           opt_writes / elapsed * item_size / (1 << 20));

    if (backend == 'b' && opt_bdb_async)
        backend = 'B';

    FILE *f = fopen("../logbench.data", "a");
    g_assert(f != NULL);
    fprintf(f, "%c\t%d\t%d\t%d\t%f\t%f\t%f\n",
            backend, item_size, opt_writes, opt_batchsize,
            elapsed, opt_writes / elapsed,
            opt_writes / elapsed * item_size / (1 << 20));
    fclose(f);

    return 0;
}
