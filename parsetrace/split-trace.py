#!/usr/bin/python
#
# Split a tcpdump trace apart into multiple files, each containing a single TCP
# flow.

import impacket, itertools, pcapy, re, socket, subprocess, sys
import impacket.ImpactDecoder, impacket.ImpactPacket

# Domain names for cloud service providers, whose traces we want to pull out.
DOMAINS = ['.amazon.com', '.amazonaws.com', '.core.windows.net',
           '204.246.', '87.238.']

# The collection of flows we've seen.  The value associated with each flow is a
# sequence number indicating in what order we saw the flows in the trace.
flows = {}

def ip_lookup(host, cache={}):
    if host not in cache:
        try:
            cache[host] = socket.gethostbyaddr(dst)[0]
        except:
            cache[host] = host
    return cache[host]

# Step 1: Parse the input file and extract a listing of all the flows that we
# care about.
def handler(header, data):
    pkt = decoder.decode(data)
    ip = pkt.child()
    tcp = ip.child()
    src = (ip.get_ip_src(), tcp.get_th_sport())
    dst = (ip.get_ip_dst(), tcp.get_th_dport())
    flow = tuple(sorted([src, dst],
                        cmp=lambda x, y: cmp(x[1], y[1]) or cmp(x[0], y[0])))
    if flow not in flows:
        flows[flow] = max(itertools.chain(flows.values(), [0])) + 1

def scan(filename):
    global decoder
    p = pcapy.open_offline(filename)
    p.setfilter(r"ip proto \tcp")
    assert p.datalink() == pcapy.DLT_EN10MB
    decoder = impacket.ImpactDecoder.EthDecoder()
    p.loop(0, handler)

for file in sys.argv[1:]:
    print "Scanning %s..." % (file,)
    scan(file)

    filters = {}
    for (((dst, dport), (src, sport)), seq) in flows.items():
        # Filter out to find just the relevant flows.  Right now we want only
        # flows to port 80 (since both S3/Azure use that as the service port
        # when unencrypted which is what we use).  We probably ought to apply
        # another filter on IP address in case there happened to be any other
        # HTTP flows during the trace capture.
        if dport != 80: continue
        name = ip_lookup(dst)
        matches = False
        for d in DOMAINS:
            if name.endswith(d): matches = True
            if name.startswith(d): matches = True
        if not matches:
            print "Host", name, "not recognized, skipping"
            continue

        filter = "tcp and (host %s and host %s) and (port %d and port %d)" \
            % (src, dst, sport, dport)
        filters[seq] = (filter, name)

    n = 0
    for (_, (filter, name)) in sorted(filters.items()):
        print "%d: %s" % (n, filter)
        subprocess.check_call(['tcpdump', '-s0', '-r', file, '-w',
                               'trace-%03d-%s' % (n, name),
                               filter])
        n += 1
