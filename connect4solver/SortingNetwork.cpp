#include <algorithm>
#include "SortingNetwork.h"

using namespace std;

// Sorting network indcies from http://jgamble.ripco.net/cgi-bin/nw.cgi?inputs=4&algorithm=best&output=svg
const array<int, 10> COMP_IDX_4 = {
    0, 1, 2, 3,
    0, 2, 1, 3,
    1, 2
};

void SortingNetwork::Sort4(array<Heuristic, BOARD_WIDTH> &scores, array<int, BOARD_WIDTH> &idx)
{
    for (int i = 0; i < 10; i += 2) {
        if (scores[COMP_IDX_4[i]] > scores[COMP_IDX_4[i + 1]]) {
            swap(scores[COMP_IDX_4[i]], scores[COMP_IDX_4[i + 1]]);
            swap(idx[COMP_IDX_4[i]], idx[COMP_IDX_4[i + 1]]);
        }
    }
}

void SortingNetwork::Sort4Reverse(array<Heuristic, BOARD_WIDTH> &scores, array<int, BOARD_WIDTH> &idx)
{
    for (int i = 0; i < 10; i += 2) {
        if (scores[COMP_IDX_4[i]] < scores[COMP_IDX_4[i + 1]]) {
            swap(scores[COMP_IDX_4[i]], scores[COMP_IDX_4[i + 1]]);
            swap(idx[COMP_IDX_4[i]], idx[COMP_IDX_4[i + 1]]);
        }
    }
}

// Sorting network indcies from http://jgamble.ripco.net/cgi-bin/nw.cgi?inputs=7&algorithm=best&output=svg
const array<int, 32> COMP_IDX_7 = {
    1, 2, 3, 4, 5, 6,
    0, 2, 3, 5, 4, 6,
    0, 1, 4, 5, 2, 6,
    0, 4, 1, 5,
    0, 3, 2, 5,
    1, 3, 2, 4,
    2, 3
};

void SortingNetwork::Sort7(array<Heuristic, BOARD_WIDTH> &scores, array<int, BOARD_WIDTH> &idx)
{
    for (int i = 0; i < 32; i += 2) {
        if (scores[COMP_IDX_7[i]] > scores[COMP_IDX_7[i + 1]]) {
            swap(scores[COMP_IDX_7[i]], scores[COMP_IDX_7[i + 1]]);
            swap(idx[COMP_IDX_7[i]], idx[COMP_IDX_7[i + 1]]);
        }
    }
}

void SortingNetwork::Sort7Reverse(array<Heuristic, BOARD_WIDTH> &scores, array<int, BOARD_WIDTH> &idx)
{
    for (int i = 0; i < 32; i += 2) {
        if (scores[COMP_IDX_7[i]] < scores[COMP_IDX_7[i + 1]]) {
            swap(scores[COMP_IDX_7[i]], scores[COMP_IDX_7[i + 1]]);
            swap(idx[COMP_IDX_7[i]], idx[COMP_IDX_7[i + 1]]);
        }
    }
}
