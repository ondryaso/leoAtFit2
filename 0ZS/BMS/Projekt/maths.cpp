/**
 * @file maths.cpp
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-12-10
 * @brief The implementation of several mathematical operations used in the LDPC encoder/decoder.
 *
 * The gaussJordan() and gaussElimination() functions are heavily inspired by code from
 * https://github.com/hichamjanati/pyldpc/blob/master/pyldpc/utils.py.
 */

#include "maths.h"

cv::Mat binaryProductFloat(const cv::Mat &X, const cv::Mat &Y) {
    cv::Mat result = X * Y;

    cv::Mat result32S;
    result.convertTo(result32S, CV_32S);

    result32S = result32S & 1; // Modulo 2 operation
    return result32S;
}

cv::Mat binaryProduct(const cv::Mat &X, const cv::Mat &Y) {
    // OpenCV cannot multiply integer matrices, so convert both to float.
    cv::Mat X32F, Y32F;
    X.convertTo(X32F, CV_32F);
    Y.convertTo(Y32F, CV_32F);

    cv::Mat result = X32F * Y32F;

    cv::Mat result32S;
    result.convertTo(result32S, CV_32S);

    result32S = result32S & 1; // Modulo 2 operation
    return result32S;
}

std::pair<cv::Mat, cv::Mat> gaussJordan(const cv::Mat &X) {
    cv::Mat A = X.clone(); // Make a working matrix
    int m = A.rows, n = A.cols;
    cv::Mat P = cv::Mat::eye(m, m, CV_32S);

    int pivot_old = -1;
    // Iterate over each column of the working matrix
    for (int j = 0; j < n; ++j) {
        // Find pivot (the first row with a 1 in the current column)
        int pivot = pivot_old + 1;
        for (int i = pivot; i < m; ++i) {
            if (A.at<int>(i, j) == 1) {
                pivot = i;
                break;
            }
        }

        if (A.at<int>(pivot, j)) {
            // Swap the rows to bring the pivot to the diagonal position
            pivot_old++;
            if (pivot_old != pivot) {
                cv::Mat temp = A.row(pivot).clone();
                A.row(pivot) = A.row(pivot_old) + 0;
                A.row(pivot_old) = temp + 0;

                // Apply the same on the permutation matrix P
                temp = P.row(pivot).clone();
                P.row(pivot) = P.row(pivot_old) + 0;
                P.row(pivot_old) = temp + 0;
            }

            // Clear all row in the column using the pivot row
            for (int i = 0; i < m; ++i) {
                if (i != pivot_old && A.at<int>(i, j) == 1) {
                    A.row(i) = abs(A.row(i) - A.row(pivot_old));
                    // Apply the same on the permutation matrix P
                    P.row(i) = abs(P.row(i) - P.row(pivot_old));
                }
            }
        }

        if (pivot_old == m - 1) {
            break;
        }
    }

    return {A, P};
}

void gaussElimination(cv::Mat &A, cv::Mat &b) {
    int n = A.rows;
    for (int i = 0; i < n; ++i) {
        // Find pivot
        int pivot = -1;
        for (int row = i; row < n; ++row) {
            if (A.at<int>(row, i) == 1) {
                pivot = row;
                break;
            }
        }
        if (pivot == -1) continue; // No pivot, skip column

        // Swap rows
        if (pivot != i) {
            cv::Mat temp;
            A.row(i).copyTo(temp);
            A.row(pivot).copyTo(A.row(i));
            temp.copyTo(A.row(pivot));

            uchar tempVal = b.at<int>(i);
            b.at<int>(i) = b.at<int>(pivot);
            b.at<int>(pivot) = tempVal;
        }

        // Eliminate
        for (int row = 0; row < n; ++row) {
            if (row != i && A.at<int>(row, i) == 1) {
                for (int col = 0; col < A.cols; ++col) {
                    A.at<int>(row, col) ^= A.at<int>(i, col);
                }
                b.at<int>(row) ^= b.at<int>(i);
            }
        }
    }
}