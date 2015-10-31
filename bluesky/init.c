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
#include <glib.h>
#include <string.h>

#include "bluesky-private.h"

int bluesky_verbose = 0;

BlueSkyOptions bluesky_options = {
    .cache_size = 1024*1024,         // Default cache size is 1 GiB
};

/* Maximum number of threads to use in any particular thread pool, or -1 for no
 * limit */
int bluesky_max_threads = 16;

/* Watermark levels for cache tuning: these control when dirty data is flushed
 * from cache, when clean data is dropped from the cache, etc.  These values
 * are measured in blocks, not bytes.
 *
 * There are a few relevant levels:
 *   low: Below this point, data is not forced out due to memory pressure
 *   medium: At this point start flushing data to get back below medium
 *   high: Flush data very aggressively (launch extra tasks if needed)
 */
int bluesky_watermark_low_dirty    = (64 << 20) / BLUESKY_BLOCK_SIZE;
int bluesky_watermark_medium_dirty = (96 << 20) / BLUESKY_BLOCK_SIZE;
int bluesky_watermark_med2_dirty   = (160 << 20) / BLUESKY_BLOCK_SIZE;
int bluesky_watermark_high_dirty   = (192 << 20) / BLUESKY_BLOCK_SIZE;

int bluesky_watermark_low_total    = (64 << 20) / BLUESKY_BLOCK_SIZE;
int bluesky_watermark_medium_total = (128 << 20) / BLUESKY_BLOCK_SIZE;
int bluesky_watermark_med2_total   = (224 << 20) / BLUESKY_BLOCK_SIZE;
int bluesky_watermark_high_total   = (256 << 20) / BLUESKY_BLOCK_SIZE;

/* Environment variables that can be used to initialize settings. */
static struct {
    const char *env;
    int *option;
} option_table[] = {
    {"BLUESKY_VERBOSE", &bluesky_verbose},
    {"BLUESKY_OPT_SYNC_STORES", &bluesky_options.synchronous_stores},
    {"BLUESKY_OPT_WRITETHROUGH", &bluesky_options.writethrough_cache},
    {"BLUESKY_OPT_SYNC_INODE_FETCH", &bluesky_options.sync_inode_fetches},
    {"BLUESKY_OPT_SYNC_FRONTENDS", &bluesky_options.sync_frontends},
    {"BLUESKY_CACHE_SIZE", &bluesky_options.cache_size},
    {"BLUESKY_OPT_FULL_SEGMENTS", &bluesky_options.full_segment_fetches},
    {"BLUESKY_OPT_NO_AGGREGATION", &bluesky_options.disable_aggregation},
    {"BLUESKY_OPT_NO_CRYPTO", &bluesky_options.disable_crypto},
    {"BLUESKY_OPT_NO_GROUP_READS", &bluesky_options.disable_read_aggregation},
    {NULL, NULL}
};

/* BlueSky library initialization. */

void bluesky_store_init_s3(void);
void bluesky_store_init_azure(void);
void bluesky_store_init_kv(void);
void bluesky_store_init_multi(void);
void bluesky_store_init_bdb(void);
void bluesky_store_init_simple(void);

/* Initialize the BlueSky library and dependent libraries. */
void bluesky_init(void)
{
    g_thread_init(NULL);
    bluesky_crypt_init();

    for (int i = 0; option_table[i].env != NULL; i++) {
        const char *val = getenv(option_table[i].env);
        if (val != NULL) {
            int v = atoi(val);
            g_print("Option %s set to %d\n", option_table[i].env, v);
            *option_table[i].option = atoi(val);
        }
    }

    bluesky_store_init();
    bluesky_store_init_kv();
    bluesky_store_init_s3();
    bluesky_store_init_azure();
    bluesky_store_init_multi();
    bluesky_store_init_bdb();
    bluesky_store_init_simple();
}
