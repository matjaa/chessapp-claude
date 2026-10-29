#include "gui/Application.hpp"

#include "gui/Assets.hpp"

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace gui
{
namespace
{
// The bot waits at least this long before moving, so its moves can be followed.
const sf::Time minimumBotThinkTime = sf::milliseconds(500);
// A first click on Resign must be confirmed by a second one within this time.
const sf::Time resignConfirmTime = sf::seconds(3);

// Side panel layout, top to bottom: status, then either the game choices
// ("Play as", "Bot level", Start/New game) or the move list, and at the
// bottom Flip board (with Resign next to it during a game).
namespace panel
{
constexpr float padding      = 18.f;
constexpr float left         = layout::panelLeft + padding;
constexpr float width        = layout::panelWidth - 2.f * padding;
constexpr float gap          = 10.f;
constexpr float buttonHeight = 40.f;

constexpr float headlineY     = layout::panelTop + padding;
constexpr float detailY       = headlineY + 38.f;
constexpr float infoY         = detailY + 32.f; // "You play White vs Medium bot"
constexpr float sideLabelY    = layout::panelTop + 140.f;
constexpr float sideButtonsY  = sideLabelY + 24.f;
constexpr float levelLabelY   = sideButtonsY + buttonHeight + 22.f;
constexpr float levelButtonsY = levelLabelY + 24.f;
constexpr float newGameY      = levelButtonsY + buttonHeight + 34.f;
constexpr float bottomRowY    = layout::panelTop + layout::boardSize - padding - buttonHeight;

// Move list area: below the status during a game, below the choices otherwise.
constexpr float listTopInGame  = layout::panelTop + 140.f;
constexpr float listTopBetween = newGameY + buttonHeight + 24.f;
constexpr float listBottom     = bottomRowY - 16.f;
constexpr float listRowHeight  = 24.f;

// Rectangle of button `column` in a row of `columns` equally wide buttons.
sf::FloatRect buttonRect(int column, int columns, float y)
{
    const float w = (width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    return {{left + static_cast<float>(column) * (w + gap), y}, {w, buttonHeight}};
}
} // namespace panel

sf::Font loadFont(const char* relativePath)
{
    const auto path = assetPath(relativePath);
    sf::Font   font;
    if (!font.openFromFile(path))
        throw std::runtime_error("Could not load font: " + path.string());
    return font;
}

// Text whose top-left corner is exactly at `position` (SFML adds some
// space above the glyphs, which is removed here).
sf::Text makeText(const sf::Font& font, const std::string& string, unsigned size, sf::Color color,
                  sf::Vector2f position)
{
    sf::Text text(font, string, size);
    text.setFillColor(color);
    text.setOrigin({0.f, text.getLocalBounds().position.y});
    text.setPosition(position);
    return text;
}
} // namespace

Application::Application()
    : m_window(sf::VideoMode({layout::windowWidth, layout::windowHeight}),
               "Chess",
               sf::Style::Titlebar | sf::Style::Close),
      m_font(loadFont("fonts/DejaVuSans.ttf")),
      m_boldFont(loadFont("fonts/DejaVuSans-Bold.ttf")),
      m_board(m_font, m_pieces),
      m_promotion(m_pieces),
      m_whiteButton("White", panel::buttonRect(0, 2, panel::sideButtonsY)),
      m_blackButton("Black", panel::buttonRect(1, 2, panel::sideButtonsY)),
      m_levelButtons{{Button("Easy", panel::buttonRect(0, 3, panel::levelButtonsY)),
                      Button("Medium", panel::buttonRect(1, 3, panel::levelButtonsY)),
                      Button("Hard", panel::buttonRect(2, 3, panel::levelButtonsY))}},
      m_newGameButton("Start game", panel::buttonRect(0, 1, panel::newGameY)),
      m_flipButton("Flip board", panel::buttonRect(0, 1, panel::bottomRowY)),
      m_resignButton("Resign", panel::buttonRect(1, 2, panel::bottomRowY))
{
    m_window.setFramerateLimit(60);
}

Application::~Application()
{
    cancelBot();
}

void Application::run()
{
    while (m_window.isOpen())
    {
        handleEvents();
        updateBot();
        updateButtons();
        render();
    }
}

// ---------------------------------------------------------------------------
// Bot
// ---------------------------------------------------------------------------
void Application::updateBot()
{
    if (!isBotThinking())
    {
        if (!isInGame() || isHumanTurn())
            return;
        // The search gets its own copy of the game and a stop flag it shares with us.
        m_botStop = std::make_shared<std::atomic<bool>>(false);
        m_botClock.restart();
        m_botTask = std::async(std::launch::async,
                               [game = m_game, difficulty = m_difficulty, seed = m_rng(), stop = m_botStop]
                               { return engine::chooseMove(game, difficulty, seed, stop.get()); });
        return;
    }

    if (m_botClock.getElapsedTime() < minimumBotThinkTime ||
        m_botTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    const engine::SearchResult result = m_botTask.get();
    if (result.found && !result.aborted)
        m_game.makeMove(result.move);
}

void Application::cancelBot()
{
    if (!isBotThinking())
        return;
    m_botStop->store(true);
    m_botTask.wait();
    m_botTask = {};
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void Application::handleEvents()
{
    while (const std::optional event = m_window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_window.close();
        }
        else if (const auto* moved = event->getIf<sf::Event::MouseMoved>())
        {
            m_mouse = m_window.mapPixelToCoords(moved->position);
        }
        else if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            m_mouse = m_window.mapPixelToCoords(pressed->position);
            if (pressed->button == sf::Mouse::Button::Right)
            {
                m_promotion.close();
                clearSelection();
                continue;
            }
            if (pressed->button != sf::Mouse::Button::Left)
                continue;

            if (handlePanelClick(m_mouse))
                continue;
            if (m_promotion.isOpen())
            {
                const auto result = m_promotion.handleClick(m_mouse, m_board);
                if (result.choice)
                {
                    if (const auto move = m_game.findLegalMove(m_promotion.from(), m_promotion.to(), *result.choice))
                        playHumanMove(*move);
                }
                clearSelection();
                continue;
            }
            if (const auto sq = m_board.squareAt(m_mouse))
                onBoardClick(*sq);
            else
                clearSelection();
        }
        else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                if (m_promotion.isOpen() || m_selected)
                {
                    m_promotion.close();
                    clearSelection();
                }
                else
                {
                    m_window.close();
                }
            }
        }
    }
}

