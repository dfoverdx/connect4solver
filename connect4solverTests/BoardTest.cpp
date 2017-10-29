#include "stdafx.h"
#include "CppUnitTest.h"
#include "../connect4solver/Board.cpp"
#include "../connect4solver/BoardState.cpp"
#include "../connect4solver/PopCount.cpp"

using namespace std;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft {
    namespace VisualStudio {
        namespace CppUnitTestFramework {
            using namespace std;
            using namespace connect4solver;

            template <> static std::wstring ToString<Board>(const class Board &b) {
                string bString = b.getBoardString();
                wstring str(bString.length(), L' ');
                copy(bString.begin(), bString.end(), str.begin());
                return str;
            }

            template <> static wstring ToString<gameOverState>(const enum gameOverState &gos) {
                switch (gos) {
                case gameOverState::blackLost:
                    return L"blackLost";
                case gameOverState::blackWon:
                    return L"blackWon";
                case gameOverState::unfinished:
                    return L"unfinished";
                default:
                    Assert::Fail(L"Unknown gameOverState referenced");
                    return L"<unknown>";
                }
            }
        }
    }
}

namespace connect4solverTests
{
    TEST_CLASS(BoardTest)
    {
    public:
        TEST_METHOD(DetermineGameOverStateVertical)
        {
            // test basic left-most column
            Board b = Board();
            Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test second column
            b = Board();
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2 + 1);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test when up one
            b = Board();
            b = b.addPiece(6);
            b = b.addPiece(0);
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test when top-most right-most
            b = Board();
            b.addPiece(6);
            b.addPiece(6);

            // test when up one
            b = Board();
            b = b.addPiece(6);
            b = b.addPiece(6);
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2 ? 0 : 6);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(6);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test for red wins
            b = Board();
            b = b.addPiece(1);

            for (int i = 1; i < 7; ++i) {
                b = b.addPiece(i % 2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackLost, b.getGameOver());
        }

