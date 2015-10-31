#!/usr/bin/python

import json, sys

times = []

for f in sys.argv[1:]:
    stats = json.load(open(f))
    for s in stats:
        if 'interpacket_times' in s:
            times += [(x[0], f, x[2:]) for x in s['interpacket_times']
                      if x[0] > 0.005]

for t in times:
    print "%f\t%s\t%s" % (t[0], t[1], ','.join(t[2]))
