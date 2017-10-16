#pragma once
#include <climits>
#include "Constants.h"
#include "GameOverState.h"
#include "Piece.h"
#include "Typedefs.h"

using namespace connect4solver;

namespace connect4solver {
    const Heuristic BLACK_WON_HEURISTIC = (std::numeric_limits<Heuristic>::max)();
    const Heuristic BLACK_LOST_HEURISTIC = (std::numeric_limits<Heuristic>::min)();

    class DllExport Board {
    private:
        const BoardState m_state;

#ifdef ALWAYS_USE_HEURISTIC

        // used when mirroring a board
        Board(const BoardState &state, const piece playerTurn, const bool isSymmetric, const Heuristic heuristic);

        static const Heuristic calculateHeuristic(const BoardState &state);

#else

        Heuristic m_heuristic;
        bool m_calculatedHeuristic;

        // used when mirroring a board
        Board(const BoardState &state, const piece playerTurn, const bool isSymmetric, const gameOverState gameOver,
            const Heuristic heuristic, const bool calculatedHeuristic);

        inline const Heuristic calculateHeuristic() const;
        static gameOverState determineGameOverState(const BoardState &state, const int x);

#endif

        // used when adding a piece to a board
        Board(const BoardState &state, const piece playerTurn, const int x);

        static bool determineIsSymmetric(const BoardState &state);
        static BoardHash calcBoardHash(const BoardState &state);

        inline static const int getColHeight(const BoardPieces &openSpaces, const int x);

    public:
        Board();
        Board(const Board &b);
        Board& operator=(const Board &other);

        const BoardHash boardHash;
        const piece playerTurn;
        const bool isSymmetric;

#ifdef ALWAYS_USE_HEURISTIC
        const Heuristic heuristic;
        const gameOverState getGameOver() const;
#else
        const gameOverState gameOver;
        const Heuristic getHeuristic();
#endif

        inline const int getColHeight(const int x) const;

        const piece getPiece(const int x, const int y) const;
        const bool isValidMove(const int x) const;

        Board addPiece(const int x) const;

        const Board mirrorBoard() const;
    };
}