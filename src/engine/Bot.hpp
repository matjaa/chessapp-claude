#pragma once

#include "core/Game.hpp"
#include "engine/Search.hpp"

#include <atomic>
#include <cstdint>

namespace engine
{
enum class Difficulty
{
    Easy,   // looks one move ahead, ignores recaptures, plays loosely
    Medium, // two moves ahead plus captures, a little randomness
    Hard    // four moves ahead plus captures, always its best move
};

const char* difficultyName(Difficulty difficulty); // "Easy" / "Medium" / "Hard"
SearchSettings settingsFor(Difficulty difficulty);

// Chooses a move for the side to move in `game`. The game's earlier
// positions are passed to the search so the bot knows about repetitions.
// `seed` drives the random choices. Safe to call from a background thread
// as long as `game` is not changed meanwhile (pass a copy).
SearchResult chooseMove(const chess::Game& game, Difficulty difficulty, std::uint32_t seed,
                        const std::atomic<bool>* stop = nullptr);
} // namespace engine
