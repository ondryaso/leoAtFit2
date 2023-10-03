/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include "WheelFreeRamseySolver.h"

WheelFreeRamseySolver::WheelFreeRamseySolver()
{
    random_engine = std::default_random_engine { std::random_device{}() };
}

MISSolver::node_set WheelFreeRamseySolver::solve(const ogdf::Graph& graph)
{
    
    MISSolver::node_set nodes(node_comparator);
    for(auto node: graph.nodes)
    {
        nodes.insert(node);
    }
    
    return wheel_free_ramsey(graph, nodes);
}

ogdf::node WheelFreeRamseySolver::choose_random_node(const MISSolver::node_set& nodes)
{
    auto it = nodes.begin();
    std::advance(it, random_engine() % nodes.size());
    return *it;
}
    
MISSolver::node_set WheelFreeRamseySolver::get_neighbors(const MISSolver::node_set& established_nodes, 
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
    
MISSolver::node_set WheelFreeRamseySolver::get_neighbors_complement(const MISSolver::node_set& established_nodes, 
                                                                    ogdf::node node)
{
    MISSolver::node_set result(node_comparator);
    
    WheelFreeRamseySolver::set_copy(result, established_nodes);
    
    result.erase(node);
    
    auto neighbors = get_neighbors(established_nodes, node);
    
    for(auto neighbor : neighbors)
    {
        result.erase(neighbor);
    }
    
    return result;
}
    
MISSolver::node_set WheelFreeRamseySolver::wheel_free_ramsey(const ogdf::Graph& graph, MISSolver::node_set nodes)
{
    //check if currently selected node form a bipartite graph
    auto reduced_graph = WheelFreeRamseySolver::graph_and_nodes_union(graph, nodes);
    ogdf::NodeArray<bool> colors(reduced_graph);
    if(ogdf::isBipartite(reduced_graph, colors))
    {
        //if graph is bipartite, that means there are 2 independent sets
        MISSolver::node_set first_set(node_comparator);
        MISSolver::node_set second_set(node_comparator);
        
        for(auto reduced_node : reduced_graph.nodes)
        {
            for(auto node : graph.nodes)
            {
                if(node->index() == reduced_node->index())
                {
                    if(colors[reduced_node])
                    {
                        first_set.insert(node);
                    }
                    else
                    {
                        second_set.insert(node);
                    }
                }
            }
        }
        
        //choose the bigger independent set
        return first_set.size() > second_set.size() ? first_set : second_set;
    }
    
    auto node = choose_random_node(nodes);
    
    //partition the algorithm for neighbors of currently chosen node
    MISSolver::node_set i1(node_comparator);
    auto temp1 = wheel_free_ramsey(graph, get_neighbors(nodes, node));
    WheelFreeRamseySolver::set_copy(i1, temp1);
    
    //partition the algorithm for everything but neighbors of currently chosen node
    MISSolver::node_set i2(node_comparator);
    auto temp2 = wheel_free_ramsey(graph, get_neighbors_complement(nodes, node));
    WheelFreeRamseySolver::set_copy(i2, temp2);
    i2.insert(node);
    
    //choose the bigger partition
    return i1.size() > i2.size() ? i1 : i2;
}