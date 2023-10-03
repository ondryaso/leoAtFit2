/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include "Greedy2Solver.h"

#include <queue>

MISSolver::node_set Greedy2Solver::greedy2_independent_set(const ogdf::Graph& input_graph)
{
    auto degree_comparator = [](const std::pair<ogdf::node, ogdf::node>& a, const std::pair<ogdf::node, ogdf::node>& b) {
        return ((a.first->degree() + a.second->degree()) / 2) >
               ((b.first->degree() + b.second->degree()) / 2);
    };
    
    std::priority_queue<std::pair<ogdf::node, ogdf::node>, 
                        std::vector<std::pair<ogdf::node, ogdf::node>>,
                        decltype(degree_comparator)> original_queue(degree_comparator);
    node_set working_set(node_comparator);
    node_set output_set(node_comparator);        
    
    //enumerate pairs of 2 independent nodes
    for(auto first_node = input_graph.firstNode(); first_node; first_node = first_node->succ())
    {
        for(auto second_node = first_node->succ(); second_node; second_node = second_node->succ())
        {
            ogdf::List<ogdf::edge> neighbors;
            second_node->adjEdges(neighbors);
            
            //check if pair of nodes are independent
            bool independent = true;
            for(auto edge: neighbors)
            {
                if(edge->nodes()[0] == first_node || 
                   edge->nodes()[1] == first_node) 
                { 
                    independent = false; 
                    break; 
                }
            }
            
            //if they are independent, sort them by their degree density
            if(independent)
                original_queue.push(std::make_pair(first_node, second_node));
        }
        
        working_set.insert(first_node);
    }
    
    while(!original_queue.empty())
    {
        //get pair of independent nodes with smallest degree density
        auto independent_node_pair = original_queue.top();
        original_queue.pop();
        
        auto first_node = independent_node_pair.first;
        auto second_node = independent_node_pair.second;
        
        //process both nodes
        auto process_node = [&](ogdf::node node)
        {
            if(working_set.find(node) == working_set.end())
                return;
            
            //add node into the solution
            output_set.insert(node);
            
            ogdf::List<ogdf::edge> neighbors;
            node->adjEdges(neighbors);
        
            //remove node and its' neighbors from explorable set
            for(auto edge: neighbors)
            {
                working_set.erase(edge->nodes()[0]);
                working_set.erase(edge->nodes()[1]);
            }
        };
        
        process_node(first_node);
        process_node(second_node);
    }
    
    return output_set;
}