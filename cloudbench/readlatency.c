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

/* Simple benchmark for Amazon S3: measures download speeds for
 * differently-sized objects and with a variable number of parallel
 * connections. */

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

FILE *statsfile;

S3BucketContext bucket;

struct thread_state {
    pthread_t thread;
    int thread_num;
    long long timestamp;

    // Time when first bytes of the response were received
    long long first_byte_timestamp;

    // Statistics for computing mean and standard deviation
    int n;
    size_t bytes_sent;
    double sum_x, sum_x2;

    double sum_f;
};

struct callback_state {
    struct thread_state *ts;
    size_t bytes_remaining;
};

#define MAX_THREADS 1024
struct thread_state threads[MAX_THREADS];

int experiment_threads, experiment_size, experiment_objects;

pthread_mutex_t barrier_mutex;
pthread_cond_t barrier_cond;
int barrier_val;

pthread_mutex_t wait_mutex;
pthread_cond_t wait_cond;
int wait_val = 0;

enum phase { LAUNCH, MEASURE, TERMINATE };
volatile enum phase test_phase;

void barrier_signal()
{
    pthread_mutex_lock(&barrier_mutex);
    barrier_val--;
    printf("Barrier: %d left\n", barrier_val);
    if (barrier_val == 0)
        pthread_cond_signal(&barrier_cond);
    pthread_mutex_unlock(&barrier_mutex);
}

long long get_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static S3Status data_callback(int bufferSize, const char *buffer,
                              void *callbackData)
{
    struct callback_state *state = (struct callback_state *)callbackData;
    state->bytes_remaining -= bufferSize;
    if (state->ts->first_byte_timestamp == 0)
        state->ts->first_byte_timestamp = get_ns();
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

static void do_get(const char *key, size_t bytes, struct thread_state *ts)
{
    struct callback_state state;
    struct S3GetObjectHandler handler;

    state.bytes_remaining = bytes;
    state.ts = ts;
    handler.responseHandler.propertiesCallback = properties_callback;
    handler.responseHandler.completeCallback = complete_callback;
    handler.getObjectDataCallback = data_callback;

    S3_get_object(&bucket, key, NULL, 0, 0, NULL, &handler, &state);
}

void *benchmark_thread(void *arg)
{
    struct thread_state *ts = (struct thread_state *)arg;
    char namebuf[64];

    printf("Warming up...\n");
    do_get("file-1048576-0", 0, ts);
    printf("Ready.\n");
    barrier_signal();

    pthread_mutex_lock(&wait_mutex);
    while (wait_val == 0)
        pthread_cond_wait(&wait_cond, &wait_mutex);
    pthread_mutex_unlock(&wait_mutex);

    ts->timestamp = get_ns();
    sprintf(namebuf, "file-%d-%d", experiment_size, ts->thread_num);
    ts->first_byte_timestamp = 0;
    do_get(namebuf, experiment_size, ts);
    long long timestamp = get_ns();
    long long elapsed = timestamp - ts->timestamp;

    barrier_signal();

    printf("Thread %d: %f elapsed\n", ts->thread_num, elapsed / 1e9);

    return NULL;
}

void launch_thread(int n)
{
    threads[n].thread_num = n;
    if (pthread_create(&threads[n].thread, NULL, benchmark_thread, &threads[n]) != 0) {
        fprintf(stderr, "Error launching thread!\n");
        exit(1);
    }
}

void wait_thread(int n)
{
    void *result;
    pthread_join(threads[n].thread, &result);
}

void launch_test(int thread_count)
{
    int i;

    barrier_val = thread_count;
    assert(thread_count <= MAX_THREADS);

    for (i = 0; i < thread_count; i++)
        launch_thread(i);

    /* Wait until all threads are ready. */
    pthread_mutex_lock(&barrier_mutex);
    while (barrier_val > 0) {
        pthread_cond_wait(&barrier_cond, &barrier_mutex);
    }
    pthread_mutex_unlock(&barrier_mutex);

    sleep(2);
    barrier_val = thread_count;
    pthread_mutex_lock(&wait_mutex);
    printf("Launching test\n");
    long long start_time = get_ns();
    wait_val = 1;
    pthread_cond_broadcast(&wait_cond);
    pthread_mutex_unlock(&wait_mutex);

    /* Wait until all threads are ready. */
    pthread_mutex_lock(&barrier_mutex);
    while (barrier_val > 0) {
        pthread_cond_wait(&barrier_cond, &barrier_mutex);
    }
    pthread_mutex_unlock(&barrier_mutex);

    long long end_time = get_ns();

    printf("Elapsed time: %f\n", (end_time - start_time) / 1e9);
    fprintf(statsfile, "%d\t%d\t%f\n", experiment_threads, experiment_size,
            (end_time - start_time) / 1e9);
}

int main(int argc, char *argv[])
{
    statsfile = fopen("readlatency.data", "a");
    if (statsfile == NULL) {
        perror("open stats file");
        return 1;
    }

    S3_initialize(NULL, S3_INIT_ALL, NULL);

    bucket.bucketName = "mvrable-benchmark";
    bucket.protocol = S3ProtocolHTTP;
    bucket.uriStyle = S3UriStyleVirtualHost;
    bucket.accessKeyId = getenv("AWS_ACCESS_KEY_ID");
    bucket.secretAccessKey = getenv("AWS_SECRET_ACCESS_KEY");

    pthread_mutex_init(&barrier_mutex, NULL);
    pthread_cond_init(&barrier_cond, NULL);
    pthread_mutex_init(&wait_mutex, NULL);
    pthread_cond_init(&wait_cond, NULL);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <threads> <size>\n", argv[0]);
        return 1;
    }

    experiment_threads = atoi(argv[1]);
    experiment_size = atoi(argv[2]);
    launch_test(experiment_threads);

    printf("Done.\n");
    fclose(statsfile);

    return 0;
}
