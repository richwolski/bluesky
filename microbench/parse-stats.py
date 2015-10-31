#!/usr/bin/python2
#
# Parse the periodically-dumped statistics counters from the BlueSky proxy.
# Can be used to reconstruct the costs for interactions with S3 over time.
#
# To plot S3 (standard region) costs:
#   plot "stats.data" using 1:($2 * 0.01e-4 + $4 * 0.01e-3 + $3 * 0.15/2**30 + $5 * 0.10/2**30) with lines
#
# NFS operation counts:
#   plot "stats.data" using 1:6 with linespoints title "NFS In", "stats.data" using 1:6 with linespoints title "NFS Out"

import os, re, sys

counter_map = [
    (re.compile(r"Store.*GETS"), 'store-get'),
    (re.compile(r"Store.*PUTS"), 'store-put'),
    (re.compile(r"NFS RPC.*In"), 'nfs-in'),
    (re.compile(r"NFS RPC.*Out"), 'nfs-out'),
]

def process_file(fp, outfile):
    timestamp = 0
    stats = {}

    outfile.write("# Timestamp\t")
    for (k, v) in counter_map:
        outfile.write("%s\t" % (v,))
    outfile.write("\n")

    def dump():
        if len(stats) > 0:
            outfile.write("%f\t" % (timestamp,))
            for (k, v) in counter_map:
                outfile.write("%d\t%d\t" % (stats.get(v, (0, 0))))
            outfile.write("\n")
            stats.clear()

    for line in fp:
        if line.startswith("****"):
            dump()

        m = re.match(r"^time=([\d.]+)", line)
        if m:
            timestamp = float(m.group(1))

        m = re.match(r"^(.*): count=(\d+) sum=(\d+)", line)
        if m:
            vals = (int(m.group(2)), int(m.group(3)))
            for (k, v) in counter_map:
                if k.match(m.group(1)):
                    if v not in stats: stats[v] = (0, 0)
                    stats[v] = (stats[v][0] + vals[0], stats[v][1] + vals[1])

    dump()

if __name__ == '__main__':
    process_file(sys.stdin, sys.stdout)
