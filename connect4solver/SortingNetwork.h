#pragma once
#include <array>
#include <memory>
#include "Constants.h"

using namespace connect4solver;

namespace connect4solver {
    class SortingNetwork
    {
    public:
        static void Sort4(std::array<Heuristic, BOARD_WIDTH> &scores, std::array<int, BOARD_WIDTH> &idx);
        static void Sort4Reverse(std::array<Heuristic, BOARD_WIDTH> &scores, std::array<int, BOARD_WIDTH> &idx);
        static void Sort7(std::array<Heuristic, BOARD_WIDTH> &scores, std::array<int, BOARD_WIDTH> &idx);
        static void Sort7Reverse(std::array<Heuristic, BOARD_WIDTH> &scores, std::array<int, BOARD_WIDTH> &idx);
    };
}