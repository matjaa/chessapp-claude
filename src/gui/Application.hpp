#pragma once

#include "core/Game.hpp"
#include "engine/Bot.hpp"
#include "gui/BoardView.hpp"
#include "gui/Button.hpp"
#include "gui/PieceTextures.hpp"
#include "gui/PromotionPicker.hpp"
#include "gui/Theme.hpp"

#include <SFML/Graphics.hpp>

#include <array>
#include <atomic>
#include <future>
#include <memory>
#include <optional>
#include <random>

namespace gui
{
// Owns the window and the game, handles input and draws everything.
//
// You play one colour, a bot plays the other. The bot thinks on a
// background thread so the window stays responsive.
//
// The side panel has two views. Between games (before the first one and
// after one ends) it shows the choices: play as White/Black, bot level and
// Start/New game. During a game it shows the move list and Resign instead.
// Flip board is always there.
class Application
{
public:
    Application();
    ~Application();

    // The board and texts keep references to the fonts and textures, so the
    // object must stay in place.
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void handleEvents();
    void onBoardClick(chess::Square sq);
    void tryMove(chess::Square from, chess::Square to);
    void select(chess::Square sq);
    void clearSelection();
    void playHumanMove(const chess::Move& move); // also starts the game if needed
    void newGame();                              // resets the board and starts playing
    void playAs(chess::Color color);
    void setDifficulty(engine::Difficulty difficulty);
    void resign();

    // True from Start/New game (or your first move) until the game ends.
    bool isInGame() const { return m_started && !m_game.isOver(); }

    // Handles a left click on the side panel. Returns true if a button was hit.
    bool handlePanelClick(sf::Vector2f point);
    // Sets the buttons' labels, positions and states for the current view.
    void updateButtons();

    bool isHumanTurn() const { return m_game.sideToMove() == m_humanColor; }
    // Starts the bot when it is its turn and plays its move once it is ready.
    void updateBot();
    // Stops a running bot search and waits for it (it ends within milliseconds).
    void cancelBot();
    bool isBotThinking() const { return m_botTask.valid(); }

    BoardHighlights currentHighlights() const;
    std::optional<chess::Square> hoveredTarget() const;

    void render();
    void drawPanel();
    void drawStatus();
    // The moves in notation, two per row; the newest rows when they do not all fit.
    void drawMoveList(float top, float bottom);

    sf::RenderWindow m_window;
    sf::Font         m_font;
    sf::Font         m_boldFont;
    PieceTextures    m_pieces;
    BoardView        m_board;
    PromotionPicker  m_promotion;

    chess::Game                  m_game;
    bool                         m_started = false; // a game was started (it may be over now)
    std::optional<chess::Square> m_selected;
    chess::MoveList              m_selectedMoves; // legal moves of the selected piece
    sf::Vector2f                 m_mouse;         // mouse position in window coordinates

    chess::Color                       m_humanColor = chess::Color::White;
    engine::Difficulty                 m_difficulty = engine::Difficulty::Medium;
    std::future<engine::SearchResult>  m_botTask;  // valid while the bot is thinking
    std::shared_ptr<std::atomic<bool>> m_botStop;  // shared with the running search
    sf::Clock                          m_botClock; // time since the bot started thinking
    std::mt19937                       m_rng{std::random_device{}()};

    // Side panel buttons (positions are set in the constructor).
    Button                m_whiteButton;
    Button                m_blackButton;
    std::array<Button, 3> m_levelButtons; // Easy, Medium, Hard
    Button                m_newGameButton;
    Button                m_flipButton;
    Button                m_resignButton;
    bool                  m_confirmResign = false; // Resign was clicked once, a second click resigns
    sf::Clock             m_resignClock;           // time since the first click
};
} // namespace gui
