#include "core/Position.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace chess
{
namespace
{
// ---------------------------------------------------------------------------
// Zobrist keys (fixed pseudo-random numbers, same on every run)
// ---------------------------------------------------------------------------
struct ZobristKeys
{
    std::uint64_t piece[2][7][64]{};
    std::uint64_t blackToMove = 0;
    std::uint64_t castling[16]{};
    std::uint64_t enPassantFile[8]{};

    ZobristKeys()
    {
        std::uint64_t state = 0x2545F4914F6CDD1Dull;
        auto next = [&state]
        {
            // splitmix64
            state += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        };
        for (auto& color : piece)
            for (auto& type : color)
                for (auto& key : type)
                    key = next();
        blackToMove = next();
        for (auto& key : castling)
            key = next();
        for (auto& key : enPassantFile)
            key = next();
    }
};

const ZobristKeys& zobrist()
{
    static const ZobristKeys keys;
    return keys;
}

std::uint64_t pieceKey(Piece piece, Square sq)
{
    return zobrist().piece[index(piece.color)][static_cast<int>(piece.type)][sq];
}

// ---------------------------------------------------------------------------
// Precomputed knight and king target squares
// ---------------------------------------------------------------------------
struct StepTables
{
    std::array<std::array<Square, 8>, 64> knight{};
    std::array<int, 64>                   knightCount{};
    std::array<std::array<Square, 8>, 64> king{};
    std::array<int, 64>                   kingCount{};

    StepTables()
    {
        const int knightSteps[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
        const int kingSteps[8][2]   = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
        for (Square sq = 0; sq < 64; ++sq)
        {
            for (const auto& step : knightSteps)
            {
                const int f = fileOf(sq) + step[0];
                const int r = rankOf(sq) + step[1];
                if (isOnBoard(f, r))
                    knight[sq][static_cast<std::size_t>(knightCount[sq]++)] = makeSquare(f, r);
            }
            for (const auto& step : kingSteps)
            {
                const int f = fileOf(sq) + step[0];
                const int r = rankOf(sq) + step[1];
                if (isOnBoard(f, r))
                    king[sq][static_cast<std::size_t>(kingCount[sq]++)] = makeSquare(f, r);
            }
        }
    }
};

const StepTables& steps()
{
    static const StepTables tables;
    return tables;
}

constexpr int diagonalDirections[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
constexpr int straightDirections[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

// Castling rights that survive a move touching each square.
constexpr std::array<int, 64> makeCastlingMask()
{
    std::array<int, 64> mask{};
    for (auto& m : mask)
        m = AllCastling;
    mask[0]  = AllCastling & ~WhiteQueenside;                  // a1
    mask[4]  = AllCastling & ~(WhiteKingside | WhiteQueenside); // e1
    mask[7]  = AllCastling & ~WhiteKingside;                   // h1
    mask[56] = AllCastling & ~BlackQueenside;                  // a8
    mask[60] = AllCastling & ~(BlackKingside | BlackQueenside); // e8
    mask[63] = AllCastling & ~BlackKingside;                   // h8
    return mask;
}
constexpr std::array<int, 64> castlingMask = makeCastlingMask();

char pieceToChar(Piece p)
{
    char c = '?';
    switch (p.type)
    {
        case PieceType::Pawn:   c = 'p'; break;
        case PieceType::Knight: c = 'n'; break;
        case PieceType::Bishop: c = 'b'; break;
        case PieceType::Rook:   c = 'r'; break;
        case PieceType::Queen:  c = 'q'; break;
        case PieceType::King:   c = 'k'; break;
        case PieceType::None:   return '.';
    }
    return p.color == Color::White ? static_cast<char>(c - 'a' + 'A') : c;
}

Piece charToPiece(char c)
{
    const Color color = (c >= 'A' && c <= 'Z') ? Color::White : Color::Black;
    const char lower  = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    switch (lower)
    {
        case 'p': return {PieceType::Pawn, color};
        case 'n': return {PieceType::Knight, color};
        case 'b': return {PieceType::Bishop, color};
        case 'r': return {PieceType::Rook, color};
        case 'q': return {PieceType::Queen, color};
        case 'k': return {PieceType::King, color};
        default:  return {};
    }
}

[[noreturn]] void fenError(std::string_view fen, const std::string& reason)
{
    throw std::invalid_argument("Invalid FEN \"" + std::string(fen) + "\": " + reason);
}
} // namespace

// ---------------------------------------------------------------------------
// Construction and FEN
// ---------------------------------------------------------------------------
Position::Position()
{
    *this = fromFen(StartFen);
}

void Position::clear()
{
    m_board.fill(Piece{});
    m_kingSquare     = {NoSquare, NoSquare};
    m_sideToMove     = Color::White;
    m_castling       = NoCastling;
    m_enPassant      = NoSquare;
    m_halfmoveClock  = 0;
    m_fullmoveNumber = 1;
    m_hash           = 0;
}

Position Position::fromFen(std::string_view fen)
{
    std::istringstream stream{std::string(fen)};
    std::string placement, side, castling = "-", enPassant = "-";
    int halfmove = 0, fullmove = 1;

    if (!(stream >> placement >> side))
        fenError(fen, "piece placement and side to move are required");
    stream >> castling >> enPassant;
    if (stream >> halfmove)
        stream >> fullmove;

    Position pos{EmptyTag{}};
    pos.clear();

    // Piece placement, starting from rank 8.
    int rank = 7, file = 0;
    for (char c : placement)
    {
        if (c == '/')
        {
            if (file != 8)
                fenError(fen, "a rank does not have 8 squares");
            --rank;
            file = 0;
            if (rank < 0)
                fenError(fen, "too many ranks");
        }
        else if (c >= '1' && c <= '8')
        {
            file += c - '0';
            if (file > 8)
                fenError(fen, "a rank has more than 8 squares");
        }
        else
        {
            const Piece piece = charToPiece(c);
            if (piece.isNone())
                fenError(fen, std::string("unknown piece '") + c + "'");
            if (file > 7)
                fenError(fen, "a rank has more than 8 squares");
            const Square sq = makeSquare(file, rank);
            if (piece.type == PieceType::King)
            {
                if (pos.m_kingSquare[static_cast<std::size_t>(index(piece.color))] != NoSquare)
                    fenError(fen, "more than one king of a colour");
                pos.m_kingSquare[static_cast<std::size_t>(index(piece.color))] = sq;
            }
            if (piece.type == PieceType::Pawn && (rank == 0 || rank == 7))
                fenError(fen, "pawn on the first or last rank");
            pos.m_board[static_cast<std::size_t>(sq)] = piece;
            ++file;
        }
    }
    if (rank != 0 || file != 8)
        fenError(fen, "piece placement must have exactly 8 ranks of 8 squares");
    if (pos.m_kingSquare[0] == NoSquare || pos.m_kingSquare[1] == NoSquare)
        fenError(fen, "each side needs exactly one king");

    // Side to move.
    if (side == "w")
        pos.m_sideToMove = Color::White;
    else if (side == "b")
        pos.m_sideToMove = Color::Black;
    else
        fenError(fen, "side to move must be 'w' or 'b'");

    // Castling rights (kept only if king and rook are on their start squares).
    if (castling != "-")
    {
        for (char c : castling)
        {
            switch (c)
            {
                case 'K': pos.m_castling |= WhiteKingside; break;
                case 'Q': pos.m_castling |= WhiteQueenside; break;
                case 'k': pos.m_castling |= BlackKingside; break;
                case 'q': pos.m_castling |= BlackQueenside; break;
                default: fenError(fen, "bad castling field");
            }
        }
    }
    const auto at = [&pos](Square sq) { return pos.m_board[static_cast<std::size_t>(sq)]; };
    if (!at(4).is(Color::White, PieceType::King))
        pos.m_castling &= ~(WhiteKingside | WhiteQueenside);
    if (!at(7).is(Color::White, PieceType::Rook))
        pos.m_castling &= ~WhiteKingside;
    if (!at(0).is(Color::White, PieceType::Rook))
        pos.m_castling &= ~WhiteQueenside;
    if (!at(60).is(Color::Black, PieceType::King))
        pos.m_castling &= ~(BlackKingside | BlackQueenside);
    if (!at(63).is(Color::Black, PieceType::Rook))
        pos.m_castling &= ~BlackKingside;
    if (!at(56).is(Color::Black, PieceType::Rook))
        pos.m_castling &= ~BlackQueenside;

    // En passant square: kept only if a legal en passant capture exists, so
    // that equal positions always get equal hashes.
    if (enPassant != "-")
    {
        const Square ep = parseSquare(enPassant);
        if (ep == NoSquare)
            fenError(fen, "bad en passant square");
        const Color mover        = ~pos.m_sideToMove; // the side that made the double step
        const int   expectedRank = pos.m_sideToMove == Color::White ? 5 : 2;
        const int   step         = mover == Color::White ? 8 : -8;
        const bool  consistent   = rankOf(ep) == expectedRank && at(ep).isNone() && at(ep - step).isNone() &&
                                 at(ep + step).is(mover, PieceType::Pawn);
        if (consistent && pos.hasLegalEnPassantCapture(ep))
            pos.m_enPassant = ep;
    }

    if (halfmove < 0 || fullmove < 1)
        fenError(fen, "bad move counters");
    pos.m_halfmoveClock  = halfmove;
    pos.m_fullmoveNumber = fullmove;

    // The side that just moved must not be in check.
    if (pos.isSquareAttacked(pos.kingSquare(~pos.m_sideToMove), pos.m_sideToMove))
        fenError(fen, "the side not to move is in check");

    pos.m_hash = pos.computeHash();
    return pos;
}

std::string Position::toFen() const
{
    std::string fen;
    for (int rank = 7; rank >= 0; --rank)
    {
        int empty = 0;
        for (int file = 0; file < 8; ++file)
        {
            const Piece p = pieceAt(makeSquare(file, rank));
            if (p.isNone())
            {
                ++empty;
                continue;
            }
            if (empty > 0)
                fen += static_cast<char>('0' + empty);
            empty = 0;
            fen += pieceToChar(p);
        }
        if (empty > 0)
            fen += static_cast<char>('0' + empty);
        if (rank > 0)
            fen += '/';
    }

    fen += m_sideToMove == Color::White ? " w " : " b ";

    if (m_castling == NoCastling)
        fen += '-';
    if (m_castling & WhiteKingside)
        fen += 'K';
    if (m_castling & WhiteQueenside)
        fen += 'Q';
    if (m_castling & BlackKingside)
        fen += 'k';
    if (m_castling & BlackQueenside)
        fen += 'q';

    fen += ' ';
    fen += squareName(m_enPassant);
    fen += ' ' + std::to_string(m_halfmoveClock) + ' ' + std::to_string(m_fullmoveNumber);
    return fen;
}

std::uint64_t Position::computeHash() const
{
    std::uint64_t h = 0;
    for (Square sq = 0; sq < 64; ++sq)
    {
        const Piece p = pieceAt(sq);
        if (!p.isNone())
            h ^= pieceKey(p, sq);
    }
    if (m_sideToMove == Color::Black)
        h ^= zobrist().blackToMove;
    h ^= zobrist().castling[m_castling];
    if (m_enPassant != NoSquare)
        h ^= zobrist().enPassantFile[fileOf(m_enPassant)];
    return h;
}

// ---------------------------------------------------------------------------
// Board editing helpers (keep the hash up to date)
// ---------------------------------------------------------------------------
void Position::putPiece(Square sq, Piece piece)
{
    m_board[static_cast<std::size_t>(sq)] = piece;
    m_hash ^= pieceKey(piece, sq);
    if (piece.type == PieceType::King)
        m_kingSquare[static_cast<std::size_t>(index(piece.color))] = sq;
}

void Position::removePiece(Square sq)
{
    const Piece piece = pieceAt(sq);
    m_hash ^= pieceKey(piece, sq);
    m_board[static_cast<std::size_t>(sq)] = Piece{};
}

// ---------------------------------------------------------------------------
// Attacks
// ---------------------------------------------------------------------------
bool Position::isSquareAttacked(Square sq, Color by) const
{
    const int file = fileOf(sq);
    const int rank = rankOf(sq);

    // Pawns: a white pawn attacks diagonally upwards, so it would stand one
    // rank below the target square (and a black pawn one rank above).
    const int pawnRank = by == Color::White ? rank - 1 : rank + 1;
    if (pawnRank >= 0 && pawnRank < 8)
    {
        for (int df : {-1, 1})
        {
            const int f = file + df;
            if (f >= 0 && f < 8 && pieceAt(makeSquare(f, pawnRank)).is(by, PieceType::Pawn))
                return true;
        }
    }

    const StepTables& t = steps();
    for (int i = 0; i < t.knightCount[sq]; ++i)
        if (pieceAt(t.knight[sq][static_cast<std::size_t>(i)]).is(by, PieceType::Knight))
            return true;
    for (int i = 0; i < t.kingCount[sq]; ++i)
        if (pieceAt(t.king[sq][static_cast<std::size_t>(i)]).is(by, PieceType::King))
            return true;

    // Sliding pieces: walk each ray until the first piece.
    for (const auto& dir : diagonalDirections)
    {
        for (int f = file + dir[0], r = rank + dir[1]; isOnBoard(f, r); f += dir[0], r += dir[1])
        {
            const Piece p = pieceAt(makeSquare(f, r));
            if (p.isNone())
                continue;
            if (p.color == by && (p.type == PieceType::Bishop || p.type == PieceType::Queen))
                return true;
            break;
        }
    }
    for (const auto& dir : straightDirections)
    {
        for (int f = file + dir[0], r = rank + dir[1]; isOnBoard(f, r); f += dir[0], r += dir[1])
        {
            const Piece p = pieceAt(makeSquare(f, r));
            if (p.isNone())
                continue;
            if (p.color == by && (p.type == PieceType::Rook || p.type == PieceType::Queen))
                return true;
            break;
        }
    }
    return false;
}

bool Position::isInsufficientMaterial() const
{
    int knights = 0, lightBishops = 0, darkBishops = 0;
    for (Square sq = 0; sq < 64; ++sq)
    {
        const Piece p = pieceAt(sq);
        switch (p.type)
        {
            case PieceType::None:
            case PieceType::King:
                break;
            case PieceType::Knight:
                ++knights;
                break;
            case PieceType::Bishop:
                // a1 (file 0 + rank 0 = even) is a dark square.
                if ((fileOf(sq) + rankOf(sq)) % 2 == 0)
                    ++darkBishops;
                else
                    ++lightBishops;
                break;
            default:
                return false; // a pawn, rook or queen can always still mate
        }
    }
    if (knights == 0)
        return lightBishops == 0 || darkBishops == 0; // K vs K, or all bishops on one colour
    return knights == 1 && lightBishops == 0 && darkBishops == 0; // K+N vs K
}

// ---------------------------------------------------------------------------
// Move generation
// ---------------------------------------------------------------------------
void Position::generatePseudoLegalMoves(MoveList& moves) const
{
    moves.clear();
    const StepTables& t = steps();
    for (Square sq = 0; sq < 64; ++sq)
    {
        const Piece p = pieceAt(sq);
        if (p.isNone() || p.color != m_sideToMove)
            continue;
        switch (p.type)
        {
            case PieceType::Pawn:   generatePawnMoves(sq, moves); break;
            case PieceType::Knight: generateStepMoves(sq, t.knight[sq], t.knightCount[sq], moves); break;
            case PieceType::Bishop: generateSlidingMoves(sq, true, false, moves); break;
            case PieceType::Rook:   generateSlidingMoves(sq, false, true, moves); break;
            case PieceType::Queen:  generateSlidingMoves(sq, true, true, moves); break;
            case PieceType::King:   generateStepMoves(sq, t.king[sq], t.kingCount[sq], moves); break;
            case PieceType::None:   break;
        }
    }
    generateCastlingMoves(moves);
}

void Position::generateLegalMoves(MoveList& moves) const
{
    MoveList pseudo;
    generatePseudoLegalMoves(pseudo);
    moves.clear();
    for (const Move& m : pseudo)
        if (isLegal(m))
            moves.push_back(m);
}

bool Position::isLegal(const Move& move) const
{
    Position next = *this;
    next.makeMove(move);
    return !next.isSquareAttacked(next.kingSquare(m_sideToMove), next.m_sideToMove);
}

void Position::generatePawnMoves(Square from, MoveList& moves) const
{
    const Color us          = m_sideToMove;
    const int   forward     = us == Color::White ? 8 : -8;
    const int   startRank   = us == Color::White ? 1 : 6;
    const int   promoRank   = us == Color::White ? 7 : 0;
    const Square oneStep    = from + forward; // always on the board: pawns never stand on the last rank

    auto addPawnMove = [&moves, promoRank](Square f, Square to, std::uint8_t flags)
    {
        if (rankOf(to) == promoRank)
        {
            moves.push_back(Move(f, to, flags, PieceType::Queen));
            moves.push_back(Move(f, to, flags, PieceType::Rook));
            moves.push_back(Move(f, to, flags, PieceType::Bishop));
            moves.push_back(Move(f, to, flags, PieceType::Knight));
        }
        else
        {
            moves.push_back(Move(f, to, flags));
        }
    };

    // Pushes.
    if (pieceAt(oneStep).isNone())
    {
        addPawnMove(from, oneStep, Move::Quiet);
        const Square twoSteps = oneStep + forward;
        if (rankOf(from) == startRank && pieceAt(twoSteps).isNone())
            moves.push_back(Move(from, twoSteps, Move::DoublePush));
    }

    // Captures, including en passant.
    for (int df : {-1, 1})
    {
        const int file = fileOf(from) + df;
        if (file < 0 || file > 7)
            continue;
        const Square to     = oneStep + df;
        const Piece  target = pieceAt(to);
        if (!target.isNone() && target.color != us)
            addPawnMove(from, to, Move::Capture);
        else if (to == m_enPassant)
            moves.push_back(Move(from, to, Move::Capture | Move::EnPassant));
    }
}

void Position::generateStepMoves(Square from, const std::array<Square, 8>& targets, int count, MoveList& moves) const
{
    for (int i = 0; i < count; ++i)
    {
        const Square to     = targets[static_cast<std::size_t>(i)];
        const Piece  target = pieceAt(to);
        if (target.isNone())
            moves.push_back(Move(from, to, Move::Quiet));
        else if (target.color != m_sideToMove)
            moves.push_back(Move(from, to, Move::Capture));
    }
}

void Position::generateSlidingMoves(Square from, bool diagonal, bool straight, MoveList& moves) const
{
    auto slide = [&](const int (&dirs)[4][2])
    {
        for (const auto& dir : dirs)
        {
            for (int f = fileOf(from) + dir[0], r = rankOf(from) + dir[1]; isOnBoard(f, r); f += dir[0], r += dir[1])
            {
                const Square to     = makeSquare(f, r);
                const Piece  target = pieceAt(to);
                if (target.isNone())
                {
                    moves.push_back(Move(from, to, Move::Quiet));
                    continue;
                }
                if (target.color != m_sideToMove)
                    moves.push_back(Move(from, to, Move::Capture));
                break;
            }
        }
    };
    if (diagonal)
        slide(diagonalDirections);
    if (straight)
        slide(straightDirections);
}

void Position::generateCastlingMoves(MoveList& moves) const
{
    const Color us   = m_sideToMove;
    const Color them = ~us;
    const int   kingsideRight  = us == Color::White ? WhiteKingside : BlackKingside;
    const int   queensideRight = us == Color::White ? WhiteQueenside : BlackQueenside;
    if ((m_castling & (kingsideRight | queensideRight)) == 0)
        return;

    const Square kingFrom = us == Color::White ? 4 : 60; // e1 / e8
    if (isSquareAttacked(kingFrom, them))
        return; // cannot castle out of check

    const auto empty = [this](Square sq) { return pieceAt(sq).isNone(); };

    // Kingside: f and g squares empty, king does not pass through or land on an attacked square.
    if ((m_castling & kingsideRight) && empty(kingFrom + 1) && empty(kingFrom + 2) &&
        !isSquareAttacked(kingFrom + 1, them) && !isSquareAttacked(kingFrom + 2, them))
    {
        moves.push_back(Move(kingFrom, kingFrom + 2, Move::Castling));
    }

    // Queenside: b, c and d squares empty; only c and d must be safe (the king never visits b).
    if ((m_castling & queensideRight) && empty(kingFrom - 1) && empty(kingFrom - 2) && empty(kingFrom - 3) &&
        !isSquareAttacked(kingFrom - 1, them) && !isSquareAttacked(kingFrom - 2, them))
    {
        moves.push_back(Move(kingFrom, kingFrom - 2, Move::Castling));
    }
}

// ---------------------------------------------------------------------------
// Making moves
// ---------------------------------------------------------------------------
void Position::makeMove(const Move& move)
{
    const ZobristKeys& z      = zobrist();
    const Color        us     = m_sideToMove;
    const Square       from   = move.fromSquare();
    const Square       to     = move.toSquare();
    const Piece        moving = pieceAt(from);
    bool               capture = false;

    // The old en passant square disappears after every move.
    if (m_enPassant != NoSquare)
        m_hash ^= z.enPassantFile[fileOf(m_enPassant)];
    m_enPassant = NoSquare;

    // Remove a captured piece.
    if (move.isEnPassant())
    {
        removePiece(us == Color::White ? to - 8 : to + 8); // the pawn that made the double step
        capture = true;
    }
    else if (!pieceAt(to).isNone())
    {
        removePiece(to);
        capture = true;
    }

    // Move the piece (promoting it if needed).
    removePiece(from);
    Piece placed = moving;
    if (move.isPromotion())
        placed.type = move.promotion;
    putPiece(to, placed);

    // Castling also moves the rook.
    if (move.isCastling())
    {
        const bool   kingside = to > from;
        const Square rookFrom = kingside ? from + 3 : from - 4;
        const Square rookTo   = kingside ? from + 1 : from - 1;
        const Piece  rook     = pieceAt(rookFrom);
        removePiece(rookFrom);
        putPiece(rookTo, rook);
    }

    // Castling rights are lost when the king or a rook moves or a rook is captured.
    m_hash ^= z.castling[m_castling];
    m_castling &= castlingMask[static_cast<std::size_t>(from)] & castlingMask[static_cast<std::size_t>(to)];
    m_hash ^= z.castling[m_castling];

    // Move counters.
    if (moving.type == PieceType::Pawn || capture)
        m_halfmoveClock = 0;
    else
        ++m_halfmoveClock;
    if (us == Color::Black)
        ++m_fullmoveNumber;

    m_sideToMove = ~us;
    m_hash ^= z.blackToMove;

    // After a double step, remember the skipped square, but only if the
    // opponent can really capture en passant (keeps repetition detection exact).
    if (move.isDoublePush())
    {
        const Square skipped = (from + to) / 2;
        if (hasLegalEnPassantCapture(skipped))
        {
            m_enPassant = skipped;
            m_hash ^= z.enPassantFile[fileOf(skipped)];
        }
    }
}

bool Position::hasLegalEnPassantCapture(Square epSquare) const
{
    // Pawns of the side to move that could capture onto epSquare stand next
    // to the pawn that just made the double step.
    const Color  us        = m_sideToMove;
    const int    pawnRank  = us == Color::White ? rankOf(epSquare) - 1 : rankOf(epSquare) + 1;
    for (int df : {-1, 1})
    {
        const int file = fileOf(epSquare) + df;
        if (file < 0 || file > 7)
            continue;
        const Square from = makeSquare(file, pawnRank);
        if (!pieceAt(from).is(us, PieceType::Pawn))
            continue;

        Position test = *this;
        test.m_enPassant = epSquare;
        if (test.isLegal(Move(from, epSquare, Move::Capture | Move::EnPassant)))
            return true;
    }
    return false;
}
} // namespace chess
