/**
 * @file    tree_mesh_builder.cpp
 *
 * @author  Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 *
 * @brief   Parallel Marching Cubes implementation using OpenMP tasks + octree early elimination
 *
 * @date    2022-12-17
 **/

#include <math.h>
#include <limits>
#include <omp.h>
#include <cstring>

#include "tree_mesh_builder.h"

TreeMeshBuilder::TreeMeshBuilder(unsigned gridEdgeSize)
        : BaseMeshBuilder(gridEdgeSize, "Octree") {
}

unsigned TreeMeshBuilder::marchCubes(const ParametricScalarField &field) {
    omp_alloctrait_t traits[2] = {
            {omp_atk_sync_hint, omp_atv_contended},
            {omp_atk_partition, omp_atv_nearest}
    };
    omp_allocator_handle_t allocator = omp_init_allocator(omp_default_mem_space, 2, traits);

    unsigned totalTriangles = 0;
    const auto gridSize = mGridSize; // const it so that it gets copied to threads
    const size_t totalCubesCount = mGridSize * mGridSize * mGridSize;

#pragma omp parallel default(none) shared(allocator, field, totalTriangles)
    {
        int tid = omp_get_thread_num();
        localTriangleStores[tid] = (Triangle_t *) omp_alloc(sizeof(Triangle_t) * totalCubesCount,
                                                            allocator);
#pragma omp barrier // Wait for the threads to initialize their space
#pragma omp single nowait
        totalTriangles = octreeTraverse(gridSize, Vec3_t<float>(), field);
    } // Parallel section end: implicit barrier

    // Merge triangle data produced by each thread
    mergeData(totalTriangles, allocator);
    omp_destroy_allocator(allocator);

    // Return total number of triangles generated.
    return totalTriangles;
}

void TreeMeshBuilder::mergeData(unsigned int totalTriangles,
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

float TreeMeshBuilder::evaluateFieldAt(const Vec3_t<float> &pos, const ParametricScalarField &field) {
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

void TreeMeshBuilder::emitTriangle(const BaseMeshBuilder::Triangle_t &triangle) {
    int tid = omp_get_thread_num();
    memcpy(localTriangleStores[tid] + localTriangleStoresHandles[tid]++,
           &triangle, sizeof(Triangle_t));
}

inline void TreeMeshBuilder::doBlock(unsigned *totalTriangles, const unsigned edgeLen, const Vec3_t<float> &block,
                                     const ParametricScalarField &field,
                                     const unsigned x, const unsigned y, const unsigned z) {
#pragma omp task default(none) shared(block, field, totalTriangles)
    {
        const Vec3_t<float> dividedBlock(block.x + (x * edgeLen),
                                         block.y + (y * edgeLen),
                                         block.z + (z * edgeLen));

        const unsigned divisionResult = octreeTraverse(edgeLen, dividedBlock, field);

#pragma omp atomic update
        *totalTriangles += divisionResult;
    }
}

unsigned TreeMeshBuilder::octreeTraverse(unsigned currentEdgeLen,
                                         const Vec3_t<float> &block, const ParametricScalarField &field) {
    if (currentEdgeLen <= EDGE_LEN_CUTOFF) {
        // "Serial" execution after cutoff
        unsigned totalTriangles = 0;

        for (unsigned x = 0u; x < currentEdgeLen; x++) {
            for (unsigned y = 0u; y < currentEdgeLen; y++) {
                for (unsigned z = 0u; z < currentEdgeLen; z++) {
                    const Vec3_t<float> cubeOffset(block.x * currentEdgeLen + x,
                                                   block.y * currentEdgeLen + y,
                                                   block.z * currentEdgeLen + z);
                    totalTriangles += buildCube(cubeOffset, field);
                }
            }
        }

        return totalTriangles;
    }

    float edgeLenInGridSpace = static_cast<float>(currentEdgeLen) * mGridResolution;
    if (isBlockEmpty(edgeLenInGridSpace, block, field))
        return 0;

    const unsigned divisionEdgeLen = currentEdgeLen >> 1;
    unsigned totalTriangles = 0;

    // TODO: test if doing this has any performance advantage compared to iterating
    // through sc_vertexNormPos. I think (and hope) it doesn't but one never knows
    // when it comes to memory accesses.
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 0, 0, 0);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 0, 0, 1);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 0, 1, 0);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 0, 1, 1);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 1, 0, 0);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 1, 0, 1);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 1, 1, 0);
    doBlock(&totalTriangles, divisionEdgeLen, block, field, 1, 1, 1);

#pragma omp taskwait
    return totalTriangles;
}

inline bool TreeMeshBuilder::isBlockEmpty(const float edgeLen, const Vec3_t<float> &block,
                                          const ParametricScalarField &field) {
    static const float multiplier = sqrt(3.0f) / 2.0f;

    const float halfEdgeLen = edgeLen / 2.0f;

    Vec3_t<float> midPoint(block.x * mGridResolution + halfEdgeLen,
                           block.y * mGridResolution + halfEdgeLen,
                           block.z * mGridResolution + halfEdgeLen);

    return evaluateFieldAt(midPoint, field) > (mIsoLevel + multiplier * edgeLen);
}

const BaseMeshBuilder::Triangle_t *TreeMeshBuilder::getTrianglesArray() const {
    return generatedData;
}
