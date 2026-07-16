#include "relations/DependencyGraph.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <unordered_map>

namespace dbscanner::relations {

DependencyGraph::DependencyGraph(std::vector<std::string> tableNames) : tables_(std::move(tableNames)) {
    for (const auto& name : tables_) {
        adjacency_.emplace(name, std::vector<std::string>{});
    }
}

void DependencyGraph::addEdge(const std::string& parent, const std::string& child) {
    auto& children = adjacency_[parent];
    if (std::find(children.begin(), children.end(), child) == children.end()) {
        children.push_back(child);
    }
    adjacency_.try_emplace(child);  // ensure child node exists even if not in tables_
}

DependencyGraph DependencyGraph::fromRelationships(const std::vector<std::string>& tableNames,
                                                     const std::vector<core::Relationship>& relationships) {
    DependencyGraph graph(tableNames);
    for (const auto& rel : relationships) {
        graph.addEdge(rel.parentTable, rel.childTable);
    }
    return graph;
}

namespace {
std::vector<std::string> canonicalizeCycle(std::vector<std::string> cycle) {
    const auto minIt = std::min_element(cycle.begin(), cycle.end());
    std::rotate(cycle.begin(), minIt, cycle.end());
    return cycle;
}
}  // namespace

std::vector<core::DependencyCycleFinding> DependencyGraph::detectCycles() const {
    enum Color { White, Gray, Black };
    std::unordered_map<std::string, Color> color;
    for (const auto& [node, _] : adjacency_) color[node] = White;

    std::vector<std::string> pathStack;
    std::unordered_map<std::string, std::size_t> pathPos;
    std::set<std::vector<std::string>> seenCanonical;
    std::vector<core::DependencyCycleFinding> cycles;

    std::function<void(const std::string&)> dfs = [&](const std::string& node) {
        color[node] = Gray;
        pathStack.push_back(node);
        pathPos[node] = pathStack.size() - 1;

        const auto it = adjacency_.find(node);
        if (it != adjacency_.end()) {
            for (const auto& child : it->second) {
                if (!color.count(child)) continue;  // defensive: unknown node
                if (color[child] == White) {
                    dfs(child);
                } else if (color[child] == Gray) {
                    std::vector<std::string> cycle(pathStack.begin() + static_cast<long>(pathPos[child]),
                                                     pathStack.end());
                    auto canonical = canonicalizeCycle(cycle);
                    if (seenCanonical.insert(canonical).second) {
                        cycles.push_back(core::DependencyCycleFinding{canonical});
                    }
                }
            }
        }

        color[node] = Black;
        pathPos.erase(node);
        pathStack.pop_back();
    };

    for (const auto& [node, _] : adjacency_) {
        if (color[node] == White) dfs(node);
    }

    return cycles;
}

std::vector<std::string> DependencyGraph::topologicalOrder() const {
    std::unordered_map<std::string, int> inDegree;
    for (const auto& [node, _] : adjacency_) inDegree[node] = 0;
    for (const auto& [parent, children] : adjacency_) {
        for (const auto& child : children) {
            inDegree[child]++;
        }
    }

    std::set<std::string> ready;
    for (const auto& [node, degree] : inDegree) {
        if (degree == 0) ready.insert(node);
    }

    std::vector<std::string> order;
    std::unordered_map<std::string, int> remaining = inDegree;

    while (!ready.empty()) {
        const std::string node = *ready.begin();
        ready.erase(ready.begin());
        order.push_back(node);

        const auto it = adjacency_.find(node);
        if (it != adjacency_.end()) {
            for (const auto& child : it->second) {
                if (--remaining[child] == 0) {
                    ready.insert(child);
                }
            }
        }
    }

    if (order.size() < adjacency_.size()) {
        std::set<std::string> ordered(order.begin(), order.end());
        std::vector<std::string> leftover;
        for (const auto& [node, _] : adjacency_) {
            if (!ordered.count(node)) leftover.push_back(node);
        }
        std::sort(leftover.begin(), leftover.end());
        order.insert(order.end(), leftover.begin(), leftover.end());
    }

    return order;
}

}  // namespace dbscanner::relations
