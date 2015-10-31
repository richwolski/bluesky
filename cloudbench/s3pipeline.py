#!/usr/bin/python2.6
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

import boto, httplib, socket, sys, time
from boto.s3.key import Key
from threading import Thread

bucket_name = 'mvrable-benchmark'
conn = boto.connect_s3(is_secure=False)

class HttpResponseParser:
    def __init__(self):
        self.buf = ""
        self.header = True
        self.content_length = None

    def process(self, data):
        self.buf = self.buf + data
        while self.parse():
            pass

    def parse(self):
        if len(self.buf) == 0:
            return False

        if not self.header and self.content_length is not None:
            consumed = min(self.content_length, len(self.buf))
            #print "Got", consumed, "bytes of data"
            self.content_length -= consumed
            self.buf = self.buf[consumed:]

            if self.content_length == 0:
                print "Completed reading body"
                self.content_length = None
                self.header = True

            return True

        crlf = self.buf.find("\r\n")
        if crlf < 0:
            return False

        line = self.buf[0:crlf]
        self.buf = self.buf[crlf+2:]
        #print "Header line:", line
        if line.lower().startswith('content-length'):
            self.content_length = int(line[16:])
            print "Content length:", self.content_length
        if line == "":
            self.header = False
        return True

class PipelinedRequester:
    def __init__(self, bucket):
        self.bucket = bucket
        self.host = conn.calling_format.build_host(conn.server_name(), bucket)
        self.sock = socket.create_connection((self.host, 80))

        t = Thread(target=self.reader_thread)
        t.setDaemon(True)
        t.start()

    def reader_thread(self):
        hrp = HttpResponseParser()
        while True:
            buf = self.sock.recv(4096)
            if len(buf) == 0: break
            hrp.process(buf)

    def send_request(self, key):
        method = 'GET'
        path = conn.calling_format.build_path_base(self.bucket, key)
        auth_path = conn.calling_format.build_auth_path(self.bucket, key)

        headers = {'User-Agent': boto.UserAgent + " (pipelined)",
                   'Content-Length': str(0)}
        conn.add_aws_auth_header(headers, method, auth_path)

        req = "%s %s HTTP/1.1\r\nHost: %s\r\n" % (method, path, self.host)
        req = req + ''.join("%s: %s\r\n" % h for h in headers.items()) + "\r\n"
        self.sock.sendall(req)

requester = PipelinedRequester(bucket_name)
for i in range(12, 18):
    requester.send_request('file-%d-1' % (1 << i,))
    if i == 12:
        time.sleep(2)
for i in range(32):
    requester.send_request('file-8192-%d' % (i,))

time.sleep(5)
