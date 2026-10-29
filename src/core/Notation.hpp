#pragma once

#include "core/Position.hpp"

#include <string>

namespace chess
{
// Standard algebraic notation (SAN) of a legal move in `pos`, as used in
// move lists: "e4", "Nf3", "exd5", "Nbd2", "R1a3", "O-O", "e8=Q",
// with "+" for check and "#" for checkmate.
std::string toSan(const Position& pos, const Move& move);
} // namespace chess
