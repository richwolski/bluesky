#!/usr/bin/python
#
# Convert a file with a sequence of data values to a CDF ready for gnuplot.

import re, sys

def split_line(l):
    m = re.match(r"^([-+\d.e]+)(.*)$", l)
    return (float(m.group(1)), m.group(2))

data = [split_line(s.strip()) for s in sys.stdin]
data.sort()

for i in range(len(data)):
    sys.stdout.write("%s\t%s\n" % ((i + 1.0) / len(data), ''.join(map(str, data[i]))))
