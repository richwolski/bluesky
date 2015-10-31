#!/usr/bin/python
#
# Copyright (C) 2010  The Regents of the University of California
# Written by Michael Vrable <mvrable@cs.ucsd.edu>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

# Run a series of simple test requests against S3 for gathering some basic
# performance numbers.

import boto, time
from boto.s3.connection import SubdomainCallingFormat
from boto.s3.key import Key
import sys, threading, time, Queue
import azure

BUCKET_NAME = 'mvrable-benchmark'
SIZES = [(1 << s) for s in range(12, 23)]

class S3TestConnection:
    def __init__(self):
        self.conn = boto.connect_s3(is_secure=False,
                                    calling_format=SubdomainCallingFormat())
        self.bucket = self.conn.get_bucket(BUCKET_NAME)

    def put_object(self, name, size):
        buf = 'A' * size
        k = Key(self.bucket, name)
        start_time = time.time()
        k.set_contents_from_string(buf)
        #print "%s: %f" % (name, time.time() - start_time)

    def get_object(self, name, byterange=None):
        k = Key(self.bucket, name)
        headers = {}
        if byterange is not None:
            headers['Range'] = 'bytes=%s-%s' % byterange
        start_time = time.time()
        buf = k.get_contents_as_string(headers=headers)
        duration = time.time() - start_time
        return (duration, len(buf))

def parallel_get(name, connections, delay1=0.0):
    #print "Get: %s x %d" % (name, len(connections))
    threads = []
    q = Queue.Queue()
    def launcher(c, name, result_queue):
        result_queue.put(c.get_object(name))
    for i in range(len(connections)):
        c = connections[i]
        threads.append(threading.Thread(target=launcher, args=(c, name, q)))
    for i in range(len(threads)):
        threads[i].start()
    for t in threads: t.join()
    res = []
    while not q.empty():
        res.append(q.get())

    if len(res) == len(connections):
        return res

def parallel_multiget(names, connections, repeat=1):
    requests = Queue.Queue()
    results = [[threading.Lock(), None] for n in names]
    for i in range(len(names)):
        for _ in range(repeat):
            requests.put((names[i], results[i]))

    threads = []
    def launcher(c, requests):
        while True:
            try:
                (n, r) = requests.get(block=False)
                # Possible data race here but it should be harmless
                if r[1] is None:
                    res = c.get_object(n)
                    r[0].acquire()
                    if r[1] is None: r[1] = time.time()
                    r[0].release()
                requests.task_done()
            except Queue.Empty:
                return
    for i in range(len(connections)):
        c = connections[i]
        threads.append(threading.Thread(target=launcher, args=(c, requests)))
    start_time = time.time()
    for i in range(len(threads)):
        threads[i].start()
    requests.join()

    return max(x[1] for x in results) - start_time

def run_test(size, threads, num, logfile=sys.stdout, delay=1.0):
    connections = [S3TestConnection() for _ in range(threads)]
    for i in range(num):
        print "    ...test", i
        res = parallel_get('file-%d-%d' % (size, i), connections)
        if res is not None:
            logfile.write(str(min(res)) + "\n")
        if delay > 0:
            time.sleep(delay)

# Ranges are specified as a start and a length.  Fractional values are
# multiplied by the total size of the object.  Negative start values measure
# from the end of the object.
TESTRANGES = [[None, None],                 # Cold read, then hot read
              [None, (0, 256)],             # Cold read, hot partial read
              [(0, 256), None],             # Cold partial, hot full
              [(-256, 256), None],          # Cold partial end, hot full
              [(0, 256), (0, 256)],         # Repeated range
              [(0, 256), (256, 256)],       # Consecutive ranges
              [(0, 256), (-256, 256)],      # Discontiguous ranges
              [(-256, 256), (0, 256)]]      # Discontiguous ranges

connection = S3TestConnection()
logfile = open('multifetch-simulation.data', 'a')
RANGE = 100
for s in SIZES:
    for i in range(RANGE):
        name = 'file-%d-%d' % (s, i)
        for r in TESTRANGES[i * len(TESTRANGES) // RANGE]:
            if r is not None:
                (x, y) = r
                if abs(x) < 1: x = int(x * s)
                if abs(y) < 1: x = int(y * s)
                if x < 0: x += s
                r = (x, x + y - 1)
            t = connection.get_object(name, r)
            print "%s %s: %s" % (name, r or "", t)

sys.exit(0)
