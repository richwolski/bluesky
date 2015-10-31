#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

int64_t now_hires()
{
    struct timespec time;

    if (clock_gettime(CLOCK_REALTIME, &time) != 0) {
        perror("clock_gettime");
        return 0;
    }

    return (int64_t)(time.tv_sec) * 1000000000 + time.tv_nsec;
}

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

int main(int argc, char *argv[])
{
    bench_write();
}
