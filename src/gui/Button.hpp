#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <utility>

namespace gui
{
// A rectangular button with a centred label. It can be disabled, and a
// "selected" button is drawn highlighted (used for the side and level choices).
// A "warning" button is drawn in red (used while Resign waits for confirmation).
class Button
{
public:
    Button(std::string label, sf::FloatRect rect);

    void setLabel(std::string label) { m_label = std::move(label); }
    void setRect(sf::FloatRect rect) { m_rect = rect; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setSelected(bool selected) { m_selected = selected; }
    void setWarning(bool warning) { m_warning = warning; }
    bool isEnabled() const { return m_enabled; }

    // True if the point is on the button and the button is enabled.
    bool isClicked(sf::Vector2f point) const;

    void draw(sf::RenderTarget& target, const sf::Font& font, sf::Vector2f mouse) const;

private:
    std::string   m_label;
    sf::FloatRect m_rect;
    bool          m_enabled  = true;
    bool          m_selected = false;
    bool          m_warning  = false;
};
} // namespace gui
