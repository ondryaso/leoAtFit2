# server.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
# Date: 2023-04-28
# KRY Project 2
# This module implements the server logic.

import logging
from socketserver import TCPServer, BaseRequestHandler

from Crypto.Cipher import AES
from Crypto.Hash import MD5, SHA3_256

import util
from util import KeyManager

# Socket blocking operations timeout in seconds
SOCK_TIMEOUT = 10


class Server(object):

    def __init__(self, port: int):
        self.port = port
        self.logger = logging.getLogger('server')
        self.key_man = KeyManager(port, 'server')
        self.socket_server = TCPServer(('127.0.0.1', port), ServerRequestHandler, bind_and_activate=False)
        self.socket_server.key_man = self.key_man
        self.socket_server.allow_reuse_address = True
        self.socket_server.timeout = SOCK_TIMEOUT

    def __del__(self):
        if hasattr(self, 'socket_server'):
            self.socket_server.server_close()

    def run(self):
        self.logger.info("Running in server mode.")
        self.key_man.ensure_own_keys()
        try:
            self.socket_server.server_bind()
            self.socket_server.server_activate()
            self.socket_server.serve_forever()
        except KeyboardInterrupt:
            self.logger.info("Ending")
            self.socket_server.server_close()
        except Exception as e:
            self.logger.error("Server error", exc_info=e)
            self.socket_server.server_close()


class ServerRequestHandler(BaseRequestHandler):
    WAITING_LEN = 0
    READING_MSG = 1

    def __init__(self, request, client_address, server):
        self.client_pub_key = server.key_man.get_rsa_pub_key('client')
        self.server_priv_key = server.key_man.get_own_rsa_priv_key()

        logger = logging.getLogger('server')
        if self.client_pub_key is None:
            logger.error("Cannot find the client's public key, won't proceed.")
            return

        if self.server_priv_key is None:
            logger.error("Cannot determine own keys, got panic attack.")
            return

        print("Client has joined.")
        print("RSA_public_key_receiver=" + server.key_man.get_own_rsa_pub_key().export_key('OpenSSH').decode())
        print("RSA_private_key_receiver_hash=" + SHA3_256.new(self.server_priv_key.export_key()).hexdigest())
        print("RSA_public_key_sender=" + self.client_pub_key.export_key('OpenSSH').decode())

        self.recv_state = ServerRequestHandler.WAITING_LEN
        # a bytearray can be used as a dynamic FIFO queue for bytes
        self.buf = bytearray()
        self.waiting_for = -1

        super().__init__(request, client_address, server)

    def handle(self) -> None:
        while True:
            # accept received data and stores it in a buffer
            data = self.request.recv(2048)
            if not data:
                break

            self.buf.extend(data)
            # check if there isn't enough data in the buffer
            self.process_data()

    def process_data(self):
        # the connection can be in two states: waiting for the initial 4 bytes of next message length
        # or waiting for the message bytes
        if self.recv_state == ServerRequestHandler.WAITING_LEN:
            if len(self.buf) >= 4:
                # read the next message length
                self.waiting_for = int.from_bytes(self.buf[0:4], 'big')
                del self.buf[0:4]

                self.recv_state = ServerRequestHandler.READING_MSG
                # the message may have been also received already
                if len(self.buf) >= self.waiting_for:
                    self.process_message()
        elif len(self.buf) >= self.waiting_for:
            self.process_message()

    def process_message(self):
        data_len = self.waiting_for
        # read the message and delete it from the buffer
        data = self.buf[0:data_len]
        del self.buf[0:data_len]
        self.waiting_for = -1
        self.recv_state = ServerRequestHandler.WAITING_LEN

        self.decrypt_message(data)
        # call process_data to check if there isn't enough data in the buffer left
        self.process_data()

    def decrypt_message(self, data: bytearray):
        data_len = len(data)
        # the first 16 B is the IV
        iv = bytes(data[0:16])
        # the last 256 are the encrypted session key, anything between is the message
        encrypted_message = data[16:(data_len - 256)]
        encrypted_session_key = bytes(data[(data_len - 256):])

        # decrypt using the server's private key
        # the message is also unpadded
        session_key = util.rsa_decrypt(encrypted_session_key, self.server_priv_key, True)
        aes = AES.new(session_key, AES.MODE_CBC, iv)

        # decrypt the message using the session key and removes the padding
        decrypted_message_padded = aes.decrypt(encrypted_message)
        decrypted_message = util.unpad_for_aes(decrypted_message_padded)

        message_data = decrypted_message[:-256]
        # the final 256 bytes of the message data is the 'signed' digest (encrypted using the client's private key)
        digest_signed = decrypted_message[-256:]
        # the plaintext digest is retrieved by 'encrypting' the signed digest using the client's pubkey
        digest_padded = util.rsa_encrypt(digest_signed, self.client_pub_key)
        digest = util.unpad_for_rsa(digest_padded, self.client_pub_key.n)

        # the digest was calculated from the IV concatenated with the message
        md5 = MD5.new(iv + message_data)
        presumed_digest = md5.digest()

        print("ciphertext=" + data.hex())
        print("RSA_AES_key=" + encrypted_session_key.hex())
        print("AES_cipher=" + encrypted_message.hex())
        print("AES_key=" + session_key.hex())
        print("text_hash=" + decrypted_message.hex())
        print("plaintext=" + message_data.decode('utf-8'))
        print("MD5=" + presumed_digest.hex())

        state = b'\x01'

        if digest == presumed_digest:
            print("The integrity of the message has not been compromised.")
        else:
            print("The integrity of the message has been compromised.")
            state = b'\x00'

        # send the acknowledgement message
        ack_msg = state + digest
        ack_msg = util.pad_for_aes(ack_msg)

        iv = bytes(decrypted_message_padded[-16:])
        aes = AES.new(session_key, AES.MODE_CBC, iv)
        ack_msg = aes.encrypt(ack_msg)

        self.request.sendall(ack_msg)
