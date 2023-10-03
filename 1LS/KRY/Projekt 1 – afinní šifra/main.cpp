/**
 * @file   main.cpp
 * @brief  An Affine Cipher encryption and decryption utility.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <vector>
#include <filesystem>
#include <set>

#include "Constants.h"
#include "Crypto.h"
#include "Analysis.h"

using std::string, std::runtime_error, std::cout, std::cerr, std::vector, std::pair, std::endl;

enum Mode {
    NONE,
    ENCRYPT,
    DECRYPT,
    CRYPTO_ANALYSIS
};

inline void printHelp(char **argv, FILE *stream = stderr) {
    string exeName = argv[0];
    std::filesystem::path p(exeName);
    auto fn = p.filename().c_str();

    fprintf(stream, HELP_STRING.data(), fn, fn, fn);
}

void doCryptoAnalysis(const string &inFile, const string &outFile, bool multiline = false) {
    std::ifstream in(inFile);
    std::ofstream out(outFile);

    string line;
    Analysis analysis;
    Crypto crypto;

    if (!std::getline(in, line)) {
        throw runtime_error("input file empty");
    }

    do {
        // Trim \r's and other bad guys
        line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
            return !std::iscntrl(ch);
        }).base(), line.end());

        auto keys = analysis.findKeys(line);
        crypto.init(keys.first, keys.second);
        string m = crypto.decrypt(line);

        out << m << std::endl;
        cout << "a=" << keys.first << ",b=" << keys.second << std::endl;
    } while (multiline && std::getline(in, line));

    in.close();
    out.close();
}

int main(int argc, char **argv) {
    int option;
    Mode mode = NONE;
    string aKeyIn, bKeyIn, inFile, outFile, textIn;
    bool multiline = false;
    int aKey, bKey;

    while ((option = getopt(argc, argv, "hmedca:b:f:o:")) != -1) {
        switch (option) {
            case 'e':
                if (mode != NONE) {
                    printHelp(argv);
                    return 1;
                }

                mode = ENCRYPT;
                break;
            case 'd':
                if (mode != NONE) {
                    printHelp(argv);
                    return 1;
                }

                mode = DECRYPT;
                break;
            case 'c':
                if (mode != NONE) {
                    printHelp(argv);
                    return 1;
                }

                mode = CRYPTO_ANALYSIS;
                break;
            case 'a':
                aKeyIn = optarg;
                break;
            case 'b':
                bKeyIn = optarg;
                break;
            case 'f':
                inFile = optarg;
                break;
            case 'o':
                outFile = optarg;
                break;
            case 'm':
                multiline = true;
                break;
            case 'h':
                printHelp(argv, stdout);
                return 0;
            default:
                printHelp(argv);
                return 1;
        }
    }

    try {
        switch (mode) {
            case NONE:
                printHelp(argv);
                return 1;
            case ENCRYPT:
            case DECRYPT: {
                if (optind == argc) {
                    // no input text
                    printHelp(argv);
                    return 1;
                }

                textIn = argv[optind];
                aKey = std::stoi(aKeyIn);
                bKey = std::stoi(bKeyIn);

                Crypto crypto;
                crypto.init(aKey, bKey);

                if (mode == ENCRYPT)
                    cout << crypto.encrypt(textIn) << std::endl;
                else
                    cout << crypto.decrypt(textIn) << std::endl;

                break;
            }
            case CRYPTO_ANALYSIS:
                doCryptoAnalysis(inFile, outFile, multiline);
                break;
        }
    } catch (std::ios_base::failure const &ex) {
        cerr << ex.what() << std::endl;
        return 3;
    } catch (std::runtime_error const &ex) {
        cerr << ex.what() << std::endl;
        return 2;
    } catch (std::exception const &ex) {
        printHelp(argv);
        return 1;
    }

    return 0;
}
