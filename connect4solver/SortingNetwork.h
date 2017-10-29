#pragma once
#include <array>
#include "Constants.h"

namespace connect4solver {
    namespace SortingNetwork {
#define compAndSwap(idx1, idx2) \
            if (a[idx1] > a[idx2]) \
            { \
                swap(a[idx1], a[idx2]); \
            }

        // Sorting network indcies from http://jgamble.ripco.net/cgi-bin/nw.cgi?inputs=4&algorithm=best&output=svg
        template<typename T>
        inline void Sort4(std::array<T, BOARD_WIDTH> &a) {
            compAndSwap(0, 1);
            compAndSwap(2, 3);
            compAndSwap(0, 2);
            compAndSwap(1, 3);
            compAndSwap(1, 2);
        }

        template<typename T>
        inline void Sort4Reverse(std::array<T, BOARD_WIDTH> &a) {
            compAndSwap(1, 0);
            compAndSwap(3, 2);
            compAndSwap(2, 0);
            compAndSwap(3, 1);
            compAndSwap(2, 1);
        }

        // Sorting network indcies from http://jgamble.ripco.net/cgi-bin/nw.cgi?inputs=7&algorithm=best&output=svg
        template<typename T>
        inline void Sort7(std::array<T, BOARD_WIDTH> &a) {
            compAndSwap(1, 2);
            compAndSwap(3, 4);
            compAndSwap(5, 6);
            compAndSwap(0, 2);
            compAndSwap(3, 5);
            compAndSwap(4, 6);
            compAndSwap(0, 1);
            compAndSwap(4, 5);
            compAndSwap(2, 6);
            compAndSwap(0, 4);
            compAndSwap(1, 5);
            compAndSwap(0, 3);
            compAndSwap(2, 5);
            compAndSwap(1, 3);
            compAndSwap(2, 4);
            compAndSwap(2, 3);
        }

        template<typename T>
        inline void Sort7Reverse(std::array<T, BOARD_WIDTH> &a) {
            compAndSwap(2, 1);
            compAndSwap(4, 3);
            compAndSwap(6, 5);
            compAndSwap(2, 0);
            compAndSwap(5, 3);
            compAndSwap(6, 4);
            compAndSwap(1, 0);
            compAndSwap(5, 4);
            compAndSwap(6, 2);
            compAndSwap(4, 0);
            compAndSwap(5, 1);
            compAndSwap(3, 0);
            compAndSwap(5, 2);
            compAndSwap(3, 1);
            compAndSwap(4, 2);
            compAndSwap(3, 2);
        }
    }
}