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

import sys

lines = list(open('delays.data'))
classes = set()
classes.add("")
for line in lines:
    line = line.strip().split('\t')
    try:
        classes.add(line[3])
    except:
        pass

cmd = "plot"
replace = {}
n = 0
print classes
for c in sorted(classes):
    if n > 0:
        cmd += ","
    cmd += ' "delays-1.data" using ($4 == %d ? $2 : 1.0/0.0):1 title "%s"' % (n, c or 'NORMAL')
    replace[c] = n
    n += 1

print cmd

fp = open('delays-1.data', 'w')
for line in lines:
    line = line.strip().split('\t')
    while len(line) < 4: line.append("")
    line[3] = str(replace[line[3]])
    fp.write('\t'.join(line) + '\n')
