/* A very simple program to lock a specified amount of memory (in megabytes)
 * into memory.  Used to decrease the available amount of free memory on a
 * system for benchmarking purposes. */

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,
                "Usage: %s <amount in MiB>\n"
                "Locks the specified number of megabytes of RAM.\n",
                argv[0]);
        return 1;
    }

    size_t bufsize = atoi(argv[1]) << 20;
    char *buf = malloc(bufsize);
    if (buf == NULL) {
        fprintf(stderr, "Unable to allocate buffer: %m\n");
        return 1;
    }

    if (mlock(buf, bufsize) < 0) {
        fprintf(stderr, "Unable to lock memory: %m\n");
        return 1;
    }

    /* Infinite loop; simply wait for the program to be killed. */
    while (1) {
        sleep(3600);
    }

    return 0;
}
