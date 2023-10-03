/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include "RamseySolver.h"

RamseySolver::RamseySolver()
{
    random_engine = std::default_random_engine { std::random_device{}() };
}
    
MISSolver::node_set RamseySolver::solve(const ogdf::Graph& graph)
{
    MISSolver::node_set nodes(node_comparator);
    for(auto node: graph.nodes)
    {
        nodes.insert(node);
    }
    
    return clique_removal(nodes).second;
}

ogdf::node RamseySolver::choose_random_node(const MISSolver::node_set& nodes)
{
    auto it = nodes.begin();
    std::advance(it, random_engine() % nodes.size());
    return *it;
}

MISSolver::node_set RamseySolver::get_neighbors(const MISSolver::node_set& established_nodes, 
                                                ogdf::node node)
{
    MISSolver::node_set result1(node_comparator);
    MISSolver::node_set result2(node_comparator);
    ogdf::List<ogdf::edge> neighbors;
    node->adjEdges(neighbors);
    
    for(auto edge: neighbors)
    {
        result1.insert(edge->nodes()[0]);
        result1.insert(edge->nodes()[1]);
    }
    
    result1.erase(node);
    
    for(auto node: result1)
    {
        if(established_nodes.find(node) != established_nodes.end())
        {
            result2.insert(node);
        }
    }
    
    return result2;
}
    
MISSolver::node_set RamseySolver::get_neighbors_complement(const MISSolver::node_set& established_nodes, 
                                                           ogdf::node node)
{
    MISSolver::node_set result(node_comparator);
    
    RamseySolver::set_copy(result, established_nodes);
    
    result.erase(node);
    
    auto neighbors = RamseySolver::get_neighbors(established_nodes, node);
    
    for(auto neighbor : neighbors)
    {
        result.erase(neighbor);
    }
    
    return result;
}

std::pair<MISSolver::node_set, MISSolver::node_set> RamseySolver::ramsey(MISSolver::node_set nodes)
{
    //recursion end condition
    if(nodes.empty())
        return std::make_pair<MISSolver::node_set, MISSolver::node_set>({}, {});
    
    auto node = choose_random_node(nodes);
    
    //partition the algorithm for neighbors for currently chosen node
    //this will calculate the independent set + clique used in later portion of the algorithm
    MISSolver::node_set c1(node_comparator);
    MISSolver::node_set i1(node_comparator);
    auto temp1 = ramsey(get_neighbors(nodes, node));
    RamseySolver::set_copy(c1, temp1.first);
    RamseySolver::set_copy(i1, temp1.second);
    c1.insert(node);
    
    //partition the algorithm for everything but neighbors of currently chosen node
    //this will calculate the independent set + clique used in later portion of the algorithm
    MISSolver::node_set c2(node_comparator); 
    MISSolver::node_set i2(node_comparator); 
    auto temp2 = ramsey(get_neighbors_complement(nodes, node));
    RamseySolver::set_copy(c2, temp2.first);
    RamseySolver::set_copy(i2, temp2.second);
    i2.insert(node);
    
    //choose the bigger partitions of independent set + clique
    auto c = c1.size() > c2.size() ? std::move(c1) : std::move(c2);
    auto i = i1.size() > i2.size() ? std::move(i1) : std::move(i2);
    
    return std::make_pair(c, i);
}
    
std::pair<std::vector<MISSolver::node_set>, MISSolver::node_set> RamseySolver::clique_removal(MISSolver::node_set nodes)
{
    //calculate initial guess using ramsey
    auto iterator = ramsey(nodes);
    
    //store the result in convinient data structure
    std::vector<MISSolver::node_set> cliques;
    
    MISSolver::node_set c(node_comparator);
    RamseySolver::set_copy(c, iterator.first);
    
    MISSolver::node_set i(node_comparator);
    RamseySolver::set_copy(i, iterator.second);
    
    //iterate through every node
    while(!nodes.empty())
    {
        //remove currently calculated clique
        MISSolver::node_set new_nodes(node_comparator);
        std::set_difference(nodes.begin(), nodes.end(), iterator.first.begin(), iterator.first.end(), std::inserter(new_nodes, new_nodes.begin()));
        
        nodes = std::move(new_nodes);
        
        //calculate next guess
        iterator = ramsey(nodes);
        
        //if we have found better result for max. independent set -> save it
        if(iterator.second.size() > i.size())
            RamseySolver::set_copy(i, iterator.second);
        
        //save clique
        RamseySolver::set_copy(c, iterator.first);
        cliques.push_back(c);
    }
    
    return std::make_pair(cliques, i);
}
