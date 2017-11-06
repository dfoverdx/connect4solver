#pragma once
#include <string>

namespace connect4solver {
    struct IoMessage {
        IoMessage(const std::string msg, const WORD color = 0x7) : msg(msg), color(color) {}

        const std::string msg;
        const WORD color;
    };
}