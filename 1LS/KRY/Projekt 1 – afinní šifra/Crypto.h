/**
 * @file   Crypto.h
 * @brief  Affine Cipher encryption and decryption class declarations.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include <string>
#include <cstring>

#ifndef KRY1_CRYPTO_H
#define KRY1_CRYPTO_H

class Crypto {

public:
    Crypto();

    /**
     * Decrypts a given character.
     * Checks if the character is in bounds, throws a runtime error if it isn't.
     * @remark Crypto must be initialized with init() before usage.
     */
    [[nodiscard]] char decryptChar(char c) const;

    /**
     * Encrypts a given character.
     * Checks if the character is in bounds, throws a runtime error if it isn't.
     * @remark Crypto must be initialized with init() before usage.
     */
    [[nodiscard]] char encryptChar(char c) const;

    /**
     * Decrypts a given ciphertext.
     * @remark Crypto must be initialized with init() before usage.
     */
    std::string decrypt(std::string input);

    /**
     * Encrypts a given ciphertext.
     * @remark Crypto must be initialized with init() before usage.
     */
    std::string encrypt(std::string input);

    /**
     * Initializes this Crypto object with a given pair of keys.
     */
    void init(int aKey, int bKey);

private:
    /**
     * Decrypts a given character unsafely, that is, without handling spaces and checking for bounds.
     */
    static inline char decryptCharUnsafe(char c, int aKeyInverse, int bKey);

    /**
     * Encrypts a given character unsafely, that is, without handling spaces and checking for bounds.
     */
    static inline char encryptCharUnsafe(char c, int aKey, int bKey);

    /**
     * Pre-computes the decryption table and stores it in charTable.
     * Only indices 'A' to 'Z', shifted by 32, are populated.
     */
    void makeDecryptTable();

    /**
     * Pre-computes the encryption table and stores it in charTable.
     * Only indices 'A' to 'Z' and 'a' to 'z', shifted by 32, are populated.
     */
    void makeEncryptTable();

    /**
     * Used to store a pre-calculated encryption mapping table.
     * Its contents correspond to the ASCII table.
     */
    char encryptTable[123]{};

    /**
     * Used to store a pre-calculated decryption mapping table.
     * @sa encryptTable
     */
    char decryptTable[123]{};

    int aKey = -1;
    int bKey = -1;
    int aKeyInverse = -1;
    bool hasEncryptTable = false;
    bool hasDecryptTable = false;
};

#endif //KRY1_CRYPTO_H
