#pragma once

#include "core/Position.hpp"
#include "gui/PieceTextures.hpp"

#include <SFML/Graphics.hpp>

#include <optional>
#include <vector>

namespace gui
{
// Everything that is highlighted on the board in one frame.
struct BoardHighlights
{
    std::optional<chess::Square> selected;    // the selected piece
    std::vector<chess::Move>     targets;     // legal moves of the selected piece
    std::optional<chess::Move>   lastMove;    // the move just played
    std::optional<chess::Square> checkedKing; // king in check
    std::optional<chess::Square> hover;       // legal target under the mouse
};

// Draws the board, coordinates, highlights and pieces, and converts mouse
// positions to squares. When flipped, Black is at the bottom.
class BoardView
{
public:
    BoardView(const sf::Font& font, const PieceTextures& pieces);

    void setFlipped(bool flipped) { m_flipped = flipped; }
    bool isFlipped() const { return m_flipped; }

    // Top-left corner of a square on screen.
    sf::Vector2f squarePosition(chess::Square sq) const;
    // Square under a point on screen, or nothing if outside the board.
    std::optional<chess::Square> squareAt(sf::Vector2f point) const;
    // Screen row (0 = top) and column (0 = left) of a square.
    int screenRow(chess::Square sq) const;
    int screenColumn(chess::Square sq) const;

    void draw(sf::RenderTarget& target, const chess::Position& position, const BoardHighlights& highlights) const;

private:
    void drawSquares(sf::RenderTarget& target) const;
    void drawCoordinates(sf::RenderTarget& target) const;
    void fillSquare(sf::RenderTarget& target, chess::Square sq, sf::Color color) const;
    void drawCheckGlow(sf::RenderTarget& target, chess::Square sq) const;
    void drawMoveMarker(sf::RenderTarget& target, chess::Square sq, bool capture) const;

    const sf::Font&      m_font;
    const PieceTextures& m_pieces;
    bool                 m_flipped = false;
};
} // namespace gui
