/* A simple file system workload generator.
 *
 * Reads and writes a number of files in the current working directory.
 *
 * Command-line arguments:
 *   File size (bytes)
 *   File count
 *   Write fraction (0.0 - 1.0)
 *   Threads
 *   Benchmark duration (seconds)
 *   Target operations per second (aggregate across all threads)
 *   Interval count (how many times to report results during the run)
 *   Directory size (number of files per numbered subdirectory)
 *   Read/write block size (0 for the entire file)
 */

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

int opt_filesize, opt_filecount, opt_threads, opt_duration, opt_intervals, opt_dirsize, opt_blocksize;
double opt_writeratio, opt_ops;

int write_threads;

struct thread_state {
    pthread_t thread;
    pthread_mutex_t lock;
    int thread_num;
    int read_count, write_count;
    double read_time, write_time, read_time2, write_time2;
};

static int64_t start_time;

#define MAX_THREADS 128
struct thread_state threads[MAX_THREADS];

static double sq(double x)
{
    return x * x;
}

static double stddev(double x, double x2, int n)
{
    if (n < 2)
        return 0;
    return sqrt((x2 / n - sq(x / n)) * n / (n - 1));
}

int64_t now_hires()
{
    struct timespec time;

    if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
        perror("clock_gettime");
        return 0;
    }

    return (int64_t)(time.tv_sec) * 1000000000 + time.tv_nsec;
}

int get_random(int range)
{
    return random() % range;
}

void sleep_micros(int duration)
{
    struct timespec req;
    if (duration <= 0)
        return;

    req.tv_sec = duration / 1000000;
    req.tv_nsec = (duration % 1000000) * 1000;

    while (nanosleep(&req, &req) < 0 && errno == EINTR)
        ;
}

void benchmark_op(struct thread_state *ts)
{
    int64_t start, end;

    start = now_hires();

    /* The space of all files is partitioned evenly based on the number of
     * threads.  Pick a file out of our particular partition. */
    int thread_num, thread_count;
    if (ts->thread_num >= write_threads) {
        /* Read */
        thread_num = ts->thread_num - write_threads;
        thread_count = opt_threads - write_threads;
    } else {
        /* Write */
        thread_num = ts->thread_num;
        thread_count = write_threads;
    }

    int n = get_random(opt_filecount / thread_count);
    n += thread_num * (opt_filecount / thread_count);
    int n1 = n / opt_dirsize, n2 = n % opt_dirsize;
    char filename[256];
    sprintf(filename, "%d/%d", n1, n2);

    /* If a smaller blocksize was requested, choose a random offset within the
     * file to use. */
    int offset = 0;
    if (opt_blocksize > 0) {
        offset = get_random(opt_filesize / opt_blocksize) * opt_blocksize;
    }

    if (ts->thread_num >= write_threads) {
        /* Read */
        FILE *f = fopen(filename, "rb");
        if (f == NULL) {
            fprintf(stderr, "fopen(%s): %m\n", filename);
            return;
        }

        if (offset != 0)
            fseek(f, offset, SEEK_SET);

        char buf[65536];
        int bytes_left = opt_blocksize > 0 ? opt_blocksize : opt_filesize;
        while (bytes_left > 0) {
            size_t read = fread(buf, 1, bytes_left < sizeof(buf)
                                         ? bytes_left : sizeof(buf), f);
            if (ferror(f))
                return;
            bytes_left -= read;
        }
        fclose(f);

        end = now_hires();
        pthread_mutex_lock(&ts->lock);
        ts->read_count++;
        ts->read_time += (end - start) / 1e9;
        ts->read_time2 += sq((end - start) / 1e9);
        pthread_mutex_unlock(&ts->lock);
    } else {
        /* Write */
        FILE *f = fopen(filename, "r+b");
        if (f == NULL) {
            fprintf(stderr, "fopen(%s): %m\n", filename);
            return;
        }

        if (offset != 0)
            fseek(f, offset, SEEK_SET);

        char buf[65536];
        int bytes_left = opt_blocksize > 0 ? opt_blocksize : opt_filesize;
        while (bytes_left > 0) {
            size_t written = fwrite(buf, 1,
                                    bytes_left < sizeof(buf)
                                     ? bytes_left : sizeof(buf),
                                    f);
            if (ferror(f))
                return;
            bytes_left -= written;
        }
        fclose(f);

        end = now_hires();
        pthread_mutex_lock(&ts->lock);
        ts->write_count++;
        ts->write_time += (end - start) / 1e9;
        ts->write_time2 += sq((end - start) / 1e9);
        pthread_mutex_unlock(&ts->lock);
    }
}

void *benchmark_thread(void *arg)
{
    struct thread_state *ts = (struct thread_state *)arg;

    int target_delay = (opt_threads / opt_ops) * 1e6;

    while (1) {
        int64_t start = now_hires();
        benchmark_op(ts);
        int64_t end = now_hires();

        int elapsed = (end - start) / 1000;
        if (elapsed < target_delay)
            sleep_micros(target_delay - elapsed);
    }

    return NULL;
}

void launch_thread(int i)
{
    memset(&threads[i], 0, sizeof(struct thread_state));
    threads[i].thread_num = i;
    pthread_mutex_init(&threads[i].lock, NULL);
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

void reset_stats(int print, double duration)
{
    int read_count = 0, write_count = 0;
    double read_time = 0, write_time = 0, read_time2 = 0, write_time2 = 0;

    for (int i = 0; i < opt_threads; i++) {
        pthread_mutex_lock(&threads[i].lock);
        read_count += threads[i].read_count;
        write_count += threads[i].write_count;
        read_time += threads[i].read_time;
        write_time += threads[i].write_time;
        read_time2 += threads[i].read_time2;
        write_time2 += threads[i].write_time2;
        threads[i].read_count = threads[i].write_count = 0;
        threads[i].read_time = threads[i].write_time = 0;
        threads[i].read_time2 = threads[i].write_time2 = 0;
        pthread_mutex_unlock(&threads[i].lock);
    }

    if (print) {
        printf("read: [%g, %f, %f]\n",
               read_count / duration, read_time / read_count,
               stddev(read_time, read_time2, read_count));
        printf("write: [%g, %f, %f]\n",
               write_count / duration, write_time / write_count,
               stddev(write_time, write_time2, write_count));
        printf("\n");
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 10) {
        fprintf(stderr, "Usage: TODO\n");
        return 1;
    }

    opt_filesize = atoi(argv[1]);
    opt_filecount = atoi(argv[2]);
    opt_writeratio = atof(argv[3]);
    opt_threads = atoi(argv[4]);
    opt_duration = atoi(argv[5]);
    opt_ops = atof(argv[6]);
    opt_intervals = atoi(argv[7]);
    opt_dirsize = atoi(argv[8]);
    opt_blocksize = atoi(argv[9]);

    srandom(time(NULL));

    start_time = now_hires();

    /* Partition threads into those that should do reads and those for writes,
     * as close as possible to the desired allocation. */
    write_threads = (int)round(opt_threads * opt_writeratio);
    fprintf(stderr, "Using %d threads for reads, %d for writes\n",
            opt_threads - write_threads, write_threads);

    for (int i = 0; i < opt_threads; i++) {
        launch_thread(i);
    }

    for (int i = 0; i < opt_intervals; i++) {
        sleep_micros(opt_duration * 1000000 / opt_intervals);
        reset_stats(1, (double)opt_duration / opt_intervals);
    }

    return 0;
}
