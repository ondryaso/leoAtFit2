/**
 * @file   Utils.cpp
 * @brief  Utility methods definitions.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include "Utils.h"

const int inverses[] = {-1, 1, -1, 9, -1, 21, -1, 15, -1, 3, -1, 19, -1,
                        -1, -1, 7, -1, 23, -1, 11, -1, 5, -1, 17, -1, 25};

int multiplicativeInverse(int a) {
    return inverses[a % 26];
}