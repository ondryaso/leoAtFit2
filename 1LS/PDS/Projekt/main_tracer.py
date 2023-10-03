# main_tracer.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)

import sys
import struct
import socket

from scapy.layers.dns import DNS, DNSRR
from scapy.layers.inet import IP, defragment, UDP, TCP
from scapy.layers.l2 import Ether
from scapy.utils import RawPcapNgReader
from bencoder import bdecode2, BTFailure
from typing import *

from btp_parser import BitTorrentParser
from tcp_tracer import TcpTracer, TcpFlow
from utp_tracer import UtpTracer


class Tracker:
    def __init__(self, ip: str, port: int, proto: str):
        self.ip = ip
        self.port = port
        self.proto = proto


class Peer:
    def __init__(self, info_hash, peer_id, ip, port):
        self.info_hash = info_hash
        self.port = port
        self.ip = ip
        self.peer_id = peer_id
        self.from_dht_nodes = []
        self.from_trackers = []
        self.own_dht_node = None
        self.pex = False
        self.sent_pieces = 0


class TorrentInfo:
    def __init__(self, info_hash):
        self.info_hash = info_hash
        self.pieces = -1
        self.max_piece_len = -1

    def set_number_of_pieces(self, bitfield_len):
        if bitfield_len > self.pieces:
            self.pieces = bitfield_len

    def piece_info(self, offset, data_len):
        possible_max = offset + data_len
        if possible_max > self.max_piece_len:
            # closest power of two
            self.max_piece_len = 1 if possible_max == 0 else 2 ** (possible_max - 1).bit_length()


class DHTNode:
    def __init__(self, node_id, ip, port):
        self.node_id = node_id
        self.ip = ip
        self.port = port
        self.known_peers = {}

    def add_known_peer(self, info_hash, peer):
        if info_hash not in self.known_peers:
            self.known_peers[info_hash] = []

        if peer in self.known_peers[info_hash]:
            return

        self.known_peers[info_hash].append(peer)
        if self not in peer.from_dht_nodes:
            peer.from_dht_nodes.append(self)


