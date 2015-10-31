#!/usr/bin/python
#
# A workload generator for the cleaner benchmarks.  This will randomly write to
# a collection of files at a fixed rate, to dirty data in the file system.
#
# DO NOT RUN FROM A DIRECTORY WITH FILES YOU CARE ABOUT--it will overwrite
# files in and under the current directory.

import os, random, sys, time

def write_file(path, size=1024**2):
    buf = 'A' * size
    f = open(path, 'w')
    f.write(buf)
    f.close()

def modify_files(files, rate=1.0, fraction=0.25):
    files = random.sample(files, int(round(len(files) * fraction)))
    print "Modifying", len(files), "files"

    last_time = time.time()
    latencies = []
    for f in files:
        now = time.time()
        next_time = last_time + (1.0/rate)
        time.sleep(max(0.0, next_time - now))
        print "Writing", f
        write_file(f)
        last_time = next_time
    print "Done"

if __name__ == '__main__':
    print "Modifying files in", os.getcwd()
    time.sleep(15)
    all_files = []
    for (path, dirs, files) in iter(os.walk(".")):
        for f in files:
            all_files.append(os.path.join(path, f))
    print len(all_files), "files total"
    modify_files(all_files, rate=1e6)
