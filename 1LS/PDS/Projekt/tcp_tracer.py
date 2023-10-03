# tcp_tracer.py
# ---
# Adapted from original code by Mostafa Razavi
# Original source: https://github.com/elektito/bttools/blob/master/utptrace.py

import logging
import sys
from serial import SerialNumber

from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, TCP

FG_FIN = 0x01
FG_SYN = 0x02
FG_RST = 0x04
FG_ACK = 0x10

CS_INIT = 1
CS_INITIATOR_SENT_SYN = 2
CS_SYN_ACKED = 3
CS_CONNECTED = 4
CS_INITIATOR_SENT_FIN = 5
CS_ACCEPTER_SENT_FIN = 6
CS_INITIATOR_FIN_ACKED = 7
CS_ACCEPTER_FIN_ACKED = 8
CS_BOTH_SENT_FIN = 9
CS_BOTH_SENT_FIN_INITIATOR_ACKED = 10
CS_BOTH_SENT_FIN_ACCEPTER_ACKED = 11
CS_PENDING_CLOSE = 12


class TcpFlow(object):
    def __init__(self,
                 initiator_ip, initiator_port,
                 accepter_ip, accepter_port,
                 seq0):
        self.initiator_ip = initiator_ip
        self.initiator_port = initiator_port
        self.accepter_ip = accepter_ip
        self.accepter_port = accepter_port
        self.tup = (initiator_ip, initiator_port, accepter_ip, accepter_port, 'tcp')
        self.seq0 = seq0
        self.seq1 = 0
        self.state = CS_INITIATOR_SENT_SYN
        self.pending = []

    def __repr__(self):
        return '<TcpFlow {}>'.format(str(self))

    def __str__(self):
        return '{}:{} => {}:{}'.format(
            self.initiator_ip,
            self.initiator_port,
            self.accepter_ip,
            self.accepter_port)


state_machine = {}


def on_state(state):
    def decorator(f):
        state_machine[state] = f
        return f

    return decorator


