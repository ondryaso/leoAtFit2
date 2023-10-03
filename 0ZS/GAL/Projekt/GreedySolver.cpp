/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include "GreedySolver.h"

#include <queue>

MISSolver::node_set GreedySolver::greedy_independent_set(const ogdf::Graph& input_graph)
{
    //collect input nodes into a set 
    auto degree_comparator = [](const ogdf::node& a, const ogdf::node& b)
    {
        return a->degree() > b->degree();
    };
    
    std::priority_queue<ogdf::node, std::vector<ogdf::node>, decltype(degree_comparator)> original_queue(degree_comparator);
    node_set working_set(MISSolver::node_comparator);
    node_set output_set(MISSolver::node_comparator);
    
    for(auto node : input_graph.nodes)
    {
        //sort nodes by they degree
        original_queue.push(node);
        
        //set containing explorable nodes
        working_set.insert(node);
    }
    
    while(!original_queue.empty())
    {
        //get node with the smallest degree
        auto node = original_queue.top();
        original_queue.pop();
        
        if(working_set.find(node) == working_set.end())
            continue;
        
        //add node into the solution
        output_set.insert(node);
        
        ogdf::List<ogdf::edge> neighbors;
        node->adjEdges(neighbors);
        
        //remove node and its' neigbors from explorable set
        for(auto edge: neighbors)
        {
            working_set.erase(edge->nodes()[0]);
            working_set.erase(edge->nodes()[1]);
        }
    }
    
    return output_set;
}