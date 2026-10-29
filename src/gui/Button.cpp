#include "gui/Button.hpp"

#include "gui/Theme.hpp"

namespace gui
{
Button::Button(std::string label, sf::FloatRect rect)
    : m_label(std::move(label)),
      m_rect(rect)
{
}

bool Button::isClicked(sf::Vector2f point) const
{
    return m_enabled && m_rect.contains(point);
}

void Button::draw(sf::RenderTarget& target, const sf::Font& font, sf::Vector2f mouse) const
{
    const bool hovered = m_enabled && m_rect.contains(mouse);

    sf::Color fill = theme::button;
    if (m_warning)
        fill = hovered ? theme::buttonWarningHover : theme::buttonWarning;
    else if (m_selected)
        fill = hovered ? theme::buttonSelectedHover : theme::buttonSelected;
    else if (hovered)
        fill = theme::buttonHover;

    sf::RectangleShape shape(m_rect.size);
    shape.setPosition(m_rect.position);
    shape.setFillColor(fill);
    target.draw(shape);

    sf::Text text(font, m_label, 16);
    text.setFillColor(m_enabled ? theme::text : theme::disabledText);
    // Centre horizontally on the glyphs and vertically on the capital-letter
    // height, so labels with and without descenders ("Easy", "Hard") line up.
    const sf::FloatRect bounds = text.getLocalBounds();
    const sf::FloatRect capital = sf::Text(font, "H", 16).getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.f, capital.position.y + capital.size.y / 2.f});
    text.setPosition({m_rect.position.x + m_rect.size.x / 2.f, m_rect.position.y + m_rect.size.y / 2.f});
    target.draw(text);
}
} // namespace gui
