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

class GreedySolver : public MISSolver {
    
public:

    GreedySolver()
    {
        
    }
    
    virtual const char* name() override
    {
        return "Greedy Solver";
    }

    virtual node_set solve(const ogdf::Graph& input) override
    {
        return greedy_independent_set(input);
    };
    
private:
    
    node_set greedy_independent_set(const ogdf::Graph& input_graph);
};