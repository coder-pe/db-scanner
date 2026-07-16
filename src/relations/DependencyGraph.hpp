#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/Relationship.hpp"
#include "core/ScanResult.hpp"

namespace dbscanner::relations {

// Directed graph of table dependencies: an edge parent -> child means "child"
// has a (declared or inferred) foreign key into "parent", i.e. parent must
// logically exist/be valid before child can be considered consistent.
class DependencyGraph {
public:
    explicit DependencyGraph(std::vector<std::string> tableNames);

    void addEdge(const std::string& parent, const std::string& child);
    static DependencyGraph fromRelationships(const std::vector<std::string>& tableNames,
                                              const std::vector<core::Relationship>& relationships);

    // Elementary cycles found via DFS back-edge detection. Not a full Johnson's
    // algorithm enumeration, but sufficient to flag circular dependencies for a
    // schema report.
    std::vector<core::DependencyCycleFinding> detectCycles() const;

    // Parents-before-children order (Kahn's algorithm). Tables that can't be
    // strictly ordered because they participate in a cycle are appended at the
    // end in alphabetical order, after everything that could be ordered.
    std::vector<std::string> topologicalOrder() const;

    const std::map<std::string, std::vector<std::string>>& adjacency() const { return adjacency_; }

private:
    std::vector<std::string> tables_;
    std::map<std::string, std::vector<std::string>> adjacency_;  // parent -> children
};

}  // namespace dbscanner::relations
