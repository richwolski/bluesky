#!/usr/bin/python

import impacket, pcapy, re, sys
import impacket.ImpactDecoder, impacket.ImpactPacket

start_time = None

flows = {}

STATE_START = 0
STATE_REQ_SENT = 1
STATE_REQ_ACKED = 2
STATE_RESP_START = 3

logfile = open('times.data', 'w')

def seq_after(x, y):
    """Compares whether x >= y in sequence number space."""

    delta = (x - y) % (1 << 32)
    return delta < (1 << 31)

class Connection:
    counter = 0

    def __init__(self, endpoints):
        self.endpoints = endpoints
        self.packets = []
        self.state = STATE_START
        self.id = Connection.counter
        self.times = []
        self.transfer_count = 0
        Connection.counter += 1
        self.last_id = 0
        self.winscale = {1: 0, -1: 0}

    def finish_transfer(self):
        if len(self.times) > 0:
            rtt = self.times[0][0]
            try:
                start = iter(t[0] for t in self.times if t[1] > 0).next()
            except:
                start = 0.0
            end = self.times[-1][0]
            data = self.times[-1][1]
            print "Connection %d Transfer #%d" % (self.id, self.transfer_count)
            print "Network RTT:", rtt
            print "Additional response delay:", start - rtt
            print "Transfer time:", end - start
            if end - start > 0:
                print "Bandwidth:", data / (end - start)
            print
            logfile.write("%d\t%d\t%d\t%f\t%f\t%f\t# %s\n"
                          % (self.id, self.transfer_count, data,
                              rtt, start - rtt, end - start, self.endpoints))
            self.transfer_count += 1
        self.times = []
        self.state = STATE_START

    def process(self, timestamp, packet):
        ip = pkt.child()
        tcp = ip.child()
        self.packets.append(packet)

        datalen = ip.get_ip_len() - ip.get_header_size() - tcp.get_header_size()
        data = tcp.get_data_as_string()[0:datalen]

        if tcp.get_th_sport() == 80:
            # Incoming packet
            direction = -1
        elif tcp.get_th_dport() == 80:
            # Outgoing packet
            direction = 1
        else:
            direction = 0

        for o in tcp.get_options():
            if o.get_kind() == o.TCPOPT_WINDOW:
                self.winscale[direction] = o.get_shift_cnt()
                print "window scale for dir %d is %d" % (direction,
                                                         o.get_shift_cnt())

        if direction < 0:
            gap = (ip.get_ip_id() - self.last_id) & 0xffff
            if 1 < gap < 256:
                print "Gap of", gap, "packets on connection", self.endpoints
            self.last_id = ip.get_ip_id()

        seq = (tcp.get_th_seq(), tcp.get_th_seq() + datalen)
        ack = tcp.get_th_ack()

        # Previous request finished
        if self.state == STATE_RESP_START and direction > 0 \
                and data.startswith('GET /'):
            self.finish_transfer()

        # New request seen on an idle connection...
        if self.state == STATE_START and direction > 0 \
                and data.startswith('GET /'):
            self.startseq = seq[1]
            self.starttime = timestamp
            self.state = STATE_REQ_SENT

        # Request is acknowledged, but response not yet seen
        if self.state == STATE_REQ_SENT and direction < 0 \
                and seq_after(ack, self.startseq):
            self.state = STATE_REQ_ACKED
            self.respseq = seq[0]
            self.times.append(((timestamp - self.starttime) / 1e6, 0))

        # Response header to request has been seen
        if self.state == STATE_REQ_ACKED and direction < 0 \
                and data.startswith("HTTP/1."):
            self.state = STATE_RESP_START

        # Data packet in response
        if self.state == STATE_RESP_START and direction < 0 and datalen > 0:
            self.times.append(((timestamp - self.starttime) / 1e6,
                               seq[1] - self.respseq))

        if self.id == 21:
            winsize = tcp.get_th_win()
            if not tcp.get_SYN():
                winsize <<= self.winscale[direction]
            print "got packet, data=%d win=%d" % (datalen, winsize)

def handler(header, data):
    global start_time
    global pkt
    (sec, us) = header.getts()
    ts = sec * 1000000 + us
    if start_time is None:
        start_time = ts
    ts -= start_time
    pkt = decoder.decode(data)

    ip = pkt.child()
    tcp = ip.child()
    src = (ip.get_ip_src(), tcp.get_th_sport())
    dst = (ip.get_ip_dst(), tcp.get_th_dport())
    flow = tuple(sorted([src, dst]))
    if flow not in flows:
        #print "New flow", flow
        flows[flow] = Connection(flow)

    flows[flow].process(ts, pkt)

def process(filename):
    global decoder
    p = pcapy.open_offline(filename)
    p.setfilter(r"ip proto \tcp")
    assert p.datalink() == pcapy.DLT_EN10MB
    decoder = impacket.ImpactDecoder.EthDecoder()
    p.loop(0, handler)

    for c in flows.values():
        c.finish_transfer()

if __name__ == '__main__':
    for f in sys.argv[1:]:
        process(f)
