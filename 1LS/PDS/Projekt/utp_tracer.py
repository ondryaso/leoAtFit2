# utp_tracer.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
# ---
# Adapted from original code by Mostafa Razavi
# Original source: https://github.com/elektito/bttools/blob/master/utptrace.py

import logging
import sys
from serial import SerialNumber

import scapy.data
from scapy.layers.l2 import Ether
from scapy.layers.inet import IP, UDP

from tcp_tracer import TcpFlow

scapy.data.MTU = 65536

ST_DATA = 0x0
ST_FIN = 0x1
ST_STATE = 0x2
ST_RESET = 0x3
ST_SYN = 0x4

CS_INIT = 1
CS_HANDSHAKE = 2
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

state_machine = {}


class UtpFlow(TcpFlow):
    def __init__(self,
                 initiator_ip, initiator_port,
                 accepter_ip, accepter_port,
                 connid, seq0):
        super().__init__(initiator_ip, initiator_port, accepter_ip, accepter_port, seq0)
        self.connid = connid
        self.state = CS_HANDSHAKE
        self.tup = (initiator_ip, initiator_port, accepter_ip, accepter_port, 'utp')

    def __repr__(self):
        return '<UtpFlow {}>'.format(str(self))

    def __str__(self):
        return '{}:{} => {}:{} (connid={})'.format(
            self.initiator_ip,
            self.initiator_port,
            self.accepter_ip,
            self.accepter_port,
            self.connid)


class Action(object):
    def __init__(self, func, state=None, packet_type=None, existing_flow=None):
        self.func = func

        if isinstance(func, Action):
            self.state = func.state
            self.packet_type = func.packet_type
            self.existing_flow = func.existing_flow
        else:
            self.state = []
            self.packet_type = []
            self.existing_flow = []

        state = state if hasattr(state, '__iter__') else [state]
        packet_type = packet_type if hasattr(packet_type, '__iter__') \
            else [packet_type]
        existing_flow = existing_flow if hasattr(existing_flow, '__iter__') \
            else [existing_flow]

        if state is not None:
            self.state.extend(state)
        if packet_type is not None:
            self.packet_type.extend(packet_type)
        if existing_flow is not None:
            self.existing_flow.extend(existing_flow)

        if self.state is not None and \
                self.packet_type is not None and \
                self.existing_flow is not None:

            for st in self.state:
                for pt in self.packet_type:
                    for ex in self.existing_flow:
                        state_machine[st, pt, ex] = self

    def __call__(self, *args, **kwargs):
        self.func(*args, **kwargs)


def on_state(state):
    def decorator(f):
        action = Action(f, state=state)
        return action

    return decorator


def on_packet_type(packet_type):
    def decorator(f):
        action = Action(f, packet_type=packet_type)
        return action

    return decorator


def on_existing_flow(existing_flow):
    def decorator(f):
        action = Action(f, existing_flow=existing_flow)
        return action

    return decorator