class TcpTracer(object):
    def __init__(self, new_flow_cb, new_segment_cb, close_flow_cb):
        self.flows = {}
        self.logger = logging.getLogger('tcptrace')
        handler = logging.StreamHandler(sys.stdout)
        self.logger.addHandler(handler)

        self.fragments = []
        self.new_flow = new_flow_cb
        self.new_segment = new_segment_cb
        self.flush_and_close = close_flow_cb

    @on_state(CS_INIT)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if not flags & FG_SYN:
            self.logger.warning('Packet from incomplete flow. Ignored.')
            return
        flow = TcpFlow(src, sport, dst, dport, seq + 1)
        flow.state = CS_INITIATOR_SENT_SYN
        self.flows[src, sport, dst, dport] = flow
        self.new_flow(flow)

    @on_state(CS_INITIATOR_SENT_SYN)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flow.tup == (dst, dport, src, sport):
            if flags & FG_SYN and flags & FG_ACK:
                flow.state = CS_SYN_ACKED
                flow.seq1 = seq + 1
                self.logger.debug('SYN ACKED')
            else:
                self.logger.debug('Expected SYN ACK. Ignored.')

    @on_state(CS_SYN_ACKED)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flow.tup == (src, sport, dst, dport):
            if flags & FG_ACK:
                flow.state = CS_CONNECTED
                self.logger.debug('Connection established.')

                if len(payload) > 0:
                    self.add_segment(flow, 0, payload, seq)

    @on_state(CS_CONNECTED)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flags & FG_RST:
            self.logger.warning('Connection RESET.')
            self.flush_and_close(flow)
            return

        if flags & FG_FIN:
            if (src, sport, dst, dport) == flow.tup:
                flow.state = CS_INITIATOR_SENT_FIN
                flow.seq0 += 1
                self.logger.debug('Initiator sent FIN.')
            else:
                flow.state = CS_ACCEPTER_SENT_FIN
                flow.seq1 += 1
                self.logger.debug('Accepter sent FIN.')
            return

        if (src, sport, dst, dport) == flow.tup:
            self.add_segment(flow, 0, payload, seq)
        elif (dst, dport, src, sport) == flow.tup:
            self.add_segment(flow, 1, payload, seq)
        else:
            self.logger.warning('Something bad has happened!')

    @on_state(CS_INITIATOR_SENT_FIN)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flow.tup == (dst, dport, src, sport) and flags & FG_ACK:
            if flags & FG_FIN:
                flow.state = CS_BOTH_SENT_FIN_INITIATOR_ACKED
                flow.seq1 += 1
                self.logger.debug('Both sent FIN and initiator\'s was acked.')
            else:
                flow.state = CS_INITIATOR_FIN_ACKED
                self.logger.debug('Initiator FIN acked.')

    @on_state(CS_ACCEPTER_SENT_FIN)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flow.tup == (src, sport, dst, dport) and flags & FG_ACK:
            if flags & FG_FIN:
                flow.state = CS_BOTH_SENT_FIN_ACCEPTER_ACKED
                flow.seq0 += 1
                self.logger.debug('Both sent FIN and accepter\'s was acked.')
            else:
                flow.state = CS_ACCEPTER_FIN_ACKED
                self.logger.debug('Accepter FIN acked.')

    @on_state(CS_INITIATOR_FIN_ACKED)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flow.tup == (dst, dport, src, sport) and flags & FG_FIN:
            flow.state = CS_BOTH_SENT_FIN_INITIATOR_ACKED
            flow.seq1 += 1
            self.logger.debug('Both sent FIN and initiator\'s was acked.')

    @on_state(CS_ACCEPTER_FIN_ACKED)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if flow.tup == (src, sport, dst, dport):
            flow.state = CS_BOTH_SENT_FIN_ACCEPTER_ACKED
            flow.seq0 += 1
            self.logger.debug('Both sent FIN and accepter\'s was acked.')

    @on_state(CS_BOTH_SENT_FIN_INITIATOR_ACKED)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if (dst, dport, src, sport) != flow.tup:
            return

        if not flags & FG_ACK:
            return

        if seq != flow.seq1:
            return

        if len(flow.pending) > 0:
            flow.state = CS_PENDING_CLOSE
        else:
            self.flush_and_close(flow)

    @on_state(CS_INITIATOR_SENT_FIN)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if (dst, dport, src, sport) == flow.tup and flags & FG_FIN:
            flow.state = CS_BOTH_SENT_FIN
            flow.seq1 += 1
            self.logger.debug('Both sent FIN.')

    @on_state(CS_BOTH_SENT_FIN)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if not flags & FG_ACK:
            return

        if (src, sport, dst, dport) == flow.tup:
            flow.state = CS_BOTH_SENT_FIN_INITIATOR_ACKED
            self.logger.warning('Both sent FIN and initiator\'s was acked.')
        elif (dst, dport, src, sport) == flow.tup:
            flow.state = CS_BOTH_SENT_FIN_ACCEPTER_ACKED
            self.logger.warning('Both sent FIN and accepter\'s was acked.')
        else:
            self.logger.warning('Something wicked has happened!')

    @on_state(CS_BOTH_SENT_FIN_ACCEPTER_ACKED)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if (src, sport, dst, dport) != flow.tup:
            return

        if not flags & FG_ACK:
            return

        if seq != flow.seq0:
            return

        if len(flow.pending) > 0:
            flow.state = CS_PENDING_CLOSE
        else:
            self.flush_and_close(flow)

    @on_state(CS_PENDING_CLOSE)
    def action(self, flow, payload, src, sport, dst, dport, flags, seq):
        if (src, sport, dst, dport) == flow.tup:
            self.add_segment(flow, 0, payload, seq)
        elif (dst, dport, src, sport) == flow.tup:
            self.add_segment(flow, 1, payload, seq)
        if len(flow.pending) == 0:
            self.flush_and_close(flow)

    def trace(self, pkt):
        assert isinstance(pkt[0], Ether) and \
               isinstance(pkt[1], IP) and \
               isinstance(pkt[2], TCP)

        src = pkt[1].src
        dst = pkt[1].dst
        sport = pkt[2].sport
        dport = pkt[2].dport
        try:
            payload = bytes(pkt[3])
        except IndexError:
            payload = b''

        seq = SerialNumber(pkt[2].seq, 32)
        flags = pkt[2].flags

        flow = self.flows.get((src, sport, dst, dport), None)
        if flow is None:
            flow = self.flows.get((dst, dport, src, sport), None)

        try:
            if flow is not None:
                state_machine[flow.state](
                    self, flow, payload, src, sport, dst, dport, flags, seq)
            else:
                state_machine[CS_INIT](
                    self, flow, payload, src, sport, dst, dport, flags, seq)
        except KeyError as e:
            self.logger.debug(
                'State not found in the state machine: state={} flags={} existing={}'.format(
                    flow.state if flow else CS_INIT, flags, flow != None))

    def add_segment(self, flow, direction, payload, seq):
        if len(payload) == 0:
            return

        fseq = flow.seq0 if direction == 0 else flow.seq1
        if seq == fseq:
            self.new_segment(flow, direction, payload)

            if direction == 0:
                flow.seq0 += len(payload)
            else:
                flow.seq1 += len(payload)

            self.logger.debug('New segment arrived from the {}.'.format(
                'initiator' if direction == 0 else 'accepter'))
        elif seq > fseq:
            flow.pending.append((payload, seq, direction))
            self.logger.debug('Out of order packet. Added to pending list.')
        else:  # seq < fseq
            self.logger.debug('Duplicate packet. Ignored.')

        added_some = True
        removed = []
        while added_some:
            added_some = False
            i = 0
            for payload, seq, direction in flow.pending:
                if direction == 0:
                    if seq == flow.seq0:
                        self.new_segment(flow, direction, payload)
                        self.logger.info('Pending segment added: {} byte(s)'.format(len(payload)))
                        flow.seq0 += len(payload)
                        added_some = True
                        removed.append(i)
                else:
                    if seq == flow.seq1:
                        self.new_segment(flow, direction, payload)
                        self.logger.info('Pending segment added: {} byte(s)'.format(len(payload)))
                        flow.seq1 += len(payload)
                        added_some = True
                        removed.append(i)
                i += 1

        flow.pending = [i for j, i in enumerate(flow.pending) if j not in flow.pending]
