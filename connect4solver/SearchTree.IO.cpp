#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include "SearchTree.h"

using namespace std;
using namespace connect4solver;

#define white setColor(0xF)
#define wlc(n, val, c) \
    ss = ostringstream(); \
    ss << val; \
    cout << setw(39) << left << n << setColor(c) << right << setw(25) << ss.str() << resetColor << endl

#define wl(n, val) wlc(n, val, 15)

HANDLE hConsoleOutput;
COORD dwCursorPosition{ 0,0 };
CONSOLE_SCREEN_BUFFER_INFO csbi;

void SearchTree::printStatus()
{
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    auto &stats = status.stats;
    auto &gc = status.gc;
    auto &pmc = status.pmc;
    auto &maxMemoryToUse = status.maxMemoryToUse;

    {
        lGuard(cal.ioMutex);
        cout << setColor(0xF) << "Starting..." << endl << endl << resetColor;
    }

    this_thread::sleep_for(.5s);

    clearScreen();

    while (true) {
        {
            lGuard(cal.ioMutex);
            ostringstream ss;

            SetConsoleCursorPosition(hConsoleOutput, dwCursorPosition);
            GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);

            while (cal.manageMemoryLock.test_and_set());
            wl("Current process size:", (pmc.PagefileUsage >> 20) << " MB / " << (maxMemoryToUse >> 20) << " MB");
            cal.manageMemoryLock.clear();

            double progress = 0.0;

#if ET
            for (int i = 0; i <= MAX_THREADS; ++i) {
                progress += status.threadProgress[i];
            }
#else // ET
            progress = status.totalProgress;
#endif // ET

            // stats
            const double hitPercent = 100.0 * stats.cacheHits / (stats.cacheHits + stats.cacheMisses);
            const ull totalSymmetryBoards = stats.asymmetricBoards + stats.possiblySymmetricBoards + stats.symmetricBoards;
            const double symmetricBoardPercent = 100.0 * stats.symmetricBoards / totalSymmetryBoards;
            const double possiblySymmetricBoardPercent = 100.0 * stats.possiblySymmetricBoards / totalSymmetryBoards;
            const ull cacheSize = stats.cacheMisses - gc.totalGarbageCollected;

            wl("Progress:                    ", fixed << setprecision(12) << 100.0 * progress << '%');
            wl("Lowest Move Depth evaluated: ", status.lowestDepthEvaluated);
            wl("Progress saved:              ", fixed << setprecision(12) << 100 * status.progressSaved << '%');
            cout << endl;

            cout << setprecision(3);
            wl("Moves evaluated:   ", status.movesEvaluated);
            wlc("Cache hits:       ", stats.cacheHits, 0xA);
            wlc("Cache misses:     ", stats.cacheMisses, 0xC);
            wl("Cache size:        ", cacheSize);
            wl("Cache hit:         ", fixed << setprecision(3) << hitPercent << '%');
            cout << endl;

            wl("Symmetric boards:                   ", stats.symmetricBoards);
            wl("Possibly symmetric boards:          ", stats.possiblySymmetricBoards);
            wl("Asymmetric boards:                  ", stats.asymmetricBoards);
            wl("Symmetric board %:                  ", fixed << setprecision(5) << symmetricBoardPercent << '%');
            wl("Possibly symmetric board %:         ", fixed << setprecision(5) << possiblySymmetricBoardPercent << '%');
            cout << endl;

            // time
            const clock_t now = clock();
            const ull ms = now - status.begin_time;
            const ull rms = static_cast<ull>(ms / progress);
            const double progressPerDay = progress * 100 * 24 * 60 * 60 * 1000 / ms;
            const double deltaPPD = progressPerDay - status.lastProgressPerDay;
            status.lastProgressPerDay = progressPerDay;
            WORD ppdColor = 7;
            if (deltaPPD > 0) {
                ppdColor = 0xA;
            }
            else if (deltaPPD < 0) { // in case progressPerDay == lastProgressPerDay, output should be gray
                ppdColor = 0xE;
            }

            cout << setprecision(10);
            wl("Time elapsed:             ", printTime(ms));
            wl("Time remaining:           ", printTime(rms));
            wl("Progress per day:         ", fixed << setprecision(10) << progressPerDay << '%');
            wlc("Delta progress per day:  ", fixed << setprecision(10) << deltaPPD << '%', ppdColor);
            cout << endl;

            // GC stats
            const double ppgc = 100.0 * progress / gc.nextGCCount;
            const double timeSpentGCPercent = gc.timeSpentGCing * 100.0 / ms;

            cout << white
                << "Garbage Collector" << endl
                << "-----------------" << endl << endl << resetColor;

            wl("GCs run:                               ", gc.nextGCCount - 1);
            wl("Total garbage collected:               ", gc.totalGarbageCollected);
            wl("Progress per GC:                       ", fixed << setprecision(10) << ppgc << '%');
            wl("Total time spent collecting garbage:   ", printTime(gc.timeSpentGCing));
            wlc("Percent time spent collecting garbage:", fixed << setprecision(2) << timeSpentGCPercent << '%', 0xB);
            wlc("Lowest Move Depth evaluated since GC: ", status.lowestDepthEvaluatedSinceLastGC, 0xE);
            cout << endl;

            if (gc.nextGCCount > 1) {
                cout
                    << "Last GC" << endl
                    << "-------" << endl << endl;

                wl("Time elapsed:       ", printTime(gc.lastGCTime));
                wl("Garbage collected:  ", gc.lastGarbageCollected);
                wl("% moves removed:    ", fixed << setprecision(5) << gc.lastPercentMovesDeleted << '%');
                wl("Memory freed:       ", (gc.lastMemoryFreed >> 20) << " MB");
                wl("Cache depth scoured:", gc.lastCacheDepthScoured);
                cout << endl;
            }

            cout << white
                << "Depths finished" << endl
                << "---------------" << endl << resetColor;

            ss = ostringstream();
            constexpr int totalWidth = 8;
            const int consoleWidth = csbi.srWindow.Right - csbi.srWindow.Left;
            const int depthsToDisplay = consoleWidth / totalWidth;

            int depthsUsed = 0;

            for (int i = 0; i < DEPTH_COUNTS_TO_TRACK && status.maxMovesAtDepth[i] != -1; ++i) {
                ++depthsUsed;
            }

            for (int i = max(0, depthsUsed - depthsToDisplay); i < DEPTH_COUNTS_TO_TRACK && status.maxMovesAtDepth[i] != -1; ++i) {
                cout << setw(totalWidth - 2) << i;
                ss << setw(2) << status.movesFinishedAtDepth[i] << " / " << status.maxMovesAtDepth[i];

                if (i < DEPTH_COUNTS_TO_TRACK - 1 && status.maxMovesAtDepth[i + 1] != -1) {
                    cout << " |";
                    ss << " |";
                }
            }

            cout << endl << white << ss.str() << endl << endl << resetColor;
#if VERBOSE
#if ET
            static_fail("TODO");
            if (threadsAvailable.all()) {
                cout << endlrc << "Move route (main thread): " << endl;
                printMoveRoute(allCurrentMoveRoutes[])
            }
            cout << endlrc << "Move routes:" << endl;
            for (int i = 0; i < MAX_THREADS; ++i) {
                array<int, BOARD_SIZE> &route = allCurrentMoveRoutes[i];
            }
#else // ET
            cout << endlrc << "Move route:" << endl;
            printMoveRoute(status.currentMoveRoute);
#endif // ET
#endif // VERBOSE
        }

        this_thread::sleep_for(PRINT_PROGRESS_INTERVAL);
    }
}

void SearchTree::printMoveRoute(array<int, BOARD_SIZE> &route) {
    for (int i = 0; i < BOARD_SIZE; ++i) {
        if (i == status.lowestDepthEvaluatedSinceLastGC) {
            cout << setColor(11);
        }
        else if (i == status.lowestDepthEvaluated) {
            cout << setColor(14);
        }

        if (route[i] == -1) {
            break;
        }

        cout << setfill(' ') << setw(2) << static_cast<int>(route[i]);
    }

    cout << endl << resetColor;
}