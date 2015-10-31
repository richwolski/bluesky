/* Blue Sky: File Systems in the Cloud
 *
 * Copyright (C) 2011  The Regents of the University of California
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

/* Simple benchmark for Amazon S3: measures download times to fetch files over
 * a single connection, with variable amounts of delay between requests.  This
 * is intended to test whether the TCP congestion control state gets reset and
 * slow start needs to restart, depending on how long the connection was left
 * idle. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "libs3.h"

const char *key = NULL;

S3BucketContext bucket;

struct callback_state {
    //struct thread_state *ts;
    size_t bytes_remaining;
};

int64_t get_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void do_sleep(int64_t delay)
{
    if (delay <= 0)
        return;

    struct timespec t;
    t.tv_sec = delay / 1000000000;
    t.tv_nsec = delay % 1000000000;
    nanosleep(&t, NULL);
}

static S3Status data_callback(int bufferSize, const char *buffer,
                              void *callbackData)
{
    struct callback_state *state = (struct callback_state *)callbackData;
    state->bytes_remaining -= bufferSize;
    /*
    if (state->ts->first_byte_timestamp == 0)
        state->ts->first_byte_timestamp = get_ns(); */
    return S3StatusOK;
}

static S3Status properties_callback(const S3ResponseProperties *properties,
                                     void *callbackData)
{
    return S3StatusOK;
}

static void complete_callback(S3Status status,
                              const S3ErrorDetails *errorDetails,
                              void *callbackData)
{
}

static void do_get(const char *key, size_t bytes)
{
    struct callback_state state;
    struct S3GetObjectHandler handler;

    state.bytes_remaining = bytes;
    //state.ts = ts;
    handler.responseHandler.propertiesCallback = properties_callback;
    handler.responseHandler.completeCallback = complete_callback;
    handler.getObjectDataCallback = data_callback;

    S3_get_object(&bucket, key, NULL, 0, 0, NULL, &handler, &state);
}

void run_test(int64_t delay_ns)
{
    for (int i = 0; i <= 25; i++) {
        int64_t start, finish;
        start = get_ns();
        do_get(key, 0);
        finish = get_ns();

        int64_t elapsed = finish - start;
        if (i > 0) {
            printf("%f\n", elapsed / 1e9);
            fflush(stdout);
        }

        do_sleep(delay_ns);
    }
}

int main(int argc, char *argv[])
{
    S3_initialize(NULL, S3_INIT_ALL, NULL);

    bucket.bucketName = "mvrable-benchmark";
    bucket.protocol = S3ProtocolHTTP;
    bucket.uriStyle = S3UriStyleVirtualHost;
    bucket.accessKeyId = getenv("AWS_ACCESS_KEY_ID");
    bucket.secretAccessKey = getenv("AWS_SECRET_ACCESS_KEY");

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file> <delay>\n", argv[0]);
        return 1;
    }

    key = argv[1];
    double inter_request_delay = atof(argv[2]);
    run_test(inter_request_delay * 1e9);

    return 0;
}
