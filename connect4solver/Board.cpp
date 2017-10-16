#include <algorithm>
#include <cassert>
#include <exception>
#include <string>
#include <utility>
#include "Board.h"
#include "PopCount.h"

using namespace std;
using namespace connect4solver;

#ifndef NDEBUG
#include <bitset>

static const string getBinary(ull l) {
    string r = string();
    for (int i = 7; i >= 0; --i) {
        r += bitset<8>(((char*)(&l))[i]).to_string() + '\n';
    }
    return r;
}

#endif // !NDEBUG

#pragma region Macros

#define colIdx(x) ((x) << 3)
#define colMask(cIdx) (EMPTY_COLUMN_BITS << (cIdx))
#define getCol(pieces, cIdx) ((pieces & colMask(cIdx)) >> (cIdx))

#ifdef ALWAYS_USE_HEURISTIC

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
checkRedWin(rShiftedPieces, rShiftedAvailableSpaces, condition);

#endif // ALWAYS_USE_HEURISTIC
#pragma endregion

namespace boardTemplates {
    template <
        uchar byte, // byte to repeat
        uchar N // 0-indexed number of repeats
    >
        struct repeatedByte {
        enum : ull { value = (repeatedByte<byte, N - 1>::value << 8) + byte };
    };

    template <uchar byte>
    struct repeatedByte<byte, 0> {
        enum : ull { value = byte };
    };
}

const BoardPieces EMPTY_COLUMN_BITS = (1 << BOARD_HEIGHT) - 1;
const BoardPieces EMPTY_OPEN_SPACES_BITS = boardTemplates::repeatedByte<EMPTY_COLUMN_BITS, MAX_COLUMN>::value;
const BoardPieces BOARD_MASK = EMPTY_OPEN_SPACES_BITS;

const BoardPieces VERTICAL_WIN = 0xF;
const BoardPieces HORIZONTAL_WIN = 0x01010101;
const BoardPieces DIAGONAL_FORWARD_WIN = 0x08040201;

#ifdef ALWAYS_USE_HEURISTIC
static const BoardPieces DIAGONAL_BACKWARD_WIN = 0x01020408ull;
#else
static const BoardPieces DIAGONAL_BACKWARD_WIN = 0x01020408ull << colIdx(3);
#endif

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

#ifdef ALWAYS_USE_HEURISTIC

Board::Board() :
    m_state({ 0, EMPTY_OPEN_SPACES_BITS }), heuristic(0), boardHash(0), playerTurn(BLACK), isSymmetric(true) {}

Board::Board(const Board &b) :
    m_state(b.m_state), heuristic(b.heuristic), boardHash(b.boardHash), playerTurn(b.playerTurn),
    isSymmetric(b.isSymmetric) {}

Board::Board(const BoardState &state, const piece playerTurn, const int x) :
    m_state(state), heuristic(calculateHeuristic(state)), boardHash(calcBoardHash(state)), playerTurn(playerTurn),
    isSymmetric(determineIsSymmetric(state)) {}

Board::Board(const BoardState &state, const piece playerTurn, const bool isSymmetric, const Heuristic heuristic) :
    m_state(state), heuristic(heuristic), boardHash(calcBoardHash(state)), playerTurn(playerTurn),
    isSymmetric(isSymmetric) {}

const gameOverState Board::getGameOver() const
{
    switch (heuristic) {
    case BLACK_WON_HEURISTIC:
        return gameOverState::blackWon;
    case BLACK_LOST_HEURISTIC:
        return gameOverState::blackLost;
    default:
        return gameOverState::unfinished;
    }
}

const Heuristic Board::calculateHeuristic(const BoardState &state)
{
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

    return scores[0] - scores[1];
}

#else

Board::Board() :
    m_state({ 0, EMPTY_OPEN_SPACES_BITS }), m_heuristic(0), m_calculatedHeuristic(true), boardHash(0),
    gameOver(gameOverState::unfinished), playerTurn(BLACK), isSymmetric(true) {}

Board::Board(const Board& b) :
    m_state(b.m_state), m_heuristic(b.m_heuristic), m_calculatedHeuristic(b.m_calculatedHeuristic),
    boardHash(b.boardHash), playerTurn(b.playerTurn), gameOver(b.gameOver), isSymmetric(b.isSymmetric) {}

Board::Board(const BoardState &state, const piece playerTurn, const bool isSymmetric, const gameOverState gameOver,
    const Heuristic heuristic, const bool calculatedHeuristic) :
    m_state(state), m_heuristic(heuristic), m_calculatedHeuristic(calculatedHeuristic),
    boardHash(calcBoardHash(state)), playerTurn(playerTurn), gameOver(gameOver), isSymmetric(isSymmetric) {}

Board::Board(const BoardState &state, const piece playerTurn, const int x) :
    m_state(state), m_heuristic(0), m_calculatedHeuristic(false),
    boardHash(calcBoardHash(state)), playerTurn(playerTurn),
    gameOver(determineGameOverState(state, x)), isSymmetric(determineIsSymmetric(state)) {}

