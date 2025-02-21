/**
 * @file    loop_mesh_builder.h
 *
 * @author  Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 *
 * @brief   Parallel Marching Cubes implementation using OpenMP loops
 *
 * @date    2022-12-03
 **/

#ifndef LOOP_MESH_BUILDER_H
#define LOOP_MESH_BUILDER_H

#include <vector>
#include "base_mesh_builder.h"

class LoopMeshBuilder : public BaseMeshBuilder {
public:
    explicit LoopMeshBuilder(unsigned gridEdgeSize);

    static constexpr unsigned MAX_THREADS = 128;

protected:
    unsigned marchCubes(const ParametricScalarField &field) override;

    float evaluateFieldAt(const Vec3_t<float> &pos, const ParametricScalarField &field) override;

    void emitTriangle(const Triangle_t &triangle) override;

    const Triangle_t *getTrianglesArray() const override;

private:
    Triangle_t *localTriangleStores[MAX_THREADS] = {}; // array size here must be >= number of threads
    size_t localTriangleStoresHandles[MAX_THREADS] = {};
    Triangle_t *generatedData{};

    void mergeData(unsigned int totalTriangles, omp_allocator_handle_t &allocator);
};

#endif // LOOP_MESH_BUILDER_H
