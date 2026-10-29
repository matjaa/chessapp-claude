// Tests for the chess rules in src/core (no GUI needed).
//
//   core_tests            run everything
//   core_tests --quick    skip the slowest perft depths (useful in Debug builds)
//
// Exit code 0 means all tests passed.

#include "core/Game.hpp"
#include "core/Notation.hpp"
#include "core/Perft.hpp"
#include "core/Position.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace chess;

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

// Plays moves given in coordinate notation ("e2e4", "e7e8q").
bool play(Game& game, const std::vector<std::string>& uciMoves)
{
    for (const std::string& text : uciMoves)
    {
        const Square from  = parseSquare(text.substr(0, 2));
        const Square to    = parseSquare(text.substr(2, 2));
        PieceType    promo = PieceType::None;
        if (text.size() == 5)
        {
            switch (text[4])
            {
                case 'q': promo = PieceType::Queen; break;
                case 'r': promo = PieceType::Rook; break;
                case 'b': promo = PieceType::Bishop; break;
                case 'n': promo = PieceType::Knight; break;
                default: break;
            }
        }
        if (!game.makeMove(Move(from, to, Move::Quiet, promo)))
        {
            std::cout << "  could not play " << text << '\n';
            return false;
        }
    }
    return true;
}

bool hasMove(const Position& pos, const std::string& uci)
{
    MoveList moves;
    pos.generateLegalMoves(moves);
    for (const Move& m : moves)
        if (m.toUci() == uci)
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// Perft
// ---------------------------------------------------------------------------
struct PerftCase
{
    const char*                name;
    const char*                fen;
    std::vector<std::uint64_t> expected; // expected[d - 1] = perft(d)
};

void runPerft(bool quick)
{
    std::cout << "\n== Perft ==\n";
    // Reference values from https://www.chessprogramming.org/Perft_Results
    const std::vector<PerftCase> cases = {
        {"Start position", Position::StartFen, {20, 400, 8902, 197281, 4865609}},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         {48, 2039, 97862, 4085603}},
        {"Position 3 (en passant, pins)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
         {14, 191, 2812, 43238, 674624}},
        {"Position 4 (promotions, castling)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
         {6, 264, 9467, 422333}},
        {"Position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379, 2103487}},
        {"Position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
         {46, 2079, 89890, 3894594}},
    };

    for (const PerftCase& c : cases)
    {
        std::cout << c.name << '\n';
        const Position pos   = Position::fromFen(c.fen);
        const std::size_t maxDepth = quick ? std::min<std::size_t>(c.expected.size(), 3) : c.expected.size();
        for (std::size_t d = 1; d <= maxDepth; ++d)
        {
            const auto          start   = std::chrono::steady_clock::now();
            const std::uint64_t nodes   = perft(pos, static_cast<int>(d));
            const auto          elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const bool          ok      = nodes == c.expected[d - 1];
            ++g_checks;
            if (!ok)
                ++g_failures;
            std::cout << "  depth " << d << ": " << nodes << (ok ? "  ok" : "  WRONG, expected ")
                      << (ok ? std::string() : std::to_string(c.expected[d - 1])) << "  (" << elapsed << " s)\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Rule tests
// ---------------------------------------------------------------------------
void testFen()
{
    std::cout << "FEN round trip\n";
    const char* fens[] = {
        Position::StartFen,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2",
    };
    for (const char* fen : fens)
        CHECK(Position::fromFen(fen).toFen() == fen);

    bool threw = false;
    try
    {
        (void)Position::fromFen("not a fen");
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    CHECK(threw);
}

void testCheckmateAndStalemate()
{
    std::cout << "Checkmate and stalemate\n";
    Game foolsMate;
    CHECK(play(foolsMate, {"f2f3", "e7e5", "g2g4", "d8h4"}));
    CHECK(foolsMate.status() == GameStatus::Checkmate);
    CHECK(foolsMate.winner() == Color::Black);
    CHECK(foolsMate.resultText() == "Checkmate - Black wins");
    CHECK(foolsMate.legalMoves().empty());
    CHECK(!foolsMate.makeMove(Move(parseSquare("e2"), parseSquare("e4"))));

    Game scholarsMate;
    CHECK(play(scholarsMate, {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7"}));
    CHECK(scholarsMate.status() == GameStatus::Checkmate);
    CHECK(scholarsMate.resultText() == "Checkmate - White wins");

    Game stalemate(Position::fromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));
    CHECK(stalemate.status() == GameStatus::Stalemate);
    CHECK(!stalemate.winner().has_value());
    CHECK(stalemate.resultText() == "Draw by stalemate");

    // Check is not mate when the king can escape.
    Game check(Position::fromFen("4k3/8/8/8/8/8/8/4K2R w K - 0 1"));
    CHECK(play(check, {"h1h8"}));
    CHECK(check.position().inCheck());
    CHECK(check.status() == GameStatus::Ongoing);
}

void testCastling()
{
    std::cout << "Castling\n";
    const Position both = Position::fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    CHECK(hasMove(both, "e1g1"));
    CHECK(hasMove(both, "e1c1"));

    // Kingside path attacked (f1).
    const Position fAttacked = Position::fromFen("4kr2/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    CHECK(!hasMove(fAttacked, "e1g1"));
    CHECK(hasMove(fAttacked, "e1c1"));

    // Landing square attacked (g1).
    const Position gAttacked = Position::fromFen("4k1r1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    CHECK(!hasMove(gAttacked, "e1g1"));

    // King in check: no castling at all.
    const Position inCheck = Position::fromFen("4r1k1/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    CHECK(!hasMove(inCheck, "e1g1"));
    CHECK(!hasMove(inCheck, "e1c1"));

    // b1 attacked does not matter for queenside castling (the king never visits b1).
    const Position bAttacked = Position::fromFen("1r2k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    CHECK(hasMove(bAttacked, "e1c1"));

    // Piece in between.
    const Position blocked = Position::fromFen("4k3/8/8/8/8/8/8/RN2K1NR w KQ - 0 1");
    CHECK(!hasMove(blocked, "e1g1"));
    CHECK(!hasMove(blocked, "e1c1"));

    // Rights are lost after the king or rook moves, even if it comes back.
    Game game(Position::fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    CHECK(play(game, {"h1h2", "a8a7", "h2h1", "a7a8"}));
    CHECK(!hasMove(game.position(), "e1g1"));
    CHECK(hasMove(game.position(), "e1c1"));
    CHECK(game.position().castlingRights() == (WhiteQueenside | BlackKingside));

    // Castling moves the rook too.
    Game castle(Position::fromFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    CHECK(play(castle, {"e1g1", "e8c8"}));
    CHECK(castle.position().pieceAt(parseSquare("f1")).is(Color::White, PieceType::Rook));
    CHECK(castle.position().pieceAt(parseSquare("h1")).isNone());
    CHECK(castle.position().pieceAt(parseSquare("d8")).is(Color::Black, PieceType::Rook));
    CHECK(castle.position().pieceAt(parseSquare("a8")).isNone());
}

void testEnPassant()
{
    std::cout << "En passant\n";
    Game game(Position::fromFen("4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1"));
    CHECK(play(game, {"d7d5"}));
    CHECK(hasMove(game.position(), "e5d6"));
    CHECK(play(game, {"e5d6"}));
    CHECK(game.position().pieceAt(parseSquare("d5")).isNone()); // captured pawn removed
    CHECK(game.position().pieceAt(parseSquare("d6")).is(Color::White, PieceType::Pawn));

    // Only on the move right after the double step.
    Game late(Position::fromFen("4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1"));
    CHECK(play(late, {"d7d5", "e1d1", "e8d8"}));
    CHECK(!hasMove(late.position(), "e5d6"));

    // Illegal when it exposes the king along the rank.
    const Position pinned = Position::fromFen("8/8/8/K2pP2r/8/8/8/7k w - d6 0 1");
    CHECK(!hasMove(pinned, "e5d6"));
    CHECK(pinned.enPassantSquare() == NoSquare);
}

void testPromotion()
{
    std::cout << "Promotion\n";
    Game game(Position::fromFen("1n6/P7/8/8/8/8/8/k6K w - - 0 1"));
    CHECK(game.isPromotion(parseSquare("a7"), parseSquare("a8")));
    CHECK(game.isPromotion(parseSquare("a7"), parseSquare("b8")));
    CHECK(!game.findLegalMove(parseSquare("a7"), parseSquare("a8")).has_value()); // needs a piece
    for (PieceType t : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight})
        CHECK(game.findLegalMove(parseSquare("a7"), parseSquare("a8"), t).has_value());
    CHECK(play(game, {"a7b8n"}));
    CHECK(game.position().pieceAt(parseSquare("b8")).is(Color::White, PieceType::Knight));
}

void testDraws()
{
    std::cout << "Draws\n";
    // Insufficient material.
    CHECK(Game(Position::fromFen("8/8/8/4k3/8/8/8/4K3 w - - 0 1")).status() == GameStatus::InsufficientMaterial);
    CHECK(Game(Position::fromFen("8/8/8/4k3/8/8/8/2B1K3 w - - 0 1")).status() == GameStatus::InsufficientMaterial);
    CHECK(Game(Position::fromFen("8/8/8/4k3/8/8/8/1N2K3 w - - 0 1")).status() == GameStatus::InsufficientMaterial);
    // Bishops on c1 and f8 are both on dark squares.
    CHECK(Game(Position::fromFen("5b2/8/8/4k3/8/8/8/2B1K3 w - - 0 1")).status() == GameStatus::InsufficientMaterial);
    // Opposite-coloured bishops, two knights, a pawn: play on.
    CHECK(Game(Position::fromFen("2b5/8/8/4k3/8/8/8/2B1K3 w - - 0 1")).status() == GameStatus::Ongoing);
    CHECK(Game(Position::fromFen("8/8/8/4k3/8/8/8/1N2KN2 w - - 0 1")).status() == GameStatus::Ongoing);
    CHECK(Game(Position::fromFen("8/8/8/4k3/8/8/4P3/4K3 w - - 0 1")).status() == GameStatus::Ongoing);

    // Capturing the last pawn ends the game by insufficient material.
    Game capture(Position::fromFen("8/8/8/8/4p2k/3K4/8/8 w - - 0 1"));
    CHECK(play(capture, {"d3e4"}));
    CHECK(capture.status() == GameStatus::InsufficientMaterial);
    CHECK(capture.resultText() == "Draw by insufficient material");

    // Threefold repetition: the start position appears for the third time.
    Game repetition;
    CHECK(play(repetition, {"g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6", "f3g1"}));
    CHECK(repetition.status() == GameStatus::Ongoing);
    CHECK(play(repetition, {"f6g8"}));
    CHECK(repetition.status() == GameStatus::ThreefoldRepetition);
    CHECK(repetition.resultText() == "Draw by threefold repetition");

    // Same placement but different castling rights is not a repetition:
    // the king's first trip loses the rights, so the start occurs only once.
    Game rights(Position::fromFen("4k3/8/8/8/8/8/8/4K2R w K - 0 1"));
    CHECK(play(rights, {"e1f1", "e8f8", "f1e1", "f8e8", "e1f1", "e8f8", "f1e1", "f8e8"}));
    CHECK(rights.status() == GameStatus::Ongoing); // start placement: 1 time with rights + 2 times without
    CHECK(play(rights, {"e1f1"}));                 // the position after e1f1 now occurs a third time
    CHECK(rights.status() == GameStatus::ThreefoldRepetition);

    // Fifty-move rule: 100 half-moves without a capture or pawn move.
    Game fifty(Position::fromFen("8/8/8/4k3/8/8/8/R3K3 w - - 99 80"));
    CHECK(fifty.status() == GameStatus::Ongoing);
    CHECK(play(fifty, {"a1a2"}));
    CHECK(fifty.status() == GameStatus::FiftyMoveRule);
    CHECK(fifty.resultText() == "Draw by fifty-move rule");

    // Checkmate on the 100th half-move counts as checkmate, not a draw.
    Game mateFirst(Position::fromFen("7k/8/6K1/8/8/8/8/R7 w - - 99 80"));
    CHECK(play(mateFirst, {"a1a8"}));
    CHECK(mateFirst.status() == GameStatus::Checkmate);
}

void testResign()
{
    std::cout << "Resign\n";
    Game game;
    CHECK(play(game, {"e2e4"}));
    game.resign(Color::White);
    CHECK(game.status() == GameStatus::Resigned);
    CHECK(game.winner() == Color::Black);
    CHECK(game.resultText() == "White resigned");
    CHECK(!game.makeMove(Move(parseSquare("e7"), parseSquare("e5"))));
}

std::string san(const char* fen, const std::string& uci)
{
    const Position pos = Position::fromFen(fen);
    MoveList moves;
    pos.generateLegalMoves(moves);
    for (const Move& m : moves)
        if (m.toUci() == uci)
            return toSan(pos, m);
    return "illegal " + uci;
}

void testNotation()
{
    std::cout << "Notation (SAN)\n";
    const char* start = Position::StartFen;
    CHECK(san(start, "e2e4") == "e4");
    CHECK(san(start, "g1f3") == "Nf3");

    // Pawn captures, en passant, promotion.
    CHECK(san("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1", "e4d5") == "exd5");
    CHECK(san("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6") == "exd6");
    CHECK(san("8/4P3/8/8/8/8/k7/4K3 w - - 0 1", "e7e8q") == "e8=Q");
    CHECK(san("8/4P3/8/8/8/8/k7/4K3 w - - 0 1", "e7e8n") == "e8=N");
    CHECK(san("3r3k/4P3/8/8/8/8/8/4K3 w - - 0 1", "e7d8q") == "exd8=Q+");

    // Castling.
    CHECK(san("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1") == "O-O");
    CHECK(san("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1c1") == "O-O-O");

    // Disambiguation by file, by rank, and by both.
    CHECK(san("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1", "b1d2") == "Nbd2");
    CHECK(san("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1", "f1d2") == "Nfd2");
    CHECK(san("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1", "a1a3") == "R1a3");
    CHECK(san("4k3/8/8/R7/8/8/8/R3K3 w - - 0 1", "a5a3") == "R5a3");
    CHECK(san("k7/8/8/8/8/2Q1Q3/8/2Q1K3 w - - 0 1", "c3d2") == "Qc3d2");
    CHECK(san("k7/8/8/8/8/2Q1Q3/8/2Q1K3 w - - 0 1", "e3d2") == "Qed2");
    // The knight on f1 is pinned by the rook on h1, so Nd2 is unambiguous.
    CHECK(san("4k3/8/8/8/8/8/8/1N2KN1r w - - 0 1", "b1d2") == "Nd2");

    // Check and checkmate, and the list kept by Game.
    Game game;
    CHECK(play(game, {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7"}));
    const std::vector<std::string> expected = {"e4", "e5", "Bc4", "Nc6", "Qh5", "Nf6", "Qxf7#"};
    CHECK(game.sanMoves() == expected);
    CHECK(san("rnbqkbnr/ppppp1pp/8/5p2/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", "d1h5") == "Qh5+");
    game.reset();
    CHECK(game.sanMoves().empty());
}

// Plays many random games and checks that the incrementally updated hash
// always equals a hash computed from scratch.
void testHashConsistency()
{
    std::cout << "Hash consistency (random games)\n";
    std::mt19937 rng(12345);
    int mismatches = 0;
    for (int gameIndex = 0; gameIndex < 300; ++gameIndex)
    {
        Position pos;
        for (int ply = 0; ply < 200; ++ply)
        {
            MoveList moves;
            pos.generateLegalMoves(moves);
            if (moves.empty())
                break;
            std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
            pos.makeMove(moves[pick(rng)]);
            if (pos.hash() != pos.computeHash())
                ++mismatches;
            // FEN round trip must give the same position.
            if (Position::fromFen(pos.toFen()).hash() != pos.hash())
                ++mismatches;
        }
    }
    CHECK(mismatches == 0);
}
} // namespace

int main(int argc, char** argv)
{
    const bool quick = argc > 1 && std::strcmp(argv[1], "--quick") == 0;

    std::cout << "== Rule tests ==\n";
    testFen();
    testCheckmateAndStalemate();
    testCastling();
    testEnPassant();
    testPromotion();
    testDraws();
    testResign();
    testNotation();
    testHashConsistency();

    runPerft(quick);

    std::cout << "\n" << (g_checks - g_failures) << " / " << g_checks << " checks passed\n";
    if (g_failures == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << g_failures << " FAILED\n";
    return g_failures == 0 ? 0 : 1;
}
