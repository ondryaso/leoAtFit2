/**
 * @file decoder.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The implementation of an LDPC decoder based on the belief propagation algorithm.
 */

#include <vector>
#include <iostream>
#include <bitset>

#include "decoder.h"
#include "maths.h"
#include "matrices_io.h"
#include "matrices_generator.h"

static std::vector<int> readInputToEnd() {
    std::vector<int> input;
    char c;

    while (std::cin.get(c)) {
        if (c == '0' || c == '1') {
            input.push_back(c == '0' ? 0 : 1);
        }
    }

    return input;
}

/**
 * Extracts the indices of the 1s in the parity check matrix H.
 * Used to pre-compute which bits are included in which parity check equation.
 * @param H The parity check matrix.
 * @param bitsHistogram The number of 1s in each row of H.
 * @param bits The indices of the 1s in each row of H.
 * @param nodesHistogram The number of 1s in each column of H.
 * @param nodes The indices of the 1s in each column of H.
 */
static void bitsAndNodes(const cv::Mat &H, std::vector<int> &bitsHistogram,
                         std::vector<int> &bits, std::vector<int> &nodesHistogram,
                         std::vector<int> &nodes) {

    int m = H.rows;
    int n = H.cols;

    std::vector<int> bitsIndices;
    std::vector<int> nodesIndices;

    bitsIndices.reserve(m * n / 2);
    nodesIndices.reserve(m * n / 2);

    bitsHistogram = std::vector<int>(m, 0);
    nodesHistogram = std::vector<int>(n, 0);

    // Iterate over the matrix to find non-zero elements
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            if (H.at<double>(i, j) != 0) {
                // Store the coordinates of the 1 in H
                bitsIndices.push_back(i);
                bitsHistogram[i] += 1;
                bits.push_back(j);
            }
        }
    }

    // Iterate in a transposed way (RIP memory locality)
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < m; ++i) {
            if (H.at<double>(i, j) != 0) {
                // Store the coordinates of the 1 in H
                nodesIndices.push_back(j);
                nodesHistogram[j] += 1;
                nodes.push_back(i);
            }
        }
    }
}

/**
 * Maps the values of a matrix: 0 -> 1, 1 -> -1.
 */
struct ToSignalFunctor {
    void operator()(double &bit, const int *position) const {
        bit = std::pow(-1.0, bit);
    }
};

/**
 * Performs the log belief propagation algorithm. Calculates the a posteriori log likelihood ratios.
 * @param[inout] Lq A matrix used to store the intermediate LLRs.
 * @param[inout] Lr A matrix used to store the extrinsic probabilities.
 * @param[out]   L_posteriori A matrix (column vector) used to store the posteriors.
 * @param[in]    Lc A matrix (column vector) of the apriori LLRs.
 * @param[in]    bitsHist The bitsHist vector given by bitsAndNodes().
 * @param[in]    bitsValues The bitsValues vector given by bitsAndNodes().
 * @param[in]    nodesHist The nodesHist vector given by bitsAndNodes().
 * @param[in]    nodesValues The nodesValues vector given by bitsAndNodes().
 * @param[in]    iteration The current iteration of the algorithm.
 */
void log_belief_propagation(cv::Mat &Lq, cv::Mat &Lr, cv::Mat &L_posteriori, const cv::Mat &Lc,
                            const std::vector<int> &bitsHist, const std::vector<int> &bitsValues,
                            const std::vector<int> &nodesHist, const std::vector<int> &nodesValues,
                            int iteration) {

    int m = Lr.rows;
    int n = Lr.cols;

    // Horizontal step
    int currentBit = 0;
    int currentNode = 0;
    for (int i = 0; i < m; ++i) {
        int ff = bitsHist[i];

        for (int jj = 0; jj < ff; ++jj) {
            int j = bitsValues[currentBit + jj];
            double x = 1;

            // First iteration: read from the input vector
            if (iteration == 0) {
                for (int kk = 0; kk < ff; ++kk) {
                    if (bitsValues[currentBit + kk] != j) {
                        x *= tanh(0.5 * Lc.at<double>(bitsValues[currentBit + kk], 0));
                    }
                }
            } else {
                for (int kk = 0; kk < ff; ++kk) {
                    if (bitsValues[currentBit + kk] != j) {
                        x *= tanh(0.5 * Lq.at<double>(i, bitsValues[currentBit + kk]));
                    }
                }
            }

            double num = 1 + x;
            double denom = 1 - x;

            if (num < 1e-9)
                Lr.at<double>(i, j, 0) = -1;
            else if (denom < 1e-9)
                Lr.at<double>(i, j, 0) = 1;
            else
                Lr.at<double>(i, j, 0) = std::log(num) - std::log(denom);
        }

        currentBit += ff;
    }

    // Vertical step
    for (int j = 0; j < n; ++j) {
        int ff = nodesHist[j];
        for (int ii = 0; ii < ff; ++ii) {
            int i = nodesValues[currentNode + ii];
            Lq.at<double>(i, j) = Lc.at<double>(j);

            for (int kk = 0; kk < ff; ++kk) {
                if (nodesValues[currentNode + kk] != i) {
                    Lq.at<double>(i, j) += Lr.at<double>(nodesValues[currentNode + kk], j);
                }
            }
        }

        currentNode += ff;
    }

    // LLR a posteriori
    currentNode = 0;
    L_posteriori = cv::Mat::zeros(n, 1, CV_64F);

    for (int j = 0; j < n; ++j) {
        int ff = nodesHist[j];
        std::vector<int> mj(nodesValues.begin() + currentNode,
                            nodesValues.begin() + currentNode + ff);
        currentNode += ff;

        double sum = Lc.at<double>(j, 0);
        for (int index: mj) {
            sum += Lr.at<double>(index, j, 0);
        }

        L_posteriori.at<double>(j, 0) = sum;
    }
}

