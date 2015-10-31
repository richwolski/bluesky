#!/usr/bin/env python
#
# Delete all files on S3 used for a benchmarking run (we don't want to be
# charged for storage of them).

import boto, sys, time, Queue
from boto.s3.connection import SubdomainCallingFormat
from threading import Lock, Thread

if len(sys.argv) > 1:
    bucket_name = sys.argv[1]
else:
    bucket_name = 'mvrable-bluesky'

DELAY = 4.0
print "Will delete all contents of %s starting in %s seconds..." \
        % (bucket_name, DELAY)
time.sleep(DELAY)

THREADS = 32
print "Using %d threads" % (THREADS,)

q = Queue.Queue(THREADS * 4096)
l = Lock()

def deletion_task():
    conn = boto.connect_s3()
    bucket = conn.get_bucket(bucket_name)
    while True:
        k = q.get()
        l.acquire()
        print k
        l.release()
        bucket.delete_key(k)
        q.task_done()

for i in range(THREADS):
    t = Thread(target=deletion_task)
    t.setDaemon(True)
    t.start()

conn = boto.connect_s3(is_secure=False,
                       calling_format=SubdomainCallingFormat())
bucket = conn.get_bucket(bucket_name)
for k in bucket:
    q.put(k.key)

q.join()
time.sleep(0.5)

print "Finished"