const Heuristic Board::getHeuristic()
{
    if (!m_calculatedHeuristic) {
        m_heuristic = calculateHeuristic();
        m_calculatedHeuristic = true;
    }

    return m_heuristic;
}

gameOverState Board::determineGameOverState(const BoardState &state, const int x)
{
    const int y = getColHeight(state.openSpaces, x) - 1;
    const int cIdx = colIdx(x);
#ifndef NDEBUG
    long long pieceIdx = cIdx + y;
    assert(static_cast<bool>(!_bittest64((long long*)&state.openSpaces, pieceIdx)));
    piece p = piece(_bittest64((long long*)&state.pieces, pieceIdx));
#else
    piece p = piece(_bittest64((long long*)&state.pieces, cIdx + y));
#endif

    const gameOverState pWon = p == RED ? gameOverState::blackLost : gameOverState::blackWon;

    BoardPieces playerPieces = p == RED ? state.pieces : ~state.pieces;
    playerPieces &= state.openSpaces ^ BOARD_MASK;

    // check vertical
    if (y >= 3 && ((playerPieces >> (colIdx(x) + y - 3)) & VERTICAL_WIN) == VERTICAL_WIN) {
        return pWon;
    }

    // check horizontal
    ShiftLimits limits = H_SHIFT_LIMITS[x];

    for (int i = limits.first; i <= limits.second; ++i) {
        if (((playerPieces >> (colIdx(i) + y)) & HORIZONTAL_WIN) == HORIZONTAL_WIN) {
            return pWon;
        }
    }

    // check diagonal /
    // shift right such that y = x
    BoardPieces shiftedPieces = playerPieces;
    int s = x;
    if (x > y) {
        // shift left
        shiftedPieces >>= colIdx(x - y);
        s = y;
    }
    else if (x < y) {
        // shift down
        shiftedPieces >>= y - x;
    }

    limits = V_SHIFT_LIMITS[s];

    for (int i = limits.first; i <= limits.second; ++i) {
        BoardPieces tmpShiftedPieces = shiftedPieces >> (9 * i);
        if ((tmpShiftedPieces & DIAGONAL_FORWARD_WIN) == DIAGONAL_FORWARD_WIN) {
            return pWon;
        }
    }

    if (x + y >= BOARD_WIDTH) {
        return gameOverState::unfinished;
    }

    // check diagonal \ 
    shiftedPieces = playerPieces;
    s = MAX_COLUMN - x;
    if (s > y) {
        // shift right
        shiftedPieces <<= colIdx(s - y);
        s = y;
    }
    else if (s < y) {
        // shift down
        shiftedPieces >>= y - s;
    }

    limits = V_SHIFT_LIMITS[s];

    for (int i = limits.first; i <= limits.second; ++i) {
        BoardPieces tmpShiftedPieces = shiftedPieces << (7 * i);
        if ((tmpShiftedPieces & DIAGONAL_BACKWARD_WIN) == DIAGONAL_BACKWARD_WIN) {
            return pWon;
        }
    }

    return gameOverState::unfinished;
}

const Heuristic Board::calculateHeuristic() const
{
    // double check that the game isn't already over
    switch (gameOver) {
    case gameOverState::blackLost:
        return BLACK_LOST_HEURISTIC;

    case gameOverState::blackWon:
        return BLACK_WON_HEURISTIC;
    }

    Heuristic scores[2] = { 0, 0 };

    for (piece p = piece(0); p < 2; p = piece(p + 1)) {
        BoardPieces playerPieces = p == RED ? m_state.pieces : ~m_state.pieces;
        playerPieces &= m_state.openSpaces ^ BOARD_MASK;
        BoardPieces availableSpaces = (playerPieces ^ m_state.openSpaces) & BOARD_MASK;

        for (int x = 0; x < BOARD_WIDTH; ++x) {
            for (int y = 0; y < BOARD_HEIGHT; ++y) {
                BoardPieces shiftedPieces = playerPieces >> (colIdx(x) + y);
                BoardPieces shiftedAvailableSpaces = availableSpaces >> (colIdx(x) + y);

                // check vertical
                if (y < 3 &&
                    (shiftedAvailableSpaces & VERTICAL_WIN) == VERTICAL_WIN &&
                    shiftedPieces & VERTICAL_WIN) {
                    scores[p] += 1ull << (popCount64(shiftedPieces & VERTICAL_WIN) - 1);
                }

                // check horizontal
                if (x < 4) {
                    if ((shiftedAvailableSpaces & HORIZONTAL_WIN) == HORIZONTAL_WIN &&
                        shiftedPieces & HORIZONTAL_WIN) {
                        scores[p] += 1ull << (popCount64(shiftedPieces & HORIZONTAL_WIN) - 1);
                    }

                    // check diagonals
                    if (y < 3) {
                        // check /
                        if ((shiftedAvailableSpaces & DIAGONAL_FORWARD_WIN) == DIAGONAL_FORWARD_WIN &&
                            shiftedPieces & DIAGONAL_FORWARD_WIN) {
                            scores[p] += 1ull << (popCount64(shiftedPieces & DIAGONAL_FORWARD_WIN));
                        }

                        // check \ 
                        shiftedPieces = playerPieces << (colIdx(x) - y);
                        shiftedAvailableSpaces = availableSpaces << (colIdx(x) - y);
                        if ((shiftedAvailableSpaces & DIAGONAL_BACKWARD_WIN) == DIAGONAL_BACKWARD_WIN &&
                            shiftedPieces & DIAGONAL_BACKWARD_WIN) {
                            scores[p] += 1ull << (popCount64(shiftedPieces * DIAGONAL_BACKWARD_WIN));
                        }
                    }
                }
            }
        }
    }

    return scores[0] - scores[1];
}

