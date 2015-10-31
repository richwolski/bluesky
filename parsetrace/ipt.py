#!/usr/bin/python
#
# Analyze the inter-packet times of requests to/from cloud storage providers.

import json, sys

RTT_EST = 0.03

def analyze_transfer(timings):
    for (delay, bytes) in timings:
        gap = False
        if delay > 2 * RTT_EST:
            gap = True
            print "Long gap of", delay
        elif delay > RTT_EST / 2:
            gap = True
            print "Short gap of", delay
        if gap:
            print "    [occurred after", bytes, "bytes]"

for f in sys.argv[1:]:
    stats = json.load(open(f))
    for s in stats:
        if 'interpacket_times' in s:
            analyze_transfer(s['interpacket_times'])
