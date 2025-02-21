/**
 * @file matrices_generator.h
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The interface of binary matrix reading and writing functions.
 */

#ifndef BMS_MATRICES_IO_H
#define BMS_MATRICES_IO_H

#include <opencv2/core/mat.hpp>

/**
 * Prints a matrix to the standard output.
 * @tparam T The type of the matrix elements.
 * @param matrix The matrix.
 */
template<typename T = int>
void printMatrix(const cv::Mat &matrix) {
    for (int i = 0; i < matrix.rows; ++i) {
        std::cout << "[";
        for (int j = 0; j < matrix.cols; ++j) {
            std::cout << matrix.at<T>(i, j);
            if (j < matrix.cols - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]\n";
    }
}

/**
 * Reads a matrix from a CSV file.
 * @param filename A path to the CSV file.
 * @return The matrix read from the CSV file.
 */
cv::Mat readMatrix(const std::string &filename);

/**
 * Saves a matrix to a CSV file.
 * @param matrix The matrix to save.
 * @param filename A path to the CSV file.
 */
void saveMatrix(const cv::Mat &matrix, const std::string &filename);

#endif //BMS_MATRICES_IO_H
