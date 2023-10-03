/**
 * GAL project - maximum independent set solvers
 *
 * authors: Bc. Ondřej Ondryáš
 *          Bc. Filip Stupka
 *
 * 14. 12. 2022
 */

#pragma once

#include <ogdf/basic/graph_generators.h>
#include <set>

class MISSolver {
public:

    std::function<bool(const ogdf::node&, const ogdf::node&)> node_comparator = [](const ogdf::node& a, const ogdf::node& b)
    {
        return a->index() < b->index();
    };
    
    using node_set = std::set<ogdf::node, decltype(node_comparator)>;

    virtual node_set solve(const ogdf::Graph&) = 0;
    virtual const char* name() = 0;
};