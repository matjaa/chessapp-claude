#pragma once

#include "core/Position.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace chess
{
// Counts all leaf positions reachable in exactly `depth` moves. Comparing
// the counts with published values is the standard way to check that move
// generation is correct.
std::uint64_t perft(const Position& position, int depth);

// Same, split per first move (handy for finding which move is wrong).
std::vector<std::pair<Move, std::uint64_t>> perftDivide(const Position& position, int depth);
} // namespace chess
