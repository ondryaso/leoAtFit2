# util.py
# Author: Ondřej Ondryáš (xondry02@stud.fit.vut.cz)
# Date: 2023-04-28
# KRY Project 2
# This module implements key loading, encryption/decryption and padding/unpadding.

import logging
import os
from Crypto.PublicKey import RSA
from Crypto.Hash import SHA3_256
from Crypto.PublicKey.RSA import RsaKey
from Crypto.Util.number import ceil_div, size as number_size, bytes_to_long, long_to_bytes
from Crypto.Random import get_random_bytes
from Crypto.Math.Numbers import Integer

CERT_FOLDER_PATH = os.path.join(".", "cert")
PUBKEY_PATH = os.path.join(".", "cert", "port-{}-{}.pub")
PRIVKEY_PATH = os.path.join(".", "cert", "port-{}-{}.key")
RSA_BITS = 2048


class KeyManager(object):
    """
    Generates keys, stores them in the "cert" folder and retrieves them when needed.
    """

    def __init__(self, key_id: int, party):
        self.id = key_id
        self.party = party
        self.logger = logging.getLogger("keys")
        self.keys = None

        if not os.path.isdir(CERT_FOLDER_PATH):
            os.mkdir(CERT_FOLDER_PATH)

    def get_rsa_pub_key(self, party):
        """
        Retrieves a public key belonging to 'party' from the storage.
        :param party: The party. Only 'server' or 'client' are used here.
        """
        pub_path = PUBKEY_PATH.format(self.id, party)

        if os.path.isfile(pub_path):
            try:
                with open(pub_path, 'rb') as pub_file:
                    pub_key = RSA.import_key(pub_file.read())
                    return pub_key
            except ValueError or IndexError or TypeError as e:
                self.logger.error(f"[{self.id}/{self.party}] Cannot read the public key of party {party}.",
                                  exc_info=e)
            except OSError as e:
                self.logger.error(f"[{self.id}/{self.party}] Cannot open public key of party {party}.",
                                  exc_info=e)
        else:
            self.logger.error(f"[{self.id}/{self.party}] Public key of party {party} doesn't exist.")

        return None

    def get_own_rsa_pub_key(self):
        if self.keys is not None:
            return self.keys[1]

        return self.get_rsa_pub_key(self.party)

    def get_own_rsa_priv_key(self):
        if self.keys is not None:
            return self.keys[0]

        return None

    def ensure_own_keys(self):
        """
        Ensures that keys for the caller exist. Loads stored keys or generates new ones.
        Public keys are stored in the OpenSSH format, private keys are stored in the PEM format.
        """
        priv_path = PRIVKEY_PATH.format(self.id, self.party)
        pub_path = PUBKEY_PATH.format(self.id, self.party)

        if os.path.isfile(priv_path):
            # Private key exists
            with open(priv_path, 'rb') as priv_file:
                priv_key = RSA.import_key(priv_file.read())
                pub_key = priv_key.public_key()

                if not os.path.isfile(pub_path):
                    # privkey present, no pubkey
                    self.logger.info(f"[{self.id}/{self.party}] Storing public key.")
                    with open(pub_path, 'wb') as pub_file:
                        pub_file.write(pub_key.export_key('OpenSSH'))
        else:
            # No private key stored, generate and save a new one
            self.logger.info(f"[{self.id}/{self.party}] Generating new key pair.")
            priv_key = RSA.generate(RSA_BITS)
            pub_key = priv_key.public_key()

            with open(priv_path, 'wb') as priv_file, open(pub_path, 'wb') as pub_file:
                priv_file.write(priv_key.export_key('PEM'))
                pub_file.write(pub_key.export_key('OpenSSH'))
                self.logger.info(f"[{self.id}/{self.party}] The key pair has been generated and stored.")

        self.keys = (priv_key, pub_key)


def _mgf1(seed: bytes, length: int) -> bytes:
    """
    A mask generation function for use in OAEP padding.
    Uses SHA3_256 as the underlying hashing function.
    Source: https://en.wikipedia.org/wiki/Mask_generation_function
    """
    h_len = SHA3_256.digest_size

    if length > (h_len << 32):
        raise ValueError("mask too long")
    t = b''
    counter = 0
    while len(t) < length:
        c = counter.to_bytes(4, 'big')
        sha = SHA3_256.new()
        sha.update(seed + c)
        t += sha.digest()
        counter += 1

    return t[:length]


def _xor(a: bytes, b: bytes) -> bytes:
    """
    Performs a XOR between the contents of two 'bytes' strings, bit-by-bit.
    The strings must be the same length.
    """
    if len(a) != len(b):
        raise ValueError("Byte strings must have the same length.")
    return bytes(x ^ y for x, y in zip(a, b))


