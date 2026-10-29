#include "engine/Evaluation.hpp"

#include <array>
#include <cstddef>

namespace engine
{
using chess::Color;
using chess::Piece;
using chess::PieceType;
using chess::Square;

namespace
{
using Table = std::array<int, 64>;

// Piece-square tables from the "Simplified Evaluation Function" by
// Tomasz Michniewski. They are written as seen from White: the first row
// is rank 8, the last row is rank 1.
constexpr Table pawnTable = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0};

constexpr Table knightTable = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50};

constexpr Table bishopTable = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20};

constexpr Table rookTable = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0};

constexpr Table queenTable = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20};

constexpr Table kingMiddlegameTable = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20};

constexpr Table kingEndgameTable = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50};

// Index into a table above for a piece of colour c on square sq.
// White's tables are mirrored vertically for Black.
std::size_t tableIndex(Square sq, Color c)
{
    const int file = chess::fileOf(sq);
    const int rank = chess::rankOf(sq);
    const int row  = c == Color::White ? 7 - rank : rank;
    return static_cast<std::size_t>(row * 8 + file);
}

// Game phase weights: 24 with all pieces on the board, 0 with only pawns and kings.
int phaseWeight(PieceType type)
{
    switch (type)
    {
        case PieceType::Knight:
        case PieceType::Bishop: return 1;
        case PieceType::Rook:   return 2;
        case PieceType::Queen:  return 4;
        default:                return 0;
    }
}

constexpr int maxPhase        = 24;
constexpr int bishopPairBonus = 30;
} // namespace

int pieceValue(PieceType type)
{
    switch (type)
    {
        case PieceType::Pawn:   return 100;
        case PieceType::Knight: return 320;
        case PieceType::Bishop: return 330;
        case PieceType::Rook:   return 500;
        case PieceType::Queen:  return 900;
        default:                return 0;
    }
}

int evaluate(const chess::Position& pos)
{
    std::array<int, 2> score{};       // material + tables, kings excluded
    std::array<int, 2> kingMid{};     // king table, middlegame
    std::array<int, 2> kingEnd{};     // king table, endgame
    std::array<int, 2> bishops{};
    int phase = 0;

    for (Square sq = 0; sq < 64; ++sq)
    {
        const Piece piece = pos.pieceAt(sq);
        if (piece.isNone())
            continue;
        const auto side = static_cast<std::size_t>(chess::index(piece.color));
        const auto i    = tableIndex(sq, piece.color);
        phase += phaseWeight(piece.type);

        switch (piece.type)
        {
            case PieceType::Pawn:   score[side] += pawnTable[i]; break;
            case PieceType::Knight: score[side] += knightTable[i]; break;
            case PieceType::Bishop: score[side] += bishopTable[i]; ++bishops[side]; break;
            case PieceType::Rook:   score[side] += rookTable[i]; break;
            case PieceType::Queen:  score[side] += queenTable[i]; break;
            case PieceType::King:
                kingMid[side] += kingMiddlegameTable[i];
                kingEnd[side] += kingEndgameTable[i];
                break;
            default: break;
        }
        score[side] += pieceValue(piece.type);
    }

    if (phase > maxPhase)
        phase = maxPhase; // possible after promotions

    std::array<int, 2> total{};
    for (std::size_t side = 0; side < 2; ++side)
    {
        const int king = (kingMid[side] * phase + kingEnd[side] * (maxPhase - phase)) / maxPhase;
        total[side]    = score[side] + king + (bishops[side] >= 2 ? bishopPairBonus : 0);
    }

    const int white = total[0] - total[1];
    return pos.sideToMove() == Color::White ? white : -white;
}
} // namespace engine
