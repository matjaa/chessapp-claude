#pragma once

#include "core/Types.hpp"

#include <SFML/Graphics.hpp>

#include <array>

namespace gui
{
// Loads the 12 piece images from assets/pieces (wK.png, bQ.png, ...).
class PieceTextures
{
public:
    PieceTextures(); // throws std::runtime_error if an image is missing

    const sf::Texture& get(chess::Piece piece) const;

    // A sprite that draws the piece scaled to `size` pixels with its top-left at `position`.
    sf::Sprite makeSprite(chess::Piece piece, sf::Vector2f position, float size) const;

private:
    std::array<sf::Texture, 12> m_textures;
};
} // namespace gui
