#pragma once

// Basic chess types shared by the rules, the engine and the GUI.
// Nothing in src/core depends on SFML.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chess
{
// ---------------------------------------------------------------------------
// Colours and pieces
// ---------------------------------------------------------------------------
enum class Color : std::uint8_t
{
    White = 0,
    Black = 1
};

constexpr Color operator~(Color c)
{
    return c == Color::White ? Color::Black : Color::White;
}

constexpr int index(Color c)
{
    return static_cast<int>(c);
}

enum class PieceType : std::uint8_t
{
    None = 0,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King
};

struct Piece
{
    PieceType type  = PieceType::None;
    Color     color = Color::White;

    constexpr bool isNone() const { return type == PieceType::None; }
    constexpr bool is(Color c, PieceType t) const { return type == t && color == c; }

    friend constexpr bool operator==(Piece a, Piece b)
    {
        return a.type == b.type && (a.type == PieceType::None || a.color == b.color);
    }
    friend constexpr bool operator!=(Piece a, Piece b) { return !(a == b); }
};

// ---------------------------------------------------------------------------
// Squares: 0 = a1, 1 = b1, ..., 7 = h1, 8 = a2, ..., 63 = h8
// ---------------------------------------------------------------------------
using Square = int;

inline constexpr Square NoSquare = -1;

constexpr int fileOf(Square sq) { return sq & 7; }  // 0 = a-file ... 7 = h-file
constexpr int rankOf(Square sq) { return sq >> 3; } // 0 = rank 1 ... 7 = rank 8
constexpr Square makeSquare(int file, int rank) { return rank * 8 + file; }
constexpr bool isOnBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

// "e4" for square 28. Returns "-" for NoSquare.
std::string squareName(Square sq);
// Parses "e4" -> 28. Returns NoSquare if the text is not a square.
Square parseSquare(std::string_view text);

// Castling rights are stored as a bit set.
enum CastlingRight : int
{
    NoCastling       = 0,
    WhiteKingside    = 1,
    WhiteQueenside   = 2,
    BlackKingside    = 4,
    BlackQueenside   = 8,
    AllCastling      = 15
};

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------
struct Move
{
    enum Flag : std::uint8_t
    {
        Quiet      = 0,
        Capture    = 1 << 0,
        DoublePush = 1 << 1,
        EnPassant  = 1 << 2,
        Castling   = 1 << 3
    };

    std::uint8_t from      = 0;
    std::uint8_t to        = 0;
    PieceType    promotion = PieceType::None; // piece a pawn promotes to, if any
    std::uint8_t flags     = Quiet;

    constexpr Move() = default;
    constexpr Move(Square fromSq, Square toSq, std::uint8_t moveFlags = Quiet, PieceType promo = PieceType::None)
        : from(static_cast<std::uint8_t>(fromSq)),
          to(static_cast<std::uint8_t>(toSq)),
          promotion(promo),
          flags(moveFlags)
    {
    }

    constexpr Square fromSquare() const { return from; }
    constexpr Square toSquare() const { return to; }
    constexpr bool isCapture() const { return (flags & Capture) != 0; }
    constexpr bool isEnPassant() const { return (flags & EnPassant) != 0; }
    constexpr bool isCastling() const { return (flags & Castling) != 0; }
    constexpr bool isDoublePush() const { return (flags & DoublePush) != 0; }
    constexpr bool isPromotion() const { return promotion != PieceType::None; }

    // Two moves are the same if they go between the same squares with the
    // same promotion piece (flags follow from the position).
    friend constexpr bool operator==(const Move& a, const Move& b)
    {
        return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
    }
    friend constexpr bool operator!=(const Move& a, const Move& b) { return !(a == b); }

    // Coordinate notation, e.g. "e2e4" or "e7e8q". Used for debugging and tests.
    std::string toUci() const;
};

// Fixed-size move container (no heap allocation, fast enough for the engine).
// No legal chess position has more than 218 moves.
class MoveList
{
public:
    void push_back(const Move& move) { m_moves[m_size++] = move; }
    void clear() { m_size = 0; }

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

    Move& operator[](std::size_t i) { return m_moves[i]; }
    const Move& operator[](std::size_t i) const { return m_moves[i]; }

    Move* begin() { return m_moves.data(); }
    Move* end() { return m_moves.data() + m_size; }
    const Move* begin() const { return m_moves.data(); }
    const Move* end() const { return m_moves.data() + m_size; }

    bool contains(const Move& move) const
    {
        for (const Move& m : *this)
            if (m == move)
                return true;
        return false;
    }

private:
    std::array<Move, 256> m_moves{};
    std::size_t           m_size = 0;
};
} // namespace chess
