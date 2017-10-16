#include <iostream>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <stack>
#include <queue>
#include "Board.h"
#include "piece.h"
#include "searchTree.h"

using namespace std;
using namespace connect4solver;

//void printNode(node_ptr node, bool doMirror);
//tinyint getInput();
//void cleanCache(NodeCache* cache);
//
//const char LEFT_ARROW = (char)75;
//const char BACKSPACE = '\b';
//const char ESCAPE = (char)27;

int main() {
    SearchTree::run();
    return 0;
    //
    //NodeCache nodeCache = NodeCache();
    //typedef pair<tinyint, node_ptr> move_pair;
    //tinyint lastMove = -1;
    //node_ptr root = Node::Empty();
    //node_ptr curNode = root;
    //stack<move_pair> moveStack;
    //moveStack.push(move_pair(-1, curNode));
    //bool doMirror = false;
    //printNode(curNode, doMirror);

    //do {
    //    while (true) {
    //        lastMove = getInput();

    //        if (lastMove == LEFT_ARROW) {
    //            if (moveStack.top().second != root) {
    //                moveStack.pop();
    //                curNode = moveStack.top().second;
    //                break;
    //            }
    //            else {
    //                continue;
    //            }
    //        }
    //        else if (lastMove == BACKSPACE) {
    //            if (moveStack.top().second != root) {
    //                nodeCache.erase(curNode->getNodeHash());
    //                tinyint move = moveStack.top().first;
    //                moveStack.pop();
    //                curNode = moveStack.top().second;
    //                curNode->removeNextNode(move);
    //                break;
    //            }
    //            else {
    //                continue;
    //            }
    //        }
    //        else if (lastMove == ESCAPE) {
    //            cleanCache(&nodeCache);
    //            break;
    //        }
    //        else if (curNode->isLegalMove(lastMove)) {
    //            if (curNode->getIsSymmetric()) {
    //                doMirror = lastMove > 3;
    //            }

    //            if (doMirror) {
    //                lastMove = MAX_COLUMN - lastMove;
    //            }

    //            //curNode = curNode->makeMove(lastMove, &nodeCache, cache);
    //            moveStack.push(move_pair(lastMove, curNode));
    //            break;
    //        }
    //    }

    //    printNode(curNode, doMirror);

    //    cout << "Cache size: " << nodeCache.size() << '\n';
    //    for (NodeCache::iterator it = nodeCache.begin(); it != nodeCache.end(); ++it) {
    //        cout << it->first << ": " << it->second.use_count() << '\n';
    //    }

    //    cout << '\n';
    //} while (curNode->getMovesTilWin() == 0 || curNode->getMovesTilWin() == BLACK_LOST);

    //return 0;
}

//
//tinyint getInput() {
//    while (true) {
//        char input = _getch();
//        //cout << (unsigned int)input << " ";
//
//        if (input >= '1' && input <= '7') {
//            return input - '1';
//        }
//
//        switch (input) {
//        case LEFT_ARROW:
//        case BACKSPACE:
//        case ESCAPE:
//            return input;
//        }
//    };
//}
//
//void printNode(node_ptr node, bool doMirror) {
//    cout << "Node hash: " << node->getNodeHash() << '\n';
//    cout << "Node depth: " << (int)(node->getNodeDepth()) << '\n';
//    cout << "Node is symmetric: " << boolalpha << node->getIsSymmetric() << '\n';
//    cout << "Currently mirrored: " << boolalpha << doMirror << '\n';
//    cout << '\n';
//
//    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
//    for (int iy = BOARD_HEIGHT - 1; iy >= 0; iy--) {
//        for (int ix = 0; ix < BOARD_WIDTH; ix++) {
//            switch (node->getPiece(ix, iy, doMirror)) {
//            case RED:
//                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
//                cout << 'r';
//                break;
//
//            case BLACK:
//                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 8);
//                cout << 'b';
//                break;
//
//            case EMPTY:
//                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
//                cout << '_';
//                break;
//
//            default:
//                throw exception("invalid piece");
//            }
//
//            cout << ' ';
//        }
//
//        cout << '\n';
//    }
//
//    cout << '\n';
//    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
//}