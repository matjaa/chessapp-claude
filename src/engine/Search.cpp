#include "engine/Search.hpp"

#include "engine/Evaluation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace engine
{
using chess::Move;
using chess::MoveList;
using chess::PieceType;
using chess::Position;

namespace
{
constexpr int Infinity = MateScore + 1;
constexpr int MaxPly   = 64;

// Move ordering scores: good captures first, then queen promotions,
// then killer moves, then everything else, underpromotions last.
constexpr int CaptureBase   = 100000;
constexpr int PromotionBase = 90000;
constexpr int KillerBase    = 80000;

class Searcher
{
public:
    Searcher(const std::vector<std::uint64_t>& history, const SearchSettings& settings, const std::atomic<bool>* stop)
        : m_path(history), m_settings(settings), m_stop(stop)
    {
    }

    SearchResult run(const Position& root, std::mt19937& rng);

private:
    int negamax(const Position& pos, int depth, int alpha, int beta, int ply);
    int quiesce(const Position& pos, int alpha, int beta, int ply);

    // Counts a node and checks the stop flag now and then.
    bool shouldStop();
    bool isRepetition(const Position& pos) const;

    int  orderScore(const Position& pos, const Move& move, int ply) const;
    void scoreMoves(const Position& pos, const MoveList& moves, std::array<int, 256>& scores, int ply) const;
    // Swaps the best-scored remaining move to position i (lazy selection sort).
    static void pickNext(MoveList& moves, std::array<int, 256>& scores, std::size_t i);
    void storeKiller(const Move& move, int ply);

    std::vector<std::uint64_t>             m_path; // game history + positions on the current search path
    SearchSettings                         m_settings;
    const std::atomic<bool>*               m_stop;
    std::array<std::array<Move, 2>, MaxPly> m_killers{};
    std::uint64_t                          m_nodes   = 0;
    bool                                   m_aborted = false;
};

bool leavesKingInCheck(const Position& child)
{
    // After the move it is the opponent's turn; the mover's king must not be attacked.
    const chess::Color mover = ~child.sideToMove();
    return child.isSquareAttacked(child.kingSquare(mover), child.sideToMove());
}

bool Searcher::shouldStop()
{
    ++m_nodes;
    if ((m_nodes & 2047) == 0 && m_stop && m_stop->load(std::memory_order_relaxed))
        m_aborted = true;
    return m_aborted;
}

bool Searcher::isRepetition(const Position& pos) const
{
    // m_path.back() is the parent position. Positions with the same side to
    // move are 2, 4, 6... plies back, and only those since the last capture
    // or pawn move can be equal.
    const std::size_t limit = std::min(static_cast<std::size_t>(pos.halfmoveClock()), m_path.size());
    for (std::size_t back = 2; back <= limit; back += 2)
        if (m_path[m_path.size() - back] == pos.hash())
            return true;
    return false;
}

int Searcher::orderScore(const Position& pos, const Move& move, int ply) const
{
    if (move.isPromotion())
        return move.promotion == PieceType::Queen ? PromotionBase + (move.isCapture() ? 1000 : 0) : -1;
    if (move.isCapture())
    {
        // Most valuable victim, least valuable attacker.
        const PieceType victim   = move.isEnPassant() ? PieceType::Pawn : pos.pieceAt(move.toSquare()).type;
        const PieceType attacker = pos.pieceAt(move.fromSquare()).type;
        return CaptureBase + pieceValue(victim) * 10 - pieceValue(attacker) / 10;
    }
    if (ply < MaxPly)
    {
        if (move == m_killers[static_cast<std::size_t>(ply)][0])
            return KillerBase + 1;
        if (move == m_killers[static_cast<std::size_t>(ply)][1])
            return KillerBase;
    }
    return 0;
}

void Searcher::scoreMoves(const Position& pos, const MoveList& moves, std::array<int, 256>& scores, int ply) const
{
    for (std::size_t i = 0; i < moves.size(); ++i)
        scores[i] = orderScore(pos, moves[i], ply);
}

void Searcher::pickNext(MoveList& moves, std::array<int, 256>& scores, std::size_t i)
{
    std::size_t best = i;
    for (std::size_t j = i + 1; j < moves.size(); ++j)
        if (scores[j] > scores[best])
            best = j;
    std::swap(moves[i], moves[best]);
    std::swap(scores[i], scores[best]);
}

void Searcher::storeKiller(const Move& move, int ply)
{
    if (ply >= MaxPly)
        return;
    auto& killers = m_killers[static_cast<std::size_t>(ply)];
    if (killers[0] != move)
    {
        killers[1] = killers[0];
        killers[0] = move;
    }
}

int Searcher::negamax(const Position& pos, int depth, int alpha, int beta, int ply)
{
    if (pos.halfmoveClock() >= 100 || isRepetition(pos) || pos.isInsufficientMaterial())
        return 0;
    if (ply >= MaxPly)
        return evaluate(pos);

    const bool inCheck = pos.inCheck();
    if (inCheck)
        ++depth; // look one move further when in check, so mates are not missed

    if (depth <= 0)
        return m_settings.quiescence ? quiesce(pos, alpha, beta, ply) : evaluate(pos);
    if (shouldStop())
        return 0;

    MoveList moves;
    pos.generatePseudoLegalMoves(moves);
    std::array<int, 256> scores;
    scoreMoves(pos, moves, scores, ply);

    m_path.push_back(pos.hash());
    int best       = -Infinity;
    int legalMoves = 0;
    for (std::size_t i = 0; i < moves.size(); ++i)
    {
        pickNext(moves, scores, i);
        const Move& move = moves[i];

        Position child = pos;
        child.makeMove(move);
        if (leavesKingInCheck(child))
            continue;
        ++legalMoves;

        const int score = -negamax(child, depth - 1, -beta, -alpha, ply + 1);
        if (m_aborted)
            break;
        if (score > best)
            best = score;
        if (score > alpha)
            alpha = score;
        if (alpha >= beta)
        {
            if (!move.isCapture())
                storeKiller(move, ply);
            break;
        }
    }
    m_path.pop_back();

    if (m_aborted)
        return 0;
    if (legalMoves == 0)
        return inCheck ? -MateScore + ply : 0; // checkmate or stalemate
    return best;
}

int Searcher::quiesce(const Position& pos, int alpha, int beta, int ply)
{
    if (shouldStop())
        return 0;
    if (ply >= MaxPly)
        return evaluate(pos);

    // When in check every evasion is searched; otherwise the side to move
    // may "stand pat" and only captures and queen promotions are tried.
    const bool inCheck = pos.inCheck();
    int best = -Infinity;
    if (!inCheck)
    {
        best = evaluate(pos);
        if (best >= beta)
            return best;
        if (best > alpha)
            alpha = best;
    }

    MoveList moves;
    pos.generatePseudoLegalMoves(moves);
    std::array<int, 256> scores;
    scoreMoves(pos, moves, scores, MaxPly); // no killers here

    int legalMoves = 0;
    for (std::size_t i = 0; i < moves.size(); ++i)
    {
        pickNext(moves, scores, i);
        const Move& move = moves[i];
        if (!inCheck && !move.isCapture() && move.promotion != PieceType::Queen)
            continue;
        if (move.isPromotion() && move.promotion != PieceType::Queen)
            continue;

        Position child = pos;
        child.makeMove(move);
        if (leavesKingInCheck(child))
            continue;
        ++legalMoves;

        const int score = -quiesce(child, -beta, -alpha, ply + 1);
        if (m_aborted)
            return 0;
        if (score > best)
            best = score;
        if (score > alpha)
            alpha = score;
        if (alpha >= beta)
            break;
    }

    if (inCheck && legalMoves == 0)
        return -MateScore + ply;
    return best;
}

SearchResult Searcher::run(const Position& root, std::mt19937& rng)
{
    SearchResult result;

    MoveList moves;
    root.generateLegalMoves(moves);
    if (moves.empty())
        return result;

    // Shuffle first so that equally good moves are picked at random, then
    // put the most promising moves first.
    std::shuffle(moves.begin(), moves.end(), rng);
    std::stable_sort(moves.begin(), moves.end(),
                     [&](const Move& a, const Move& b) { return orderScore(root, a, 0) > orderScore(root, b, 0); });

    result.found = true;
    result.move  = moves[0];

    std::uniform_int_distribution<int> noise(-m_settings.noise, m_settings.noise);
    const int maxDepth = std::max(1, m_settings.depth);

    m_path.push_back(root.hash());
    for (int depth = 1; depth <= maxDepth; ++depth)
    {
        // With noise, every root move needs its exact score (full window);
        // otherwise a normal alpha-beta search only proves the best move.
        const bool exact = m_settings.noise > 0 && depth == maxDepth;

        int         alpha     = -Infinity;
        int         bestScore = -Infinity;
        int         bestPick  = -Infinity; // score + noise
        std::size_t bestIndex = 0;
        for (std::size_t i = 0; i < moves.size(); ++i)
        {
            Position child = root;
            child.makeMove(moves[i]);
            const int score = exact ? -negamax(child, depth - 1, -Infinity, Infinity, 1)
                                    : -negamax(child, depth - 1, -Infinity, -alpha, 1);
            if (m_aborted)
                break;

            // Noise never hides a forced mate in either direction.
            const bool mate = score >= MateThreshold || score <= -MateThreshold;
            const int  pick = exact && !mate ? score + noise(rng) : score;
            if (pick > bestPick)
            {
                bestPick  = pick;
                bestScore = score;
                bestIndex = i;
            }
            if (score > alpha)
                alpha = score;
        }
        if (m_aborted)
        {
            result.aborted = true;
            break; // keep the result of the last completed depth
        }

        result.move  = moves[bestIndex];
        result.score = bestScore;
        result.depth = depth;

        // Search the best move first in the next iteration.
        std::rotate(moves.begin(), moves.begin() + static_cast<std::ptrdiff_t>(bestIndex),
                    moves.begin() + static_cast<std::ptrdiff_t>(bestIndex) + 1);

        if (bestScore >= MateThreshold)
            break; // a forced mate was found; a deeper search cannot do better
    }

    result.nodes = m_nodes;
    return result;
}
} // namespace

SearchResult search(const Position& root,
                    const std::vector<std::uint64_t>& history,
                    const SearchSettings& settings,
                    std::mt19937& rng,
                    const std::atomic<bool>* stop)
{
    Searcher searcher(history, settings, stop);
    return searcher.run(root, rng);
}
} // namespace engine