bool Application::handlePanelClick(sf::Vector2f point)
{
    if (m_flipButton.isClicked(point))
    {
        m_board.setFlipped(!m_board.isFlipped());
        return true;
    }

    if (!isInGame())
    {
        // Game choices, only shown between games.
        if (m_whiteButton.isClicked(point))
        {
            playAs(chess::Color::White);
            return true;
        }
        if (m_blackButton.isClicked(point))
        {
            playAs(chess::Color::Black);
            return true;
        }
        for (std::size_t i = 0; i < m_levelButtons.size(); ++i)
        {
            if (m_levelButtons[i].isClicked(point))
            {
                setDifficulty(static_cast<engine::Difficulty>(i));
                return true;
            }
        }
        if (m_newGameButton.isClicked(point))
        {
            newGame();
            return true;
        }
        return false;
    }

    if (m_resignButton.isClicked(point))
    {
        if (m_confirmResign)
        {
            resign();
        }
        else
        {
            m_confirmResign = true;
            m_resignClock.restart();
        }
        return true;
    }
    return false;
}

void Application::updateButtons()
{
    if (m_confirmResign && (m_game.isOver() || m_resignClock.getElapsedTime() > resignConfirmTime))
        m_confirmResign = false;

    m_whiteButton.setSelected(m_humanColor == chess::Color::White);
    m_blackButton.setSelected(m_humanColor == chess::Color::Black);
    for (std::size_t i = 0; i < m_levelButtons.size(); ++i)
        m_levelButtons[i].setSelected(static_cast<engine::Difficulty>(i) == m_difficulty);

    m_newGameButton.setLabel(m_started ? "New game" : "Start game");

    // During a game Flip board shares the bottom row with Resign.
    m_flipButton.setRect(panel::buttonRect(0, isInGame() ? 2 : 1, panel::bottomRowY));
    m_resignButton.setWarning(m_confirmResign);
    m_resignButton.setLabel(m_confirmResign ? "Confirm?" : "Resign");
}

