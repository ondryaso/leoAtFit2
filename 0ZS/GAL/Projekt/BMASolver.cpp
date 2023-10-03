/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#include "BMASolver.h"

using namespace ogdf;

constexpr int costInfinity = std::numeric_limits<int>::max();

bool contains(const std::unordered_set<int>& set, int item)
{
    return set.find(item) != set.end();
}

bool contains(const std::set<int>& set, int item)
{
    return set.find(item) != set.end();
}

inline std::pair<std::vector<int>::iterator, std::vector<int>::iterator>
BMASolver::getIncludedTargetVertices(int sourceNodeIndex, RoundContainer& currentRound, int currentRoundIndex,
        std::vector<int>& nodeIndices, std::vector<int>& tmpVector)
{
    BMA_LOG
    {
        std::cout << "u = " << BMA_VNAME(sourceNodeIndex) << "\n";
        std::cout << "> Cost[u, " << currentRoundIndex << "] = ";
        if (currentRound.cost[sourceNodeIndex] == costInfinity)
            std::cout << "inf\n";
        else
            std::cout << currentRound.cost[sourceNodeIndex] << '\n';
    }


    auto findU = currentRound.exclude.find(sourceNodeIndex);
    auto beginIt = nodeIndices.begin();
    auto endIt = nodeIndices.end();

    if (findU != currentRound.exclude.end())
    {
        BMA_LOG
        {
            std::cout << "> Exclude[u, " << currentRoundIndex << "] = {";
            for (const auto& i: findU->second)
            {
                std::cout << BMA_VNAME(i) << ',';
            }
            std::cout << "}\n";
        }

        beginIt = tmpVector.begin();
        endIt = std::set_difference(nodeIndices.begin(), nodeIndices.end(),
                findU->second.begin(), findU->second.end(), tmpVector.begin());
    }
    else
    {
        BMA_LOG
        {
            std::cout << "> Exclude[u, round] = empty set\n";
        }
    }

    BMA_LOG
    {
        std::cout << "> v ∈ {";
        for (auto vIt = beginIt; vIt != endIt; vIt++)
        {
            std::cout << BMA_VNAME(*vIt) << ',';
        }
        std::cout << "}\n";
    }

    return { beginIt, endIt };
}


