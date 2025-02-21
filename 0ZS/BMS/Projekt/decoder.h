/**
 * @file decoder.h
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The interface of an LDPC decoder based on the belief propagation algorithm.
 */

#ifndef BMS_DECODER_H
#define BMS_DECODER_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core.hpp>

/**
 * Runs a belief-propagation decoding algorithm for a given input, using a given parity check matrix.
 * Assumes a binary symmetric channel (BSC) with a given bit flip probability.
 * @param Hi The parity check matrix.
 * @param yi The input.
 * @param bitFlipProbability The bit flip probability.
 * @param maxIterations The maximum number of iterations for the algorithm.
 * @return The decoded codeword.
 */
cv::Mat decode(const cv::Mat &Hi, const cv::Mat &yi, double bitFlipProbability = 0.15, int maxIterations = 500);

/**
 * Computes the input message corresponding to a codeword.
 * @param tG A transposed coding matrix.
 * @param x A codeword.
 * @return The input message.
 */
cv::Mat getMessageFromCodeword(const cv::Mat &tG, const cv::Mat &x);

/**
 * Runs the decoder with a given parity check matrix.
 * @param matrixPath A path to the parity check matrix.
 */
void runDecode(const std::string &matrixPath);

#endif //BMS_DECODER_H
