/**
 * @file    loop_mesh_builder.cpp
 *
 * @author  Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 *
 * @brief   Parallel Marching Cubes implementation using OpenMP loops
 *
 * @date    2022-12-03
 **/

#include <math.h>
#include <limits>
#include <omp.h>
#include <cstring>

#include "loop_mesh_builder.h"

LoopMeshBuilder::LoopMeshBuilder(unsigned gridEdgeSize)
        : BaseMeshBuilder(gridEdgeSize, "OpenMP Loop") {

}

unsigned LoopMeshBuilder::marchCubes(const ParametricScalarField &field) {
    // Compute total number of cubes in the grid.
    const unsigned gridSize = mGridSize;
    const size_t totalCubesCount = gridSize * gridSize * gridSize;
    unsigned totalTriangles = 0;

    omp_alloctrait_t traits[2] = {
            {omp_atk_sync_hint, omp_atv_contended},
            {omp_atk_partition, omp_atv_nearest}
    };
    omp_allocator_handle_t allocator = omp_init_allocator(omp_default_mem_space, 2, traits);

#pragma omp parallel default(none) shared(field, allocator, totalTriangles)
    {
        int tid = omp_get_thread_num();
        localTriangleStores[tid] = (Triangle_t *) omp_alloc(sizeof(Triangle_t) * totalCubesCount,
                                                            allocator);

        // Loop over each coordinate in the 3D grid.
#pragma omp for reduction(+:totalTriangles) collapse(3) schedule(nonmonotonic:dynamic,32)//nonmonotonic:dynamic,32)
        for (size_t x = 0u; x < gridSize; x++) {
            for (size_t y = 0u; y < gridSize; y++) {
                for (size_t z = 0u; z < gridSize; z++) {
                    const Vec3_t<float> cubeOffset(x, y, z);
                    totalTriangles += buildCube(cubeOffset, field);
                }
            }
        }
    }

    // Merge triangle data produced by each thread
    mergeData(totalTriangles, allocator);
    omp_destroy_allocator(allocator);

    // Return total number of triangles generated.
    return totalTriangles;
}

void LoopMeshBuilder::mergeData(unsigned int totalTriangles,
                                omp_allocator_handle_t &allocator) {
    if (generatedData != nullptr) {
        free(generatedData);
    }

    generatedData = (Triangle_t *) malloc(sizeof(Triangle_t) * totalTriangles);
    Triangle_t *data = generatedData;

    for (size_t arrIndex = 0; arrIndex < MAX_THREADS; arrIndex++) {
        auto size = localTriangleStoresHandles[arrIndex];
        if (size == 0)
            continue;

        memcpy(data, localTriangleStores[arrIndex], size * sizeof(Triangle_t));
        omp_free(localTriangleStores[arrIndex], allocator);
        data += size;
    }
}

float LoopMeshBuilder::evaluateFieldAt(const Vec3_t<float> &pos, const ParametricScalarField &field) {
    const Vec3_t<float> *pPoints = field.getPoints().data();
    const auto count = unsigned(field.getPoints().size());
    float value = std::numeric_limits<float>::max();

    for (unsigned i = 0; i < count; ++i) {
        float distanceSquared = (pos.x - pPoints[i].x) * (pos.x - pPoints[i].x)
                                + (pos.y - pPoints[i].y) * (pos.y - pPoints[i].y)
                                + (pos.z - pPoints[i].z) * (pos.z - pPoints[i].z);

        if (value > distanceSquared)
            value = distanceSquared;
    }

    return sqrt(value);
}

void LoopMeshBuilder::emitTriangle(const BaseMeshBuilder::Triangle_t &triangle) {
    int tid = omp_get_thread_num();
    memcpy(localTriangleStores[tid] + localTriangleStoresHandles[tid]++,
           &triangle, sizeof(Triangle_t));
}

const BaseMeshBuilder::Triangle_t *LoopMeshBuilder::getTrianglesArray() const {
    return generatedData;
}
