#pragma once
#include "MissionDefinition.hpp"

namespace auriga::mission
{
// Validates the complete graph, including unused predicates and every branch.
// Cross-node cycles and shared mutable child state are rejected in schema 1.
Compilation CompileDefinition(const Definition &source);
} // namespace auriga::mission
