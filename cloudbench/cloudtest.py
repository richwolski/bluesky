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
import azure

BUCKET_NAME = 'mvrable-benchmark'
SIZES = [64, 4096, 32 << 10, 256 << 10, 1 << 20, 4 << 20, 32 << 20]

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
        print "%s: %f" % (name, time.time() - start_time)

    def get_object(self, name):
        k = Key(self.bucket, name)
        start_time = time.time()
        buf = k.get_contents_as_string()
        print "%s: %f" % (name, time.time() - start_time)

class AzureTestConnection:
    def __init__(self):
        self.conn = azure.Connection()

    def put_object(self, name, size):
        buf = 'A' * size
        start_time = time.time()
        self.conn.make_request('/benchmark/' + name, 'PUT', buf,
                               {'x-ms-blob-type': 'BlockBlob'})
        print "%s: %f" % (name, time.time() - start_time)

    def get_object(self, name):
        start_time = time.time()
        self.conn.make_request('/benchmark/' + name, 'GET')
        print "%s: %f" % (name, time.time() - start_time)

def run_test():
    print "==== S3 ===="
    c = S3TestConnection()
    for repeat in range(4):
        for size in SIZES:
            c.put_object('file-%d-%d' % (size, repeat), size)

    c = S3TestConnection()
    for repeat in range(4):
        for size in SIZES:
            c.get_object('file-%d-%d' % (size, repeat))

    print "==== AZURE ===="
    c = AzureTestConnection()
    for repeat in range(4):
        for size in SIZES:
            c.put_object('file-%d-%d' % (size, repeat), size)

    c = AzureTestConnection()
    for repeat in range(4):
        for size in SIZES:
            c.get_object('file-%d-%d' % (size, repeat))

if __name__ == '__main__':
    #run_test()
    SIZES = [4096, 32 << 10, 256 << 10, 1 << 20, 4 << 20]
    PRIME = (1 << 20) + (1 << 10)
    c = AzureTestConnection()
    for size in SIZES:
        c.put_object('file-%d-%d' % (size, 0), size)
    c.put_object('file-%d-%d' % (PRIME, 0), PRIME)

    for size in SIZES:
        for n in range(50):
            c = AzureTestConnection()
            c.get_object('file-%d-%d' % (PRIME, 0))
            time.sleep(2.0)
            c.get_object('file-%d-%d' % (size, 0))
