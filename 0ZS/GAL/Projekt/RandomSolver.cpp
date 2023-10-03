/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include "RandomSolver.h"

MISSolver::node_set RandomSolver::solve(const ogdf::Graph& input)
{
    MISSolver::node_set result(node_comparator);
    
    //generate random distribution of nodes in the graph
    //get their independent set and choose result with max nodes
    for(size_t i = 0; i < m_num_iter; i++)
    {
        auto temp = random_independent_set(input);
        
        if(temp.size() > result.size())
        {
            result = temp;
        }
    }
    
    return result;
};

MISSolver::node_set RandomSolver::random_independent_set(const ogdf::Graph& input_graph)
{
    //collect input nodes into a set 
    MISSolver::node_set original_set(node_comparator);
    for(auto node = input_graph.firstNode(); node != nullptr; node = node->succ())
    {
        original_set.insert(node);
    }
    
    //create output maximal indipendent set of nodes
    MISSolver::node_set output_set(node_comparator);
    
    std::vector<size_t> shuffle(input_graph.numberOfNodes());
    std::iota(shuffle.begin(), shuffle.end(), 0);
    auto random_device = std::random_device {};
    auto rng = std::default_random_engine { random_device() };
    std::shuffle(shuffle.begin(), shuffle.end(), rng);
    
    for(auto index = 0; index < shuffle.size(); index++)
    {
        ogdf::node node = nullptr;
        for(auto n : input_graph.nodes)
        {
            if(n->index() == shuffle[index])
            {
                node = n;
                break;
            }
        }
        
        auto inspected_node = original_set.find(node);
        
        //if currently selected node is still in our input set, we can explore it
        if(inspected_node != original_set.end())
        {
            //insert it into the output set
            output_set.insert(*inspected_node);
            //remove it from input set
            original_set.erase(inspected_node);
            
            //remove every adjaced node from the input set
            ogdf::List<ogdf::edge> neighbors;
            (*inspected_node)->adjEdges(neighbors);
            
            for(auto e : neighbors)
            {
                original_set.erase(e->nodes()[0]);
                original_set.erase(e->nodes()[1]);
            }
        }
    }
    
    return output_set;
}