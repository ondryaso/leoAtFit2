/**
 * @file BatchMandelCalculator.cc
 * @author Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 * @brief Implementation of Mandelbrot calculator that uses SIMD parallelization over small batches
 * @date 2022-11-16
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <cstdlib>
#include <xmmintrin.h>

constexpr int L3_BLOCK_SIZE = 512;
constexpr int L2_BLOCK_SIZE = 64;

#include "BatchMandelCalculator.h"

BatchMandelCalculator::BatchMandelCalculator(unsigned matrixBaseSize, unsigned limit) :
        BaseMandelCalculator(matrixBaseSize, limit, "BatchMandelCalculator") {
    data = static_cast<uint32_t *>(aligned_alloc(64, height * width * sizeof(uint32_t)));
    zReals = static_cast<float *>(aligned_alloc(64, L2_BLOCK_SIZE * sizeof(float)));
    zImags = static_cast<float *>(aligned_alloc(64, L2_BLOCK_SIZE * sizeof(float)));
}

BatchMandelCalculator::~BatchMandelCalculator() {
    free(data);
    free(zReals);
    free(zImags);

    data = nullptr;
    zReals = nullptr;
    zImags = nullptr;
}

int *BatchMandelCalculator::calculateMandelbrot() {
    uint32_t *const pdata = data;
    float *const zR = zReals;
    float *const zI = zImags;

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

    // BATCH SIZES
    const uint32_t l3_batch_n = L3_BLOCK_SIZE;
    const uint32_t l2_batch_n = L2_BLOCK_SIZE;

    const uint32_t l3_blocks = width / l3_batch_n;
    const uint32_t l2_blocks = l3_batch_n / l2_batch_n;

    float cI = y_start - dy;

    for (uint32_t y = 0; y < half_height; y++) {
        uint32_t *const line_ptr = pdata + y * width;
        cI += dy;

        // Divide the line on two levels: the "L2" number of iterations corresponds to cache line size
        // the "L3" attempts to keep reusing data on a single cache level
        for (uint32_t x_l3 = 0; x_l3 < l3_blocks; x_l3++) {
            for (uint32_t x_l2 = 0; x_l2 < l2_blocks; x_l2++) {

                const uint32_t x_base = x_l3 * l3_batch_n + x_l2 * l2_batch_n;
                uint32_t *const block_ptr = line_ptr + x_base;

                // Prepare data for this block of a line
#pragma omp simd aligned(zR:64, zI:64, block_ptr:64)
                for (uint32_t x_local = 0; x_local < l2_batch_n; x_local++) {
                    zR[x_local] = x_start + (x_base + x_local) * dx;
                    zI[x_local] = cI;
                    block_ptr[x_local] = limit;
                }

                uint32_t iters_done = 0;
                for (uint32_t iter = 0; iters_done < l2_batch_n && iter < limit; iter++) {
#pragma omp simd aligned(zR:64, zI:64, block_ptr:64) reduction(+:iters_done)
                    for (uint32_t x_local = 0; x_local < l2_batch_n; x_local++) {
                        // Surprisingly, using continue here yields the best results...
                        // I guess the compiler does some proper masking magic here
                        if (block_ptr[x_local] != limit) {
                            continue;
                        }

                        float zRi = zR[x_local];
                        float zIi = zI[x_local];

                        float zRSq = zRi * zRi;
                        float zISq = zIi * zIi;

                        if ((zRSq + zISq) > 4.0f) {
                            block_ptr[x_local] = iter;
                            iters_done += 1;
                        }

                        zI[x_local] = 2.0f * zRi * zIi + cI;
                        // I tried pre-calculating the x_start + x * dx values and using them here instead of
                        // computing them in each iteration. When testing locally, it yielded better results (approx.
                        // by 10%), however, on Barbora, it seems to make really no significant difference.
                        zR[x_local] = zRSq - zISq + x_start + (x_base + x_local) * dx;
                    }
                }
            }
        }
    }

    // Copy upper half to lower half
    // I'm thinking how this could be further optimized (maybe by controlling the prefetcher somehow)
    for (uint32_t y = half_height; y < height; y++) {
#pragma omp simd aligned(pdata:64)
        for (uint32_t x = 0; x < width; x++) {
            pdata[y * width + x] = pdata[(height - y - 1) * width + x];
        }
    }

    return reinterpret_cast<int *>(data);
}
