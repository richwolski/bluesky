#!/usr/bin/python
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

import struct, sys

DATAPOINT = '<II'

def load_log(f):
    data = []
    size = struct.calcsize(DATAPOINT)
    d = f.read(size)
    while len(d) == size:
        i = struct.unpack(DATAPOINT, d)
        data.append(i)
        d = f.read(size)
    return data

if __name__ == '__main__':
    blocksize = int(sys.argv[1])
    data = []
    for f in sys.argv[2:]:
        data += load_log(open(f))
    data.sort()

    duration = data[-1][0] - data[0][0]
    #print "Time span: %d to %d (%d seconds)" % (data[0][0], data[-1][0], duration)

    start = data[0][0] + 5
    end = data[-1][0] - 5
    truncated = [d for d in data if start <= d[0] <= end]
    #print len(data), len(truncated)
    duration = float(end - start + 1)
    #print duration
    #print "# ops/sec\tbandwidth (MB/s)\tLatency(ms)"
    print "%s\t%s\t%s" % (len(truncated) / duration,
                          len(truncated) / duration * blocksize / 1024.0**2,
                          sum(d[1] for d in truncated) / len(truncated) / 1000)
