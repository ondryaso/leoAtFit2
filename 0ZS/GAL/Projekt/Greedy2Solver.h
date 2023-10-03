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

class Greedy2Solver : public MISSolver 
{
public:

    Greedy2Solver()
    {
        
    }
    
    virtual const char* name() override
    {
        return "Greedy2 Solver";
    }
    
    virtual node_set solve(const ogdf::Graph& graph) override
    {
        return greedy2_independent_set(graph);
    }
    
private:

    node_set greedy2_independent_set(const ogdf::Graph& input_graph);
};