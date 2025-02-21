/**
 * @file LineMandelCalculator.h
 * @author Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 * @brief Implementation of Mandelbrot calculator that uses SIMD parallelization over lines
 * @date 2022-11-12
 */

#include <BaseMandelCalculator.h>

class LineMandelCalculator : public BaseMandelCalculator {
public:
    LineMandelCalculator(unsigned matrixBaseSize, unsigned limit);

    ~LineMandelCalculator();

    int *calculateMandelbrot();

private:
    uint32_t *data;
    float *zReals;
    float *zImags;
};