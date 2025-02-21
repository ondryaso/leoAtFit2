/**
 * @file matrices_generator.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The interface of an LDPC parity check matrix generator and coding matrix calculator.
 */

#ifndef BMS_MATRICES_GENERATOR_H
#define BMS_MATRICES_GENERATOR_H

#include <opencv2/core/mat.hpp>
#include "maths.h"

/**
 * Generate a (non-systematic) coding matrix G from a parity check matrix H using Gauss-Jordan elimination.
 * @param H The parity check matrix.
 * @return A transposed coding matrix G.
 */
cv::Mat codingMatrix(const cv::Mat &H);

/**
 * Generate a parity check matrix with the given number of codeword bits.
 * @param nCode The number of codeword bits.
 * @param seed A seed for the random number generator.
 * @return A parity check matrix.
 */
cv::Mat parityCheckMatrix(int nCode, unsigned int seed = 0);

#endif //BMS_MATRICES_GENERATOR_H
