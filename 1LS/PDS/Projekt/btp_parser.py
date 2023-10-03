# btp_parser.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
# ---
# Adapted from original code by Mostafa Razavi
# Original source: https://github.com/elektito/bttools/blob/master/utptrace.py

from bencoder import bencode, bdecode2, BTFailure
import hashlib
import logging
import sys
import struct


# Source: https://stackoverflow.com/a/57748513
class ByteFIFO:
    def __init__(self):
        self._buf = bytearray()

    def put(self, data: bytes):
        self._buf.extend(data)

    def get(self, size) -> bytes:
        data = self._buf[:size]
        # The fast delete syntax
        del self._buf[:size]
        return data

    def peek(self, size) -> bytes:
        return self._buf[:size]

    def get_buffer(self):
        return self._buf

    def __len__(self):
        return len(self._buf)


class BitTorrentParserError(Exception):
    pass


class UnexpectedEndOfStreamError(BitTorrentParserError):
    def __init__(self, msg='Unexpected end of stream.'):
        super(UnexpectedEndOfStreamError, self).__init__(msg)


class InvalidBitTorrentStreamError(BitTorrentParserError):
    def __init__(self, msg='Invalid BitTorrent stream.'):
        super(InvalidBitTorrentStreamError, self).__init__(msg)


message_parsers = {}
extended_message_parsers = {}


def register_message(n):
    def decorator(f):
        def wrapper(*args, **kwargs):
            f(*args, **kwargs)

        message_parsers[n] = f
        return wrapper

    return decorator


def register_extended_message(n):
    def decorator(f):
        def wrapper(*args, **kwargs):
            f(*args, **kwargs)

        extended_message_parsers[n] = f
        return wrapper

    return decorator