class Monitor:
    def __init__(self, bootstrap_delta_cutoff: int):
        self.upn = 0
        """A counter for processed UDP datagrams."""
        self.pn = 0
        """A counter for all processed input packets."""
        self.bootstrap_delta_cutoff = bootstrap_delta_cutoff
        """Maximum number of received UDP packets between DHT bootstrap nodes' responses."""

        self.dns_possible_dht: Dict[str, str] = {}
        """IP-hostname mappings for possible DHT nodes discovered from DNS queries"""
        self.dns_possible_trackers: Dict[str, str] = {}
        """IP-hostname mappings for possible trackers discovered from DNS queries"""
        self.dht_transactions: Dict[Tuple[str, int, str, int, bytes], Tuple[DHTNode, bytes | None]] = {}
        """A temporary storage for IDs of discovered DHT requests"""

        self.peer_dicts: Dict[bytes, Dict[Tuple[str, int], Peer]] = {}
        """Mappings of an info-hash to a collection of its discovered Peers, identified by their (IP, port)
           contact information."""
        self.peers_by_ips: Dict[Tuple[str, int], List[Peer]] = {}
        """Mappings of an (IP, port) tuple to a list of Peer objects with this contact information (each torrent gets
           its own Peer object)."""
        self.torrents: Dict[bytes, TorrentInfo] = {}
        """Mappings of an info-hash to a TorrentInfo object."""

        self.dht_bootstrap_nodes: List[DHTNode] = []
        """DHT nodes that haven't been discovered from another known DHT node."""
        self.last_bootstrap_upn = -1
        """The UDP datagram counter value for the last discovery of a bootstrap node."""

        self.dht_nodes_by_ips: Dict[Tuple[str, int], DHTNode] = {}
        """Mappings of an (IP, port) tuple to a single DHT node object."""

        self.udptp_known_ips: List[str] = []
        """IPs of known UDP trackers"""
        self.udptp_known_trans_ids: Dict[bytes, bytes] = {}
        """Mappings of an UDP tracker transaction ID to an info hash."""
        self.trackers: Dict[Tuple[str, int, str], Tracker] = {}
        """Mappings of an (IP, port, tracker proto) to a Tracker object."""

        self.utp_tracer = UtpTracer(self.on_new_flow, self.on_new_segment, self.on_close_flow)
        self.tcp_tracer = TcpTracer(self.on_new_flow, self.on_new_segment, self.on_close_flow)
        """The uTorrent Transport Protocol tracer"""
        self.btp_parsers: Dict[Any, Tuple[BitTorrentParser, BitTorrentParser]] = {}

        self.out = OutputManager(self)
        """The output generator."""

    # noinspection PyTypeChecker
    def trace_pcapng(self, file):
        """Reads a Pcap(ng) file, defragments IP packets and runs tracing on found UDP datagrams."""
        reader = RawPcapNgReader(file)
        fragments = []

        for pkt_data in reader:
            self.pn += 1
            if self.pn % 10000 == 0:
                self.out.log_pn(self.pn)

            p = Ether(pkt_data[0])

            if not isinstance(p[1], IP):
                continue

            if p[IP].flags & 1 == 1 or p[IP].frag > 0:
                # Fragmented IP packet
                fragments += p
                fragments = defragment(fragments)
                defragmented = []
                for f in fragments:
                    if f[IP].flags & 1 == 0 and f[IP].frag == 0:
                        defragmented.append(f)
                fragments = [f for f in fragments if f not in defragmented]
                for df in defragmented:
                    if isinstance(df[2], UDP):
                        self.trace_udp(df)
                    elif isinstance(df[2], TCP):
                        self.trace_tcp(df)

            elif isinstance(p[2], UDP):
                self.trace_udp(p)
            elif isinstance(p[2], TCP):
                self.trace_tcp(p)

    def trace_udp(self, packet):
        self.upn += 1

        payload = bytes(packet[3])
        if len(payload) < 4:
            return

        # try interpreting as a uTP / BT Peer proto message
        self.utp_tracer.trace(packet)

        # try interpreting as a DHT message
        if self.is_possible_dht(payload):
            self.trace_dht(packet, payload)
            return

        # may be a DNS request to a tracker
        if DNS in packet:
            self.trace_dns(packet)
            return

        # or it may be the UDP Tracker Protocol
        if self.trace_udp_tracker_proto(packet, payload):
            return

    def trace_tcp(self, packet):
        # TODO: HTTP trackers
        self.tcp_tracer.trace(packet)

    @staticmethod
    def is_possible_dht(payload):
        # dht packets are bencoded dicts, they must start with 'dX:' where X is a digit
        if payload[0] != 100 or payload[1] < 48 or payload[1] > 57 or payload[2] != 58:
            return False
        # ... and end with 'e'
        if payload[-1] != 101:
            return False

        return True

    def trace_udp_tracker_proto(self, packet, payload):
        payload_len = len(payload)

        if payload_len >= 16 and payload[0:12] == b'\x00\x00\x04\x17\x27\x10\x19\x80\x00\x00\x00\x00':
            # almost certainly connect request
            self.udptp_known_ips.append(packet[IP].dst)
            return True
        elif payload_len >= 16 and payload[0:4] == b'\x00\x00\x00\x00':
            # probably connect response
            if packet[IP].src not in self.udptp_known_ips:
                return False

            contact = (packet[IP].src, packet[UDP].sport, 'udp')
            if contact not in self.trackers:
                self.trackers[contact] = Tracker(contact[0], contact[1], contact[2])

        elif payload_len >= 98 and payload[8:12] == b'\x00\x00\x00\x01':
            # possible announce request
            if packet[IP].dst not in self.udptp_known_ips:
                return False

            tid = payload[12:16]
            info_hash = payload[16:36]
            if len(tid) == 0 or len(info_hash) == 0:
                self.out.log_udptp_format_error()
                return False

            contact = (packet[IP].dst, packet[UDP].dport, 'udp')
            if contact not in self.trackers:
                return False

            tracker = self.trackers[contact]
            self.udptp_known_trans_ids[tid] = info_hash
            return self.make_peer_from_udp_announce_req(packet, payload, tracker)
        elif payload_len >= 20 and payload[0:4] == b'\x00\x00\x00\x01':
            # possible announce response
            if packet[IP].src not in self.udptp_known_ips:
                return False

            tid = payload[4:8]
            if len(tid) == 0:
                self.out.log_udptp_format_error()
                return False

            if tid not in self.udptp_known_trans_ids:
                return False

            contact = (packet[IP].src, packet[UDP].sport, 'udp')
            if contact not in self.trackers:
                return False

            info_hash = self.udptp_known_trans_ids.pop(tid)
            tracker = self.trackers[contact]

            self.make_peer_from_udp_announce_resp(payload, info_hash, tracker)
            return True

    def make_peer_from_udp_announce_req(self, packet, payload, tracker):
        info_hash = payload[16:36]
        peer_id = payload[36:56]
        port = payload[96:98]
        if len(info_hash) == 0 or len(peer_id) == 0 or len(port) == 0:
            self.out.log_udptp_format_error()
            return False

        ip = packet[IP].src
        p = self.get_or_add_peer(info_hash, ip, struct.unpack('!H', port)[0], peer_id)
        if tracker not in p.from_trackers:
            p.from_trackers.append(tracker)

        return True

    def make_peer_from_udp_announce_resp(self, payload, info_hash, tracker):
        num_items = (len(payload) - 20) // 6
        for i in range(num_items):
            peer_ip = payload[20 + i * 6:20 + i * 6 + 4]
            port = payload[20 + i * 6 + 4:20 + i * 6 + 6]
            if len(peer_ip) == 0 or len(port) == 0:
                continue
            p = self.get_or_add_peer(info_hash, self.ip_to_str(peer_ip), struct.unpack('!H', port)[0])
            if tracker not in p.from_trackers:
                p.from_trackers.append(tracker)

    def get_dht_flow(self, packet, trans_id):
        key_cand = (packet[IP].dst, packet[IP].src, packet[UDP].dport, packet[UDP].sport, trans_id)
        if key_cand in self.dht_transactions:
            return self.dht_transactions.pop(key_cand)
        # key_cand = (packet[IP].src, packet[IP].dst, packet[UDP].sport, packet[UDP].dport, trans_id)
        # if key_cand in self.dht_transactions:
        #     return self.dht_transactions.pop(key_cand)

        return None

    def set_dht_flow(self, packet, trans_id, val):
        key_cand = (packet[IP].src, packet[IP].dst, packet[UDP].sport, packet[UDP].dport, trans_id)
        # if key_cand not in self.dht_transactions:
        #     key_cand = (packet[IP].dst, packet[IP].src, packet[UDP].dport, packet[UDP].sport, trans_id)

        self.dht_transactions[key_cand] = val

    def trace_dht(self, packet, payload):
        try:
            decoded, _ = bdecode2(payload)
        except BTFailure:
            return

        if b'y' not in decoded:
            # message type not present
            return

        msg_type = decoded[b'y']
        if msg_type == b'q':
            if b'q' not in decoded or b'a' not in decoded:
                return
            self.trace_dht_req(packet, decoded)
        elif msg_type == b'r':
            if b'r' not in decoded:
                return
            self.trace_dht_resp(packet, decoded)

    def trace_dht_req(self, packet, decoded):
        req_type = decoded[b'q']
        args = decoded[b'a']

        node, had_contact = self.get_or_add_node(None, packet[IP].dst, packet[UDP].dport)
        i_hash = args[b'info_hash'] if b'info_hash' in args else None

        if b't' in decoded:
            self.set_dht_flow(packet, decoded[b't'], (node, i_hash))

        # ping messages
        if i_hash is None:
            return

        if req_type == b'get_peers':
            if not had_contact and (self.last_bootstrap_upn == -1
                                    or self.upn - self.bootstrap_delta_cutoff < self.last_bootstrap_upn) \
                    or (self.bootstrap_delta_cutoff == 0 and packet[IP].dst in self.dns_possible_dht):
                self.dht_bootstrap_nodes.append(node)
                self.last_bootstrap_upn = self.upn

        elif req_type == b'announce_peer':
            p = self.get_or_add_peer(i_hash, packet[IP].src, packet[UDP].sport)
            node.add_known_peer(i_hash, p)

    def trace_dht_resp(self, packet, decoded):
        resp = decoded[b'r']
        if b'id' not in resp:
            return

        node_id = resp[b'id']
        if b't' in decoded:
            node: DHTNode
            req = self.get_dht_flow(packet, decoded[b't'])

            if req is None:
                self.out.log_unsolicited_dht_response(node_id, packet[IP].src, packet[UDP].sport, decoded[b't'])
                return

            node, i_hash = req
            if node.node_id is not None and node.node_id != node_id:
                self.out.log_node_id_mismatch(node, node_id, packet[IP].src, packet[UDP].sport)

            node.node_id = node_id

            if i_hash is None:
                # responses to ping messages
                return

            if b'values' in resp:
                self.extract_peers(i_hash, node, resp[b'values'])

            if b'nodes' in resp:
                self.extract_nodes(resp[b'nodes'])

    def get_or_add_node(self, node_id, ip, port):
        contact = (ip, port)
        node_exists = contact in self.dht_nodes_by_ips
        if node_exists:
            node = self.dht_nodes_by_ips[contact]
        else:
            node = DHTNode(node_id, ip, port)
            # self.dht_nodes.append(node)
            self.dht_nodes_by_ips[contact] = node

        # we know BT peer(s) that hosts this DHT node, link them
        if contact in self.peers_by_ips:
            for peer in self.peers_by_ips[contact]:
                if peer.own_dht_node is not None and peer.own_dht_node != node:
                    print(f"warning: overwriting associated node for peer {peer.peer_id}")
                peer.own_dht_node = node

        return node, node_exists

    def get_or_add_peer(self, info_hash, ip: str, port: int, peer_id=None):
        """Gets a Peer object for a given combination of torrent (info_hash), ip and port.
           Creates a new one if this Peer hasn't been discovered yet."""

        if info_hash not in self.peer_dicts:
            self.peer_dicts[info_hash] = {}
            self.torrents[info_hash] = TorrentInfo(info_hash)

        contact = (ip, port)
        if contact in self.peer_dicts[info_hash]:
            # this peer is already added to the torrent's collection of peers
            return self.peer_dicts[info_hash][contact]
        else:
            p = Peer(info_hash, peer_id, ip, port)
            # assign the new peer to the torrent's collection
            self.peer_dicts[info_hash][contact] = p
            # also add it to the 'global' list of peers known for the IP/port
            if contact in self.peers_by_ips:
                self.peers_by_ips[contact].append(p)
            else:
                self.peers_by_ips[contact] = [p]
            # we might know a node residing on the same address
            if contact in self.dht_nodes_by_ips:
                node = self.dht_nodes_by_ips[contact]
                if p.own_dht_node is not None and p.own_dht_node != node:
                    print(f"warning: overwriting associated node for peer {peer_id}")
                p.own_dht_node = node

            return p

    def extract_nodes(self, compact_node_list):
        num_nodes = len(compact_node_list) // 26
        for i in range(num_nodes):
            block_start = i * 26
            block_end = block_start + 26

            # extract the 20B block and IPv4 address/port
            block = compact_node_list[block_start:block_start + 20]
            ip_port_bytes = compact_node_list[block_start + 20:block_end]

            # unpack the IPv4 address and port
            ip_bytes, port = struct.unpack('!4sH', ip_port_bytes)
            ip_addr = self.ip_to_str(ip_bytes)

            self.get_or_add_node(block, ip_addr, port)

    def extract_compact_peers(self, info_hash, compact_peer_list, pex=True):
        num_peers = len(compact_peer_list) // 6
        for i in range(num_peers):
            ip_port_bytes = compact_peer_list[i * 6:i * 6 + 6]

            # unpack the IPv4 address and port
            ip_bytes, port = struct.unpack('!4sH', ip_port_bytes)
            ip_addr = self.ip_to_str(ip_bytes)

            p = self.get_or_add_peer(info_hash, ip_addr, port)
            p.pex = pex

    def extract_peers(self, info_hash, dht_node, peer_list):
        for peer_contact in peer_list:
            # unpack the next 6 B into a tuple of 4B IP address and 2B port
            ip_bytes, port = struct.unpack('!4sH', peer_contact)
            ip_addr = self.ip_to_str(ip_bytes)
            p = self.get_or_add_peer(info_hash, ip_addr, port)
            if dht_node is not None:
                dht_node.add_known_peer(info_hash, p)

    def trace_dns(self, packet):
        if DNSRR not in packet:
            return

        name: bytes = packet[DNSRR].rrname
        val: str = packet[DNSRR].rdata
        if b'torrent' in name or b'dht' in name:
            self.dns_possible_dht[val] = name.decode()
        if b'torrent' in name or b'tracker' in name:
            self.dns_possible_trackers[val] = name.decode()

    def on_new_flow(self, flow: TcpFlow):
        if flow.tup in self.btp_parsers:
            self.out.log_error(f"Encountered a new peer connection between {flow.initiator_ip} "
                               f"and {flow.accepter_ip} before closing the old one")
            self.btp_parsers.pop(flow.tup)

        self.btp_parsers[flow.tup] = (BitTorrentParser(self, flow.initiator_ip, flow.initiator_port),
                                      BitTorrentParser(self, flow.accepter_ip, flow.accepter_port))

    def on_new_segment(self, flow: TcpFlow, direction: int, payload):
        if flow.tup not in self.btp_parsers:
            return

        parser: BitTorrentParser = self.btp_parsers[flow.tup][direction]
        parser.process_segment(payload)

    def on_close_flow(self, flow: TcpFlow):
        self.btp_parsers.pop(flow.tup)

    @staticmethod
    def ip_to_str(ip_bytes):
        return '.'.join(str(b) for b in ip_bytes)


