#!/usr/bin/python

import json, sys

for f in sys.argv[1:]:
    stats = json.load(open(f))
    addr = ''
    for s in stats:
        try:
            addr = s['addr']
        except:
            pass

        try:
            size = s['bytes']
            lat1 = s['start_latency']
            lat2 = s['finish_latency']
            print "%d\t%f\t%f\t%s" % (size, lat1, lat2, addr)
        except:
            pass
