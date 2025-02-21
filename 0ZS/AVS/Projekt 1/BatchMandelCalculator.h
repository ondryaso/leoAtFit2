/**
 * @file BatchMandelCalculator.h
 * @author Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 * @brief Implementation of Mandelbrot calculator that uses SIMD parallelization over small batches
 * @date 2022-11-16
 */
#ifndef BATCHMANDELCALCULATOR_H
#define BATCHMANDELCALCULATOR_H

#include <BaseMandelCalculator.h>

class BatchMandelCalculator : public BaseMandelCalculator {
public:
    BatchMandelCalculator(unsigned matrixBaseSize, unsigned limit);

    ~BatchMandelCalculator();

    int *calculateMandelbrot();
private:
    uint32_t *data;
    float *zReals;
    float *zImags;
    float *cReals;
};

#endif