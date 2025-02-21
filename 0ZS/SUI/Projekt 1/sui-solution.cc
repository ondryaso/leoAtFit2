#include <queue>
#include <utility>
#include <stack>
#include <set>
#include <unordered_map>
#include <algorithm>
#include "search-strategies.h"
#include "memusage.h"

#include "card.h"

using std::make_shared;
using std::tuple;

typedef std::shared_ptr<SearchState> SearchState_ptr;

int constexpr MEM_BOUND = 50 * 1024 * 1024;  // 50 MiB

// Hashes a search state based on the top cards in all storages
std::size_t hash(const SearchState &state) {
    std::size_t h = 144451;

    auto const &s = state.state_;
    for (auto const &c: s.all_storage) {
        auto const &top = c->topCard();
        if (top) {
            std::size_t color_value = 4;
            switch (top->color) {
                case Color::Heart:
                    color_value = 0;
                    break;
                case Color::Diamond:
                    color_value = 1;
                    break;
                case Color::Club:
                    color_value = 2;
                    break;
                case Color::Spade:
                    color_value = 3;
                    break;
            }

            h ^= color_value + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= (std::size_t) top->value + 0x9e3779b9 + (h << 6) + (h >> 2);
        } else {
            h ^= 0x9e3779b9 + (h << 6) + (h >> 2);
        }
    }

    return h;
}

// Equality operator for SearchState that compares the internal states
bool operator==(const SearchState &lhs, const SearchState &rhs) {
    return lhs.state_ == rhs.state_;
}

// A hash provider for std::shared_ptr<SearchState> that hashes the contained SearchStates
struct SearchStatePtrHash {
    std::size_t operator()(const SearchState_ptr &s) const noexcept {
        return hash(*s);
    }
};

// An equality comparer for std::shared_ptr<SearchState> that compares the contained SearchStates
struct SearchStatePtrEq {
    bool operator()(const SearchState_ptr &lhs, const SearchState_ptr &rhs) const noexcept {
        return *lhs == *rhs;
    }
};

// Stores information about an A* state
struct as_state_info {
    SearchState_ptr predecessor_state;
    SearchAction predecessor_action;
    unsigned actual_cost;
    double last_frontier_cost;

    as_state_info(SearchState_ptr pred_state, SearchAction predecessor_action, unsigned actual_cost,
                  double last_frontier_cost)
            : predecessor_state(std::move(pred_state)), predecessor_action(predecessor_action),
              actual_cost(actual_cost),
              last_frontier_cost(last_frontier_cost) {}
};

// Stores information about an A* frontier node
struct as_search_node {
    double cost;
    SearchState_ptr state;

    as_search_node(double cost, SearchState_ptr state)
            : cost(cost), state(std::move(state)) {}

    friend bool operator<(as_search_node const &lhs, as_search_node const &rhs) {
        // This must really be > as std:priority_queue returns the highest value
        // (as derived from the ordering) at the top
        return lhs.cost > rhs.cost;
    }
};

// Stores information about a DFS frontier node
struct dfs_stack_item {
    SearchState_ptr state;
    SearchAction action;
    int depth;

    dfs_stack_item(SearchState_ptr state, SearchAction action, int depth)
            : state(std::move(state)), action(action), depth(depth) {}
};


enum RealColor {
    Black, Red
};

RealColor getColor(Card card) {
    // let's kindly use a switch to determine the card color :)
    switch (card.color) {
        case Color::Heart:
        case Color::Diamond:
            return RealColor::Red;
        default:
            return RealColor::Black;
    }
}


