#include <gtest/gtest.h>

#include <algorithm>

#include "relations/DependencyGraph.hpp"

using namespace dbscanner::relations;

namespace {
int indexOf(const std::vector<std::string>& order, const std::string& name) {
    auto it = std::find(order.begin(), order.end(), name);
    return it == order.end() ? -1 : static_cast<int>(std::distance(order.begin(), it));
}
}  // namespace

TEST(DependencyGraph, TopologicalOrderPutsParentsBeforeChildren) {
    DependencyGraph graph({"CUSTOMERS", "ORDERS", "ORDER_ITEMS", "PRODUCTS"});
    graph.addEdge("CUSTOMERS", "ORDERS");
    graph.addEdge("ORDERS", "ORDER_ITEMS");
    graph.addEdge("PRODUCTS", "ORDER_ITEMS");

    const auto order = graph.topologicalOrder();

    ASSERT_EQ(order.size(), 4u);
    EXPECT_LT(indexOf(order, "CUSTOMERS"), indexOf(order, "ORDERS"));
    EXPECT_LT(indexOf(order, "ORDERS"), indexOf(order, "ORDER_ITEMS"));
    EXPECT_LT(indexOf(order, "PRODUCTS"), indexOf(order, "ORDER_ITEMS"));
}

TEST(DependencyGraph, IsolatedTableWithNoEdgesStillAppears) {
    DependencyGraph graph({"CUSTOMERS", "AUDIT_LOG"});
    const auto order = graph.topologicalOrder();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_NE(indexOf(order, "AUDIT_LOG"), -1);
}

TEST(DependencyGraph, DetectsSimpleCycle) {
    DependencyGraph graph({"A", "B"});
    graph.addEdge("A", "B");
    graph.addEdge("B", "A");

    const auto cycles = graph.detectCycles();

    ASSERT_EQ(cycles.size(), 1u);
    EXPECT_EQ(cycles[0].tablesInCycle.size(), 2u);
}

TEST(DependencyGraph, NoCyclesInAcyclicGraph) {
    DependencyGraph graph({"A", "B", "C"});
    graph.addEdge("A", "B");
    graph.addEdge("B", "C");

    EXPECT_TRUE(graph.detectCycles().empty());
}

TEST(DependencyGraph, TopologicalOrderStillCompleteWithCycle) {
    DependencyGraph graph({"A", "B", "C"});
    graph.addEdge("A", "B");
    graph.addEdge("B", "A");
    graph.addEdge("A", "C");

    const auto order = graph.topologicalOrder();

    // A cyclic pair can't be strictly ordered, but every node must still appear.
    ASSERT_EQ(order.size(), 3u);
    EXPECT_NE(indexOf(order, "A"), -1);
    EXPECT_NE(indexOf(order, "B"), -1);
    EXPECT_NE(indexOf(order, "C"), -1);
}

TEST(DependencyGraph, FromRelationshipsBuildsEdgesFromParentToChild) {
    dbscanner::core::Relationship rel;
    rel.parentTable = "CUSTOMERS";
    rel.childTable = "ORDERS";

    auto graph = DependencyGraph::fromRelationships({"CUSTOMERS", "ORDERS"}, {rel});
    const auto order = graph.topologicalOrder();

    EXPECT_LT(indexOf(order, "CUSTOMERS"), indexOf(order, "ORDERS"));
}
