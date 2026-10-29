#pragma once

#include "core/Position.hpp"

namespace engine
{
// Scores are in centipawns (100 = one pawn).
inline constexpr int MateScore = 100000;
// Any score at least this large means "forced mate found".
inline constexpr int MateThreshold = MateScore - 1000;

// Value of a piece type (king = 0, it can never be captured).
int pieceValue(chess::PieceType type);

// Static evaluation from the point of view of the side to move:
// material, piece-square tables (the king table changes from middlegame
// to endgame as pieces come off) and a small bonus for the bishop pair.
int evaluate(const chess::Position& pos);
} // namespace engine
