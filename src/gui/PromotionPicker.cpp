#include "gui/PromotionPicker.hpp"

#include "gui/Theme.hpp"

namespace gui
{
PromotionPicker::PromotionPicker(const PieceTextures& pieces) : m_pieces(pieces)
{
}

void PromotionPicker::open(chess::Square from, chess::Square to, chess::Color color)
{
    m_open  = true;
    m_from  = from;
    m_to    = to;
    m_color = color;
}

sf::FloatRect PromotionPicker::choiceRect(int i, const BoardView& board) const
{
    // The column starts on the promotion square and grows towards the middle
    // of the board (downwards if that square is on the top row).
    const sf::Vector2f start = board.squarePosition(m_to);
    const float        step  = board.screenRow(m_to) == 0 ? layout::squareSize : -layout::squareSize;
    return sf::FloatRect({start.x, start.y + step * static_cast<float>(i)}, {layout::squareSize, layout::squareSize});
}

PromotionPicker::ClickResult PromotionPicker::handleClick(sf::Vector2f point, const BoardView& board)
{
    ClickResult result;
    for (int i = 0; i < 4; ++i)
    {
        if (choiceRect(i, board).contains(point))
        {
            result.choice = choices[static_cast<std::size_t>(i)];
            m_open        = false;
            return result;
        }
    }
    result.cancelled = true;
    m_open           = false;
    return result;
}

void PromotionPicker::draw(sf::RenderTarget& target, const BoardView& board, sf::Vector2f mouse) const
{
    if (!m_open)
        return;

    // Dim the board.
    sf::RectangleShape dim({layout::boardSize, layout::boardSize});
    dim.setPosition({layout::boardLeft, layout::boardTop});
    dim.setFillColor(theme::overlayDim);
    target.draw(dim);

    for (int i = 0; i < 4; ++i)
    {
        const sf::FloatRect rect    = choiceRect(i, board);
        const bool          hovered = rect.contains(mouse);
        const sf::Vector2f  centre  = rect.position + rect.size / 2.f;

        // Round background behind each piece.
        const float     radius = layout::squareSize * (hovered ? 0.5f : 0.45f);
        sf::CircleShape circle(radius, 48);
        circle.setOrigin({radius, radius});
        circle.setPosition(centre);
        circle.setFillColor(hovered ? theme::promotionHover : theme::promotionChoice);
        target.draw(circle);

        const float size = layout::squareSize * (hovered ? 1.0f : 0.85f);
        target.draw(m_pieces.makeSprite({choices[static_cast<std::size_t>(i)], m_color},
                                        centre - sf::Vector2f(size, size) / 2.f, size));
    }
}
} // namespace gui
