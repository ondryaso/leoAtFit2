/**
 * @file matrices_generator.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The implementation of an LDPC parity check matrix generator and coding matrix calculator.
 *
 * The functions are heavily inspired by code from
 * https://github.com/hichamjanati/pyldpc/blob/master/pyldpc/code.py.
 * They were simplified in line with the assignment and adapted to use OpenCV matrices.
 */

#include <random>
#include "matrices_generator.h"

cv::Mat codingMatrix(const cv::Mat &H) {
    int nCode = H.cols;

    // Transpose of H
    cv::Mat H_transposed;
    cv::transpose(H, H_transposed);

    auto [Href_colonnes, tQ] = gaussJordan(H_transposed);

    cv::Mat Href_diag;
    cv::transpose(Href_colonnes, Href_diag);
    Href_diag = gaussJordan(Href_diag).first;

    int nBits = nCode - (int) cv::sum(Href_diag)[0];

    cv::Mat Y = cv::Mat::zeros(nCode, nBits, CV_32S);
    cv::Mat identity = cv::Mat::eye(nBits, nBits, CV_32S);
    identity.copyTo(Y(cv::Rect(0, nCode - nBits, nBits, nBits)));

    cv::Mat Q;
    cv::transpose(tQ, Q);

    cv::Mat tG = binaryProduct(Q, Y);
    return tG;
}

cv::Mat parityCheckMatrix(int nCode, unsigned int seed) {
    int dC = nCode / 2; // Number of bits in a parity-check equation

    // Total number of parity check equations
    int nEquations = nCode - 2; // = nCode * dV / dC = 2*len(in) * (len(in)-1) / len(in)
    cv::Mat H = cv::Mat::zeros(nEquations, nCode, CV_32S);

    // block size = nEquations / dV = 2

    // Fill the first block with dC consecutive ones in each row of the block
    for (int i = 0; i < 2; ++i) {
        for (int j = i * dC; j < (i + 1) * dC; ++j) {
            H.at<int>(i, j) = 1;
        }
    }

    // Each odd row is filled with a random permutation of the first row (half of the bits in the row are 1s,
    // they're randomly scattered). The following row then contains the negation of the previous row.
    std::mt19937 rng(seed);
    for (int i = 2; i < H.rows; i += 2) {
        for (int j = 0; j < dC; ++j) {
            H.at<int>(i, j) = 1;
        }

        auto range = H.rowRange(i, i + 1);
        std::shuffle(range.begin<int>(), range.end<int>(), rng);
        H.rowRange(i + 1, i + 2) = ~range & 1;
    }

    return H;
}