class UtpTracer(object):
    def __init__(self, new_flow_cb, new_segment_cb, close_flow_cb):
        self.flows = {}
        self.logger = logging.getLogger('utptrace')
        handler = logging.StreamHandler(sys.stdout)
        self.logger.addHandler(handler)

        self.new_flow = new_flow_cb
        self.new_segment = new_segment_cb
        self.flush_and_close = close_flow_cb

    @on_state(CS_INIT)
    @on_packet_type(ST_SYN)
    @on_existing_flow(False)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        flow = UtpFlow(src, sport, dst, dport, connid, seq + 1)
        assert flow.tup not in self.flows
        self.flows[src, sport, dst, dport, connid] = flow
        self.new_flow(flow)

    @on_state(CS_HANDSHAKE)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            # self.logger.warning('Expected SYN ACK. Ignored.')
            return

        flow.seq1 = seq
        flow.state = CS_SYN_ACKED
        self.logger.debug('SYN acked.')

    @on_state(CS_HANDSHAKE)
    @on_packet_type(ST_SYN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            self.logger.debug('Duplicate SYN.')
            return

        self.logger.info(
            'Two peers trying simultaneously to initiate a connection. '
            'Letting the second one win.')
        self.flush_and_close(flow)

        flow = UtpFlow(src, sport, dst, dport, connid, seq + 1)
        self.flows[src, sport, dst, dport, connid] = flow
        self.new_flow(flow)

    @on_state(CS_SYN_ACKED)
    @on_packet_type(ST_FIN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            flow.state = CS_INITIATOR_SENT_FIN
            self.logger.debug('Initiator sent FIN before the connection was completely established.')
        elif (dst, dport, src, sport) == flow.tup[:-1]:
            flow.state = CS_ACCEPTER_SENT_FIN
            self.logger.debug('Accepter sent FIN before the connection was completely established.')

    @on_state([CS_SYN_ACKED, CS_CONNECTED])
    @on_packet_type(ST_DATA)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            self.add_segment(flow, 0, payload, seq)
            flow.state = CS_CONNECTED
        elif (dst, dport, src, sport) == flow.tup[:-1]:
            self.add_segment(flow, 1, payload, seq)
            flow.state = CS_CONNECTED
        else:
            # self.logger.warning('Something bad has happened!')
            pass

    @on_state(CS_CONNECTED)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        self.logger.debug('ACK.')

    @on_state(CS_CONNECTED)
    @on_packet_type(ST_RESET)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        self.logger.warning('Connection RESET.')
        self.flush_and_close(flow)

    @on_state(CS_CONNECTED)
    @on_packet_type(ST_FIN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            flow.state = CS_INITIATOR_SENT_FIN
            self.logger.debug('Initiator sent FIN.')
        else:
            flow.state = CS_ACCEPTER_SENT_FIN
            self.logger.debug('Accepter sent FIN.')

    @on_state(CS_INITIATOR_SENT_FIN)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        flow.state = CS_INITIATOR_FIN_ACKED
        self.logger.debug('Initiator FIN acked.')

    @on_state(CS_ACCEPTER_SENT_FIN)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (dst, dport, src, sport) == flow.tup[:-1]:
            flow.state = CS_ACCEPTER_FIN_ACKED
            self.logger.debug('Accepter FIN acked.')

    @on_state(CS_INITIATOR_FIN_ACKED)
    @on_packet_type(ST_FIN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            flow.state = CS_BOTH_SENT_FIN_INITIATOR_ACKED

    @on_state(CS_ACCEPTER_FIN_ACKED)
    @on_packet_type(ST_FIN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            flow.state = CS_BOTH_SENT_FIN_ACCEPTER_ACKED

    @on_state(CS_BOTH_SENT_FIN_INITIATOR_ACKED)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (dst, dport, src, sport) != flow.tup[:-1]:
            return

        if len(flow.pending) > 0:
            flow.state = CS_PENDING_CLOSE
        else:
            self.flush_and_close(flow)

    @on_state(CS_INITIATOR_SENT_FIN)
    @on_packet_type(ST_FIN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (dst, dport, src, sport) == flow.tup[:-1]:
            flow.state = CS_BOTH_SENT_FIN

    @on_state(CS_ACCEPTER_SENT_FIN)
    @on_packet_type(ST_FIN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            flow.state = CS_BOTH_SENT_FIN

    @on_state(CS_BOTH_SENT_FIN)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            flow.state = CS_BOTH_SENT_FIN_INITIATOR_ACKED
        else:
            flow.state = CS_BOTH_SENT_FIN_ACCEPTER_ACKED

    @on_state(CS_BOTH_SENT_FIN_ACCEPTER_ACKED)
    @on_packet_type(ST_STATE)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) != flow.tup[:-1]:
            return

        if len(flow.pending) > 0:
            flow.state = CS_PENDING_CLOSE
        else:
            self.flush_and_close(flow)

    @on_state(CS_PENDING_CLOSE)
    @on_packet_type(ST_DATA)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            self.add_segment(flow, 0, payload, seq)
        else:
            self.add_segment(flow, 1, payload, seq)
        if len(flow.pending) == 0:
            self.flush_and_close(flow)

    @on_state(CS_INITIATOR_SENT_FIN)
    @on_state(CS_ACCEPTER_SENT_FIN)
    @on_state(CS_INITIATOR_FIN_ACKED)
    @on_state(CS_ACCEPTER_FIN_ACKED)
    @on_state(CS_BOTH_SENT_FIN)
    @on_state(CS_BOTH_SENT_FIN_INITIATOR_ACKED)
    @on_state(CS_BOTH_SENT_FIN_ACCEPTER_ACKED)
    @on_packet_type(ST_DATA)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        if (src, sport, dst, dport) == flow.tup[:-1]:
            self.add_segment(flow, 0, payload, seq)
        else:
            self.add_segment(flow, 1, payload, seq)

    @on_state(CS_INITIATOR_SENT_FIN)
    @on_state(CS_ACCEPTER_SENT_FIN)
    @on_state(CS_INITIATOR_FIN_ACKED)
    @on_state(CS_ACCEPTER_FIN_ACKED)
    @on_state(CS_BOTH_SENT_FIN)
    @on_state(CS_BOTH_SENT_FIN_INITIATOR_ACKED)
    @on_state(CS_BOTH_SENT_FIN_ACCEPTER_ACKED)
    @on_packet_type(ST_SYN)
    @on_existing_flow(True)
    def action(self, flow, payload, src, sport, dst, dport, connid, seq):
        self.flush_and_close(flow)
        state_machine[CS_INIT, ST_SYN, False](
            self, flow, payload, src, sport, dst, dport, connid, seq)

    def trace(self, pkt):
        assert isinstance(pkt[0], Ether) and \
               isinstance(pkt[1], IP) and \
               isinstance(pkt[2], UDP)

        payload = bytes(pkt[3])
        if len(payload) < 20:
            return

        version = (payload[0]) & 0x0f
        if version != 1:
            return

        type = ((payload[0]) & 0xf0) >> 4
        if type > 4:
            return

        extension = (payload[1])
        ext_len = 0
        while extension != 0:
            if len(payload) < 20 + ext_len + 1:
                return
            extension = (payload[20 + ext_len])
            length = (payload[20 + ext_len + 1])
            ext_len += 2 + length

        connid = ((payload[2]) << 8) | \
                 ((payload[3]) << 0)

        src = pkt[1].src
        dst = pkt[1].dst
        sport = pkt[2].sport
        dport = pkt[2].dport

        seq = ((payload[16]) << 8) | \
              ((payload[17]) << 0)
        seq = SerialNumber(seq, 16)

        flow = self.flows.get((src, sport, dst, dport, connid if type == ST_SYN else connid - 1), None)
        if flow is None:
            flow = self.flows.get((dst, dport, src, sport, connid), None)

        if flow is not None:
            s, t, e = flow.state, type, True
        else:
            s, t, e = CS_INIT, type, False

        action = state_machine.get((s, t, e), None)
        if action:
            action(self, flow, payload[20 + ext_len:], src, sport, dst, dport, connid, seq)
        else:
            self.logger.debug(
                'State not found in the state machine: state={} type={} existing={}'.format(
                    s, t, e))

    def add_segment(self, flow, direction, payload, seq):
        fseq = flow.seq0 if direction == 0 else flow.seq1
        if seq == fseq:
            self.new_segment(flow, direction, payload)

            if direction == 0:
                flow.seq0 += 1
            else:
                flow.seq1 += 1

            # self.logger.info('New segment arrived from the {}.'.format(
            #    'initiator' if direction == 0 else 'accepter'))
        elif seq > fseq:
            flow.pending.append((payload, seq, direction))
            self.logger.debug('Out of order packet. Added to pending list.')
        else:  # seq < fseq
            self.logger.debug('Duplicate packet. Ignored.')
            pass

        added_some = True
        removed = []
        while added_some:
            added_some = False
            i = 0
            for payload, seq, direction in flow.pending:
                if direction == 0:
                    if seq == flow.seq0:
                        self.new_segment(flow, direction, payload)
                        # self.logger.info('Pending segment added: {} byte(s)'.format(len(payload)))
                        flow.seq0 += 1
                        added_some = True
                        removed.append((seq, direction))
                else:
                    if seq == flow.seq1:
                        self.new_segment(flow, direction, payload)
                        # self.logger.info('Pending segment added: {} byte(s)'.format(len(payload)))
                        flow.seq1 += 1
                        added_some = True
                        removed.append((seq, direction))
                i += 1

        flow.pending = [i for i in flow.pending if (i[1], i[2]) not in removed]
