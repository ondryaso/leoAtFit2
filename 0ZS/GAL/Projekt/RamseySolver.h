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

#include <ogdf/basic/simple_graph_alg.h>

class RamseySolver : public MISSolver 
{
public:

    RamseySolver();
    
    virtual const char* name() override
    {
        return "Ramsey Solver";
    }
    
    virtual node_set solve(const ogdf::Graph& graph) override;
    
private:

    std::default_random_engine random_engine;
    
    /**
     * the reason why this function is here is because c++17 is dumb...
     * you cannot copy sets with custom comparers using '='
     * well... you can, but then the comparator function doesn't work
     * because the comparator function won't be copied into the new set :/
     */
    static void set_copy(node_set& dest, const node_set& source)
    {
        dest.clear();
        
        for(auto node : source)
        {
            dest.insert(node);
        }
    }

    ogdf::node choose_random_node(const node_set& nodes);
    node_set get_neighbors(const node_set& established_nodes, ogdf::node node);
    node_set get_neighbors_complement(const node_set& established_nodes, ogdf::node node);
    std::pair<node_set, node_set> ramsey(node_set nodes);
    std::pair<std::vector<node_set>, node_set> clique_removal(node_set nodes);
    
};