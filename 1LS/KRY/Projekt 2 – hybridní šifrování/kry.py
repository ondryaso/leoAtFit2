# kry.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
# Date: 2023-04-28
# KRY Project 2
# This module implements the program's entry logic.

import os
import sys
import argparse

from client import Client
import logging

from server import Server


def main():
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)

    parser = argparse.ArgumentParser(description='KRY2 Project: Hybrid encryption')
    parser.add_argument('-type', action='store', help='sets program mode, either c (client) or s (server)')
    parser.add_argument('-port', action='store', help='sets the port to listen on')
    args = parser.parse_args()

    try:
        mode = args.type if args.type else os.environ['TYPE']
        port_str = args.port if args.port else os.environ['PORT']
        port = int(port_str)
    except KeyError:
        logging.error("The program mode and port must be set. "
                      "Either the -type and -port parameters or environment variables TYPE and PORT may be used.")
        sys.exit(1)
    except ValueError:
        logging.error("Invalid port number.")
        sys.exit(1)

    if mode == 'c':
        client = Client(port)
        client.run()
    elif mode == 's':
        server = Server(port)
        server.run()
    else:
        logging.error("Program mode must be set to either 'c' (client) or 's' (server).")
        sys.exit(1)


if __name__ == '__main__':
    main()