void Application::onBoardClick(chess::Square sq)
{
    if (m_game.isOver() || !isHumanTurn())
        return;

    // Second click: move the selected piece if the square is a legal target.
    if (m_selected)
    {
        for (const chess::Move& move : m_selectedMoves)
        {
            if (move.toSquare() == sq)
            {
                tryMove(*m_selected, sq);
                return;
            }
        }
        if (*m_selected == sq)
        {
            clearSelection(); // clicking the selected piece again deselects it
            return;
        }
    }

    // First click (or clicking another own piece): select it.
    const chess::Piece piece = m_game.position().pieceAt(sq);
    if (!piece.isNone() && piece.color == m_game.sideToMove())
        select(sq);
    else
        clearSelection();
}

void Application::tryMove(chess::Square from, chess::Square to)
{
    if (m_game.isPromotion(from, to))
    {
        m_promotion.open(from, to, m_game.sideToMove()); // the move is made when a piece is chosen
        return;
    }
    if (const auto move = m_game.findLegalMove(from, to))
        playHumanMove(*move);
    clearSelection();
}

void Application::playHumanMove(const chess::Move& move)
{
    // Before the first game you can also start it by simply moving (as White).
    if (m_game.makeMove(move))
        m_started = true;
}

void Application::select(chess::Square sq)
{
    m_selected = sq;
    m_selectedMoves.clear();
    for (const chess::Move& move : m_game.legalMoves())
    {
        // A promotion appears four times (one per piece); show its square once.
        if (move.fromSquare() == sq && (move.promotion == chess::PieceType::None || move.promotion == chess::PieceType::Queen))
            m_selectedMoves.push_back(move);
    }
}

void Application::clearSelection()
{
    m_selected.reset();
    m_selectedMoves.clear();
}

void Application::newGame()
{
    cancelBot();
    m_promotion.close();
    clearSelection();
    m_confirmResign = false;
    m_game.reset();
    m_started = true;
}

void Application::playAs(chess::Color color)
{
    // Only possible between games; the choice is used from the next start.
    m_humanColor = color;
    m_board.setFlipped(color == chess::Color::Black); // your pieces at the bottom
}

void Application::resign()
{
    cancelBot();
    m_promotion.close();
    clearSelection();
    m_confirmResign = false;
    m_game.resign(m_humanColor);
}

