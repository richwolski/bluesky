#!/usr/bin/python
#
# Read a pcap dump containing a single TCP connection and analyze it to
# determine as much as possible about the performance of that connection.
# (Specifically designed for measuring performance of fetches to Amazon S3.)

import impacket, json, pcapy, re, sys
import impacket.ImpactDecoder, impacket.ImpactPacket

# Estimate of the network RTT
RTT_EST = 0.03 * 1e6

def dump_data(obj):
    return json.dumps(result_list, sort_keys=True, indent=2)

class Packet:
    def __init__(self, connection, ts, pkt):
        self.connection = connection
        self.ts = ts
        self.pkt = pkt
        self.ip = self.pkt.child()
        self.tcp = self.ip.child()

        self.datalen = self.ip.get_ip_len() - self.ip.get_header_size() \
                        - self.tcp.get_header_size()
        self.data = self.tcp.get_data_as_string()[0:self.datalen]

        self.seq = (self.tcp.get_th_seq(), self.tcp.get_th_seq() + self.datalen)
        self.ack = self.tcp.get_th_ack()
        self.id = self.ip.get_ip_id()

        if self.tcp.get_th_sport() == 80:
            # Incoming packet
            self.direction = -1
        elif self.tcp.get_th_dport() == 80:
            # Outgoing packet
            self.direction = 1
        else:
            self.direction = 0

    def __repr__(self):
        return "<Packet[%s]: id=%d seq=%d..%d ack=%d %s>" % \
            ({-1: '<', 1: '>', 0: '?'}[self.direction], self.id,
             self.seq[0], self.seq[1], self.ack, self.ts)

class TcpAnalysis:
    def __init__(self):
        self.start_time = None
        self.decoder = impacket.ImpactDecoder.EthDecoder()
        self.packets = []

    def process_file(self, filename):
        """Load a pcap file and process the packets contained in it."""

        p = pcapy.open_offline(filename)
        p.setfilter(r"ip proto \tcp")
        assert p.datalink() == pcapy.DLT_EN10MB
        p.loop(0, self.packet_handler)

    def packet_handler(self, header, data):
        """Callback function run by the pcap parser for each packet."""

        (sec, us) = header.getts()
        ts = sec * 1000000 + us
        if self.start_time is None:
            self.start_time = ts
        ts -= self.start_time
        pkt = Packet(self, ts, self.decoder.decode(data))
        self.packets.append(pkt)

def split_trace(packets, predicate, before=True):
    """Split a sequence of packets apart where packets satisfy the predicate.

    If before is True (default), the split happens just before the matching
    packet; otherwise it happens just after.
    """

    segment = []
    for p in packets:
        if predicate(p):
            if before:
                if len(segment) > 0:
                    yield segment
                segment = [p]
            else:
                segment.append(p)
                yield segment
                segment = []
        else:
            segment.append(p)
    if len(segment) > 0:
        yield segment

def analyze_get(packets, prev_time = None):
    packets = iter(packets)
    p = packets.next()

    start_ts = p.ts
    id_out = p.id

    # Check for connection establishment (SYN/SYN-ACK) and use that to estimate
    # th network RTT.
    if p.tcp.get_SYN():
        addr = p.ip.get_ip_dst()
        p = packets.next()
        #print "Connection establishment: RTT is", p.ts - start_ts
        return {'syn_rtt': (p.ts - start_ts) / 1e6, 'addr': addr}

    # Otherwise, we expect the first packet to be the GET request itself
    if not(p.direction > 0 and p.data.startswith('GET')):
        #print "Doesn't seem to be a GET request..."
        return

    # Find the first response packet containing data
    while not(p.direction < 0 and p.datalen > 0):
        p = packets.next()

    resp_ts = p.ts
    id_in = p.id
    start_seq = p.seq[0]
    tot_bytes = (p.seq[1] - start_seq) & 0xffffffff
    spacings = []

    #print "Response time:", resp_ts - start_ts

    # Scan through the incoming packets, looking for gaps in either the IP ID
    # field or in the timing
    last_ts = resp_ts
    last_was_short = False
    for p in packets:
        gap = False
        flags = []
        bytenr = (p.seq[1] - start_seq) & 0xffffffff
        if not p.direction < 0: continue
        if p.tcp.get_FIN(): continue

        if last_was_short:
            flags.append('LAST_PACKET_SHORT')
        last_was_short = False
        if p.id != (id_in + 1) & 0xffff:
            gap = True
            flags.append('IPID_GAP')
        if p.datalen not in (1448, 1460):
            last_was_short = True
        if (p.seq[0] - start_seq) & 0xffffffff != tot_bytes:
            flags.append('OUT_OF_ORDER')
        if ((p.seq[0] - start_seq) & 0xffffffff) % 9000 == 0:
            flags.append('9000')
        spacings.append(((p.ts - last_ts) / 1e6, bytenr) + tuple(flags))
        last_ts = p.ts
        id_in = p.id
        tot_bytes = max(tot_bytes, bytenr)

    #print "Transferred %d bytes in %s seconds, initial response after %s" % (tot_bytes, last_ts - start_ts, resp_ts - start_ts)
    if prev_time is not None:
        prev_delay = start_ts - prev_time
    else:
        prev_delay = 0
    return {'bytes': tot_bytes,
            'start_latency': resp_ts - start_ts,
            'finish_latency': last_ts - start_ts,
            'interpacket_times': spacings,
            'delay_from_previous': prev_delay}

if __name__ == '__main__':
    for f in sys.argv[1:]:
        conn = TcpAnalysis()
        conn.process_file(f)
        ts = 0
        def request_start(p):
            return p.direction > 0 and p.datalen > 0
        result_list = []
        prev_time = None
        for s in split_trace(conn.packets, request_start):
            s = list(s)
            if False:
                for p in s:
                    #if p.ts - ts > 0.01:
                        #print "----"
                    #if p.ts - ts > 2 * RTT_EST:
                        #print "LONG DELAY\n----"
                    ts = p.ts
                    #print p
                    #if p.direction > 0 and p.datalen > 0:
                        #print "Request:", repr(p.data)
            results = analyze_get(s, prev_time)
            if results is not None:
                result_list.append(results)
            prev_time = s[-1].ts
            #print "===="

        print dump_data(result_list)
