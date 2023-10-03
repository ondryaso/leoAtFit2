/**
 * @file   Crypto.cpp
 * @brief  Affine Cipher encryption and decryption class definitions.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include <system_error>

#include "Crypto.h"
#include "Constants.h"
#include "Utils.h"

using std::string, std::runtime_error;

inline char Crypto::decryptCharUnsafe(char c, int aKeyInverse, int bKey) {
    int res = (aKeyInverse * (((int) c) - 65 - bKey)) % 26;
    // C++ modular arithmetics yields negative outputs for negative inputs
    if (res < 0)
        res += 26;

    return (char) (res + 65);
}

inline char Crypto::encryptCharUnsafe(char c, int aKey, int bKey) {
    int res = (((int) c - 65) * aKey + bKey) % 26;
    // C++ modular arithmetics yields negative outputs for negative inputs
    if (res < 0)
        res += 26;

    return (char) (res + 65);
}

Crypto::Crypto() {
    memset(decryptTable, ' ', 123);
    memset(encryptTable, ' ', 123);
}

char Crypto::decryptChar(char c) const {
    if (c == ' ')
        return ' ';

    if (c < 'A' || c > 'Z')
        throw runtime_error(string("invalid input: ") + c);

    return decryptCharUnsafe(c, aKeyInverse, bKey);
}

char Crypto::encryptChar(char c) const {
    if (c == ' ')
        return ' ';

    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
        throw runtime_error(string("invalid input: ") + c);

    if (c > 'Z')
        c -= ('a' - 'A');

    return encryptCharUnsafe(c, aKey, bKey);
}

void Crypto::makeDecryptTable() {
    if (hasDecryptTable)
        return;


    for (char c = 'A'; c <= 'Z'; c++) {
        decryptTable[c] = decryptCharUnsafe(c, aKeyInverse, bKey);
    }

    hasDecryptTable = true;
}

void Crypto::makeEncryptTable() {
    if (hasEncryptTable)
        return;

    for (char c = 'A'; c <= 'Z'; c++) {
        encryptTable[c] = encryptCharUnsafe(c, aKey, bKey);
        encryptTable[c + 32] = encryptTable[c];
    }

    hasEncryptTable = true;
}

string Crypto::decrypt(string input) {
    makeDecryptTable();

    for (char &c: input) {
        c = decryptTable[c];
    }

    return input;
}

string Crypto::encrypt(string input) {
    makeEncryptTable();

    for (char &c: input) {
        c = encryptTable[c];
    }

    return input;
}

void Crypto::init(int a, int b) {
    aKey = a;
    bKey = b;
    aKeyInverse = multiplicativeInverse(a);

    if (aKeyInverse == -1)
        throw runtime_error("the given key is invalid");

    hasDecryptTable = hasEncryptTable = false;
}
