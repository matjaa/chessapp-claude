#pragma once

#include <SFML/Graphics/Color.hpp>

// Window layout (pixels) and colours used by the GUI.
namespace gui
{
namespace layout
{
inline constexpr unsigned windowWidth  = 1040;
inline constexpr unsigned windowHeight = 720;

inline constexpr float squareSize = 80.f;
inline constexpr float boardLeft  = 40.f; // room on the left for the rank labels 1-8
inline constexpr float boardTop   = 20.f;
inline constexpr float boardSize  = squareSize * 8.f;

inline constexpr float panelLeft  = boardLeft + boardSize + 30.f;
inline constexpr float panelTop   = boardTop;
inline constexpr float panelWidth = static_cast<float>(windowWidth) - panelLeft - 20.f;
} // namespace layout

namespace theme
{
inline const sf::Color background(48, 46, 43);
inline const sf::Color panel(38, 36, 33);
inline const sf::Color text(235, 235, 230);
inline const sf::Color mutedText(160, 158, 150);
inline const sf::Color alertText(235, 90, 80);
inline const sf::Color disabledText(105, 103, 98);

inline const sf::Color button(62, 59, 55);
inline const sf::Color buttonHover(80, 77, 72);
inline const sf::Color buttonSelected(105, 138, 72);      // chosen side / level
inline const sf::Color buttonSelectedHover(120, 155, 85);
inline const sf::Color buttonWarning(170, 55, 48);        // Resign waiting for confirmation
inline const sf::Color buttonWarningHover(195, 65, 55);

inline const sf::Color moveListBackground(30, 29, 27);

inline const sf::Color lightSquare(240, 217, 181);
inline const sf::Color darkSquare(181, 136, 99);
inline const sf::Color coordinateText(200, 196, 186);

inline const sf::Color lastMove(205, 210, 106, 150);  // tint on the from/to squares
inline const sf::Color selected(20, 85, 30, 120);     // selected piece's square
inline const sf::Color hoverTarget(20, 85, 30, 90);   // legal target under the mouse
inline const sf::Color moveMarker(20, 85, 30, 110);   // dots / rings on legal targets
inline const sf::Color checkCentre(255, 0, 0, 230);   // red glow under a king in check
inline const sf::Color checkEdge(231, 0, 0, 0);

inline const sf::Color overlayDim(0, 0, 0, 110);
inline const sf::Color promotionChoice(230, 230, 225);
inline const sf::Color promotionHover(250, 190, 90);
} // namespace theme
} // namespace gui
