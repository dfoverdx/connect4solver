#include "stdafx.h"
#include "CppUnitTest.h"
#include "../connect4solver/Board.cpp"
#include "../connect4solver/PopCount.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Microsoft {
    namespace VisualStudio {
        namespace CppUnitTestFramework {
            using namespace std;
            using namespace connect4solver;

            //template <> static std::wstring ToString<Board>(const class Board &b) {
            //    return L"";
            //}

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
#ifdef ALWAYS_USE_HEURISTIC
#define getGameOver(board) board.getGameOver()
#define getHeuristic(board) board.heuristic
#else
#define getGameOver(board) board.gameOver
#define getHeuristic(board) board.getHeuristic()
#endif

    TEST_CLASS(BoardTest)
    {
    public:
        TEST_METHOD(DetermineGameOverStateVertical)
        {
            // test basic left-most column
            Board b = Board();
            Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test second column
            b = Board();
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2 + 1);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test when up one
            b = Board();
            b = b.addPiece(6);
            b = b.addPiece(0);
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i % 2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

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
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(6);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test for red wins
            b = Board();
            b = b.addPiece(1);

            for (int i = 1; i < 7; ++i) {
                b = b.addPiece(i % 2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackLost, getGameOver(b));
        }

        TEST_METHOD(DetermineGameOverStateHorizontal) {
            // test basic bottom left horizontal
            Board b = Board();
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i / 2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(3);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test bottom starting at col 1
            b = Board();
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(i / 2 + 1);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            //test bottom with gap
            b = Board();
            b = b.addPiece(0);
            b = b.addPiece(0);
            for (int i = 0; i < 4; ++i) {
                b = b.addPiece(i / 2 + 2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test top right
            b = Board();
            for (int i = 0; i < BOARD_HEIGHT - 1; ++i) {
                for (int j = 3; j < BOARD_WIDTH; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i % 3 == 0) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            for (int i = 3; i < BOARD_WIDTH - 1; ++i) {
                b = b.addPiece(i);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");

                b = b.addPiece(2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(6);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test for red wins
            b = Board();
            b = b.addPiece(0);
            for (int i = 0; i < 6; ++i) {
                b = b.addPiece(1 + i / 2);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackLost, getGameOver(b), L"Game over set before it should have");
        }

        TEST_METHOD(DetermineGameOverStateDiagonalUpRight) {
            // test basic bottom left position
            Board b = Board();
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 4; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                b = b.addPiece(6);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(6);
            Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");

            b = b.addPiece(3);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test middle position
            b = Board();
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(6);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            b = b.addPiece(6);
            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test with gap
            b = Board();
            b = b.addPiece(0);
            b = b.addPiece(1);
            for (int i = 0; i < 4; ++i) {
                for (int j = 2; j < 4; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                b = b.addPiece(6);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(1);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test top right position
            b = Board();
            for (int i = 0; i < 5; ++i) {
                for (int j = 3; j < BOARD_WIDTH; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i > 1) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            b = b.addPiece(0);
            b = b.addPiece(MAX_COLUMN);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test red wins
            b = Board();
            b = b.addPiece(6);
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(6);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            b = b.addPiece(6);
            b = b.addPiece(4);
            Assert::AreEqual(gameOverState::blackLost, getGameOver(b));
        }

        TEST_METHOD(DetermineGameOverStateDiagonalUpLeft) {
            // test basic bottom right position
            Board b = Board();
            for (int i = 0; i < 3; ++i) {
                for (int j = MAX_COLUMN; j > 2; --j) {
                    b = b.addPiece(j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                b = b.addPiece(0);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");

            b = b.addPiece(3);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test middle position
            b = Board();
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            b = b.addPiece(0);
            b = b.addPiece(2);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test with gap
            b = Board();
            b = b.addPiece(6);
            b = b.addPiece(5);
            for (int i = 0; i < 4; ++i) {
                for (int j = 2; j < 4; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                b = b.addPiece(0);
                Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
            }

            b = b.addPiece(5);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test top left position
            b = Board();
            for (int i = 0; i < 5; ++i) {
                for (int j = 3; j < BOARD_WIDTH; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i > 1) {
                    b = b.addPiece(6);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            b = b.addPiece(6);
            b = b.addPiece(0);
            Assert::AreEqual(gameOverState::blackWon, getGameOver(b));

            // test red wins
            b = Board();
            b = b.addPiece(0);
            for (int i = 0; i < 4; ++i) {
                for (int j = 1; j < 5; ++j) {
                    b = b.addPiece(MAX_COLUMN - j);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }

                if (i > 0) {
                    b = b.addPiece(0);
                    Assert::AreEqual(gameOverState::unfinished, getGameOver(b), L"Game over set before it should have");
                }
            }

            b = b.addPiece(0);
            b = b.addPiece(2);
            Assert::AreEqual(gameOverState::blackLost, getGameOver(b));
        }
    };
}