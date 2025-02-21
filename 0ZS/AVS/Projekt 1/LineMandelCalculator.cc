/**
 * @file LineMandelCalculator.cc
 * @author Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 * @brief Implementation of Mandelbrot calculator that uses SIMD parallelization over lines
 * @date 2022-11-12
 */
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <cstdlib>
#include <immintrin.h>


#include "LineMandelCalculator.h"


LineMandelCalculator::LineMandelCalculator(unsigned matrixBaseSize, unsigned limit) :
        BaseMandelCalculator(matrixBaseSize, limit, "LineMandelCalculator") {
    data = static_cast<uint32_t *>(aligned_alloc(64, height * width * sizeof(uint32_t)));
    zReals = static_cast<float *>(aligned_alloc(64, width * sizeof(float)));
    zImags = static_cast<float *>(aligned_alloc(64, width * sizeof(float)));
}

LineMandelCalculator::~LineMandelCalculator() {
    free(data);
    free(zReals);
    free(zImags);

    data = nullptr;
    zReals = nullptr;
    zImags = nullptr;
}

int *LineMandelCalculator::calculateMandelbrot() {
    uint32_t *pdata = data;
    float *zR = zReals;
    float *zI = zImags;

#if defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER)
    __assume_aligned(pdata, 64);
    __assume_aligned(zR, 64);
    __assume_aligned(zI, 64);
#endif

    const auto x_start = static_cast<float>(this->x_start);
    const auto y_start = static_cast<float>(this->y_start);
    const auto dx = static_cast<float>(this->dx);
    const auto dy = static_cast<float>(this->dy);

    const uint32_t limit = this->limit;
    const uint32_t width = this->width;
    const uint32_t height = this->height;
    const uint32_t half_height = height >> 1;

    for (uint32_t y = 0; y < half_height; y++) {
        float cI = y_start + y * dy;

        // Prepare data for this block of a line
#pragma omp simd aligned(zR:64, zI:64, pdata:64)
        for (uint32_t x = 0; x < width; x++) {
            zR[x] = x_start + x * dx;
            zI[x] = cI;
            pdata[x] = limit;
        }

        uint32_t iters_done = 0;
        for (uint32_t iter = 0; iters_done < width && iter < limit; iter++) {
            uint32_t *data_row = pdata;

            float *zR_row = zR;
            float *zI_row = zI;

#pragma omp simd aligned(zR:64, zI:64, pdata:64, data_row:64) reduction(+:iters_done)
            for (uint32_t x = 0; x < width; x++, data_row++, zR_row++, zI_row++) {
                float zRi = *zR_row;
                float zIi = *zI_row;

                float zRSq = zRi * zRi;
                float zISq = zIi * zIi;

                const bool cond = (zRSq + zISq) > 4.0f && *data_row == limit;
                if (cond) {
                    *data_row = iter;
                    iters_done += 1;
                }

                *zI_row = 2.0f * zRi * zIi + cI;
                // It would be better if we could get rid of the uint->float conversion here but that would mean
                // using a counting variable similar to cI that would introduce a strong data dependency between
                // iterations (it doesn't matter in case of cI because the y-iterations are really long anyway).
                *zR_row = zRSq - zISq + x_start + x * dx;
            }
        }

        pdata += width;
    }

    pdata = data;
    for (uint32_t y = half_height; y < height; y++) {
#pragma omp simd aligned(pdata:64)
        for (uint32_t x = 0; x < width; x++) {
            pdata[y * width + x] = pdata[(height - y - 1) * width + x];
        }
    }

    return reinterpret_cast<int *>(data);
}