std::set<ogdf::node, decltype(MISSolver::node_comparator)> BMASolver::solve(const ogdf::Graph& graph)
{
    const int roundCount = graph.numberOfNodes() + 1;

    std::unordered_map<int, node> nodes;
    std::vector<int> nodeIndices;
    std::sort(nodeIndices.begin(), nodeIndices.end());

    auto rounds = new RoundContainer[roundCount];

    // Initialization
    auto& roundZero = rounds[0];
    for (const auto& u: graph.nodes)
    {
        const auto uIndex = u->index();
        nodeIndices.push_back(uIndex);
        nodes[uIndex] = u;

        // Exclude[u, 0] <- { u }
        auto& excludeU = roundZero.exclude[uIndex];
        excludeU.insert(uIndex);
        // Previous[u, 0] <- no node // not represented here
        // Cost[u, 0] <- 1
        roundZero.cost[uIndex] = 1;

        // for each neighbour v of u
        List<ogdf::edge> neighbours;
        u->adjEdges(neighbours);
        for (const auto& edge: neighbours)
        {
            // Exclude[u, 0] <- Exclude[u, 0] U { v }
            if (edge->source() != u)
                continue;

            int inIndex = edge->target()->index();
            auto inserted = excludeU.insert(inIndex);

            if (inserted.second)
            {
                // Cost[u, 0] <- Cost[u, 0] + 1
                roundZero.cost[uIndex] += 1;
            }
        }

        // for 1 <= round <= |V|
        for (int roundI = 1; roundI < roundCount; roundI++)
        {
            auto& nextRound = rounds[roundI];
            // Previous[u, round] <- no node // not represented here
            // Cost[u, round] <- infinity
            nextRound.cost[uIndex] = costInfinity;
            nextRound.exclude.insert(std::pair(uIndex, std::set<int>()));
        }
    }

    int lastRoundI = 0;

    std::vector<int> tmpVector(graph.numberOfNodes() + 1);
    std::vector<int> tmpVExclude(graph.numberOfNodes() + 1);

    auto startTime = std::chrono::high_resolution_clock::now();

    // Relaxation
    // for 0 <= round <= (|V| - 1)
    for (int roundI = 0; roundI < roundCount - 1; roundI++)
    {
        BMA_LOG std::cout << "\n--- ROUND " << roundI << "---\n";

        auto& currentRound = rounds[roundI];
        bool hasCostChange = false;

        // for all (u, v) in (u in V, v in V\Exclude[u, round])
        for (const auto& u: graph.nodes)
        {
            const auto uIndex = u->index();

            // Determine the (V \ Exclude[u, round]) set
            auto x = getIncludedTargetVertices(uIndex, currentRound, roundI, nodeIndices, tmpVector);
            auto& beginIt = x.first;
            const auto& endIt = x.second;

            for (auto& vIt = beginIt; vIt != endIt; vIt++)
            {
                const int vIndex = *vIt;
                // if Cost[u, round] < inf
                if (currentRound.cost[uIndex] == costInfinity)
                    continue;

                // vExclude <- Exclude[u, round] U { v }
                // Copying the whole Exclude[u, round] set is redundant
                // it's better to operate over a small vector of values that would be added when making the union
                const auto& origExclude = currentRound.exclude[uIndex];
                tmpVExclude.clear();
                tmpVExclude.push_back(vIndex);

                // vCost <- Cost[u, round] + 1
                auto vCost = currentRound.cost[uIndex] + 1;

                // for each neighbour j of v
                const auto& neighbours = nodes[vIndex]->adjEntries;
                for (const auto& j: neighbours)
                {
                    const int jIndex = j->twinNode()->index();

                    // if j notin vExclude
                    if (!contains(origExclude, jIndex))
                    {
                        // vExclude <- vExclude U { j }
                        tmpVExclude.push_back(jIndex);
                        // vCost <- vCost + 1
                        vCost++;
                    }
                }

                auto& nextRound = rounds[roundI + 1];
                if (nextRound.cost[vIndex] > vCost)
                {
                    // Previous[v, round + 1] <- u
                    nextRound.previous[vIndex] = uIndex;
                    // Exclude[v, round + 1] <- vExclude
                    auto& actualVExclude = nextRound.exclude[vIndex];

                    actualVExclude.clear();
                    for (const auto& i: origExclude)
                    {
                        actualVExclude.insert(i);
                    }
                    for (const auto& i: tmpVExclude)
                    {
                        actualVExclude.insert(i);
                    }

                    // Cost[v, round + 1] <- vCost
                    nextRound.cost[vIndex] = vCost;
                    hasCostChange = true;
                }
            }
        }

        if (!hasCostChange)
        {
            lastRoundI = roundI;
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();

    BMA_LOG std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
                      << " ms\n";

    BMA_LOG std::cout << "Last round: " << lastRoundI << '\n';

    const auto& ret = getResult(rounds, lastRoundI, nodes);
    delete[] rounds;
    return ret;
}

std::set<ogdf::node, decltype(MISSolver::node_comparator)>
BMASolver::getResult(RoundContainer* rounds, int lastRoundIndex, std::unordered_map<int, node> nodes)
{
    std::set<ogdf::node, decltype(MISSolver::node_comparator)> ret(MISSolver::node_comparator);

    // Select a vertex that ended with non-inf cost
    // Every one of such vertices can be selected, the resulting MIS will be the same size
    const RoundContainer& lastRound = rounds[lastRoundIndex];
    int vertexId = -1;
    for (const auto& item: lastRound.cost)
    {
        if (item.second != costInfinity)
        {
            vertexId = item.first;
            break;
        }
    }
    if (vertexId == -1)
    {
        throw std::runtime_error("algorithm error: cannot find result path");
    }

    BMA_LOG std::cout << "\nMIS = {";

    // Construct the set by following the Previous array through rounds
    while (lastRoundIndex >= 0)
    {
        BMA_LOG
        { std::cout << BMA_VNAME(vertexId) << ','; }
        ret.insert(nodes[vertexId]);
        vertexId = rounds[lastRoundIndex].previous[vertexId];
        lastRoundIndex--;
    }

    BMA_LOG std::cout << "}\n";

    return ret;
}

const char* BMASolver::name()
{
    return "Bellman-Ford based MIS Algorithm Solver";
}
