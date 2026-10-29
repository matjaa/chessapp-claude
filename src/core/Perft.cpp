#include "core/Perft.hpp"

namespace chess
{
std::uint64_t perft(const Position& position, int depth)
{
    if (depth <= 0)
        return 1;

    MoveList moves;
    position.generateLegalMoves(moves);
    if (depth == 1)
        return moves.size();

    std::uint64_t nodes = 0;
    for (const Move& m : moves)
    {
        Position next = position;
        next.makeMove(m);
        nodes += perft(next, depth - 1);
    }
    return nodes;
}

std::vector<std::pair<Move, std::uint64_t>> perftDivide(const Position& position, int depth)
{
    std::vector<std::pair<Move, std::uint64_t>> result;
    MoveList moves;
    position.generateLegalMoves(moves);
    for (const Move& m : moves)
    {
        Position next = position;
        next.makeMove(m);
        result.emplace_back(m, perft(next, depth - 1));
    }
    return result;
}
} // namespace chess
