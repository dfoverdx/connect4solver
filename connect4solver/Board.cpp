#include <algorithm>
#include <cassert>
#include <exception>
#include <intrin.h>
#include <string>
#include <utility>
#include "Board.h"
#include "Constants.Board.h"
#include "PopCount.h"

using namespace std;
using namespace connect4solver;

#pragma region Macros

#define colIdx(x) ((x) << 3)
#define colMask(cIdx) (EMPTY_COLUMN_BITS << (cIdx))
#define getCol(pieces, cIdx) ((pieces & colMask(cIdx)) >> (cIdx))

#define checkWin(pieces, spaces, condition, winHueristic, piece) \
if ((spaces & condition) == condition && \
    pieces & condition) { \
    popCount = popCount64(pieces & condition); \
    if (popCount == 4) { \
        return winHueristic; \
    } \
    else { \
        scores[piece] += 1ull << (popCount - 1); \
    } \
}

#define checkBlackWin(pieces, spaces, condition) checkWin(pieces, spaces, condition, BLACK_WON_HEURISTIC, 0)
#define checkRedWin(pieces, spaces, condition) checkWin(pieces, spaces, condition, BLACK_LOST_HEURISTIC, 1)

#define checkWins(condition) \
checkBlackWin(bShiftedPieces, bShiftedAvailableSpaces, condition); \
checkRedWin(rShiftedPieces, rShiftedAvailableSpaces, condition)
#pragma endregion

#pragma region Constants
constexpr uchar SYMMETRIC('\3');
constexpr uchar POSSIBLY_SYMMETRIC('\2');
constexpr uchar IMPOSSIBLY_SYMMETRIC('\0');

constexpr BoardPieces VERTICAL_WIN = 0xF;
constexpr BoardPieces HORIZONTAL_WIN = 0x01010101;
constexpr BoardPieces DIAGONAL_FORWARD_WIN = 0x08040201;

static const BoardPieces DIAGONAL_BACKWARD_WIN = 0x01020408ull;

typedef pair<int, int> ShiftLimits;

static const ShiftLimits H_SHIFT_LIMITS[BOARD_WIDTH]{
    { 0, 0 },
    { 0, 1 },
    { 0, 2 },
    { 0, 3 },
    { 1, 3 },
    { 2, 3 },
    { 3, 3 }
};

static const ShiftLimits V_SHIFT_LIMITS[BOARD_HEIGHT]{
    { 0, 0 },
    { 0, 1 },
    { 0, 2 },
    { 0, 2 },
    { 1, 2 },
    { 2, 2 }
};
#pragma endregion

#pragma region Board definitions
#pragma region Constructors
// default constructor -- empty board
Board::Board() :
    m_state({ 0, EMPTY_OPEN_SPACES_BITS }), heuristic(0), boardHash(0), playerTurn(BLACK), symmetry(SYMMETRIC),
    validMoves(0x7F) {}

// copy constructor
Board::Board(const Board &b) :
    m_state(b.m_state), heuristic(b.heuristic), boardHash(b.boardHash), playerTurn(b.playerTurn),
    symmetry(b.symmetry), validMoves(b.validMoves) {}

// construtor used when adding a piece
Board::Board(const BoardState &state, const BoardHash &prevHash, const bool possiblySymmetric, const piece playerTurn,
    const int x) :
    m_state(state), heuristic(calcHeuristic(state)), boardHash(calcBoardHash(prevHash, playerTurn, x)),
    playerTurn(playerTurn), symmetry(calcSymmetry(possiblySymmetric, state)),
    validMoves(calcValidMoves(state)) {}

// constructor used when building board from hash or mirroring a board
Board::Board(const BoardState &state, const piece playerTurn, const uchar symmetry, const Heuristic heuristic) :
    m_state(state), heuristic(heuristic), boardHash(calcBoardHash(state)), playerTurn(playerTurn),
    symmetry(symmetry), validMoves(calcValidMoves(state)) {}
#pragma endregion

