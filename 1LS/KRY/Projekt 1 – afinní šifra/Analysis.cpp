/**
 * @file   Analysis.cpp
 * @brief  Affine Cipher cryptoanalysis suite definitions.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include <sstream>
#include <algorithm>
#include <map>

#include "Analysis.h"
#include "Utils.h"
#include "Crypto.h"

using std::pair, std::vector, std::string;

bool sortPairByValue(const pair<int, double> &x, const pair<int, double> &y) { return x.second > y.second; }

pair<int, int> Analysis::findKeys(const string &line) {
    Crypto crypto;
    auto candidateKeys = findCandidateKeys(line);
    int iterations = std::min(10, (int) candidateKeys.size());
    double bestRatio = 0;
    pair<int, int> bestKey;

    for (int i = 0; i < iterations; i++) {
        auto key = candidateKeys[i];

        crypto.init(key.first, key.second);
        auto decrypted = crypto.decrypt(line);
        auto knownWords = analyseWords(decrypted);
        double ratio = (double) knownWords.second / knownWords.first;

        if (ratio > bestRatio) {
            bestRatio = ratio;
            bestKey = key;
        }
    }

    if (bestRatio > SUFFICIENT_KNOWN_WORDS_RATIO) {
        // If the known words ratio is high enough for our key, return it
        return bestKey;
    } else {
        // Otherwise, try to find the key by matching with top N words of the language
        double dictAttackRatio;
        auto dictKey = findByDictionary(line, dictAttackRatio);

        // but only return the value if it is better than the previous one
        if (dictAttackRatio > bestRatio)
            return dictKey;
        else
            return bestKey;
    }
}

pair<int, int> Analysis::findByDictionary(const string &line, double &finalRatio) {
    Crypto crypto;
    double bestRatio = 0;
    pair<int, int> bestKey;

    // the A key must be coprime to 26
    for (int aKey: A_KEY_OPTS) {
        // the effect of the B key is the same mod 26
        for (int bKey = 0; bKey < 26; bKey++) {
            crypto.init(aKey, bKey);
            auto decrypted = crypto.decrypt(line);
            auto knownWords = analyseWords(decrypted);
            double ratio = (double) knownWords.second / knownWords.first;

            if (ratio > bestRatio) {
                bestRatio = ratio;
                bestKey = {aKey, bKey};
            }
        }
    }

    finalRatio = bestRatio;
    return bestKey;
}

vector<pair<int, int>> Analysis::findCandidateKeys(const std::string &line) {
    // Compute frequencies of letters in the ciphertext
    auto inFreqs = makeLetterFrequencies(line);

    // Sort ciphertext letters by their frequency
    vector<pair<int, double>> inLetters;
    inLetters.reserve(26);

    for (int c = 0; c < 26; c++) {
        inLetters.emplace_back(c, inFreqs[c]);
    }

    sort(inLetters.begin(), inLetters.end(), sortPairByValue);

    vector<pair<int, int>> foundKeys;
    std::map<std::pair<int, int>, int> foundKeysCounts;

    int a, b;
    // Go through all pairs of letters
    for (int i = 0; i < 26; i++) {
        for (int j = 0; j < 26; j++) {
            if (i == j) continue;

            // Assume that the first i-th and j-th encrypted letters correspond
            // to the i-th and j-th statistically most common letter
            int c1 = inLetters[i].first;
            int c2 = inLetters[j].first;
            int p1 = ALPHABET_LETTERS_SORTED[i] - 'A';
            int p2 = ALPHABET_LETTERS_SORTED[j] - 'A';

            // Calculate a, b by solving a system of modular equations:
            // C1 = a * P1 + b  (mod 26)
            // C2 = a * P2 + b  (mod 26)
            // that is, (C1-C2)*(P1-P2)^-1 = a

            int mi = multiplicativeInverse(p1 - p2);
            // Multiplicative inverse may not exist for the chosen pair
            if (mi == -1) continue;

            // Get the coefficients (adjust for C++'s modular arithmetics)
            a = ((c1 - c2) * mi) % 26;
            if (a < 0) a += 26;
            b = (c1 - a * p1) % 26;
            if (b < 0) b += 26;

            // Check if the A coefficient is valid, that is, coprime with 26
            if (std::find(A_KEY_OPTS, A_KEY_OPTS + 12, a) == A_KEY_OPTS + 12)
                continue;

            // Append the found 'possible key' or increase its count by one
            pair<int, int> key = {a, b};
            auto existing = std::find(foundKeys.begin(), foundKeys.end(), key);

            if (existing == foundKeys.end()) {
                foundKeys.push_back(key);
                foundKeysCounts.insert({key, 1});
            } else {
                foundKeysCounts[key]++;
            }
        }
    }

    // Sort the found keys by number of occurrences and return
    std::sort(foundKeys.begin(), foundKeys.end(),
              [&foundKeysCounts](const pair<int, int> &x, const pair<int, int> &y) {
                  return foundKeysCounts[x] > foundKeysCounts[y];
              });

    return foundKeys;
}

vector<double> Analysis::makeLetterFrequencies(const string &str) {
    vector<double> freq(26, 0);

    for (char c: str) {
        if (c == ' ')
            continue;

        // Shift lowercase letters to uppercase
        if (c > 'Z')
            c -= 32;

        freq[c - 65]++;
    }

    double len = str.length();
    for (double &f: freq) {
        f = f / len;
    }

    return freq;
}

pair<int, int> Analysis::analyseWords(const string &str) {
    std::istringstream iss(str);

    int total = 0, known = 0;
    string word;
    auto end = TOP_N_WORDS.end();

    do {
        iss >> word;
        total++;
        known += (TOP_N_WORDS.find(word) == end) ? 0 : 1;
    } while (iss);

    return {total, known};
}
