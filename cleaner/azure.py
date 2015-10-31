# Blue Sky: File Systems in the Cloud
#
# Copyright (C) 2011  The Regents of the University of California
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

"""A simple Python library for accessing the Windows Azure blob service."""

import base64, hashlib, hmac, httplib, os, time, urllib
import xml.dom.minidom

# The version of the Azure API we implement; sent in the x-ms-version header.
API_VERSION = '2009-09-19'

def uri_decode(s):
    return urllib.unquote_plus(s)

def uri_encode(s):
    return urllib.quote_plus(s)

def xmlGetText(nodelist):
    text = []
    def walk(nodes):
        for node in nodes:
            if node.nodeType == node.TEXT_NODE:
                text.append(node.data)
            else:
                walk(node.childNodes)
    walk(nodelist)
    return ''.join(text)

def xmlParse(s):
    return xml.dom.minidom.parseString(s)

def buildQueryString(uri, params={}):
    for (k, v) in params.items():
        if v is None: continue
        kv = '%s=%s' % (uri_encode(k), uri_encode(v))
        if '?' not in uri:
            uri += '?' + kv
        else:
            uri += '&' + kv
    return uri

def add_auth_headers(headers, method, path, account, key):
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

    resource = "/" + account
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

    h = hmac.new(key, digestmod=hashlib.sha256)
    h.update(StringToSign)

    signature = base64.b64encode(h.digest())
    headers['Authorization'] = "SharedKey %s:%s" % (account, signature)

class AzureError(RuntimeError):
    def __init__(self, response):
        self.response = response
        self.details = response.read()

class AzureConnection:
    def __init__(self, account=None, key=None):
        if account is None:
            account = os.environ['AZURE_ACCOUNT_NAME']
        self.account = account
        self.host = account + ".blob.core.windows.net"
        #self.conn = httplib.HTTPConnection(self.host)

        if key is None:
            key = os.environ['AZURE_SECRET_KEY']
        self.key = base64.b64decode(key)

    def _make_request(self, path, method='GET', body="", headers={}):
        headers = headers.copy()
        headers['Content-Length'] = str(len(body))
        if len(body) > 0:
            headers['Content-MD5'] \
                = base64.b64encode(hashlib.md5(body).digest())
        add_auth_headers(headers, method, path, self.account, self.key)

        conn = httplib.HTTPConnection(self.host)
        conn.request(method, path, body, headers)
        response = conn.getresponse()
        if response.status // 100 != 2:
            raise AzureError(response)
        return response
        #print "Response:", response.status
        #print "Headers:", response.getheaders()
        #body = response.read()

    def list(self, container, prefix=''):
        marker = None
        while True:
            path = '/' + container + '?restype=container&comp=list'
            path = buildQueryString(path, {'prefix': prefix, 'marker': marker})
            r = self._make_request(path)
            xml = xmlParse(r.read())

            blobs = xml.getElementsByTagName('Blob')
            for b in blobs:
                yield xmlGetText(b.getElementsByTagName('Name'))

            marker = xmlGetText(xml.getElementsByTagName('NextMarker'))
            if marker == "":
                return

    def get(self, container, key):
        path = "/%s/%s" % (container, key)
        r = self._make_request(path)
        return r.read()

    def put(self, container, key, value):
        path = "/%s/%s" % (container, key)
        r = self._make_request(path, method='PUT', body=value,
                               headers={'x-ms-blob-type': 'BlockBlob'})

    def delete(self, container, key):
        path = "/%s/%s" % (container, key)
        r = self._make_request(path, method='DELETE')

def parallel_delete(container, keys):
    import Queue
    from threading import Lock, Thread

    keys = list(iter(keys))

    q = Queue.Queue(16384)
    l = Lock()

    def deletion_task():
        conn = AzureConnection()
        while True:
            k = q.get()
            l.acquire()
            print k
            l.release()
            conn.delete(container, k)
            q.task_done()

    for i in range(128):
        t = Thread(target=deletion_task)
        t.setDaemon(True)
        t.start()

    for k in keys:
        q.put(k)
    q.join()

if __name__ == '__main__':
    container = 'bluesky'
    conn = AzureConnection()

    conn.put(container, "testkey", "A" * 40)
    print "Fetch result:", conn.get(container, "testkey")

    parallel_delete(container, conn.list(container))
