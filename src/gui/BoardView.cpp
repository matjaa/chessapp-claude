#include "gui/BoardView.hpp"

#include "gui/Theme.hpp"

#include <cmath>
#include <string>

namespace gui
{
using chess::Square;

BoardView::BoardView(const sf::Font& font, const PieceTextures& pieces) : m_font(font), m_pieces(pieces)
{
}

int BoardView::screenRow(Square sq) const
{
    return m_flipped ? chess::rankOf(sq) : 7 - chess::rankOf(sq);
}

int BoardView::screenColumn(Square sq) const
{
    return m_flipped ? 7 - chess::fileOf(sq) : chess::fileOf(sq);
}

sf::Vector2f BoardView::squarePosition(Square sq) const
{
    return {layout::boardLeft + static_cast<float>(screenColumn(sq)) * layout::squareSize,
            layout::boardTop + static_cast<float>(screenRow(sq)) * layout::squareSize};
}

std::optional<Square> BoardView::squareAt(sf::Vector2f point) const
{
    const float x = point.x - layout::boardLeft;
    const float y = point.y - layout::boardTop;
    if (x < 0.f || y < 0.f || x >= layout::boardSize || y >= layout::boardSize)
        return std::nullopt;
    const int column = static_cast<int>(x / layout::squareSize);
    const int row    = static_cast<int>(y / layout::squareSize);
    const int file   = m_flipped ? 7 - column : column;
    const int rank   = m_flipped ? row : 7 - row;
    return chess::makeSquare(file, rank);
}

void BoardView::draw(sf::RenderTarget& target, const chess::Position& position, const BoardHighlights& h) const
{
    drawSquares(target);
    drawCoordinates(target);

    if (h.lastMove)
    {
        fillSquare(target, h.lastMove->fromSquare(), theme::lastMove);
        fillSquare(target, h.lastMove->toSquare(), theme::lastMove);
    }
    if (h.selected)
        fillSquare(target, *h.selected, theme::selected);
    if (h.hover)
        fillSquare(target, *h.hover, theme::hoverTarget);
    if (h.checkedKing)
        drawCheckGlow(target, *h.checkedKing);

    // Pieces.
    for (Square sq = 0; sq < 64; ++sq)
    {
        const chess::Piece piece = position.pieceAt(sq);
        if (!piece.isNone())
            target.draw(m_pieces.makeSprite(piece, squarePosition(sq), layout::squareSize));
    }

    // Legal move markers go on top of the pieces (rings around capturable pieces).
    for (const chess::Move& move : h.targets)
    {
        if (h.hover && *h.hover == move.toSquare())
            continue; // the hover highlight already marks it
        const bool capture = move.isCapture() && !move.isEnPassant();
        drawMoveMarker(target, move.toSquare(), capture);
    }
}

void BoardView::drawSquares(sf::RenderTarget& target) const
{
    sf::RectangleShape square({layout::squareSize, layout::squareSize});
    for (Square sq = 0; sq < 64; ++sq)
    {
        const bool dark = (chess::fileOf(sq) + chess::rankOf(sq)) % 2 == 0; // a1 is dark
        square.setFillColor(dark ? theme::darkSquare : theme::lightSquare);
        square.setPosition(squarePosition(sq));
        target.draw(square);
    }
}

void BoardView::drawCoordinates(sf::RenderTarget& target) const
{
    const unsigned size = 18;
    for (int i = 0; i < 8; ++i)
    {
        // Ranks 1-8 along the left edge.
        const Square rankSquare = chess::makeSquare(0, i);
        sf::Text     rankLabel(m_font, std::string(1, static_cast<char>('1' + i)), size);
        rankLabel.setFillColor(theme::coordinateText);
        rankLabel.setOrigin(rankLabel.getLocalBounds().getCenter());
        rankLabel.setPosition({layout::boardLeft / 2.f,
                               squarePosition(rankSquare).y + layout::squareSize / 2.f});
        target.draw(rankLabel);

        // Files A-H along the bottom edge.
        const Square fileSquare = chess::makeSquare(i, 0);
        sf::Text     fileLabel(m_font, std::string(1, static_cast<char>('A' + i)), size);
        fileLabel.setFillColor(theme::coordinateText);
        fileLabel.setOrigin(fileLabel.getLocalBounds().getCenter());
        fileLabel.setPosition({squarePosition(fileSquare).x + layout::squareSize / 2.f,
                               layout::boardTop + layout::boardSize + 20.f});
        target.draw(fileLabel);
    }
}

void BoardView::fillSquare(sf::RenderTarget& target, Square sq, sf::Color color) const
{
    sf::RectangleShape rect({layout::squareSize, layout::squareSize});
    rect.setPosition(squarePosition(sq));
    rect.setFillColor(color);
    target.draw(rect);
}

void BoardView::drawCheckGlow(sf::RenderTarget& target, Square sq) const
{
    // Radial gradient: a triangle fan from a red centre to a transparent rim.
    const sf::Vector2f centre = squarePosition(sq) + sf::Vector2f(layout::squareSize, layout::squareSize) / 2.f;
    const float        radius = layout::squareSize * 0.62f;
    const int          points = 40;

    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, static_cast<std::size_t>(points + 2));
    fan[0].position = centre;
    fan[0].color    = theme::checkCentre;
    for (int i = 0; i <= points; ++i)
    {
        const float angle = static_cast<float>(i) / static_cast<float>(points) * 6.2831853f;
        auto&       v     = fan[static_cast<std::size_t>(i + 1)];
        v.position        = centre + sf::Vector2f(std::cos(angle), std::sin(angle)) * radius;
        v.color           = theme::checkEdge;
    }
    // Keep the glow inside its square.
    sf::View   clipView = target.getView();
    const auto size     = sf::Vector2f(target.getSize());
    const auto topLeft  = squarePosition(sq);
    clipView.setViewport(sf::FloatRect({topLeft.x / size.x, topLeft.y / size.y},
                                       {layout::squareSize / size.x, layout::squareSize / size.y}));
    clipView.setCenter(topLeft + sf::Vector2f(layout::squareSize, layout::squareSize) / 2.f);
    clipView.setSize({layout::squareSize, layout::squareSize});

    const sf::View previous = target.getView();
    target.setView(clipView);
    target.draw(fan);
    target.setView(previous);
}

void BoardView::drawMoveMarker(sf::RenderTarget& target, Square sq, bool capture) const
{
    const sf::Vector2f centre = squarePosition(sq) + sf::Vector2f(layout::squareSize, layout::squareSize) / 2.f;
    if (capture)
    {
        // Ring around the piece that can be captured.
        const float     radius    = layout::squareSize * 0.46f;
        const float     thickness = layout::squareSize * 0.07f;
        sf::CircleShape ring(radius - thickness, 48);
        ring.setOrigin({radius - thickness, radius - thickness});
        ring.setPosition(centre);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(thickness);
        ring.setOutlineColor(theme::moveMarker);
        target.draw(ring);
    }
    else
    {
        const float     radius = layout::squareSize * 0.16f;
        sf::CircleShape dot(radius, 32);
        dot.setOrigin({radius, radius});
        dot.setPosition(centre);
        dot.setFillColor(theme::moveMarker);
        target.draw(dot);
    }
}
} // namespace gui
