/**
 * @file    tree_mesh_builder.h
 *
 * @author  Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
 *
 * @brief   Parallel Marching Cubes implementation using OpenMP tasks + octree early elimination
 *
 * @date    2022-12-17
 **/

#ifndef TREE_MESH_BUILDER_H
#define TREE_MESH_BUILDER_H

#include "base_mesh_builder.h"

class TreeMeshBuilder : public BaseMeshBuilder {
public:
    static constexpr unsigned EDGE_LEN_CUTOFF = 1;
    static constexpr unsigned MAX_THREADS = 128;

    explicit TreeMeshBuilder(unsigned gridEdgeSize);

protected:
    unsigned marchCubes(const ParametricScalarField &field) override;

    float evaluateFieldAt(const Vec3_t<float> &pos, const ParametricScalarField &field) override;

    void emitTriangle(const Triangle_t &triangle) override;

    const Triangle_t *getTrianglesArray() const override;

    unsigned octreeTraverse(unsigned currentEdgeLen, const Vec3_t<float> &block, const ParametricScalarField &field);

    inline bool isBlockEmpty(const float edgeLen, const Vec3_t<float> &block, const ParametricScalarField &field);

private:
    Triangle_t *localTriangleStores[MAX_THREADS] = {}; // array size here must be >= number of threads
    size_t localTriangleStoresHandles[MAX_THREADS] = {};
    Triangle_t *generatedData{};

    void mergeData(unsigned int totalTriangles, omp_allocator_handle_t &allocator);

    inline void doBlock(unsigned int *totalTriangles, const unsigned edgeLen, const Vec3_t<float> &block,
                        const ParametricScalarField &field, unsigned int x, unsigned int y, unsigned int z);
};

#endif // TREE_MESH_BUILDER_H
