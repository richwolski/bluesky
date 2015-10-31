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
    const char *filename;
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

#if 0
void bench_write()
{
    FILE *f = fopen("writetest", "wb");
    if (f == NULL) {
        perror("fopen");
        return;
    }

    char buf[4096];
    for (int i = 0; i < 16; i++) {
        int64_t start, end;
        start = now_hires();
        rewind(f);
        fwrite(buf, 1, sizeof(buf), f);
        fflush(f);
        end = now_hires();
        printf("Pass %d: Time = %"PRIi64"\n", i, end - start);
    }
}
#endif

void *benchmark_thread(void *arg)
{
    struct thread_state *ts = (struct thread_state *)arg;

    printf("Opening %s\n", ts->filename);

    int64_t start, end;

    start = now_hires();

    //struct stat stat_buf;
    //stat(namebuf, &stat_buf);

    FILE *f = fopen(ts->filename, "rb");
    if (f == NULL) {
        perror("fopen");
        return NULL;
    }

    char buf[65536];
    while (fread(buf, 1, sizeof(buf), f) > 0) { }

    end = now_hires();
    printf("Thread %d: Time = %"PRIi64"\n", ts->thread_num, end - start);

    fclose(f);

    return NULL;
}

void launch_thread(int i, const char *filename)
{
    threads[i].thread_num = i;
    threads[i].filename = filename;
    printf("Launching thread %d [%s]...\n", i, filename);
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
    int threads = argc - 1;

    printf("Testing with %d threads\n", threads);

    for (int i = 0; i < threads; i++) {
        launch_thread(i, argv[i + 1]);
    }

    for (int i = 0; i < threads; i++) {
        wait_thread(i);
    }

    return 0;
}
