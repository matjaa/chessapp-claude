#pragma once

#include "core/Types.hpp"
#include "gui/BoardView.hpp"
#include "gui/PieceTextures.hpp"

#include <SFML/Graphics.hpp>

#include <array>
#include <optional>

namespace gui
{
// The choice shown when a pawn reaches the last rank: queen, rook, bishop
// and knight in a column on the promotion square, the rest of the board dimmed.
class PromotionPicker
{
public:
    explicit PromotionPicker(const PieceTextures& pieces);

    void open(chess::Square from, chess::Square to, chess::Color color);
    void close() { m_open = false; }
    bool isOpen() const { return m_open; }

    chess::Square from() const { return m_from; }
    chess::Square to() const { return m_to; }

    // Result of a click while the picker is open.
    struct ClickResult
    {
        bool                            cancelled = false; // clicked outside the choices
        std::optional<chess::PieceType> choice;            // chosen piece
    };
    ClickResult handleClick(sf::Vector2f point, const BoardView& board);

    void draw(sf::RenderTarget& target, const BoardView& board, sf::Vector2f mouse) const;

private:
    // Screen rectangle of choice i (0 = queen ... 3 = knight).
    sf::FloatRect choiceRect(int i, const BoardView& board) const;

    static constexpr std::array<chess::PieceType, 4> choices = {chess::PieceType::Queen, chess::PieceType::Rook,
                                                                 chess::PieceType::Bishop, chess::PieceType::Knight};

    const PieceTextures& m_pieces;
    bool                 m_open  = false;
    chess::Square        m_from  = chess::NoSquare;
    chess::Square        m_to    = chess::NoSquare;
    chess::Color         m_color = chess::Color::White;
};
} // namespace gui
