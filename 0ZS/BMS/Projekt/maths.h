/**
 * @file maths.h
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The interface of several mathematical operations used in the LDPC encoder/decoder.
 */

#ifndef BMS_MATHS_H
#define BMS_MATHS_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core.hpp>

/**
 * Calculate a matrix product in the binary field Z/2Z.
 * This method can be used on matrices of any type.
 * @param X The first matrix.
 * @param Y The second matrix.
 * @return The matrix product of X and Y in the binary field Z/2Z. The matrix is of type CV_32S.
 */
cv::Mat binaryProduct(const cv::Mat &X, const cv::Mat &Y);

/**
 * Calculate a matrix product in the binary field Z/2Z.
 * This method can must be used on float matrices for which OpenCV supports matrix multiplication.
 * Both matrices must be of the same type.
 * @param X The first matrix.
 * @param Y The second matrix.
 * @return The matrix product of X and Y in the binary field Z/2Z. The matrix is of type CV_32S.
 */
cv::Mat binaryProductFloat(const cv::Mat &X, const cv::Mat &Y);

/**
 * Perform the Gauss-Jordan elimination on a binary matrix.
 * @param X The matrix to perform Gauss-Jordan elimination on.
 * @return A pair of matrices (A, P) where A is the reduced row echelon form of X; and P is the permutation matrix.
 */
std::pair<cv::Mat, cv::Mat> gaussJordan(const cv::Mat &X);

/**
 * Solve a linear system in the binary field Z/2Z using Gaussian elimination.
 * The elimination is performed in-place on the input matrices.
 * @param A The matrix of coefficients.
 * @param b The vector of constants.
 */
void gaussElimination(cv::Mat &A, cv::Mat &b);

#endif //BMS_MATHS_H