inline const gameOverState Board::getGameOver() const
{
    switch (heuristic) {
    case BLACK_WON_HEURISTIC:
        return gameOverState::blackWon;
    case BLACK_LOST_HEURISTIC:
    case CATS_GAME_HEURISTIC:
        return gameOverState::blackLost;
    default:
        return gameOverState::unfinished;
    }
}

const Heuristic Board::calcHeuristic(const BoardState &state)
{
    assert(state.openSpaces != EMPTY_OPEN_SPACES_BITS);
    BoardPieces bPieces = ~state.pieces & (state.openSpaces ^ BOARD_MASK);
    BoardPieces bAvailableSpaces = (bPieces ^ state.openSpaces) & BOARD_MASK;
    BoardPieces rPieces = state.pieces & (state.openSpaces ^ BOARD_MASK);
    BoardPieces rAvailableSpaces = (rPieces ^ state.openSpaces) & BOARD_MASK;

    Heuristic scores[2] = { 0, 0 };
    ull popCount;

    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            BoardPieces bShiftedPieces = bPieces >> (colIdx(x) + y);
            BoardPieces bShiftedAvailableSpaces = bAvailableSpaces >> (colIdx(x) + y);
            BoardPieces rShiftedPieces = rPieces >> (colIdx(x) + y);
            BoardPieces rShiftedAvailableSpaces = rAvailableSpaces >> (colIdx(x) + y);

            if (y < 3) {
                checkWins(VERTICAL_WIN);
            }

            if (x < 4) {
                checkWins(HORIZONTAL_WIN);

                // check diagonals
                if (y < 3) {
                    checkWins(DIAGONAL_FORWARD_WIN);
                    checkWins(DIAGONAL_BACKWARD_WIN);
                }
            }
        }
    }

    if (scores[0] == 0) {                   // no way that black can win
        if (scores[1] == 0) {               // no way that either player can win
            return CATS_GAME_HEURISTIC;
        }
        else
        {
            return BLACK_LOST_HEURISTIC;
        }
    }

    return scores[0] - scores[1];
}

Board& Board::operator=(const Board &other) {
    if (this == &other) {
        return *this;
    }

    assignConst(BoardState, m_state, other);
    assignConst(Heuristic, heuristic, other);
    assignConst(BoardHash, boardHash, other);
    assignConst(piece, playerTurn, other);
    assignConst(uchar, symmetry, other);
    assignConst(ValidMoves, validMoves, other);
    return *this;
}

const Board Board::fromHash(BoardHash hash)
{
    piece playerTurn;
    BoardState state = BoardState::fromHash(hash, playerTurn);
    uchar symmetry = calcSymmetry(true, state);
    Heuristic heuristic = 0;
    if (state.openSpaces != EMPTY_OPEN_SPACES_BITS) {
        heuristic = calcHeuristic(state);
    }

    return Board(state, playerTurn, symmetry, heuristic);
}

// currently unused, so performance doesn't matter
inline const int Board::getColHeight(const int x) const
{
    // todo: figure out how to use const element
    BoardHashBitField bf = boardHash;
    return static_cast<int>(bf.heights[x]);
}

const piece Board::getPiece(const int x, const int y) const {
    assert(x / BOARD_WIDTH == 0);
    assert(y / BOARD_HEIGHT == 0);
    BoardPieces pieceMask = 1ull << (colIdx(x) + y);
    if (m_state.openSpaces & pieceMask) {
        return piece::empty;
    }

    return piece(!!(m_state.pieces & pieceMask));
}

// Gets the 6th (BOARD_HEIGHTth) bit of each byte in m_state.openSpaces and returns their values in a uchar.
// from https://stackoverflow.com/a/46866751/3120446 
inline const ValidMoves Board::calcValidMoves(const BoardState &state)
{
    // puts openSpaces into a vector
    __m128i n = _mm_set_epi64x(0, state.openSpaces);

    // _mm_slli_epi64: shifts bits up 2 so the 6th bit is at the 8th bit position
    // _mm_movemask_epi8: takes the high bit of every byte in the vector and concatenates them
    return uchar(_mm_movemask_epi8(_mm_slli_epi64(n, (8 - BOARD_HEIGHT))));
}

