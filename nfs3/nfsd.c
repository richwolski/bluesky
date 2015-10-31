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

/* Main entry point for an NFSv3 server which will proxy requests to a cloud
 * filesystem. */

#include "mount_prot.h"
#include "nfs3_prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <glib.h>

#include "bluesky.h"

void register_rpc();

BlueSkyFS *fs;
BlueSkyStore *store;

/* A cookie value returned for uncommitted writes and verified by commits; this
 * should be a unique value each time the server is started. */
char nfsd_instance_verf_cookie[NFS3_WRITEVERFSIZE];

static void shutdown_handler(int num)
{
    g_print("SIGINT caught, shutting down...\n");
    g_print("Proxy statistics:\n");
    bluesky_stats_dump_all();
    exit(1);
}

int main(int argc, char *argv[])
{
    int i;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, shutdown_handler);

    bluesky_init();
    g_set_prgname("nfsd");

    const char *target = getenv("BLUESKY_TARGET");
    if (target == NULL)
        target = "s3";

    const char *key = getenv("BLUESKY_KEY");
    if (key == NULL)
        key = "";

    const char *profile_output = getenv("BLUESKY_PROFILE_OUT");
    if (profile_output != NULL)
        bluesky_profile_set_output(fopen(profile_output, "a"));

    store = bluesky_store_new(target);
    fs = bluesky_init_fs("export", store, key);

    const char *stats_output = getenv("BLUESKY_STATS_OUT");
    if (stats_output != NULL)
        bluesky_stats_run_periodic_dump(fopen(stats_output, "a"));

    bluesky_crypt_random_bytes(nfsd_instance_verf_cookie,
                               sizeof(nfsd_instance_verf_cookie));

    register_rpc();

    svc_run();
    fprintf(stderr, "%s", "svc_run returned");
    exit(1);
}
