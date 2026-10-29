#include "engine/Bot.hpp"

#include <random>
#include <vector>

namespace engine
{
const char* difficultyName(Difficulty difficulty)
{
    switch (difficulty)
    {
        case Difficulty::Easy:   return "Easy";
        case Difficulty::Medium: return "Medium";
        case Difficulty::Hard:   return "Hard";
    }
    return "";
}

SearchSettings settingsFor(Difficulty difficulty)
{
    SearchSettings s;
    switch (difficulty)
    {
        case Difficulty::Easy:
            s.depth      = 1;
            s.quiescence = false; // does not see that a capture can be taken back
            s.noise      = 150;   // often picks a clearly weaker move
            break;
        case Difficulty::Medium:
            s.depth      = 2;
            s.quiescence = true;
            s.noise      = 30;
            break;
        case Difficulty::Hard:
            s.depth      = 4;
            s.quiescence = true;
            s.noise      = 0;
            break;
    }
    return s;
}

SearchResult chooseMove(const chess::Game& game, Difficulty difficulty, std::uint32_t seed,
                        const std::atomic<bool>* stop)
{
    // Rebuild the hashes of the positions before the current one.
    std::vector<std::uint64_t> history;
    history.reserve(game.moves().size());
    chess::Position pos = game.startPosition();
    for (const chess::Move& move : game.moves())
    {
        history.push_back(pos.hash());
        pos.makeMove(move);
    }

    std::mt19937 rng(seed);
    return search(game.position(), history, settingsFor(difficulty), rng, stop);
}
} // namespace engine
