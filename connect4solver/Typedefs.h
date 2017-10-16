#pragma once

#define DllExport __declspec(dllexport)
#define assignConst(type, member, other) *const_cast<type*>(&member) = other.member

typedef char tinyint;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef unsigned long ulong;
typedef unsigned long long ull;

namespace connect4solver {
    typedef DllExport ull BoardHash;
    typedef DllExport long long Heuristic;
    typedef DllExport ull BoardPieces;

    struct DllExport BoardState {
        BoardState& operator=(const BoardState &other) {
            assignConst(BoardPieces, pieces, other);
            assignConst(BoardPieces, openSpaces, other);
            return *this;
        }

        const BoardPieces pieces;
        const BoardPieces openSpaces;
    };
}