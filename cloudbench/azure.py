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

"""A simple test API for accessing Windows Azure blob storage.

Parts of the code are modeled after boto (a library for accessing Amazon Web
Services), but this code is far less general and is meant only as a
proof-of-concept."""

import base64, hashlib, hmac, httplib, os, time

# The version of the Azure API we implement; sent in the x-ms-version header.
API_VERSION = '2009-09-19'

def uri_decode(s):
    # TODO
    return s

def add_auth_headers(headers, method, path):
    header_order = ['Content-Encoding', 'Content-Language', 'Content-Length',
                    'Content-MD5', 'Content-Type', 'Date', 'If-Modified-Since',
                    'If-Match', 'If-None-Match', 'If-Unmodified-Since',
                    'Range']

    if not headers.has_key('Date'):
        headers['Date'] = time.strftime("%a, %d %b %Y %H:%M:%S GMT",
                                        time.gmtime())
    if not headers.has_key('x-ms-version'):
        headers['x-ms-version'] = API_VERSION

    StringToSign = method + "\n"
    for h in header_order:
        if h in headers:
            StringToSign += headers[h] + "\n"
        else:
            StringToSign += "\n"

    # Add Canonicalized Headers
    canonized = []
    for (k, v) in headers.items():
        k = k.lower()
        if k.startswith('x-ms-'):
            canonized.append((k, v))
    canonized.sort()
    for (k, v) in canonized:
        StringToSign += "%s:%s\n" % (k, v)

    # Add CanonicalizedHeaders Resource
    account_name = os.environ['AZURE_ACCOUNT_NAME']
    account_name = 'bluesky'
    resource = "/" + account_name
    if '?' not in path:
        resource += path
    else:
        (path, params) = path.split('?', 1)
        params = [p.split('=') for p in params.split("&")]
        params = dict((k.lower(), uri_decode(v)) for (k, v) in params)
        resource += path
        for k in sorted(params):
            resource += "\n%s:%s" % (k, params[k])
    StringToSign += resource

    # print "String to sign:", repr(StringToSign)

    secret_key = os.environ['AZURE_SECRET_KEY']
    secret_key = base64.b64decode(secret_key)
    h = hmac.new(secret_key, digestmod=hashlib.sha256)
    h.update(StringToSign)

    signature = base64.b64encode(h.digest())
    headers['Authorization'] = "SharedKey %s:%s" % (account_name, signature)

class Connection:
    def __init__(self):
        self.host = os.environ['AZURE_ACCOUNT_NAME'] + ".blob.core.windows.net"
        self.conn = httplib.HTTPConnection(self.host)

    def make_request(self, path, method='GET', body="", headers={}):
        headers = headers.copy()
        headers['Content-Length'] = str(len(body))
        if len(body) > 0:
            headers['Content-MD5'] \
                = base64.b64encode(hashlib.md5(body).digest())
        add_auth_headers(headers, method, path)

        # req = "%s %s HTTP/1.1\r\nHost: %s\r\n" % (method, path, host)
        # req = req + ''.join("%s: %s\r\n" % h for h in headers.items()) + "\r\n"
        # print req

        self.conn.request(method, path, body, headers)
        response = self.conn.getresponse()
        print "Response:", response.status
        print "Headers:", response.getheaders()
        body = response.read()

if __name__ == '__main__':
    # generate_request("/?comp=list")
    buf = 'A' * 1048576
    conn = Connection()
    for i in range(16):
        conn.make_request('/benchmark/file-1M-' + str(i), 'PUT', buf,
                          {'x-ms-blob-type': 'BlockBlob'})

    conn = Connection()
    for i in range(16):
        conn.make_request('/benchmark/file-1M-' + str(i), 'GET')
