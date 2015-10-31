#!/usr/bin/python
#
# A simple benchmark for Blue Sky that will read and/or write a collection of
# files with a specified working set size, and measure the response time to do
# so.

import json, math, os, random, sys, threading, time

THREADS = 1

def percentile(rank, elements):
    """Return the value at a fraction rank from the beginning of the list."""

    # Convert a fraction [0.0, 1.0] to a fractional index in elements.
    rank = rank * (len(elements) - 1)

    # Round the index value to the nearest integer values, then interpolate.
    prev = int(math.floor(rank))
    next = int(math.ceil(rank))
    frac = rank - prev

    return (1.0 - frac) * elements[prev] + frac * elements[next]

class WorkerThread:
    """Performs a mix of file system operations and records the performance."""

    PARAMS = ['duration', 'write_fraction', 'wss_count', 'tot_count',
              'filesize', 'target_ops']

    def __init__(self):
        self.stats = []
        self.duration = 1800.0      # Seconds for which to run
        self.write_fraction = 0.5   # Fraction of operations which are writes
        self.wss_count = 2048       # Files in the working set
        self.tot_count = 2048       # Total number of files created
        self.filesize = 32 * 1024   # Size of files to work with
        self.target_ops = 40        # Target operations/second/thread

    def get_params(self):
        params = {}
        for p in self.PARAMS:
            params[p] = getattr(self, p)
        return params

    def setup(self):
        for i in range(self.tot_count):
            filename = "file-%d" % (i,)
            fp = open(filename, 'w')
            fp.write('\0' * self.filesize)
            fp.close()

    def run(self):
        stop_time = time.time() + self.duration

        while True:
            time1 = time.time()
            if time1 >= stop_time: break
            info = self._operation()
            time2 = time.time()
            self.stats.append((time1, time2 - time1, info))
            #print self.stats[-1]
            delay = time1 + (1.0 / self.target_ops) - time2
            if delay > 0: time.sleep(delay)

    def _operation(self):
        """Run a single file system test (i.e., read or write a file)."""

        filename = "file-%d" % (random.randrange(self.wss_count),)

        if random.uniform(0.0, 1.0) < self.write_fraction:
            fp = open(filename, 'w')
            fp.write('\0' * self.filesize)
            fp.close()
            return ('write', filename)
        else:
            fp = open(filename, 'r')
            fp.read()
            fp.close()
            return ('read', filename)

def print_distribution_stats(stats):
    stats = sorted(stats)
    print "  Count:", len(stats)
    if len(stats) == 0: return
    print "  Average:", sum(stats) / len(stats)
    for (s, p) in [("Min", 0.0), ("Med", 0.5), ("90%", 0.9),
                   ("95%", 0.95), ("Max", 1.0)]:
        print "  %s: %s" % (s, percentile(p, stats))

def run_stats(stats):
    duration = max(x[0] for x in stats) - min(x[0] for x in stats)
    latencies = [x[1] for x in stats]
    latencies.sort()
    print "Experiment duration:", duration
    print "READS:"
    print_distribution_stats([x[1] for x in stats if x[2][0] == 'read'])
    print "WRITES:"
    print_distribution_stats([x[1] for x in stats if x[2][0] == 'write'])

fp = open('/tmp/results.json', 'a')

def run(filecount, writefrac, filesize):
    workers = []
    threads = []
    for i in range(THREADS):
        w = WorkerThread()
        w.write_fraction = writefrac
        w.wss_count = w.tot_count = filecount
        w.filesize = filesize
        if i == 0: w.setup()
        t = threading.Thread(target=w.run)
        threads.append(t)
        workers.append(w)
        t.start()

    print json.dumps(workers[0].get_params(), indent=2)

    for t in threads:
        t.join()

    results = []
    for w in workers:
        results += w.stats
    results.sort()

    fp.write(json.dumps(workers[0].get_params(), indent=2) + "\n\n")
    fp.write(json.dumps(results, indent=2))
    fp.write("\n\n")
    run_stats(results)

if __name__ == '__main__':
    for filesize in [32, 256, 2048]:            # KiB
        for totsize in [256, 512, 1024]:        # MiB
            filecount = totsize * 1024 / filesize
            for writefrac in [0.0, 0.5]:
                run(filecount, writefrac, filesize * 1024)
    fp.close()