class OutputManager(object):
    no_node_id_msg = 'node ID unknown                         '

    def __init__(self, monitor: Monitor):
        self.monitor = monitor
        self.show_errors = True
        self.show_intermediary = False

        self.show_init = False
        self.show_peers = False
        self.show_nodes = False
        self.show_download = False
        self.find_hostnames = False
        self.all_peers = False

    def print_final_report(self):
        if self.show_init:
            self.report_all_bootstrap_nodes()

        if self.show_peers:
            self.report_all_peers()

        if self.show_download:
            self.report_all_downloads()

        if self.show_nodes:
            self.report_all_nodes()

    def report_all_downloads(self):
        print("=== DOWNLOADED TORRENTS ===")
        for info_hash in self.monitor.torrents:
            self.report_download(info_hash)

    def report_download(self, info_hash: bytes):
        if not self.has_peers(info_hash):
            return

        torrent = self.monitor.torrents[info_hash]
        peers = self.monitor.peer_dicts[info_hash]
        has_size = torrent.max_piece_len != -1 and torrent.pieces != -1

        print(f"info_hash: {info_hash.hex()}")
        print(f"# of pieces (approx.): {torrent.pieces if torrent.pieces != -1 else 'unknown'}")
        print(f"size of piece (approx.): "
              f"{torrent.max_piece_len // 1024 if torrent.max_piece_len != -1 else 'unknown'} KiB")
        print(f"total size (approx.): {torrent.pieces * torrent.max_piece_len // 1024 if has_size else 'unknown'} KiB")
        print(f"contributing peers:")

        for peer in peers.values():
            if peer.sent_pieces == 0 and not self.all_peers:
                continue

            if torrent.pieces != -1:
                print(f"> {peer.ip}:{peer.port} (detected {peer.sent_pieces} pieces ~ "
                      f"{100.0 * peer.sent_pieces / torrent.pieces:.2f} %)")
            else:
                print(f"> {peer.ip}:{peer.port}")

        print("")

    def report_all_peers(self):
        print("=== PEERS ===")
        for info_hash in self.monitor.peer_dicts:
            self.report_peers(info_hash)

    def has_peers(self, info_hash: bytes):
        peers = self.monitor.peer_dicts[info_hash]

        has_peer = False
        for peer in peers.values():
            if peer.sent_pieces == 0:
                continue
            has_peer = True
            break

        return has_peer

    def report_peers(self, info_hash: bytes):
        if not self.has_peers(info_hash) and not self.all_peers:
            return

        peers = self.monitor.peer_dicts[info_hash]

        print(f"Peers for {info_hash.hex()}:")
        print("ip\tport\tpeer id\tDHT node id\tflags")

        for peer in peers.values():
            if peer.sent_pieces == 0 and not self.all_peers:
                continue

            dht_node_id = peer.own_dht_node.node_id.hex() \
                if peer.own_dht_node is not None and peer.own_dht_node.node_id is not None \
                else 'node ID unknown'

            pex = "\t[PEX]" if peer.pex else ""

            print(f"{peer.ip}\t{peer.port}\t{peer.peer_id.hex() if peer.peer_id is not None else 'peed ID unknown'}\t"
                  f"{dht_node_id}{pex}")

            if len(peer.from_dht_nodes) > 0:
                for node in peer.from_dht_nodes:
                    self.report_node(node, ">> ")

            if len(peer.from_trackers) > 0:
                print("> Discovered from trackers:")
                for tracker in peer.from_trackers:
                    self.report_tracker(tracker, ">> ")

        print("")

    @staticmethod
    def report_tracker(tracker: Tracker, prefix=""):
        print(f"{prefix}{tracker.proto}://{tracker.ip}:{tracker.port}")

    def report_all_nodes(self):
        print("=== ALL NODES ===")
        for node in self.monitor.dht_nodes_by_ips.values():
            self.report_node(node, full=True)

    @staticmethod
    def report_node(node: DHTNode, prefix="", full=False):
        print(f"{prefix}{node.ip}\t{node.port}\t{node.node_id.hex() if node.node_id is not None else 'unknown'}")
        if full and len(node.known_peers) > 0:
            print(f"{prefix}> Known peers:")
            for peer_list in node.known_peers.values():
                for peer in peer_list:
                    print(f"{prefix}>> {peer.ip}:{peer.port}")

    def report_all_bootstrap_nodes(self):
        print("=== BOOTSTRAP NODES ===")
        for n in self.monitor.dht_bootstrap_nodes:
            self.report_bootstrap_node(n)

        print("")

    def report_bootstrap_node(self, node: DHTNode, intermediary: bool = False):
        if intermediary and not self.show_intermediary:
            return

        n = self._get_bootstrap_node(node)
        print(
            f"{'[INTM. BOOTSTRAP N.] ' if intermediary else ''}"
            f"{n['ip']}\t{n['port']}"
            f"\t{n['node_id'] if n['node_id'] is not None else OutputManager.no_node_id_msg}"
            f"\t{n['hostname_capture'] or '-'}\t{n['hostname_query'] or '-'}")

    def log_unsolicited_dht_response(self, node_id: bytes, sip: str, sport: int, tid: bytes):
        if not self.show_errors:
            return

        sys.stderr.write(f"Unsolicited DHT response from {node_id.hex()} ({sip}:{sport}), transaction '{tid.hex()}',"
                         f" packet #{self.monitor.pn}\n")

    def log_node_id_mismatch(self, node: DHTNode, new_node_id: bytes, sip: str, sport: int):
        if not self.show_errors:
            return

        sys.stderr.write(f"Node ID mismatch for {sip}:{sport}, previously known as: {node.node_id.hex()},"
                         f" ({node.ip}:{node.port}), now known as: {new_node_id.hex()}, packet #{self.monitor.pn}\n")

    def log_error(self, msg: str):
        if not self.show_errors:
            return

        sys.stderr.write(msg + "\n")

    def log_udptp_format_error(self):
        if not self.show_errors:
            return

        sys.stderr.write("Invalid UDP Tracker Proto message.\n")

    def _get_bootstrap_node(self, node: DHTNode):
        hostname_c = self.monitor.dns_possible_dht[node.ip][:-1] if node.ip in self.monitor.dns_possible_dht else None

        if self.find_hostnames:
            try:
                query = socket.gethostbyaddr(node.ip)
                hostname_q = query[0] if query is not None and query[0] is not None else None
            except socket.herror:
                hostname_q = None
        else:
            hostname_q = None

        return {'node_id': node.node_id.hex() if node.node_id is not None else None,
                'hostname_capture': hostname_c, 'hostname_query': hostname_q, 'ip': node.ip, 'port': node.port}

    @staticmethod
    def log_pn(pn):
        sys.stderr.write(f"Processed {pn} packets\n")
