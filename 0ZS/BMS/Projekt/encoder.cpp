/**
 * @file encoder.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The implementation of an LDPC encoder.
 */

#include <opencv2/core/mat.hpp>
#include <iostream>
#include <random>

#include "maths.h"
#include "matrices_generator.h"
#include "matrices_io.h"

static std::vector<char> readInputToEnd() {
    std::vector<char> input;
    char c;

    while (std::cin.get(c)) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            input.push_back(c);
        }
    }

    return input;
}

void runEncode(const std::string &matrixPath) {
    auto input = readInputToEnd();
    auto inputLen = static_cast<int>(input.size());
    auto inputBits = inputLen * 8;

    cv::Mat H;

    // Read the parity check matrix from a file if provided, otherwise generate a new one and save
    if (matrixPath.empty()) {
        // Make the seed random
        unsigned int randomSeed;
        try {
            std::random_device randomDevice;
            randomSeed = randomDevice();
        } catch (const std::exception &e) {
            // Not doing cryptography here...
            randomSeed = static_cast<unsigned int>(time(nullptr));
        }

        H = parityCheckMatrix(inputBits * 2, randomSeed);
        saveMatrix(H, "matica.csv");
    } else {
        H = readMatrix(matrixPath);

        if (H.cols != inputBits * 2) {
            std::cerr << "Error: Input size does not match the size of the provided parity check matrix.\n";
            return;
        }
    }

    // Calculate the coding matrix
    cv::Mat G = codingMatrix(H);

    // Create a vector of 0s and 1s from the input
    cv::Mat inputMatrix = cv::Mat::zeros(inputBits, 1, CV_32S);
    for (int i = 0; i < inputBits; ++i) {
        const int byte = i / 8;
        const int bit = 7 - i % 8;

        const int inputBit = (input[byte] & (1 << (bit))) >> bit;
        inputMatrix.at<int>(i, 0) = inputBit;
    }

    // Encode the input bits by multiplying by the coding matrix
    cv::Mat encoded = binaryProduct(G, inputMatrix);

    // The result is a column vector (n_output_bits x 1 matrix), print it
    for (int i = 0; i < encoded.rows; i++) {
        std::cout << encoded.at<int>(i, 0);
    }

    std::cout << std::endl;
}
