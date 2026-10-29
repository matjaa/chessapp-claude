#pragma once

#include "core/Position.hpp"

#include <atomic>
#include <cstdint>
#include <random>
#include <vector>

namespace engine
{
struct SearchSettings
{
    int  depth      = 4;    // full-width search depth in plies
    bool quiescence = true; // keep following captures past the depth limit
    int  noise      = 0;    // random +/- centipawns added to each root move's score
};

struct SearchResult
{
    chess::Move   move;
    bool          found   = false; // false only if there is no legal move
    int           score   = 0;     // from the side to move's point of view
    int           depth   = 0;     // last fully completed depth
    std::uint64_t nodes   = 0;
    bool          aborted = false; // stopped early through the stop flag
};

// Alpha-beta search (negamax) with iterative deepening, quiescence search,
// check extensions, MVV-LVA and killer move ordering.
//
// `history` holds the hashes of the positions played before `root`
// (oldest first), so that the search can see repetition draws.
// `stop` may be set from another thread; the search then returns quickly
// with `aborted` set.
SearchResult search(const chess::Position& root,
                    const std::vector<std::uint64_t>& history,
                    const SearchSettings& settings,
                    std::mt19937& rng,
                    const std::atomic<bool>* stop = nullptr);
} // namespace engine
