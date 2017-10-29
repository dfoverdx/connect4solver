#pragma once
#include <climits>
#include <memory>
#include <bitfield.h>
#include <string>
#include "BoardState.h"
#include "Constants.h"
#include "GameOverState.h"
#include "Helpers.h"
#include "Piece.h"
#include "Typedefs.h"

using namespace connect4solver;

unitTestClass(BoardTest);

namespace connect4solver {
    BEGIN_BITFIELD_TYPE_DLL_EXPORT(ValidMoves, uchar)
        ADD_BITFIELD_ARRAY(cols, 0, 1, BOARD_WIDTH)
    END_BITFIELD_TYPE()

    class DllExport Board {
        unitTestFriend(BoardTest);

    /// structs ///
    public:
        struct SymmetryBitField {
            uchar
                isSymmetric : 1,
                possiblySymmetric : 1;

            explicit SymmetryBitField(const uchar uc) : 
                isSymmetric((uc & '\1') != 0), possiblySymmetric((uc & '\2') != 0) {}

            operator uchar() const {
                return isSymmetric | possiblySymmetric << 1;
            }

            SymmetryBitField& operator=(uchar uc) {
                return SymmetryBitField(uc);
            }

            SymmetryBitField& operator=(const SymmetryBitField bf) {
                return SymmetryBitField(bf);
            }
        };

    /// members ///
    private:
        const BoardState m_state;
        
        // used when adding a piece to a board
        Board(const BoardState &state, const BoardHash &prevHash, const bool possiblySymmetric, const piece playerTurn,
            const int x);

        // building board from hash or used when mirroring a board
        Board(const BoardState &state, const piece playerTurn, const uchar symmetry, const Heuristic heuristic);

    public:
        Board();
        Board(const Board &b);
        Board& operator=(const Board &other);

        ~Board() {}

        union {
            const BoardHash boardHash;
            const BoardHashBitField boardHashBF;
        };

        union {
            const uchar symmetry;
            const SymmetryBitField symmetryBF;
        };

        const piece playerTurn;
        const Heuristic heuristic;
        const ValidMoves validMoves;

        inline const int getColHeight(const int x) const;
        inline const gameOverState getGameOver() const;
        const piece getPiece(const int x, const int y) const;
        const bool isValidMove(const int x) const;

        Board addPiece(int x) const;
        const BoardHash getNextBoardHash(int x) const;
        const Board mirrorBoard() const;

        const std::string getBoardString() const;

    /// statics ///
    private:
        static const Heuristic calcHeuristic(const BoardState &state);
        static const uchar calcSymmetry(const bool possiblySymmetric, const BoardState &state);
        static const BoardHash calcBoardHash(const BoardState &state);
        static const BoardHash calcBoardHash(const BoardHash prevHash, const piece playerTurn, const int x);
        static const std::string getBoardString(const BoardPieces &pieces, const BoardPieces &openSpaces);
        inline static const ValidMoves calcValidMoves(const BoardState &state);

    public:
        friend bool operator==(const Board &lhs, const Board &rhs);
        friend bool operator!=(const Board &lhs, const Board &rhs);

        static const Board fromHash(BoardHash hash);
        static const std::string getBoardString(BoardHash hash);
    };
}