def rsa_encrypt(padded_message: bytes, key: RsaKey) -> bytes:
    """
    Encrypts a message using a given RSA key (c = m^e mod n)
    The message must be already padded to the cipher's block size.
    """
    modulus_bits = number_size(key.n)
    k = ceil_div(modulus_bits, 8)

    padded_int = bytes_to_long(padded_message)
    encrypted_int = int(pow(Integer(padded_int), key.e, key.n))

    return long_to_bytes(encrypted_int, k)


def rsa_decrypt(message: bytes, key: RsaKey, padded: bool) -> bytes:
    """
    Decrypts a message using a given RSA key (m = c^d mod n).
    When 'padded' is True, attempts to unpad the result (using OAEP).
    """
    modulus_bits = number_size(key.n)
    k = ceil_div(modulus_bits, 8)

    encrypted_int = bytes_to_long(message)
    decrypted_int = int(pow(Integer(encrypted_int), key.d, key.n))
    decrypted = long_to_bytes(decrypted_int, k)

    if padded:
        return _depad_content(decrypted, k)
    else:
        return decrypted


def pad_for_aes(message: bytes) -> bytes:
    """
    Pads a message for use in the AES cipher.
    PKCS#7 padding is used, so the message is padded with repeating bytes
    with their values equal to the number of added bytes.
    """
    block_size = 16  # AES always uses 16B blocks
    padding_len = block_size - len(message) % block_size
    return message + bytes([padding_len]) * padding_len


def unpad_for_aes(padded_message: bytes) -> bytes:
    """
    Unpads a message padded using PKCS#7. The padding length is determined by
    any of the pad bytes.
    """
    padding_len = padded_message[-1]
    return padded_message[:-padding_len]


def pad_for_rsa(message: bytes, rsa_modulus: int) -> bytes:
    """
    Pads a message for use in the RSA cipher.
    The RSA-OAEP padding scheme is used.
    The maximum length of the message for 2048b RSA is 190 B.
    Uses SHA3_256 as the hash function.
    :param message: The message to pad.
    :param rsa_modulus: The modulus of the RSA key that will be used in the process.
    """
    # The length of the modulus is used in the calculation.
    modulus_bits = number_size(rsa_modulus)
    k = ceil_div(modulus_bits, 8)
    return _pad_content(message, k)


def _pad_content(message: bytes, k: int) -> bytes:
    """
    Source: https://en.wikipedia.org/wiki/Optimal_asymmetric_encryption_padding
    :param message: The message to pad.
    :param k: The 'k' parameter (the length of the modulus).
    """
    label = b''  # no need to use a label, but it must be present in the algorithm
    hash_fun = SHA3_256
    m_len = len(message)
    h_len = hash_fun.digest_size

    if m_len > k - 2 * h_len - 2:
        raise ValueError("Message is too long.")

    l_hash = hash_fun.new(label).digest()
    ps_len = k - m_len - 2 * h_len - 2
    ps = b'\x00' * ps_len

    data_block = l_hash + ps + b'\x01' + message
    assert (len(data_block) == k - h_len - 1)

    seed = get_random_bytes(h_len)
    data_block_mask = _mgf1(seed, len(data_block))
    masked_data_block = _xor(data_block, data_block_mask)
    seed_mask = _mgf1(masked_data_block, h_len)
    masked_seed = _xor(seed, seed_mask)

    return b'\x00' + masked_seed + masked_data_block


def unpad_for_rsa(message: bytes, rsa_modulus: int) -> bytes:
    """
    Unpads a message padded using OAEP with SHA3_256.
    """
    modulus_bits = number_size(rsa_modulus)
    k = ceil_div(modulus_bits, 8)
    return _depad_content(message, k)


def _depad_content(message: bytes, k: int):
    """
    Unpads a message padded using OAEP with SHA3_256.
    """
    label = b''
    hash_fun = SHA3_256
    h_len = hash_fun.digest_size

    if message[0] != 0:
        raise ValueError("Message is not in OAEP format.")

    # EM = 0x00 || masked_seed || masked_db, len EM = 1 + h_len + (k - h_len - 1)
    if len(message) != k:
        raise ValueError("Message is not in OAEP format.")

    masked_seed = message[1:(1 + h_len)]
    masked_data_block = message[(1 + h_len):]

    assert (len(masked_seed) == h_len)
    assert (len(masked_data_block) == k - h_len - 1)

    seed_mask = _mgf1(masked_data_block, h_len)
    seed = _xor(masked_seed, seed_mask)
    data_block_mask = _mgf1(seed, len(masked_data_block))
    data_block = _xor(masked_data_block, data_block_mask)

    l_hash_presumed = hash_fun.new(label).digest()
    l_hash_actual = data_block[0:h_len]

    # Check label hash
    if l_hash_actual != l_hash_presumed:
        raise ValueError("Invalid padding.")

    # Find position of 0x01, check everything between end of l_hash and 0x01 are zeros
    one_pos = data_block.index(1, h_len)
    for b in data_block[h_len:one_pos]:
        if b != 0:
            raise ValueError("Invalid padding.")

    return data_block[one_pos + 1:]