std::vector<SearchAction> AStarSearch::solve(const SearchState &init_state) {
    if (init_state.isFinal())
        return {};

    // The current "open" queue
    std::priority_queue<as_search_node> frontier;
    // Information about all discovered states
    // The custom equality comparer actually determines equality of the contained SearchStates' internal states
    // in contrast to the default equality comparer which compares the pointers
    std::unordered_map<SearchState_ptr, as_state_info, SearchStatePtrHash, SearchStatePtrEq> states;

    bool found = false;
    const auto actual_mem_limit = mem_limit_ - MEM_BOUND;

    // The initial state metadata. Uses a dummy predecessor action.
    as_state_info init_node(nullptr, SearchAction({}, {}), 0, 0);
    auto const init_state_ptr = make_shared<SearchState>(init_state);

    states.emplace(init_state_ptr, init_node);
    frontier.emplace(0, init_state_ptr);

    SearchState_ptr current_state;

    while (!frontier.empty()) {
        if (getCurrentRSS() > actual_mem_limit) {
            return {};
        }

        auto const &current_top = frontier.top();

        // The frontier queue may contain "outdated" entries (better paths have been already found for something
        // inserted earlier).
        auto const &node_info = states.find(current_top.state)->second;
        if (node_info.last_frontier_cost < current_top.cost) {
            frontier.pop();
            continue;
        }

        // Check if the top state is final
        current_state = current_top.state;
        if (current_state->isFinal()) {
            found = true;
            break;
        }

        frontier.pop();
        unsigned current_cost = node_info.actual_cost;

        auto const actions = current_state->actions();
        for (auto const &action: actions) {
            auto new_state = action.execute(*current_state);
            auto new_state_ptr = make_shared<SearchState>(new_state);

            // Check if we've already seen this state
            auto const node = states.find(new_state_ptr);

            unsigned new_actual_cost = current_cost + 1;
            double cost_with_heuristic = new_actual_cost + compute_heuristic(new_state, *heuristic_);

            if (node == states.end()) {
                // This is a brand-new state
                as_state_info new_node(current_state, action, new_actual_cost, cost_with_heuristic);
                states.emplace(new_state_ptr, new_node);
                frontier.emplace(cost_with_heuristic, new_state_ptr);
            } else if (new_actual_cost < node->second.actual_cost) {
                // We've already seen the state and the cost on this path is lower than the previous one
                // Update the discovered state's info
                auto &p = node->second;
                p.predecessor_state = current_state;
                p.predecessor_action = action;
                p.actual_cost = new_actual_cost;
                p.last_frontier_cost = cost_with_heuristic;
                frontier.emplace(cost_with_heuristic, new_state_ptr);
            }
        }
    }

    if (!found)
        return {};  // The search has ended without encountering a final state

    // Reconstruct the action path from the final state to the initial state
    auto end_node = states.find(current_state);
    auto depth = end_node->second.actual_cost;
    std::deque<SearchAction *> ret_actions;

    while (depth != 0) {
        ret_actions.push_front(&end_node->second.predecessor_action);
        end_node = states.find(end_node->second.predecessor_state);
        depth--;
    }

    std::vector<SearchAction> ret;
    for (auto const &action_ptr: ret_actions)
        ret.push_back(*action_ptr);

    return ret;
}


std::vector<SearchAction> DepthFirstSearch::solve(const SearchState &init_state) {
    if (init_state.isFinal())
        return {};

    // The current "open" stack
    std::stack<dfs_stack_item> stack;
    // Information about the chain of actions taken to reach the current state
    // on top of the stack from the root
    std::deque<SearchAction> current_actions;

    // Insert the initial state together with a dummy predecessor action
    stack.emplace(make_shared<SearchState>(init_state), SearchAction({}, {}), 0);

    bool found = false;
    int previous_depth = 0;  // Keeps track of the last inspected state's depth
    // The depth at which inspected states' successors are no longer added to the stack
    const int max_depth_when_generating = depth_limit_ - 1;
    const auto actual_mem_limit = mem_limit_ - MEM_BOUND;

    while (!stack.empty() && !found) {
        if (getCurrentRSS() > actual_mem_limit) {
            return {};
        }

        auto const &stack_item = stack.top();
        auto current_depth = stack_item.depth;

        // When jumping up the search tree, a number of actions, equal to the number
        // of levels jumped over, must be removed from the current action chain list
        auto const depth_difference = previous_depth - current_depth;
        if (depth_difference == 1)
            current_actions.pop_back();
        else if (depth_difference > 0) {
            for (int i = 0; i < depth_difference; i++)
                current_actions.pop_back();
        }

        auto current_state = stack_item.state;
        current_actions.push_back(stack_item.action);
        stack.pop();

        auto actions = current_state->actions();
        bool had = false;
        for (const auto &action: actions) {
            SearchState new_state = action.execute(*current_state);

            if (new_state.isFinal()) {
                current_actions.push_back(action);
                found = true;
                had = true;
                break;
            } else if (current_depth < max_depth_when_generating) {
                stack.emplace(make_shared<SearchState>(new_state), action,
                              current_depth + 1);
                had = true;
            }
        }

        if (!had) {
            // If no actions were added to the stack, the current state is a dead end
            // The action leading to it, pushed before the for loop, is useless and must be popped
            current_actions.pop_back();
        }

        previous_depth = current_depth;
    }

    if (!found)
        return {};

    current_actions.pop_front();  // Remove the initial dummy action
    std::vector<SearchAction> ret(current_actions.begin(), current_actions.end());
    return ret;
}


