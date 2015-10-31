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
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <glib.h>

#include "bluesky-private.h"

/* Small test program for BlueSkyFS.  Doesn't do much useful. */

int main(int argc, char *argv[])
{
    g_thread_init(NULL);

    printf("BlueSkyFS starting...\n");

    printf("  time = %"PRIi64"\n", bluesky_get_current_time());

    BlueSkyFS *fs = bluesky_new_fs("export");

    BlueSkyInode *root;
    root = bluesky_new_inode(BLUESKY_ROOT_INUM, fs, BLUESKY_DIRECTORY);
    root->nlink = 1;
    root->mode = 0755;
    bluesky_insert_inode(fs, root);

    BlueSkyInode *file;
    file = bluesky_new_inode(bluesky_fs_alloc_inode(fs), fs, BLUESKY_REGULAR);
    file->nlink = 1;
    file->mode = 0755;
    bluesky_insert_inode(fs, file);
    bluesky_directory_insert(root, "foo", file->inum);

    file = bluesky_new_inode(bluesky_fs_alloc_inode(fs), fs, BLUESKY_REGULAR);
    file->nlink = 1;
    file->mode = 0755;
    bluesky_insert_inode(fs, file);
    bluesky_directory_insert(root, "bar", file->inum);

    file = bluesky_new_inode(bluesky_fs_alloc_inode(fs), fs, BLUESKY_REGULAR);
    file->nlink = 1;
    file->mode = 0755;
    bluesky_insert_inode(fs, file);
    bluesky_directory_insert(root, "baz", file->inum);

    bluesky_directory_dump(root);
    bluesky_directory_lookup(root, "foo");
    bluesky_directory_lookup(root, "bar");
    bluesky_directory_lookup(root, "baz");

    return 0;
}
