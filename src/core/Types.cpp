#include "core/Types.hpp"

namespace chess
{
std::string squareName(Square sq)
{
    if (sq < 0 || sq > 63)
        return "-";
    std::string name;
    name += static_cast<char>('a' + fileOf(sq));
    name += static_cast<char>('1' + rankOf(sq));
    return name;
}

Square parseSquare(std::string_view text)
{
    if (text.size() != 2)
        return NoSquare;
    const int file = text[0] - 'a';
    const int rank = text[1] - '1';
    if (!isOnBoard(file, rank))
        return NoSquare;
    return makeSquare(file, rank);
}

std::string Move::toUci() const
{
    std::string text = squareName(from) + squareName(to);
    switch (promotion)
    {
        case PieceType::Knight: text += 'n'; break;
        case PieceType::Bishop: text += 'b'; break;
        case PieceType::Rook:   text += 'r'; break;
        case PieceType::Queen:  text += 'q'; break;
        default: break;
    }
    return text;
}
} // namespace chess
