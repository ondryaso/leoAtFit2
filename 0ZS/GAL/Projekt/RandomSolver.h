/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#pragma once

#include "MISSolver.h"

#include <algorithm>
#include <random>

class RandomSolver : public MISSolver {
    
public:

    RandomSolver(size_t num_iter = 5) : m_num_iter(num_iter)
    {
        
    }
    
    virtual const char* name() override
    {
        return "Random Solver";
    }

    virtual node_set solve(const ogdf::Graph& input) override;
    
private:
    
    size_t m_num_iter { 0 };
    
    node_set random_independent_set(const ogdf::Graph& input_graph);
};