/**
 * Checks if x is a codeword in code given by a parity check matrix H.
 * @param H The parity check matrix.
 * @param x The codeword (as a column vector).
 */
bool isCodeWord(const cv::Mat &H, const cv::Mat &x) {
    cv::Mat product = binaryProductFloat(H, x);
    return cv::countNonZero(product) == 0;
}

cv::Mat decode(const cv::Mat &Hi, const cv::Mat &yi, double bitFlipProbability, int maxIterations) {
    cv::Mat H, y;

    // The calculations are done on doubles (OpenCV cannot do multiplication on integers)
    Hi.convertTo(H, CV_64F);
    yi.convertTo(y, CV_64F);

    int m = H.rows, n = H.cols;

    std::vector<int> bitsHist, bitsValues, nodesHist, nodesValues;
    bitsAndNodes(H, bitsHist, bitsValues, nodesHist, nodesValues);

    // Assuming a BSC channel, the log likelihood ratios for the apriori message probabilities are
    // log(p/(1-p)) if the received bit is 1 and log((1-p)/p) if the received bit is 0.
    double var = -std::log(bitFlipProbability / (1 - bitFlipProbability));
    // y = (-1)^y
    y.forEach<double>(ToSignalFunctor());
    // apriori probs are Lc = y * var
    cv::Mat Lc = y * var;

    cv::Mat Lq = cv::Mat::zeros(m, n, CV_64F);
    cv::Mat Lr = cv::Mat::zeros(m, n, CV_64F);
    cv::Mat x;

    int iteration = 0;
    for (; iteration < maxIterations; ++iteration) {
        cv::Mat L_posteriori;
        log_belief_propagation(Lq, Lr, L_posteriori, Lc, bitsHist, bitsValues,
                               nodesHist, nodesValues, iteration);

        cv::Mat posteriorBits = (L_posteriori <= 0);

        posteriorBits.convertTo(x, CV_64F);
        x = cv::min(1.0, x);

        if (isCodeWord(H, x)) {
            break;
        }
    }

    if (iteration == maxIterations - 1) {
        std::cerr << "Decoding stopped before convergence." << std::endl;
    }

    cv::Mat x32S;
    x.convertTo(x32S, CV_32S);
    return x32S;
}

cv::Mat getMessageFromCodeword(const cv::Mat &tG, const cv::Mat &x) {
    // Implements pyldpc'S get_message:
    // https://github.com/hichamjanati/pyldpc/blob/a821ccd1eb3a13b8a0f66ebba8d9923ce2f528ef/pyldpc/decoder.py#L186

    int k = tG.cols;

    cv::Mat rtG = tG.clone();
    cv::Mat rx = x.clone();

    gaussElimination(rtG, rx);

    cv::Mat message = cv::Mat::zeros(1, k, CV_32S);

    message.at<int>(0, k - 1) = rx.at<int>(k - 1);

    for (int i = k - 2; i >= 0; --i) {
        message.at<int>(0, i) = rx.at<int>(i);

        auto rtGRow = rtG.row(i).colRange(i + 1, k);
        auto msgCol = message.colRange(i + 1, k).t();
        cv::Mat product = binaryProduct(rtGRow, msgCol);

        message.at<int>(0, i) -= product.at<int>(0, 0);
    }

    return abs(message);
}

void runDecode(const std::string &matrixPath) {
    auto input = readInputToEnd();
    auto inputBits = static_cast<int>(input.size());
    cv::Mat H = readMatrix(matrixPath);

    if (H.cols != inputBits) {
        std::cerr << "Error: Input size does not match the size of the provided parity check matrix.\n";
        return;
    }

    cv::Mat G = codingMatrix(H);
    auto inputMat = cv::Mat(input);
    // Run the belief propagation algorithm
    cv::Mat d = decode(H, inputMat);
    // Decode the resulting codeword
    cv::Mat y = getMessageFromCodeword(G, d);

    if (y.cols % 8 != 0) {
        std::cerr << "The number of decoded bits is not a multiple of 8." << std::endl;
    }

    // Compose the bits into bytes and print as characters
    for (int i = 0; i < y.cols; i += 8) {
        if (i + 8 > y.cols) {
            break;
        }

        std::bitset<8> byte;
        for (int j = 0; j < 8; ++j) {
            byte[7 - j] = y.at<int>(0, i + j);
        }
        char c = static_cast<char>(byte.to_ulong());
        std::cout << c;
    }

    std::cout << std::endl;
}
