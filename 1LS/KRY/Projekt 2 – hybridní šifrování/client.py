# client.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
# Date: 2023-04-28
# KRY Project 2
# This module implements the client logic.

import logging
import sys
from typing import Tuple

from Crypto.Hash import MD5, SHA3_256
from Crypto.PublicKey.RSA import RsaKey
from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes

import util
from util import KeyManager
from socket import socket, AF_INET, SOCK_STREAM

# Socket blocking operations timeout in seconds
SOCK_TIMEOUT = 10


def print_keys(priv: RsaKey, pub: RsaKey, server_pub: RsaKey):
    print("RSA_public_key_sender=" + pub.export_key('OpenSSH').decode())
    print("RSA_private_key_sender_hash=" + SHA3_256.new(priv.export_key()).hexdigest())
    print("RSA_public_key_receiver=" + server_pub.export_key('OpenSSH').decode())


def get_input():
    print("Enter input: ", end='', flush=True)
    return sys.stdin.readline().strip()


def make_aes_key(padding_param_n: int) -> Tuple[bytes, bytes]:
    """Creates a random 16B key."""
    # this is actually just an alias for /dev/urandom, I think
    key = get_random_bytes(16)
    padded = util.pad_for_rsa(key, padding_param_n)
    print("AES_key=" + key.hex())
    print("AES_key_padding=" + padded.hex())
    return key, padded


def make_digest(message: bytes, padding_param_n: int):
    """
    Creates an MD5 digest for a message.
    Returns the digest and its padded version.
    """
    md5 = MD5.new()
    md5.update(message)
    digest = md5.digest()
    padded = util.pad_for_rsa(digest, padding_param_n)
    print("MD5=" + digest.hex())
    print("MD5_padding=" + padded.hex())
    return digest, padded


def make_message(input_text: str, client_priv_key: RsaKey, server_pub_key: RsaKey):
    input_message = input_text.encode('utf-8')
    # An input vector for the AES CBC mode – its integrity must be established
    iv = get_random_bytes(16)

    # Create an AES session key, pad it (OAEP) and encrypt it with server's public key
    session_key, session_key_padded = make_aes_key(client_priv_key.n)
    session_key_encrypted = util.rsa_encrypt(session_key_padded, server_pub_key)

    # Create an MD5 digest of the input message prepended with IV (to ensure its integrity),
    # pad it (OAEP) and sign in with client's private key
    digest, digest_padded = make_digest(iv + input_message, client_priv_key.n)
    # Signing using a private key is actually performed as a 'decryption' using this key (without unpadding, obviously)
    digest_signed = util.rsa_decrypt(digest_padded, client_priv_key, False)

    # The encrypted message is User Input + Digest, padded for AES
    message_to_encrypt = util.pad_for_aes(input_message + digest_signed)

    aes = AES.new(session_key, AES.MODE_CBC, iv)
    encrypted_message = aes.encrypt(message_to_encrypt)

    print("RSA_MD5_hash=" + digest_signed.hex())
    print("AES_IV=" + iv.hex())
    print("AES_cipher=" + encrypted_message.hex())
    print("RSA_AES_key=" + session_key_encrypted.hex())

    return iv, encrypted_message, session_key_encrypted, digest, message_to_encrypt, session_key


class Client(object):

    def __init__(self, port: int):
        self.port = port
        self.logger = logging.getLogger('client')
        self.key_man = KeyManager(port, 'client')
        self.socket = socket(AF_INET, SOCK_STREAM)
        self.retries = 0

    def __del__(self):
        if hasattr(self, 'socket'):
            self.socket.close()

    def run(self):
        try:
            self._run()
        except KeyboardInterrupt:
            self.logger.info("Ending.")
            if hasattr(self, 'socket'):
                self.socket.close()

    def _run(self):
        self.logger.info("Running in client mode.")
        self.key_man.ensure_own_keys()

        priv_key = self.key_man.get_own_rsa_priv_key()
        pub_key = self.key_man.get_own_rsa_pub_key()
        server_pub_key = self.key_man.get_rsa_pub_key('server')

        if server_pub_key is None:
            self.logger.error("Cannot find server's public key, won't proceed.")
            return

        if priv_key is None or pub_key is None:
            self.logger.error("Cannot determine own keys, got panic attack.")
            return

        print_keys(priv_key, pub_key, server_pub_key)

        self.socket.settimeout(SOCK_TIMEOUT)

        try:
            self.socket.connect(('127.0.0.1', self.port))
        except OSError as e:
            self.logger.error("Cannot connect to the server. Backing off.", exc_info=e)
            return

        input_text = get_input()

        while True:
            try:
                iv, encrypted_message, encrypted_session_key, digest, plaintext_message, session_key = \
                    make_message(input_text, priv_key, server_pub_key)
                # Messages are sent to the server as a stream of 4 bytes of length and the message.
                # A message is composed of 16 B of the IV, the message and 256 B of encrypted session key.
                total_len = len(iv) + len(encrypted_message) + len(encrypted_session_key)
                payload = total_len.to_bytes(4, 'big') + iv + encrypted_message + encrypted_session_key
                ok = self._send(payload)
                if not ok:
                    return

                res = self._recv_ack(digest, session_key, plaintext_message[-16:])
                if res:
                    input_text = get_input()
            except TimeoutError:
                self.logger.error("Timeout when sending or receiving a message. Halting.")
                return

    def _send(self, payload: bytes) -> bool:
        """
        Sends a message, if error occurs, tries at max 3 times.
        """
        try:
            self.socket.sendall(payload)
            self.retries = 0
            return True
        except OSError as e:
            if self.retries < 3:
                self.retries += 1
                self.logger.warning(f"Cannot send message. Retrying {self.retries}/3.")
                return self._send(payload)
            else:
                self.logger.error(f"Cannot send message. Halting.", exc_info=e)
                return False

    def _recv_ack(self, digest: bytes, session_key: bytes, iv: bytes) -> bool:
        """
        Waits for the incoming 32 B of an acknowledgement message from the server.
        The ack message is encrypted using the same session key as the original data.
        The first byte is either a 1, signalising success, or 0, signalising that the server
        has failed in checking the message's integrity. The next bytes are the original digest.
        """
        ack_msg = self.socket.recv(32)
        if not ack_msg or len(ack_msg) < 32:
            print("The message may have been delivered, malformed or no ACK received. Resending.")
            return False

        aes = AES.new(session_key, AES.MODE_CBC, iv)
        ack_msg = aes.decrypt(ack_msg)
        ack_msg = util.unpad_for_aes(ack_msg)

        if len(ack_msg) != len(digest) + 1:
            print("Invalid ACK received from the server. Message may have been compromised.")
            return True

        ack_digest = ack_msg[1:]
        if ack_digest != digest:
            print("Received ACK with invalid digest. Message may have been compromised.")
            return True

        if ack_digest[0] == 0:
            print("The message has been delivered but its integrity has been compromised according to the server.")

        print("The message has been delivered.")
        return True