std::vector<SearchAction> BreadthFirstSearch::solve(const SearchState &init_state) {
    if (init_state.isFinal()) {
        return {};
    }

    std::queue<SearchState_ptr> open;
    std::unordered_map<SearchState_ptr, std::tuple<SearchState_ptr, SearchAction>, SearchStatePtrHash, SearchStatePtrEq>
            closed;

    auto init_state_ptr = std::make_shared<SearchState>(init_state);
    // Initialize the Open and Closed lists
    // Open starts with the initial state
    open.push(init_state_ptr);

    // Closed starts with the initial state pointing to null pointers
    // This is later used as a break condition in path reconstruction
    closed.insert(std::make_pair(init_state_ptr, std::tuple{nullptr, SearchAction({}, {})}));

    while (!open.empty()) {
        // Take out the next state to expand from Open
        SearchState_ptr state = open.front();
        open.pop();

        if (getCurrentRSS() >= BreadthFirstSearch::mem_limit_ - MEM_BOUND) {
            // Return an empty path if the memory limit has been reached
            return {};
        }

        for (auto const &action: state->actions()) {
            SearchState_ptr nextState = std::make_shared<SearchState>(action.execute(*state));

            if (nextState->isFinal()) {
                // A solution has been found
                std::vector<SearchAction> path = {action};

                // Reconstruct the path from the Closed list
                while (true) {
                    auto item = closed.find(state);
                    auto tuple = item->second;

                    state = std::get<0>(tuple);
                    auto const &path_action = std::get<1>(tuple);

                    // Path reconstruction has reached the initial state
                    if (state == nullptr) {
                        break;
                    }

                    path.insert(path.begin(), path_action);
                }

                return path;
            }

            // Check if the newly discovered nextState is already present in Closed
            if (closed.find(nextState) == closed.end()) {
                // If not, insert it into Open
                open.push(nextState);

                // And insert it into Closed with additional information
                // pointing to the current state and the action that was
                // used to discover this nextState
                closed.insert(
                        std::make_pair(nextState, std::tuple{state, action})
                );
            }
        }

    }

    return {};
}

double StudentHeuristic::distanceLowerBound(const GameState &state) const {
    double cardsInStacks = 0;
    double wellPlaced = 0;

    // Consider only cards that are in stacks
    for (auto const &stack: state.stacks) {

        RealColor prevColor;
        int prevValue;

        bool isFirst = true;

        for (auto const &card: stack.storage()) {
            RealColor color = getColor(card);

            // We consider a card to be WELL PLACED
            // if it's the first in the stack
            // or if it has a different color than the previous card
            // and its value is one less than the previous card
            if (isFirst || (prevColor != color && prevValue - 1 == card.value)) {
                wellPlaced++;
            }
            isFirst = false;
            prevColor = color;
            prevValue = card.value;
        }

        cardsInStacks += (double) stack.nbCards();
    }

    // return the number of NOT well placed cards
    return cardsInStacks - wellPlaced;
}
