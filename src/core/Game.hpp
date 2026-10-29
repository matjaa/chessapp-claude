#pragma once

#include "core/Position.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chess
{
enum class GameStatus
{
    Ongoing,
    Checkmate,
    Stalemate,
    ThreefoldRepetition,
    FiftyMoveRule,
    InsufficientMaterial,
    Resigned
};

// A whole game: the current position, the moves played so far and the
// result. Draws are declared automatically as soon as they happen.
class Game
{
public:
    Game();                                  // standard starting position
    explicit Game(const Position& start);

    void reset(const Position& start = Position());

    const Position& position() const { return m_position; }
    const Position& startPosition() const { return m_start; }
    Color sideToMove() const { return m_position.sideToMove(); }

    // Legal moves in the current position (empty once the game is over).
    const MoveList& legalMoves() const { return m_legalMoves; }

    // Finds the legal move from -> to. For a pawn reaching the last rank,
    // pass the promotion piece; without it, no move is found.
    std::optional<Move> findLegalMove(Square from, Square to, PieceType promotion = PieceType::None) const;
    // True if moving from -> to is a legal pawn promotion (the GUI then asks
    // which piece to promote to).
    bool isPromotion(Square from, Square to) const;

    // Plays a move. Returns false (and changes nothing) if the move is not
    // legal or the game is already over.
    bool makeMove(const Move& move);

    // The given side gives up; the other side wins.
    void resign(Color side);

    GameStatus status() const { return m_status; }
    bool isOver() const { return m_status != GameStatus::Ongoing; }
    // The winner after checkmate or resignation; empty for draws and ongoing games.
    std::optional<Color> winner() const { return m_winner; }
    // E.g. "Checkmate - White wins", "Draw by threefold repetition",
    // "Black resigned". Empty while the game is ongoing.
    std::string resultText() const;

    const std::vector<Move>& moves() const { return m_moves; }
    // The same moves in standard algebraic notation ("e4", "Nf3", "O-O"...).
    const std::vector<std::string>& sanMoves() const { return m_sanMoves; }
    std::optional<Move> lastMove() const;

private:
    void updateStatus();
    bool isThreefoldRepetition() const;

    Position                   m_start;
    Position                   m_position;
    MoveList                   m_legalMoves;
    std::vector<Move>          m_moves;
    std::vector<std::string>   m_sanMoves;
    std::vector<std::uint64_t> m_hashes; // hash of every position reached, for repetition
    GameStatus                 m_status = GameStatus::Ongoing;
    std::optional<Color>       m_winner;
};

std::string colorName(Color c); // "White" / "Black"
} // namespace chess
