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
#include <ogdf/basic/NodeSet.h>
#include <unordered_set>

#define BMA_LOG_ENABLED 0
#define BMA_VNAME(i) ((char)('a' + (i)))
#define BMA_LOG if (BMA_LOG_ENABLED)

struct RoundContainer {
    std::unordered_map<int, std::set<int>> exclude;
    std::unordered_map<int, int> previous;
    std::unordered_map<int, int> cost;
};

class BMASolver : public MISSolver {
public:
    std::set<ogdf::node, decltype(node_comparator)> solve(const ogdf::Graph& graph) override;

    const char* name() override;

private:
    static std::pair<std::vector<int>::iterator, std::vector<int>::iterator>
    getIncludedTargetVertices(int sourceNodeIndex, RoundContainer& currentRound, int currentRoundIndex,
            std::vector<int>& nodeIndices, std::vector<int>& tmpVector);

    std::set<ogdf::node, decltype(node_comparator)> getResult(RoundContainer* rounds, int lastRoundIndex,
            std::unordered_map<int, ogdf::node> nodes);
};
