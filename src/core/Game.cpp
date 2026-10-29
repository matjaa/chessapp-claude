#include "core/Game.hpp"

#include "core/Notation.hpp"

#include <cstddef>

namespace chess
{
std::string colorName(Color c)
{
    return c == Color::White ? "White" : "Black";
}

Game::Game()
{
    reset(Position());
}

Game::Game(const Position& start)
{
    reset(start);
}

void Game::reset(const Position& start)
{
    m_start    = start;
    m_position = start;
    m_moves.clear();
    m_sanMoves.clear();
    m_hashes.clear();
    m_hashes.push_back(start.hash());
    m_status = GameStatus::Ongoing;
    m_winner.reset();
    updateStatus();
}

std::optional<Move> Game::findLegalMove(Square from, Square to, PieceType promotion) const
{
    for (const Move& m : m_legalMoves)
        if (m.fromSquare() == from && m.toSquare() == to && m.promotion == promotion)
            return m;
    return std::nullopt;
}

bool Game::isPromotion(Square from, Square to) const
{
    for (const Move& m : m_legalMoves)
        if (m.fromSquare() == from && m.toSquare() == to && m.isPromotion())
            return true;
    return false;
}

bool Game::makeMove(const Move& move)
{
    if (isOver())
        return false;
    const auto legal = findLegalMove(move.fromSquare(), move.toSquare(), move.promotion);
    if (!legal)
        return false;

    m_sanMoves.push_back(toSan(m_position, *legal)); // needs the position before the move
    m_position.makeMove(*legal);                     // use the generated move so the flags are right
    m_moves.push_back(*legal);
    m_hashes.push_back(m_position.hash());
    updateStatus();
    return true;
}

void Game::resign(Color side)
{
    if (isOver())
        return;
    m_status = GameStatus::Resigned;
    m_winner = ~side;
    m_legalMoves.clear();
}

std::optional<Move> Game::lastMove() const
{
    if (m_moves.empty())
        return std::nullopt;
    return m_moves.back();
}

void Game::updateStatus()
{
    m_position.generateLegalMoves(m_legalMoves);

    if (m_legalMoves.empty())
    {
        if (m_position.inCheck())
        {
            m_status = GameStatus::Checkmate;
            m_winner = ~m_position.sideToMove();
        }
        else
        {
            m_status = GameStatus::Stalemate;
        }
    }
    else if (m_position.isInsufficientMaterial())
    {
        m_status = GameStatus::InsufficientMaterial;
    }
    else if (m_position.halfmoveClock() >= 100) // 50 moves by each side without a capture or pawn move
    {
        m_status = GameStatus::FiftyMoveRule;
    }
    else if (isThreefoldRepetition())
    {
        m_status = GameStatus::ThreefoldRepetition;
    }

    if (isOver())
        m_legalMoves.clear();
}

bool Game::isThreefoldRepetition() const
{
    // Only positions since the last capture or pawn move can repeat, and
    // the hash already includes side to move, castling and en passant rights.
    const std::uint64_t current  = m_hashes.back();
    const std::size_t   lookback = static_cast<std::size_t>(m_position.halfmoveClock());
    const std::size_t   last     = m_hashes.size() - 1;
    const std::size_t   first    = last > lookback ? last - lookback : 0;

    int count = 0;
    for (std::size_t i = first; i <= last; ++i)
        if (m_hashes[i] == current)
            ++count;
    return count >= 3;
}

std::string Game::resultText() const
{
    switch (m_status)
    {
        case GameStatus::Ongoing:              return "";
        case GameStatus::Checkmate:            return "Checkmate - " + colorName(*m_winner) + " wins";
        case GameStatus::Stalemate:            return "Draw by stalemate";
        case GameStatus::ThreefoldRepetition:  return "Draw by threefold repetition";
        case GameStatus::FiftyMoveRule:        return "Draw by fifty-move rule";
        case GameStatus::InsufficientMaterial: return "Draw by insufficient material";
        case GameStatus::Resigned:             return colorName(~*m_winner) + " resigned";
    }
    return "";
}
} // namespace chess