const bool Board::isValidMove(const int x) const
{
    assert(x / BOARD_WIDTH == 0);
    return (m_state.openSpaces & colMask(colIdx(x))) > 0;
}

Board Board::addPiece(int x) const {
    assert(x / BOARD_WIDTH == 0);
    if (!isValidMove(x)) {
        throw exception(("Tried to add piece to full column: " + to_string((int)x)).c_str());
    }

    if (getGameOver() != gameOverState::unfinished) {
        throw exception("Tried to add a piece when the game is over");
    }

    BoardPiecesBitField piecesBF = m_state.piecesBF;
    BoardPiecesBitField openSpacesBF = m_state.openSpacesBF;

    ulong pieceIndex;
    _BitScanForward(&pieceIndex, static_cast<ulong>(openSpacesBF.cols[x]));
    assert(pieceIndex < BOARD_HEIGHT);

    piecesBF.cols[x] |= BoardPieces(playerTurn) << pieceIndex;
    openSpacesBF.cols[x] <<= 1;

    return Board({ piecesBF, openSpacesBF }, boardHash, symmetryBF.possiblySymmetric, piece(!playerTurn), x);
}

const BoardHash Board::getNextBoardHash(int x) const
{
    return calcBoardHash(boardHash, piece(!playerTurn), x);
}

const uchar Board::calcSymmetry(const bool possiblySymmetric, const BoardState &state) {
    if (!possiblySymmetric) {
        return 0;
    }

    BoardPiecesBitField pbf = state.piecesBF;
    BoardPiecesBitField osbf = state.openSpacesBF;
    SymmetryBitField sbf(SYMMETRIC);

    // loop in reverse as most good moves will be near the center, meaning the edges will be most volatile
    for (int i = MAX_SYMMETRIC_COLUMN - 1; i >= BOARD_WIDTH % 2; --i) {
        if (osbf.cols[i] == osbf.cols[MAX_COLUMN - i]) {
            if (pbf.cols[i] != pbf.cols[MAX_COLUMN - 1]) {
                return IMPOSSIBLY_SYMMETRIC;
            }
        }
        else {
            sbf.isSymmetric = false;
            uchar colL = uchar(pbf.cols[i] & ~osbf.cols[i]);
            uchar colR = uchar(pbf.cols[MAX_COLUMN - i] & ~osbf.cols[MAX_COLUMN - i]);

            if (colL != colR) {
                return IMPOSSIBLY_SYMMETRIC;
            }
        }
    }

    return sbf;
}

const BoardHash Board::calcBoardHash(const BoardState &state) {
    BoardPieces os = state.openSpaces ^ EMPTY_OPEN_SPACES_BITS;
    uchar* openSpaces = (uchar*)(&os);
    uchar* pieces = (uchar*)&state.pieces;
    //return
    //    (static_cast<BoardHash>(pieces[6]) << (BOARD_HEIGHT * 6)) |
    //    (static_cast<BoardHash>(pieces[5]) << (BOARD_HEIGHT * 5)) |
    //    (static_cast<BoardHash>(pieces[4]) << (BOARD_HEIGHT * 4)) |
    //    (static_cast<BoardHash>(pieces[3]) << (BOARD_HEIGHT * 3)) |
    //    (static_cast<BoardHash>(pieces[2]) << (BOARD_HEIGHT * 2)) |
    //    (static_cast<BoardHash>(pieces[1]) << (BOARD_HEIGHT * 1)) |
    //    static_cast<BoardHash>(pieces[0]) | 
    //    (static_cast<BoardHash>(popCountByte(openSpaces[6])) << (COLUMN_HEIGHT_REQUIRED_BITS * 6) |
    //     static_cast<BoardHash>(popCountByte(openSpaces[5])) << (COLUMN_HEIGHT_REQUIRED_BITS * 5) |
    //     static_cast<BoardHash>(popCountByte(openSpaces[4])) << (COLUMN_HEIGHT_REQUIRED_BITS * 4) |
    //     static_cast<BoardHash>(popCountByte(openSpaces[3])) << (COLUMN_HEIGHT_REQUIRED_BITS * 3) |
    //     static_cast<BoardHash>(popCountByte(openSpaces[2])) << (COLUMN_HEIGHT_REQUIRED_BITS * 2) |
    //     static_cast<BoardHash>(popCountByte(openSpaces[1])) << (COLUMN_HEIGHT_REQUIRED_BITS * 1) |
    //     static_cast<BoardHash>(popCountByte(openSpaces[0]))) << BOARD_SIZE;

    BoardHashBitField hashBF;
    for (int i = 0; i < BOARD_WIDTH; ++i) {
        hashBF.cols[i] = pieces[i];
        hashBF.heights[i] = popCountByte(openSpaces[i]);
    }

    return hashBF;
}

