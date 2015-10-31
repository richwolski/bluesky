#!/usr/bin/python
#
# A small synthetic write benchmark for BlueSky.  We want to see what happens
# when there are write bursts that fill up the disk cache faster than it can be
# drained to the cloud.  This tool simply writes out 1 MB files to a directory,
# recording the time needed to write each one.  The rate at which files are
# written out can be specified in files (/megabytes) per second.

import os, sys, time

DIRSIZE=100

def write_file(path, n, size=1024**2):
    buf = 'A' * size
    try: os.mkdir('%s/%d' % (path, n // DIRSIZE))
    except: pass
    t1 = time.time()
    f = open('%s/%d/%d' % (path, n // DIRSIZE, n), 'w')
    f.write(buf)
    f.close()
    t2 = time.time()
    return t2 - t1

def run_writebench(path, rate):
    count = 0
    start_time = time.time()
    last_time = start_time
    latencies = []
    while last_time < start_time + 120:
        now = time.time()
        next_time = start_time + (1.0/rate) * count
        time.sleep(max(0.0, next_time - now))
        last_time = time.time()
        latencies.append(write_file(path, count))
        print "%s\t%s" % (latencies[-1], time.time() - start_time)
        count += 1
    end_time = time.time()
    duration = end_time - start_time
    print "# %s MB/s (%d MB/%s seconds)" % (count / duration, count, duration)
    return latencies

if __name__ == '__main__':
    rate = float(sys.argv[1])
    latencies = run_writebench('.', rate)
