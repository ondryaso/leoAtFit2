/**
 * @file   Analysis.h
 * @brief  Affine Cipher cryptoanalysis suite declarations.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include <string>
#include <vector>
#include <utility>
#include "Constants.h"

#ifndef KRY1_ANALYSIS_H
#define KRY1_ANALYSIS_H

class Analysis {
public:
    std::pair<int, int> findKeys(const std::string &line);


private:
    /**
     * Finds key candidates by solving systems of modular equations for the
     * @param line
     * @return
     */
    static std::vector<std::pair<int, int>> findCandidateKeys(const std::string &line);

    /**
     * Returns the number of words and the number of "known" words in a given string.
     * A word is known if it is one of the top N words defined in TOP_N_WORDS.
     * @return A pair of { total, known }.
     */
    std::pair<int, int> analyseWords(const std::string &str);

    /**
     * Calculates the frequency distribution of letters (A-Z) in a given string.
     * @param str The input string. It is expected to only consist of letters A-Z, a-z and spaces.
     * @return A vector of 26 elements where the first contains the frequency of As, the last of Zs, etc.
     */
    static std::vector<double> makeLetterFrequencies(const std::string &str);

    std::pair<int, int> findByDictionary(const std::string &line, double &ratio);
};


#endif //KRY1_ANALYSIS_H