#endif // ALWAYS_USE_HEURISTIC

Board& Board::operator=(const Board &other) {
    if (this == &other) {
        return *this;
    }

    assignConst(BoardState, m_state, other);

#ifdef ALWAYS_USE_HEURISTIC
    assignConst(Heuristic, heuristic, other);
#else
    m_heuristic = other.m_heuristic;
    m_calculatedHeuristic = other.m_calculatedHeuristic;
    assignConst(gameOverState, gameOver, other);
#endif // ALWAYS_USE_HEURISTIC

    assignConst(BoardHash, boardHash, other);
    assignConst(piece, playerTurn, other);
    assignConst(bool, isSymmetric, other);
    return *this;
}

inline const int Board::getColHeight(const int x) const
{
    return getColHeight(m_state.openSpaces, x);
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

const bool Board::isValidMove(const int x) const
{
    assert(x / BOARD_WIDTH == 0);
    return (m_state.openSpaces & colMask(colIdx(x))) > 0;
}

Board Board::addPiece(const int x) const {
    assert(x / BOARD_WIDTH == 0);
    if (!isValidMove(x)) {
        throw exception(("Tried to add piece to full column: " + to_string((int)x)).c_str());
    }

#ifdef ALWAYS_USE_HEURISTIC
    if (getGameOver() != gameOverState::unfinished) {
        throw exception("Tried to add a piece when the game is over");
    }
#else
    if (gameOver != gameOverState::unfinished) {
        throw exception("Tried to add a piece when the game is over");
    }
#endif // ALWAYS_USE_HEURISTIC

    BoardPieces openSpacesMask = colMask(colIdx(x));
    BoardPieces openSpaces = m_state.openSpaces & openSpacesMask;
    BoardPieces pieceMask = 0;
    _BitScanForward64((ulong*)&pieceMask, openSpaces);
    pieceMask = 1ll << pieceMask;
    openSpaces &= openSpaces << 1;
    openSpaces |= m_state.openSpaces & (-1ll ^ (0xFFll << colIdx(x)));

    BoardPieces pieces = m_state.pieces ^ (-playerTurn ^ m_state.pieces) & pieceMask;
    return Board({ pieces, openSpaces }, piece(!playerTurn), x);
}

bool Board::determineIsSymmetric(const BoardState &state) {
    // loop in reverse as most good moves will be near the center, meaning the edges will be most volatile
    for (int i = SYMMETRIC_MID_COLUMN - 1; i >= BOARD_WIDTH % 2; --i) {
        int leftShift = (SYMMETRIC_BOARD_WIDTH - BOARD_WIDTH % 2 + i) * 8;
        int rightShift = (SYMMETRIC_MID_COLUMN - i) * 8;
        BoardHash leftMask = EMPTY_COLUMN_BITS << leftShift;
        BoardHash rightMask = EMPTY_COLUMN_BITS << rightShift;

        if (((state.openSpaces & leftMask) >> leftShift) != ((state.openSpaces & rightMask) >> rightShift) ||
            ((state.pieces & leftMask) >> leftShift) != ((state.pieces & rightMask) >> rightShift)) {
            return false;
        }
    }

    return true;
}

BoardHash Board::calcBoardHash(const BoardState &state) {
    // hash function essentially stolen from 
    // https://codereview.stackexchange.com/questions/171999/specializing-stdhash-for-stdarray

    BoardHash result = 0;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        result = result * 0x1F + ((state.pieces & colMask(colIdx(x))) >> colIdx(x)) + ((~state.openSpaces & colMask(colIdx(x))) >> colIdx(x));
    }

    return result;
}

const Board Board::mirrorBoard() const {
    if (isSymmetric) {
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

#ifdef ALWAYS_USE_HEURISTIC
    assert(heuristic == calculateHeuristic({ pieces, openSpaces }));
    return Board({ pieces, openSpaces }, playerTurn, isSymmetric, heuristic);
#else
    return Board({ pieces, openSpaces }, playerTurn, isSymmetric, gameOver, m_heuristic, m_calculatedHeuristic);
#endif // ALWAYS_USE_HEURISTIC
}

inline const int Board::getColHeight(const BoardPieces &openSpaces, const int x)
{
    unsigned long height;
    if (!_BitScanForward64(&height, getCol(openSpaces, colIdx(x)))) {
        return BOARD_HEIGHT;
    }

    return int(height);
}