class BitTorrentParser(object):
    def __init__(self, monitor, ip: str, port: int):
        self.monitor = monitor
        self.ip = ip
        self.port = port

        self.infos = {}
        self.current_infohash = ''
        self.current_peerid = ''

        self.logger = logging.getLogger('btpparse')
        handler = logging.StreamHandler(sys.stdout)
        self.logger.addHandler(handler)

        self.message_buffer = ByteFIFO()
        self.next_len = 68
        self.reading_len = False
        self.had_handshake = False
        self.wrong = False
        self.extended_message_associations = {}

    def new_message(self, msg_type: str, bitfield=None, index=None, begin=None, length=None, data=None, port=None,
                    unknown_message_type=None):
        if msg_type == "piece":
            try:
                self.monitor.peer_dicts[self.current_infohash][(self.ip, self.port)].sent_pieces += 1
            except KeyError:
                self.monitor.out.log_error("Piece infohash/peer not found")
        elif msg_type == "bitfield":
            bits = len(bitfield) * 8
            if self.current_infohash in self.monitor.torrents:
                torrent = self.monitor.torrents[self.current_infohash]
                torrent.set_number_of_pieces(bits)
        elif begin is not None and length is not None:
            if self.current_infohash in self.monitor.torrents:
                torrent = self.monitor.torrents[self.current_infohash]
                torrent.piece_info(begin, length)

    def new_extended_message(self, extension_name: str, **kwargs):
        if extension_name == 'ut_pex':
            val = kwargs['value']
            added = val[b'added']
            self.monitor.extract_compact_peers(self.current_infohash, None, added)

    def init_peer(self, peer_id, info_hash):
        self.monitor.get_or_add_peer(info_hash, self.ip, self.port, peer_id)

    def process_segment(self, payload: bytes):
        if self.wrong:
            return

        self.message_buffer.put(payload)
        in_buffer = len(self.message_buffer)
        while in_buffer >= self.next_len:
            if self.reading_len:
                len_b = self.message_buffer.get(4)
                self.next_len = struct.unpack('!I', len_b)[0]
                self.reading_len = False
                in_buffer = len(self.message_buffer)
                continue

            if self.had_handshake:
                self.process_message()
            else:
                self.process_handshake()

            self.reading_len = True
            self.next_len = 4
            in_buffer = len(self.message_buffer)

    def add_info(self, info):
        self.infos[hashlib.sha1(bencode(info)).digest()] = info

    def process_handshake(self):
        stream = self.message_buffer.get(68)

        pstrlen, pstr, reserved, infohash, peerid = struct.unpack('!B19s8s20s20s', stream)
        self.current_infohash = infohash
        self.current_peerid = peerid

        if pstrlen != 19 or pstr != b'BitTorrent protocol':
            self.logger.debug("not BTP stream")
            self.wrong = True

        self.logger.debug('pstr: {}'.format(pstr))
        self.logger.debug('reserved: {}'.format(reserved.hex()))
        self.logger.debug('infohash: {}'.format(infohash.hex()))
        self.logger.debug('peerid: {}'.format(peerid.hex()))
        self.had_handshake = True

        self.init_peer(peerid, infohash)

    def process_message(self):
        stream = self.message_buffer.get(self.next_len)
        if len(stream) == 0:
            return

        message_type = stream[0]

        try:
            if self.next_len > 16393:
                self.logger.warning('Message length is over 16393. Possibly corrupted.')

            message_parsers[message_type](self, stream[1:], self.next_len - 1)
        except KeyError:
            self.logger.warning('[MESSAGE] UNKNOWN MESSAGE ID: {}'.format(message_type))
            self.__new_message('unknown', unknown_message_type=message_type)
        except Exception as e:
            self.logger.warning("Error occurred in message processing", exc_info=e)
            self.monitor.out.log_error(str(e))

    @register_extended_message('ut_metadata')
    def parse_message_ut_metadata(self, stream, length):
        try:
            ut_metadata, _ = bdecode2(bytes(stream))
        except BTFailure:
            raise InvalidBitTorrentStreamError()
        if ut_metadata['msg_type'] == 0:
            self.logger.info(
                '[MESSAGE] [EXTENDED] ut_metadata: request for piece {}'.format(
                    ut_metadata['piece']))
        elif ut_metadata['msg_type'] == 1:
            size = length - 2 - bencode(ut_metadata)
            self.logger.info(
                '[MESSAGE] [EXTENDED] ut_metadata: piece {} of size {}'.format(
                    ut_metadata['piece'], size))
        elif ut_metadata['msg_type'] == 2:
            self.logger.info(
                '[MESSAGE] [EXTENDED] ut_metadata: reject request for piece {}'.format(
                    ut_metadata['piece']))
        self.__new_extended_message('ut_metadata', piece=ut_metadata['piece'])

    @register_extended_message('upload_only')
    def parse_message_upload_only(self, stream, length):
        payload = stream
        self.logger.info('[MESSAGE] [EXTENDED] upload_only: turned {}'.format(
            'off' if payload[0] == '\x00' else 'on'))
        self.__new_extended_message('upload_only', value=(payload[0] != '\x00'))

    @register_extended_message('lt_tex')
    def parse_message_lt_tex(self, stream, length):
        try:
            lt_tex, _ = bdecode2(bytes(stream))
        except BTFailure:
            raise InvalidBitTorrentStreamError()
        self.logger.info('[MESSAGE] [EXTENDED] lt_tex: announced {} tracker(s).'.format(
            len(lt_tex['added'])))
        self.__new_extended_message('lt_tex', added=lt_tex['added'])

    @register_extended_message('ut_pex')
    def parse_message_ut_pex(self, stream, length):
        try:
            ut_pex, _ = bdecode2(bytes(stream))
        except BTFailure:
            raise InvalidBitTorrentStreamError()
        added = ut_pex[b'added']
        prefer_encryption = len([i for i in ut_pex[b'added.f'] if i & 0x01 == 1])
        seeders = len([i for i in ut_pex[b'added.f'] if i & 0x02 == 1])
        self.logger.info(
            '[MESSAGE] [EXTENDED] ut_pex: added {} peers ({} prefer(s) '
            'encryption; {} is/are seeder(s)). dropped {}.'.format(
                len(added) / 6,  # In compact form, each 6 bytes
                # represents an IPv4 address and a
                # port number.
                prefer_encryption,
                seeders,
                len(ut_pex[b'dropped']) / 6))

        if all(k in ut_pex for k in [b'added6', b'added6.f', b'dropped6']) and \
                (len(ut_pex[b'added6']) > 0 or len(ut_pex[b'dropped6']) > 0):
            added = ut_pex[b'added6']
            prefer_encryption = len([i for i in ut_pex[b'added6.f'] if i & 0x01 == 1])
            seeders = len([i for i in ut_pex[b'added6.f'] if i & 0x02 == 1])
            self.logger.info(
                '[MESSAGE] [EXTENDED]         also added {} IPv6 peers '
                '({} prefer(s) encryption; {} is/are seeder(s)). '
                'dropped {}.'.format(
                    len(added) / 18,  # In compact form, each 18 bytes
                    # represents an IPv6 address and
                    # a port number.
                    prefer_encryption,
                    seeders,
                    len(ut_pex[b'dropped6']) / 18))

        self.__new_extended_message('ut_pex', value=ut_pex)

    @register_message(20)
    def parser_message_extended(self, stream, length):
        ext_msg_type = stream[0]
        if ext_msg_type == 0:
            handshake = stream[1:]
            handshake, _ = bdecode2(bytes(handshake))
            self.logger.info('[MESSAGE] [EXTENDED] HANDSHAKE: {}'.format(handshake))

            for name, number in handshake[b'm'].items():
                if number == 0:
                    # disable this extension
                    self.extended_message_associations = {
                        k: v for k, v in self.extended_message_associations.items()
                        if v != name}
                else:
                    # add this extension
                    self.extended_message_associations[number] = name
        elif ext_msg_type in self.extended_message_associations:
            name = self.extended_message_associations[ext_msg_type].decode()

            if name not in extended_message_parsers:
                self.logger.info('[MESSAGE][EXTENDED] "{}" message.'.format(name))
                self.new_extended_message(name)
                return

            extended_message_parsers[name](self, stream[1:], length)
        else:
            self.logger.warning(
                '[MESSAGE] [EXTENDED] UNKNOWN MESSAGE ID: {}'.format(ext_msg_type))

    @register_message(0)
    def parse_message_choke(self, stream, length):
        self.logger.info('[MESSAGE] CHOKE')
        self.__new_message('choke')

    @register_message(1)
    def parse_message_unchoke(self, stream, length):
        self.logger.info('[MESSAGE] UNCHOKE')
        self.__new_message('unchoke')

    @register_message(2)
    def parse_message_interested(self, stream, length):
        self.logger.info('[MESSAGE] INTERESTED')
        self.__new_message('interested')

    @register_message(3)
    def parse_message_not_interested(self, stream, length):
        self.logger.info('[MESSAGE] NOT INTERESTED')
        self.__new_message('not_interested')

    @register_message(4)
    def parse_message_have(self, stream, length):
        index = struct.unpack('!I', stream[0:4])[0]
        self.logger.info('[MESSAGE] HAVE: {}'.format(index))
        self.__new_message('have', index=index)

    @register_message(5)
    def parse_message_bitfield(self, stream, length):
        self.__new_message('bitfield', bitfield=stream)

    @register_message(6)
    def parse_message_request(self, stream, length):
        index, begin, length = struct.unpack('!3I', stream[0:12])

        self.logger.info(
            '[MESSAGE] REQUEST: index={} begin={} length={}'.format(
                index, begin, length))
        self.__new_message('request', index=index, begin=begin, length=length)

    @register_message(7)
    def parse_message_piece(self, stream, length):
        index, begin = struct.unpack('!2I', stream[0:8])
        block_size = length - 8
        data = stream[8:]

        assert (len(data) == block_size)
        self.logger.info(
            '[MESSAGE] PIECE: index={} begin={} length={}'.format(
                index, begin, block_size))
        self.__new_message('piece', index=index, begin=begin, data=data)

    @register_message(8)
    def parse_message_cancel(self, stream, length):
        index, begin, length = struct.unpack('!3I', stream[0:12])
        self.logger.info(
            '[MESSAGE] CANCEL: index={} begin={} length={}'.format(
                index, begin, length))
        self.__new_message('cancel', index=index, begin=begin, length=length)

    @register_message(9)
    def parse_message_port(self, stream, length):
        port = struct.unpack('!H', stream[0:2])[0]
        self.logger.info('[MESSAGE] PORT: {}'.format(port))
        self.__new_message('port', port=port)

    @register_message(0x0d)
    def parse_message_suggest_piece(self, stream, length):
        index = struct.unpack('!I', stream[0:4])[0]
        self.logger.info('[MESSAGE] SUGGEST PIECE: {}'.format(index))
        self.__new_message('suggest_piece', index=index)

    @register_message(0x0e)
    def parse_message_have_all(self, stream, length):
        self.logger.info('[MESSAGE] HAVE ALL')
        self.__new_message('have_all')

    @register_message(0x0f)
    def parse_message_have_none(self, stream, length):
        self.logger.info('[MESSAGE] HAVE NONE')
        self.__new_message('have_none')

    @register_message(0x10)
    def parse_message_reject(self, stream, length):
        index, begin, length = struct.unpack('!3I', stream[0:12])

        self.logger.info(
            '[MESSAGE] REJECT: index={} begin={} length={}'.format(
                index, begin, length))
        self.__new_message('reject')

    @register_message(0x11)
    def parse_message_allowed_fast(self, stream, length):
        index = struct.unpack('!I', stream[0:4])[0]
        self.logger.info('[MESSAGE] ALLOWED FAST: {}'.format(index))
        self.__new_message('allowed_fast')

    def __new_extended_message(self, name, **attrs):
        self.new_extended_message(name, **attrs)

    def __new_message(self, name, **attrs):
        self.new_message(name, **attrs)
