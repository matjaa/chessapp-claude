// Tests for the bots in src/engine (no GUI needed).
//
//   engine_tests            run everything
//   engine_tests --quick    skip the Hard-bot game (useful in Debug builds)
//
// Exit code 0 means all tests passed.

#include "core/Game.hpp"
#include "engine/Bot.hpp"
#include "engine/Evaluation.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

using namespace chess;
using namespace engine;

namespace
{
int g_failures = 0;
int g_checks   = 0;

#define CHECK(condition)                                                                       \
    do                                                                                         \
    {                                                                                          \
        ++g_checks;                                                                            \
        if (!(condition))                                                                      \
        {                                                                                      \
            ++g_failures;                                                                      \
            std::cout << "  FAILED: " #condition "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        }                                                                                      \
    } while (false)

constexpr Difficulty allLevels[] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};

std::string bestMove(const char* fen, Difficulty level, std::uint32_t seed = 1)
{
    const Game game(Position::fromFen(fen));
    const SearchResult result = chooseMove(game, level, seed);
    return result.found ? result.move.toUci() : "none";
}

void testEvaluation()
{
    std::cout << "Evaluation\n";
    // The start position is symmetric.
    CHECK(evaluate(Position()) == 0);
    // A queen up is good for the side that has it, whoever is to move.
    CHECK(evaluate(Position::fromFen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1")) > 800);
    CHECK(evaluate(Position::fromFen("4k3/8/8/8/8/8/8/3QK3 b - - 0 1")) < -800);
    // Mirrored positions get the same score for the side to move.
    CHECK(evaluate(Position::fromFen("4k3/8/8/8/3N4/8/PPP5/4K3 w - - 0 1")) ==
          evaluate(Position::fromFen("4k3/ppp5/8/3n4/8/8/8/4K3 b - - 0 1")));
}

void testMateInOne()
{
    std::cout << "Every level plays mate in one\n";
    // Back-rank mate: Ra1-a8#.
    for (Difficulty level : allLevels)
        for (std::uint32_t seed = 1; seed <= 5; ++seed)
            CHECK(bestMove("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1", level, seed) == "a1a8");
}

void testMateInTwo()
{
    std::cout << "Hard finds mate in two\n";
    // Rook ladder: 1. Ra7 (or Rb7) Kg8 2. R...8#.
    const Game game(Position::fromFen("7k/8/8/8/8/8/R7/1R4K1 w - - 0 1"));
    const SearchResult result = chooseMove(game, Difficulty::Hard, 1);
    CHECK(result.found);
    CHECK(result.score >= MateThreshold);
}

void testTactics()
{
    std::cout << "Medium and Hard take a free queen and avoid a poisoned pawn\n";
    for (Difficulty level : {Difficulty::Medium, Difficulty::Hard})
    {
        // The black queen on d5 is undefended.
        CHECK(bestMove("4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1", level) == "d2d5");
        // Qxd6 would lose the queen to c7xd6.
        CHECK(bestMove("4k3/2p5/3p4/8/8/8/8/3QK3 w - - 0 1", level) != "d1d6");
    }
}

void testNoMoves()
{
    std::cout << "No move in a finished game\n";
    // Black is checkmated.
    const Game game(Position::fromFen("R5k1/5ppp/8/8/8/8/8/6K1 b - - 0 1"));
    CHECK(!chooseMove(game, Difficulty::Hard, 1).found);
}

void testStopFlag()
{
    std::cout << "Stop flag ends the search\n";
    const std::atomic<bool> stop{true};
    const SearchResult result = chooseMove(Game(), Difficulty::Hard, 1, &stop);
    CHECK(result.aborted);
    CHECK(result.found); // a legal move is still returned
}

// Plays a whole game between two levels. Returns +1 if White wins,
// -1 if Black wins, 0 for a draw (or after maxPlies).
int playGame(Difficulty white, Difficulty black, std::uint32_t seed, double& slowestMs, int maxPlies = 300)
{
    Game game;
    for (int ply = 0; ply < maxPlies && !game.isOver(); ++ply)
    {
        const Difficulty level = game.sideToMove() == Color::White ? white : black;
        const auto start  = std::chrono::steady_clock::now();
        const auto result = chooseMove(game, level, seed + static_cast<std::uint32_t>(ply));
        const double ms   = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (level == Difficulty::Hard && ms > slowestMs)
            slowestMs = ms;

        CHECK(result.found);
        const bool legal = result.found && game.makeMove(result.move);
        CHECK(legal);
        if (!legal)
            return 0;
    }
    if (!game.winner())
        return 0;
    return *game.winner() == Color::White ? 1 : -1;
}

void testGames(bool quick)
{
    std::cout << "Bot games (all moves legal)\n";
    double slowest = 0.0;

    // Medium should beat Easy most of the time.
    int mediumPoints = 0; // win = 2, draw = 1
    const int games = 4;
    for (int i = 0; i < games; ++i)
    {
        const bool mediumWhite = i % 2 == 0;
        const int  r = playGame(mediumWhite ? Difficulty::Medium : Difficulty::Easy,
                                mediumWhite ? Difficulty::Easy : Difficulty::Medium,
                                static_cast<std::uint32_t>(100 + i * 1000), slowest);
        mediumPoints += 1 + (mediumWhite ? r : -r);
    }
    std::cout << "  Medium vs Easy: " << mediumPoints / 2.0 << " / " << games << " points for Medium\n";
    CHECK(mediumPoints > games); // more than half the points

    if (quick)
        return;

    const int r = playGame(Difficulty::Hard, Difficulty::Medium, 7, slowest);
    std::cout << "  Hard (White) vs Medium: " << (r > 0 ? "Hard wins" : r < 0 ? "Medium wins" : "draw") << '\n';
    std::cout << "  slowest Hard move: " << static_cast<int>(slowest) << " ms\n";
    CHECK(r >= 0);
}
} // namespace

int main(int argc, char** argv)
{
    const bool quick = argc > 1 && std::strcmp(argv[1], "--quick") == 0;

    std::cout << "== Engine tests ==\n";
    testEvaluation();
    testMateInOne();
    testMateInTwo();
    testTactics();
    testNoMoves();
    testStopFlag();
    testGames(quick);

    std::cout << "\n" << (g_checks - g_failures) << " / " << g_checks << " checks passed\n";
    if (g_failures == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << g_failures << " FAILED\n";
    return g_failures == 0 ? 0 : 1;
}