        TEST_METHOD(DetermineGameOverStateHorizontal) {
            // test basic bottom left horizontal
            Board b = Board();
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i / 2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(3);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test bottom starting at col 1
            b = Board();
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i / 2 + 1);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            //test bottom with gap
            b = Board();
            b = b.addPiece(0);
            b = b.addPiece(0);
            for (int i = 0; i < 4; ++i) {
                b = b.addPiece(i / 2 + 2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test top right
            b = Board();
            for (int i = 0; i < BOARD_HEIGHT - 1; ++i) {
                for (int j = 3; j < BOARD_WIDTH; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i % 3 == 0) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            for (int i = 3; i < BOARD_WIDTH - 1; ++i) {
                b = b.addPiece(i);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");

                b = b.addPiece(2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(6);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test for red wins
            b = Board();
            b = b.addPiece(0);
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(1 + i / 2);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackLost, b.getGameOver(), L"Game over set before it should have");
        }

        TEST_METHOD(DetermineGameOverStateDiagonalUpRight) {
            // test basic bottom left position
            Board b = Board();
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 4; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                b = b.addPiece(6);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(6);
            Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");

            b = b.addPiece(3);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test middle position
            b = Board();
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(6);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            b = b.addPiece(6);
            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test with gap
            b = Board();
            b = b.addPiece(0);
            b = b.addPiece(1);
            for (int i = 0; i < 4; ++i) {
                for (int j = 2; j < 4; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                b = b.addPiece(6);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test top right position
            b = Board();
            for (int i = 0; i < 5; ++i) {
                for (int j = 3; j < BOARD_WIDTH; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i > 1) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            b = b.addPiece(0);
            b = b.addPiece(MAX_COLUMN);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test red wins
            b = Board();
            b = b.addPiece(6);
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(6);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            b = b.addPiece(6);
            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackLost, b.getGameOver());
        }

        TEST_METHOD(DetermineGameOverStateDiagonalUpLeft) {
            // test basic bottom right position
            Board b = Board();
            for (int i = 0; i < 3; ++i) {
                for (int j = MAX_COLUMN; j > 2; --j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                b = b.addPiece(0);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");

            b = b.addPiece(3);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test middle position
            b = Board();
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            b = b.addPiece(0);
            b = b.addPiece(2);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test with gap
            b = Board();
            b = b.addPiece(6);
            b = b.addPiece(5);
            for (int i = 0; i < 4; ++i) {
                for (int j = 2; j < 4; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                b = b.addPiece(0);
                Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
            }

            b = b.addPiece(5);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test top left position
            b = Board();
            for (int i = 0; i < 5; ++i) {
                for (int j = 3; j < BOARD_WIDTH; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i > 1) {
                    b = b.addPiece(6);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            b = b.addPiece(6);
            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::blackWon, b.getGameOver());

            // test red wins
            b = Board();
            b = b.addPiece(0);
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, b.getGameOver(), L"Game over set before it should have");
                }
            }

            b = b.addPiece(0);
            b = b.addPiece(2);
            Assert::AreEqual(gameOverState::blackLost, b.getGameOver());
        }

        TEST_METHOD(CalcBoardHash) {
            Board b;
            Assert::AreEqual(b.boardHash, static_cast<BoardHash>(0), L"Board hash for empty board was not 0.");

            BoardPiecesBitField ps(0);
            BoardPiecesBitField os(EMPTY_OPEN_SPACES_BITS);
            BoardHash hTest = Board::calcBoardHash({ ps, os });
            Assert::AreEqual(b.boardHash, hTest, L"Board hash for empty board did not match manually created Board hash using static calcBoardHash");

            for (int i = 0; i < BOARD_WIDTH; ++i) {
                Board b2 = b.addPiece(i);
                Assert::AreEqual(b2.boardHash, 
                    static_cast<BoardHash>(1ll << (BOARD_SIZE + COLUMN_HEIGHT_REQUIRED_BITS * i)), 
                    L"Board hash for single piece was not 1 << SQUISHED_BOARD_COLUMN_BITS * i + BOARD_HEIGHT.");

                
                BoardPiecesBitField ps2 = ps;
                BoardPiecesBitField os2 = os;

                os2.cols[i] <<= 1;
                hTest = Board::calcBoardHash({ ps2, os2 });

                Assert::AreEqual(b2.boardHash, hTest, 
                    L"Board hash for single piece did not match manually created Board hash using static calcBoardHash");
                
                b2 = b2.addPiece(i);
                Assert::AreEqual(b2.boardHash, 
                    static_cast<BoardHash>(((0x2ll << BOARD_HEIGHT * i) | (0x2ll << (BOARD_SIZE + COLUMN_HEIGHT_REQUIRED_BITS * i)))),
                        L"BoardHash after two moves on the same column was incorrect.");

                os2.cols[i] <<= 1;
                ps2.cols[i] |= 0x2;
                hTest = Board::calcBoardHash({ ps2, os2 });

                Assert::AreEqual(b2.boardHash, hTest, 
                    L"Board hash for two moves on the same column did not match manually created Board hash using static calcBoardHash");

                for (int j = 0; j < BOARD_WIDTH; ++j) {
                    if (j == i) {
                        continue;
                    }

                    Board b3 = b2.addPiece(j);
                    BoardHash b3hash = (1ll << (BOARD_SIZE + COLUMN_HEIGHT_REQUIRED_BITS * j)) | b2.boardHash;

                    Assert::AreEqual(b3.boardHash, b3hash,
                        L"BoardHash after three moves was incorrect.");

                    BoardPiecesBitField ps3 = ps2;
                    BoardPiecesBitField os3 = os2;

                    os3.cols[j] <<= 1;
                    hTest = Board::calcBoardHash({ ps3, os3 });

                    Assert::AreEqual(b3.boardHash, hTest,
                        L"BoardHash after three moves did not match manually created Board hash using static calacBoardHash.");
                }
            }
        }

        TEST_METHOD(BoardFromBoardHash) {
            Board b = Board::fromHash(0);
            Assert::AreEqual(b.boardHash, static_cast<BoardHash>(0), L"Board generated from hash=0 was incorrect.");

            const int moves[] = { 3, 3, 3, 3, 3, 3, 2, 2, 6, 2, 6, 2, 2, 4, 5, 4, 5, 5, 2, 5, 6, 6, 6, 5, 5, 0, 6, 0, 0, 4, 4, 4, 4, 0, 0, 1, 1, 1, 1, 1 };
            Board bTest;
            
            for (int i = 0; i < 39; ++i) {
                b = b.addPiece(moves[i]);
                bTest = Board::fromHash(b.boardHash);

                Assert::AreEqual(b, bTest, L"Board generated from hash did not match original board.");
            }
        }
    };
}