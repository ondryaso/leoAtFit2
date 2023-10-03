#!/usr/bin/env python3
# bt-monitor.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)

import logging

import main_tracer

if __name__ == '__main__':
    def main():
        logging.getLogger("scapy.runtime").setLevel(logging.ERROR)
        logging.getLogger('tcptrace').setLevel(logging.ERROR)
        logging.getLogger('utptrace').setLevel(logging.ERROR)
        logging.getLogger('btpparse').setLevel(logging.ERROR)

        args = parse_args()
        m = main_tracer.Monitor(args.bootstrap_cutoff)

        m.out.show_errors = args.ve
        m.out.show_init = args.init
        m.out.show_peers = args.peers
        m.out.show_nodes = args.nodes
        m.out.show_download = args.download
        m.out.all_peers = args.vp
        m.out.find_hostnames = args.bh

        print("dejte si kávičku, za chvíli jsem hotov\n")
        m.trace_pcapng(args.pcap)
        m.out.print_final_report()


    def parse_args():
        import argparse

        parser = argparse.ArgumentParser(description="A simple BitTorrent communication detection tool.")

        parser.add_argument("-pcap", metavar="<file>", type=str, required=True,
                            help="the input pcap(ng) file")

        parser.add_argument("-init", action="store_true", help="list detected bootstrap nodes")
        parser.add_argument("-peers", action="store_true", help="list detected peer neighbors")
        parser.add_argument("-nodes", action="store_true", help="list contacted DHT nodes")
        parser.add_argument("-download", action="store_true", help="list detected file transfers")
        parser.add_argument("-ve", action="store_true", help="verbose: print detection errors")
        parser.add_argument("-bootstrap-cutoff", metavar="<# of packets>", type=int, required=False,
                            help="the maximum number of UDP packets received between what's considered "
                                 "as bootstrap node requests. Set to zero to only show nodes the IPs of "
                                 "which have been resolved in DNS queries", default=20)
        parser.add_argument("-vp", action="store_true", help="verbose: print all peers, even if they didn't transmit"
                                                             "any pieces")
        parser.add_argument("-bh", action="store_true", help="find hostname for bootstrap nodes")

        args = parser.parse_args()

        if not any([args.init, args.peers, args.nodes, args.download]):
            parser.error("At least one of -init, -peers, -nodes or -download must be used.")

        return args


    main()