void Application::setDifficulty(engine::Difficulty difficulty)
{
    m_difficulty = difficulty; // only possible between games, so no bot is thinking
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
std::optional<chess::Square> Application::hoveredTarget() const
{
    if (!m_selected || m_promotion.isOpen())
        return std::nullopt;
    const auto sq = m_board.squareAt(m_mouse);
    if (!sq)
        return std::nullopt;
    for (const chess::Move& move : m_selectedMoves)
        if (move.toSquare() == *sq)
            return sq;
    return std::nullopt;
}

BoardHighlights Application::currentHighlights() const
{
    BoardHighlights h;
    h.selected = m_selected;
    if (!m_promotion.isOpen())
        h.targets.assign(m_selectedMoves.begin(), m_selectedMoves.end());
    h.lastMove = m_game.lastMove();
    h.hover    = hoveredTarget();

    const chess::Position& pos = m_game.position();
    if (pos.inCheck())
        h.checkedKing = pos.kingSquare(pos.sideToMove());
    return h;
}

void Application::render()
{
    m_window.clear(theme::background);
    m_board.draw(m_window, m_game.position(), currentHighlights());
    m_promotion.draw(m_window, m_board, m_mouse);
    drawPanel();
    m_window.display();
}

void Application::drawPanel()
{
    sf::RectangleShape background({layout::panelWidth, layout::boardSize});
    background.setPosition({layout::panelLeft, layout::panelTop});
    background.setFillColor(theme::panel);
    m_window.draw(background);

    drawStatus();

    if (isInGame())
    {
        drawMoveList(panel::listTopInGame, panel::listBottom);
        m_resignButton.draw(m_window, m_font, m_mouse);
    }
    else
    {
        m_window.draw(makeText(m_font, "Play as", 15, theme::mutedText, {panel::left, panel::sideLabelY}));
        m_window.draw(makeText(m_font, "Bot level", 15, theme::mutedText, {panel::left, panel::levelLabelY}));
        m_whiteButton.draw(m_window, m_font, m_mouse);
        m_blackButton.draw(m_window, m_font, m_mouse);
        for (const Button& button : m_levelButtons)
            button.draw(m_window, m_font, m_mouse);
        m_newGameButton.draw(m_window, m_font, m_mouse);
        drawMoveList(panel::listTopBetween, panel::listBottom); // the finished game's moves
    }
    m_flipButton.draw(m_window, m_font, m_mouse);
}

void Application::drawStatus()
{
    std::string headline;
    std::string detail;
    sf::Color   detailColor = theme::mutedText;
    if (!m_started)
    {
        headline = "New game";
        detail   = "Choose your side and the bot";
    }
    else if (m_game.isOver())
    {
        headline    = "Game over";
        detail      = m_game.resultText();
        detailColor = theme::text;
    }
    else
    {
        headline = isHumanTurn() ? "Your move" : "Bot is thinking...";
        if (m_game.position().inCheck())
        {
            detail      = "Check!";
            detailColor = theme::alertText;
        }
    }
    m_window.draw(makeText(m_boldFont, headline, 24, theme::text, {panel::left, panel::headlineY}));
    if (!detail.empty())
    {
        // Long results ("Draw by insufficient material") are made smaller to fit.
        unsigned size = 19;
        sf::Text text = makeText(m_font, detail, size, detailColor, {panel::left, panel::detailY});
        while (text.getLocalBounds().size.x > panel::width && size > 12)
            text = makeText(m_font, detail, --size, detailColor, {panel::left, panel::detailY});
        m_window.draw(text);
    }

    // The choices are hidden during a game, so say what they were.
    if (isInGame())
    {
        const std::string info = "You play " + chess::colorName(m_humanColor) + " vs " +
                                 engine::difficultyName(m_difficulty) + " bot";
        m_window.draw(makeText(m_font, info, 15, theme::mutedText, {panel::left, panel::infoY}));
    }
}

void Application::drawMoveList(float top, float bottom)
{
    const std::vector<std::string>& moves = m_game.sanMoves();
    if (moves.empty())
        return;

    sf::RectangleShape box({panel::width, bottom - top});
    box.setPosition({panel::left, top});
    box.setFillColor(theme::moveListBackground);
    m_window.draw(box);

    // One row per move number: "12.  Nf3  Nc6". The game always starts with
    // White, so move i is White's when i is even.
    const std::size_t rows    = (moves.size() + 1) / 2;
    const auto        fitting = static_cast<std::size_t>((bottom - top - 12.f) / panel::listRowHeight);
    const std::size_t first   = rows > fitting ? rows - fitting : 0; // keep the newest moves in view

    const float numberX = panel::left + 10.f;
    const float whiteX  = panel::left + 58.f;
    const float blackX  = panel::left + 158.f;
    for (std::size_t row = first; row < rows; ++row)
    {
        const float y = top + 8.f + static_cast<float>(row - first) * panel::listRowHeight;
        m_window.draw(makeText(m_font, std::to_string(row + 1) + ".", 16, theme::mutedText, {numberX, y}));
        for (std::size_t side = 0; side < 2; ++side)
        {
            const std::size_t i = row * 2 + side;
            if (i >= moves.size())
                break;
            const bool      newest = i + 1 == moves.size();
            const sf::Font& font   = newest ? m_boldFont : m_font;
            m_window.draw(makeText(font, moves[i], 16, theme::text, {side == 0 ? whiteX : blackX, y}));
        }
    }
}
} // namespace gui
