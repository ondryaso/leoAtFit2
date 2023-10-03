/**
 * @file   Constants.h
 * @brief  Different constants used for configuring the computation.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include <string>
#include <unordered_set>

#ifndef KRY1_CONSTANTS_H
#define KRY1_CONSTANTS_H

#define ALT_LETTER_SET 1

/**
 * Specifies how many words from the TOP_N_WORDS list must be present
 * in a message to presume that a key candidate is correct.
 */
constexpr double SUFFICIENT_KNOWN_WORDS_RATIO = 0.2;

constexpr const std::string_view HELP_STRING =
        "Usage:\n%s -e -a a_key -b b_key input_plaintext\tencrypts the given text\n"
        "%s -d -a a_key -b b_key input_ciphertext\tdecrypts the given text\n"
        "%s -c [-m] -f in_file -o out_file\t\tattempts to decrypt the text without key in the given file using frequency analysis\n\n"
        "a_key must be an integer coprime with 26\n"
        "b_key must be an integer\n"
        "if -m is specified for the analysis mode, the file may contain multiple lines interpreted as individual messages\n\0";

#if ALT_LETTER_SET == 0
/**
 * Czech letters sorted by their frequency (in descending order).
 * @remark Source: https://www.matweb.cz/frekvencni-analyza/
 */
constexpr const char ALPHABET_LETTERS_SORTED[] = {'E', 'A', 'O', 'I', 'N', 'L', 'S', 'T', 'R', 'V',
                                               'D', 'M', 'U', 'K', 'Z', 'P', 'C', 'Y', 'H', 'J',
                                               'B', 'G', 'F', 'W', 'X', 'Q'};
#else
/**
 * Czech letters sorted by their frequency (in descending order).
 * @remark Source: https://nlp.fi.muni.cz/cs/FrekvenceSlovLemmat
 */
constexpr const char ALPHABET_LETTERS_SORTED[] = {'E', 'A', 'O', 'I', 'N', 'L', 'S', 'T', 'R', 'V',
                                                  'D', 'M', 'U', 'K', 'Z', 'P', 'C', 'Y', 'H', 'J',
                                                  'B', 'G', 'F', 'W', 'X', 'Q'};
#endif

/**
 * Top 400 Czech words excluding one-character-long ones.
 * @remark Source: https://cs.wiktionary.org/wiki/P%C5%99%C3%ADloha:Frekven%C4%8Dn%C3%AD_seznam_(%C4%8De%C5%A1tina)/%C4%8CNK_SYN2005/1-1000
 */
const extern std::unordered_set<std::string> TOP_N_WORDS;

/**
 * All possible values of the A key.
 */
constexpr const int A_KEY_OPTS[] = {1, 3, 5, 7, 9, 11, 15, 17, 19, 21, 23, 25};

#endif //KRY1_CONSTANTS_H
