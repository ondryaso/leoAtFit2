/**
 * @file main.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief An LDPC encoder/decoder CLI utility.
 *
 * This file contains the main() function and the argument parsing logic.
 * Call either with -e to encode or -d to decode.
 *
 * In the encode mode, the parity check matrix can optionally be specified with -m. If not specified,
 * a new random LDPC matrix is generated and saved to matica.csv.
 *
 * In the decode mode, the parity check matrix must be specified with -m.
 *
 * The provided matrix file must exist and contain a valid matrix stored in CSV format.
 * Neither of these requirements is checked.
 */

#include <iostream>
#include "decoder.h"

void runEncode(const std::string &matrixPath);

int main(int argc, char *argv[]) {
    bool encode = false;
    bool decode = false;
    std::string path;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-e") {
            if (decode) {
                std::cerr << "Error: Both -e and -d cannot be used together.\n";
                return 1;
            }
            encode = true;
        } else if (arg == "-d") {
            if (encode) {
                std::cerr << "Error: Both -e and -d cannot be used together.\n";
                return 1;
            }
            decode = true;
        } else if (arg == "-m" && i + 1 < argc) {
            i++;
            path = argv[i];
        } else {
            std::cerr << "Error: Unknown argument or missing value for -m.\n";
            return 1;
        }
    }

    if (!encode && !decode) {
        std::cerr << "Error: Either -e or -d must be specified.\n";
        return 1;
    }

    if (encode) {
        runEncode(path);
    } else {
        runDecode(path);
    }

    return 0;
}