const BoardHash Board::calcBoardHash(const BoardHash prevHash, const piece playerTurn, const int x)
{
    const piece prevPlayerTurn(static_cast<piece>(!playerTurn));
    const int colBitOffset = BOARD_HEIGHT * x;
    const int heightBitOffset = BOARD_SIZE + 3 * x;
    BoardHash next = prevHash + (0x1ll << heightBitOffset);
    int prevColHeight = (prevHash >> (heightBitOffset)) & 0x7;
    next |= static_cast<BoardHash>(prevPlayerTurn) << (colBitOffset + prevColHeight);

    return next;
}

const Board Board::mirrorBoard() const {
    if (symmetryBF.isSymmetric) {
        return Board(*this);
    }

    BoardPieces pieces = 0;
    BoardPieces openSpaces = 0;
    for (int x = MAX_COLUMN; x >= 0; --x) {
        pieces <<= 8;
        openSpaces <<= 8;
        pieces += getCol(m_state.pieces, colIdx(x));
        openSpaces += getCol(m_state.openSpaces, colIdx(x));
    }

    assert(heuristic == calcHeuristic({ pieces, openSpaces }));
    return Board(BoardState({ pieces, openSpaces }), playerTurn, symmetry, heuristic);
}

const string Board::getBoardString() const
{
    return Board::getBoardString(m_state.pieces, m_state.openSpaces);
}

const string Board::getBoardString(BoardHash hash)
{
    Board b = Board::fromHash(hash);
    return Board::getBoardString(b.m_state.pieces, b.m_state.openSpaces);
}

const string Board::getBoardString(const BoardPieces &pieces, const BoardPieces &openSpaces)
{
    string result(BOARD_SIZE * 2, ' ');

    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            char spacesCol = ((openSpaces & (EMPTY_COLUMN_BITS << (x << 3))) >> (x << 3)) ^ EMPTY_COLUMN_BITS;
            char piecesCol = (pieces & (EMPTY_COLUMN_BITS << (x << 3))) >> (x << 3);

            if (spacesCol & (1 << y)) {
                if (piecesCol & (1 << y)) {
                    result[(BOARD_WIDTH * (MAX_ROW - y) + x) * 2] = 'r';
                }
                else {
                    result[(BOARD_WIDTH * (MAX_ROW - y) + x) * 2] = 'b';
                }
            }
            else {
                result[(BOARD_WIDTH * (MAX_ROW - y) + x) * 2] = '_';
            }
        }

        result[BOARD_WIDTH * (y + 1) * 2 - 1] = '\n';
    }

    return result;
}

inline bool connect4solver::operator==(const Board &lhs, const Board &rhs)
{
    return lhs.boardHash == rhs.boardHash &&
        lhs.heuristic == rhs.heuristic &&
        lhs.symmetry == rhs.symmetry &&
        lhs.playerTurn == rhs.playerTurn;
}

inline bool connect4solver::operator!=(const Board &lhs, const Board &rhs)
{
    return !(lhs == rhs);
}

#pragma endregion