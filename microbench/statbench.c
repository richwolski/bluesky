#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct thread_state {
    pthread_t thread;
    int thread_num;
    long long timestamp;
};

#define MAX_THREADS 128
struct thread_state threads[MAX_THREADS];

int64_t now_hires()
{
    struct timespec time;

    if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
        perror("clock_gettime");
        return 0;
    }

    return (int64_t)(time.tv_sec) * 1000000000 + time.tv_nsec;
}

void *benchmark_thread(void *arg)
{
    struct thread_state *ts = (struct thread_state *)arg;

    char namebuf[64];
    sprintf(namebuf, "file-%d", ts->thread_num + 1);
    printf("Opening %s\n", namebuf);

    int64_t start, end;

    start = now_hires();

    struct stat stat_buf;
    stat(namebuf, &stat_buf);

    end = now_hires();
    printf("Thread %d: Time = %"PRIi64"\n", ts->thread_num, end - start);

    return NULL;
}

void launch_thread(int i)
{
    threads[i].thread_num = i;
    printf("Launching thread %d...\n", i);
    if (pthread_create(&threads[i].thread, NULL, benchmark_thread, &threads[i]) != 0) {
        fprintf(stderr, "Error launching thread!\n");
        exit(1);
    }
}

void wait_thread(int n)
{
    void *result;
    pthread_join(threads[n].thread, &result);
}

int main(int argc, char *argv[])
{
    int threads = 8;

    if (argc > 1)
        threads = atoi(argv[1]);
    if (threads > MAX_THREADS)
        threads = MAX_THREADS;

    printf("Testing with %d threads\n", threads);

    for (int i = 0; i < threads; i++) {
        launch_thread(i);
    }

    for (int i = 0; i < threads; i++) {
        wait_thread(i);
    }

    return 0;
}
