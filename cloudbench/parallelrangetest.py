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

def parallel_rangeget(name, size, connections, pieces, repeat=1):
    requests = Queue.Queue()
    results = [[threading.Lock(), None] for n in range(pieces)]
    for _ in range(repeat):
        for i in range(pieces):
            requests.put((i, results[i]))
    blocksize = size // pieces

    threads = []
    def launcher(c, requests):
        while True:
            try:
                (i, r) = requests.get(block=False)
                # Possible data race here but it should be harmless
                if r[1] is None:
                    res = c.get_object(name, byterange=(blocksize * i,
                                                        blocksize * (i+1) - 1))
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

connections = [S3TestConnection() for _ in range(128)]

for i in [(1 << x) for x in range(8)]:
    s = (1 << 22)
    print i, parallel_rangeget('file-%d-0' % (s,), s, connections, i)
    time.sleep(4.0)
sys.exit(0)

logfile = open('multifetch-simulation.data', 'a')
for s in [(1 << s) for s in range(16, 27)]:
    print "Priming objects: %d-byte objects" % (s,)
    run_test(s, 1, 100, open('/dev/null', 'w'), 0.0)

    for blocksize in [x << 20 for x in (4, 8, 16, 32, 64, 128)]:
        if s > blocksize: continue
        for t in [4, 2, 1]:
            for rep in range(10):
                count = blocksize // s
                print "Running tests: %d-byte blocks, %d-byte objects, %d parallel fetches" % (blocksize, s, t)
                print "Object count:", count
                if count * t > len(connections):
                    conns = connections
                else:
                    conns = connections[0 : count * t]

                objects = ['file-%d-%d' % (s, i % 100) for i in range(count)]
                r = parallel_multiget(objects, conns, t)
                print r
                logfile.write('%s\t%s\t%s\t%s\t%s\n' % (s, blocksize >> 20, t, len(conns), r))
                logfile.flush()
                time.sleep(2.0)
