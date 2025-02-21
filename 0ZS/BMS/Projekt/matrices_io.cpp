/**
 * @file matrices_io.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The implementation of binary matrix reading and writing functions.
 */

#include <iostream>
#include <fstream>
#include <sstream>

#include "matrices_io.h"

cv::Mat readMatrix(const std::string &filename) {
    using std::string, std::vector, std::ifstream;

    vector<string> lines;
    ifstream file(filename);
    string line;

    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    // Count number of commas in the first line
    int n_cols = 0;
    for (char c: lines[0]) {
        if (c == ',') {
            n_cols++;
        }
    }

    // Trailing comma
    if (lines[0].back() != ',') {
        n_cols++;
    }

    int n_rows = static_cast<int>(lines.size());

    cv::Mat matrix = cv::Mat::zeros(n_rows, n_cols, CV_32S);

    // Read lines and parse into matrix
    for (int i = 0; i < n_rows; ++i) {
        std::stringstream ss(lines[i]);
        string token;
        int j = 0;
        while (std::getline(ss, token, ',')) {
            matrix.at<int>(i, j) = std::stoi(token);
            j++;
        }
    }

    return matrix;
}

void saveMatrix(const cv::Mat &matrix, const std::string &filename) {
    using std::ofstream;

    ofstream file(filename);

    for (int i = 0; i < matrix.rows; ++i) {
        for (int j = 0; j < matrix.cols - 1; ++j) {
            file << matrix.at<int>(i, j) << ",";
        }
        file << matrix.at<int>(i, matrix.cols - 1) << "\n";
    }

    file.flush();
    file.close();
}