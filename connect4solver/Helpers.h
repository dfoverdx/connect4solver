#pragma once
#include <bitset>
#include <conio.h>
#include <iomanip>
#include <ostream>
#include <string>
#include <Windows.h>
#include "Typedefs.h"

#pragma region RequiredBits
template <ull N>
struct RequiredBits {
    enum : int { value = 1 + RequiredBits<(N >> 1)>::value };
};

template <>
struct RequiredBits<1> {
    enum : int { value = 1 };
};

template <>
struct RequiredBits<0> {
    enum : int { value = 1 };
};
#pragma endregion

#pragma region RepeatedByte
template <
    uchar byte, // byte to repeat
    int N // 0-indexed number of repeats
>
struct RepeatedByte {
    static_assert(N <= 8, "Repeated byte can only be repeated up to 8 times and fit in a 64-bit integer");
    enum : ull { value = (RepeatedByte<byte, N - 1>::value << 8) + byte };
};

template <uchar byte>
struct RepeatedByte<byte, 0> {
    enum : ull { value = byte };
};
#pragma endregion

#pragma region Exponent
template <ull base, int power>
struct Exponent {
    enum : ull { value = (Exponent<base, power - 1>::value) * base };
};

template <ull base>
struct Exponent<base, 0> {
    enum : ull { value = 1 };
};
#pragma endregion

template<typename T>
const std::string getBinary(T val) {
    std::string r = string(sizeof(val) * 9); // 9 = 8 bits + \n
    char* byte = (char*)&val;
    for (int i = 0; i < sizeof(val); ++i) {
        r.replace(9 * i, 8, std::bitset<8>(byte[i]).to_string());
        r[9 * i + 8] = '\n';
    }

    return r;
}

//template const std::string getBinary<ull>(ull);

namespace helperSetConsoleColor {
    struct setConsoleColor {
        setConsoleColor(WORD color) : m_c(color) {}
        const WORD m_c;
    };
}

template <class _Elem, class _Traits>
std::basic_ostream<_Elem, _Traits>&
operator<<(std::basic_ostream<_Elem, _Traits> &i, helperSetConsoleColor::setConsoleColor &c)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c.m_c);
    return i;
}

inline std::ostream& resetColor(std::ostream &s) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    return s;
}

inline helperSetConsoleColor::setConsoleColor setColor(WORD color) {
    return helperSetConsoleColor::setConsoleColor(color);
}

namespace helperPrintTime {
    struct printTime {
        printTime(ull ms) : m_ms(ms) {}
        const ull m_ms;
    };
}

template <class _Elem, class _Traits>
std::basic_ostream<_Elem, _Traits>&
operator<<(std::basic_ostream<_Elem, _Traits> &i, helperPrintTime::printTime &pt)
{
    auto w = i.width();
    auto f = i.fill();
    ull seconds = pt.m_ms / CLOCKS_PER_SEC;
    ull minutes = seconds / 60;
    ull hours = minutes / 60;
    ull days = hours / 24;

    i 
        << days << ':' << std::setfill('0') << std::setw(2)
        << hours % 24 << ':' << std::setw(2)
        << minutes % 60 << ':' << std::setw(2)
        << seconds % 60 << std::setw(w) << std::setfill(f);

    return i;
}

inline helperPrintTime::printTime printTime(ull ms) {
    return helperPrintTime::printTime(ms);
}

inline void clearScreen()
{
    HANDLE                     hStdOut;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD                      count;
    DWORD                      cellCount;
    COORD                      homeCoords = { 0, 0 };

    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) return;

    /* Get the number of cells in the current buffer */
    if (!GetConsoleScreenBufferInfo(hStdOut, &csbi)) return;
    cellCount = csbi.dwSize.X *csbi.dwSize.Y;

    /* Fill the entire buffer with spaces */
    if (!FillConsoleOutputCharacter(
        hStdOut,
        (TCHAR) ' ',
        cellCount,
        homeCoords,
        &count
    )) return;

    /* Fill the entire buffer with the current colors and attributes */
    if (!FillConsoleOutputAttribute(
        hStdOut,
        csbi.wAttributes,
        cellCount,
        homeCoords,
        &count
    )) return;

    /* Move the cursor home */
    SetConsoleCursorPosition(hStdOut, homeCoords);
}