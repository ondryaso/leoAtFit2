/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include <iostream>
#include <queue>
#include <memory>
#include <filesystem>
#include <chrono>

#include <ogdf/basic/graph_generators.h>
#include <ogdf/fileformats/GraphIO.h>

#include "RandomSolver.h"
#include "GreedySolver.h"
#include "Greedy2Solver.h"
#include "RamseySolver.h"
#include "WheelFreeRamseySolver.h"
#include "BMASolver.h"

#define STAT_OUT 1

bool test_independency(const ogdf::Graph& graph, const MISSolver::node_set& nodes)
{
    for (auto first_node: nodes)
    {
        for (auto second_node: nodes)
        {
            if (first_node != second_node)
            {
                for (auto edge: graph.edges)
                {
                    if ((edge->nodes()[0] == first_node && edge->nodes()[1] == second_node) ||
                        (edge->nodes()[0] == second_node && edge->nodes()[1] == first_node))
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

template<class Solver, bool isLast = false>
void solve(const ogdf::Graph& graph)
{
    using namespace std::chrono;

    auto solver = std::make_unique<Solver>();

    auto start = high_resolution_clock::now();
    auto result = solver->solve(graph);
    auto end = high_resolution_clock::now();
    auto timeDelta = duration_cast<milliseconds>(end - start).count();

#if STAT_OUT
    std::cout << result.size() << ',' << (test_independency(graph, result) ? "1," : "0,")
              << timeDelta;
    if (isLast)
        std::cout << std::endl;
    else
        std::cout << ',';
#else
    std::cout << "    " << solver->name() << ": " << result.size() << " - "
              << (test_independency(graph, result) ? "success" : "fail") <<
              " in " << timeDelta << " ms" << std::endl;
#endif
}

void test_random_graph(size_t num_nodes, size_t num_edges)
{
    ogdf::Graph graph;
    ogdf::randomSimpleGraph(graph, num_nodes, num_edges);
    solve<RandomSolver>(graph);
    solve<GreedySolver>(graph);
    solve<Greedy2Solver>(graph);
    solve<RamseySolver>(graph);
    solve<WheelFreeRamseySolver>(graph);
}

ogdf::Graph graph_complement(const ogdf::Graph& graph)
{
    ogdf::Graph result = graph;
    while (result.numberOfEdges() != 0)
    {
        result.delEdge(result.firstEdge());
    }

    for (auto first_node: graph.nodes)
    {
        for (auto second_node: graph.nodes)
        {
            auto edge = graph.searchEdge(first_node, second_node);

            if (!edge)
            {
                for (auto res_first_node: result.nodes)
                {
                    for (auto res_second_node: result.nodes)
                    {
                        if (res_first_node->index() == first_node->index() &&
                            res_second_node->index() == second_node->index())
                        {
                            result.newEdge(res_first_node, res_second_node);
                        }
                    }
                }
            }
        }
    }

    return result;
}

void test_benchmark_graph(const std::string& path, const std::string& name)
{
    std::ifstream file = std::ifstream(path, std::ios::in);
    if (!file.is_open())
        throw std::runtime_error(std::string("read_matrix_market: couldn't open ") + path);

    ogdf::Graph clique_benchmark_graph;
    if (!ogdf::GraphIO::readMatrixMarket(clique_benchmark_graph, file))
        return;

    std::cerr << "Generating graph complement for '" << path << "'..." << std::endl;
    auto mis_benchmark_graph = graph_complement(clique_benchmark_graph);

#if STAT_OUT
    std::cout << '"' << name << "\"," << mis_benchmark_graph.numberOfNodes() << ','
              << mis_benchmark_graph.numberOfEdges() << ',';
#else
    std::cout << "Testing: '" << path << "' - <nodes: " << mis_benchmark_graph.numberOfNodes() << ", edges: "
              << mis_benchmark_graph.numberOfEdges() << ">" << std::endl;
#endif
    solve<RandomSolver>(mis_benchmark_graph);
    solve<GreedySolver>(mis_benchmark_graph);
    solve<Greedy2Solver>(mis_benchmark_graph);
    solve<RamseySolver>(mis_benchmark_graph);
    solve<WheelFreeRamseySolver>(mis_benchmark_graph);
    solve<BMASolver, true>(mis_benchmark_graph);

}

int main(int argc, char* argv[])
{
    std::string benchmark_path = "../dimacs_benchmark";

#if STAT_OUT
    std::cout
            << "name,nodes,edges,rnd_i,rnd_ok,rnd_time,gr1_i,gr1_ok,gr1_time,gr2_i,gr2_ok,gr2_time,ram_i,ram_ok,ram_time,wfram_i,wfram_ok,wfram_time,bma_i,bma_ok,bma_time"
            << std::endl;
#endif
    for (const auto& entry: std::filesystem::directory_iterator(benchmark_path))
    {
        test_benchmark_graph(std::string(entry.path()), entry.path().filename());
    }
    return 0;
}
