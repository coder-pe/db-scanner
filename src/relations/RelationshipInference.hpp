#pragma once

#include <vector>

#include "core/Relationship.hpp"
#include "core/TableMetadata.hpp"

namespace dbscanner::relations {

// Proposes relationships not already covered by declaredRelationships, based
// on a naming-convention heuristic: a column like CUSTOMER_ID (or CUSTOMERID)
// on a child table is proposed as referencing the single-column primary key
// of a table named CUSTOMER / CUSTOMERS (singular/plural variants), provided
// the column's data type family (numeric/text/date) matches the candidate
// primary key's. Each result carries a confidence score in (0, 1].
std::vector<core::Relationship> inferRelationships(
    const std::vector<core::TableInfo>& tables,
    const std::vector<core::Relationship>& declaredRelationships);

}  // namespace dbscanner::relations
