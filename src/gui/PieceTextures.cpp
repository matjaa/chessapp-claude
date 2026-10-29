#include "gui/PieceTextures.hpp"

#include "gui/Assets.hpp"

#include <stdexcept>
#include <string>

namespace gui
{
namespace
{
std::size_t textureIndex(chess::Piece piece)
{
    // White pieces 0-5, black pieces 6-11, in PieceType order Pawn..King.
    const auto typeIndex = static_cast<std::size_t>(piece.type) - 1;
    return static_cast<std::size_t>(chess::index(piece.color)) * 6 + typeIndex;
}
} // namespace

PieceTextures::PieceTextures()
{
    const char  colors[2] = {'w', 'b'};
    const char  types[6]  = {'P', 'N', 'B', 'R', 'Q', 'K'};
    for (std::size_t c = 0; c < 2; ++c)
    {
        for (std::size_t t = 0; t < 6; ++t)
        {
            const std::string name = std::string("pieces/") + colors[c] + types[t] + ".png";
            const auto        path = assetPath(name);
            sf::Texture&      tex  = m_textures[c * 6 + t];
            if (!tex.loadFromFile(path))
                throw std::runtime_error("Could not load piece image: " + path.string());
            // The images are 160 px and drawn at 80 px: smoothing + mipmaps keep them crisp.
            tex.setSmooth(true);
            (void)tex.generateMipmap();
        }
    }
}

const sf::Texture& PieceTextures::get(chess::Piece piece) const
{
    return m_textures[textureIndex(piece)];
}

sf::Sprite PieceTextures::makeSprite(chess::Piece piece, sf::Vector2f position, float size) const
{
    const sf::Texture& tex = get(piece);
    sf::Sprite         sprite(tex);
    const float        scale = size / static_cast<float>(tex.getSize().x);
    sprite.setScale({scale, scale});
    sprite.setPosition(position);
    return sprite;
}
} // namespace gui
