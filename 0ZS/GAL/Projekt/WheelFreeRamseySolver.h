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

class WheelFreeRamseySolver : public MISSolver 
{
public:

    WheelFreeRamseySolver();
    
    virtual const char* name() override
    {
        return "Wheel Free Ramsey Solver";
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

    static ogdf::Graph graph_and_nodes_union(const ogdf::Graph& graph, const node_set& nodes)
    {
        ogdf::Graph result = graph;
        for(auto node : graph.nodes)
        {
            if(nodes.find(node) == nodes.end())
            {
                for(auto res_node : result.nodes)
                {
                    if(res_node->index() == node->index())
                    {
                        result.delNode(res_node);
                        break;
                    }
                }
            }
        }
        return result;
    }
    
    ogdf::node choose_random_node(const node_set& nodes);
    node_set get_neighbors(const node_set& established_nodes, ogdf::node node);
    node_set get_neighbors_complement(const node_set& established_nodes, ogdf::node node);
    node_set wheel_free_ramsey(const ogdf::Graph& graph, node_set nodes);
    
};