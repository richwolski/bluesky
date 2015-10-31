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

#define MAX_THREADS 128
struct thread_state threads[MAX_THREADS];

int experiment_threads, experiment_size, experiment_objects;
int range_size = 0;

pthread_mutex_t barrier_mutex;
pthread_cond_t barrier_cond;
int barrier_val;

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

static void do_get(const char *key, size_t bytes, struct thread_state *ts,
                   size_t offset)
{
    struct callback_state state;
    struct S3GetObjectHandler handler;

    state.bytes_remaining = bytes;
    state.ts = ts;
    handler.responseHandler.propertiesCallback = properties_callback;
    handler.responseHandler.completeCallback = complete_callback;
    handler.getObjectDataCallback = data_callback;

    S3_get_object(&bucket, key, NULL, offset, range_size, NULL, &handler, &state);
}

void *benchmark_thread(void *arg)
{
    struct thread_state *ts = (struct thread_state *)arg;
    char namebuf[64];
    int i = 0;
    int stage = 0;
    int measuring = 0;

    ts->n = 0;
    ts->sum_x = ts->sum_x2 = ts->sum_f = 0.0;
    ts->bytes_sent = 0;

    ts->timestamp = get_ns();
    while (test_phase != TERMINATE) {
        int object = random() % experiment_objects;
        int offset = 0;
        sprintf(namebuf, "file-%d-%d", experiment_size, object);
        if (range_size) {
            offset = (random() % (experiment_size / range_size)) * range_size;
        }
        ts->first_byte_timestamp = 0;
        do_get(namebuf, experiment_size, ts, offset);
        long long timestamp = get_ns();
        long long elapsed = timestamp - ts->timestamp;

        printf("Elapsed[%d-%d]: %lld ns\n", ts->thread_num, i, elapsed);
        printf("    first data after: %lld ns\n",
               ts->first_byte_timestamp - ts->timestamp);
        if (measuring && test_phase == MEASURE) {
            double e = elapsed / 1e9;
            double f = (ts->first_byte_timestamp - ts->timestamp) / 1e9;
            ts->n++;
            ts->sum_x += e;
            ts->sum_x2 += e * e;
            ts->sum_f += f;
            ts->bytes_sent += range_size ? range_size : experiment_size;
        }

        i++;
        if (stage == 0 && i > 2) {
            barrier_signal();
            stage = 1;
        } else if (stage == 1 && ts->n >= 2) {
            barrier_signal();
            stage = 2;
        }

        ts->timestamp = timestamp;
        if (test_phase == MEASURE)
            measuring = 1;
    }

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
    long long start_time = get_ns();

    test_phase = LAUNCH;
    barrier_val = thread_count;
    assert(thread_count <= MAX_THREADS);

    printf("Launching...\n");

    for (i = 0; i < thread_count; i++)
        launch_thread(i);

    /* Wait until all threads are ready. */
    pthread_mutex_lock(&barrier_mutex);
    while (barrier_val > 0) {
        pthread_cond_wait(&barrier_cond, &barrier_mutex);
    }
    pthread_mutex_unlock(&barrier_mutex);

    printf("Measuring...\n");
    barrier_val = thread_count;
    test_phase = MEASURE;

    /* Ensure all threads have measured some activity, then a bit more. */
    pthread_mutex_lock(&barrier_mutex);
    while (barrier_val > 0) {
        pthread_cond_wait(&barrier_cond, &barrier_mutex);
    }
    pthread_mutex_unlock(&barrier_mutex);
    printf("Data in from all threads...\n");
    sleep(5);

    printf("Terminating...\n");
    test_phase = TERMINATE;

    for (i = 0; i < thread_count; i++)
        wait_thread(i);

    int n = 0;
    double sum_x = 0.0, sum_x2 = 0.0, sum_f = 0.0;
    double bandwidth = 0.0;
    for (i = 0; i < thread_count; i++) {
        n += threads[i].n;
        sum_x += threads[i].sum_x;
        sum_x2 += threads[i].sum_x2;
        sum_f += threads[i].sum_f;
        bandwidth += threads[i].bytes_sent / threads[i].sum_x;
    }

    double elapsed = (get_ns() - start_time) / 1e9;
    printf("*** %d threads, %d byte objects, %d byte ranges\n",
           experiment_threads, experiment_size, range_size);
    printf("Elapsed: %f s\n", elapsed);
    printf("Data points: %d\n", n);
    double mx = sum_x / n;
    double sx = sqrt((sum_x2 - 2*sum_x*mx + n*mx*mx) / (n - 1));
    printf("Time: %f ± %f s\n", mx, sx);
    printf("Latency to first byte: %f\n", sum_f / n);
    printf("Bandwidth: %f B/s\n", bandwidth);

    fprintf(statsfile, "%d\t%d\t%f\t%d\t%f\t%f\t%f\n",
            experiment_threads, experiment_size, elapsed, n,
            mx, sx, bandwidth);

    printf("Finished.\n");
}

int main(int argc, char *argv[])
{
    statsfile = fopen("readbench.data", "a");
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

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <threads> <size> <object-count>\n", argv[0]);
        return 1;
    }

    experiment_threads = atoi(argv[1]);
    experiment_size = atoi(argv[2]);
    experiment_objects = atoi(argv[3]);
    if (argc > 4) {
        range_size = atoi(argv[4]);
    }
    assert(experiment_objects > 0);
    launch_test(experiment_threads);

    printf("Done.\n");
    fclose(statsfile);

    return 0;
}
