#!/usr/bin/python
#
# Print mean and standard deviation of a series of numbers read from stdin.

import sys

inputs = ''.join(sys.stdin.readlines()).split()
vals = map(float, inputs)
mean = sum(vals) / len(vals)
if len(vals) > 1:
    stddev = (sum((x - mean)**2 for x in vals) / (len(vals) - 1)) ** 0.5
else:
    stddev = 0.0

print "%s\t%s" % (mean, stddev)
