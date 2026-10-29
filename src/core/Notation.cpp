#include "core/Notation.hpp"

namespace chess
{
namespace
{
char pieceLetter(PieceType type)
{
    switch (type)
    {
        case PieceType::Knight: return 'N';
        case PieceType::Bishop: return 'B';
        case PieceType::Rook:   return 'R';
        case PieceType::Queen:  return 'Q';
        case PieceType::King:   return 'K';
        default:                return '?';
    }
}

char fileLetter(Square sq)
{
    return static_cast<char>('a' + fileOf(sq));
}

char rankDigit(Square sq)
{
    return static_cast<char>('1' + rankOf(sq));
}
} // namespace

std::string toSan(const Position& pos, const Move& move)
{
    const Square    from  = move.fromSquare();
    const Square    to    = move.toSquare();
    const PieceType piece = pos.pieceAt(from).type;

    std::string san;
    if (move.isCastling())
    {
        san = fileOf(to) > fileOf(from) ? "O-O" : "O-O-O";
    }
    else if (piece == PieceType::Pawn)
    {
        if (move.isCapture())
        {
            san += fileLetter(from);
            san += 'x';
        }
        san += squareName(to);
        if (move.isPromotion())
        {
            san += '=';
            san += pieceLetter(move.promotion);
        }
    }
    else
    {
        san += pieceLetter(piece);

        // If another piece of the same type can also reach the target square,
        // add the file of the moving piece, or its rank, or both.
        MoveList legal;
        pos.generateLegalMoves(legal);
        bool ambiguous = false;
        bool sameFile  = false;
        bool sameRank  = false;
        for (const Move& other : legal)
        {
            const Square otherFrom = other.fromSquare();
            if (other.toSquare() != to || otherFrom == from || pos.pieceAt(otherFrom).type != piece)
                continue;
            ambiguous = true;
            sameFile |= fileOf(otherFrom) == fileOf(from);
            sameRank |= rankOf(otherFrom) == rankOf(from);
        }
        if (ambiguous)
        {
            if (!sameFile)
                san += fileLetter(from);
            else if (!sameRank)
                san += rankDigit(from);
            else
            {
                san += fileLetter(from);
                san += rankDigit(from);
            }
        }

        if (move.isCapture())
            san += 'x';
        san += squareName(to);
    }

    // Check or checkmate after the move.
    Position after = pos;
    after.makeMove(move);
    if (after.inCheck())
    {
        MoveList replies;
        after.generateLegalMoves(replies);
        san += replies.empty() ? '#' : '+';
    }
    return san;
}
} // namespace chess
