#!/usr/bin/python

import json, sys

times = []

for f in sys.argv[1:]:
    stats = json.load(open(f))
    for n in range(len(stats)):
        s = stats[n]
        name = "%s-%d" % (f, n)
        event_count = 0
        event_locs = []
        delay_total = 0.0
        if 'interpacket_times' in s:
            for x in s['interpacket_times']:
                flags = x[2:]
                if 'LAST_PACKET_SHORT' in flags and '9000' in flags:
                    event_count += 1
                    delay_total += x[0]
                    event_locs.append(x[1])
            total_time = s['finish_latency'] * 1e-6
            if event_count > 0 or True:
                #print "%s: %d %s" % (name, event_count, event_locs)
                print "%d\t%s\t%s\t%f" % (event_count, delay_total, event_count and delay_total / event_count or 0.0, total_time)

for t in times:
    print "%f\t%s\t%s" % (t[0], t[1], ','.join(t[2]))

