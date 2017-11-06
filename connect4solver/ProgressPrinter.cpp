#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <Windows.h>
#include "GarbageCollector.h"
#include "IoMessage.h"
#include "Macros.h"
#include "ProgressPrinter.h"
#include "SearchTree.h"
#include "TypeDefs.h"

using namespace std;
using namespace connect4solver;
using namespace connect4solver::gc;

void printMoveRoute(const SearchTree::SearchTreeState &stState, array<int, BOARD_SIZE> &route);
void printProgress();

#define white setColor(0xF)
#define clearStream() ss.clear(); ss.str("")
#define wlc(n, val, c) \
    clearStream(); \
    ss << val << ends; \
    cout << setw(39) << left << n << setColor(c) << right << setw(25) << ss.str().data() << resetColor << endl

#define wl(n, val) wlc(n, val, 0xF)

HANDLE hConsoleOutput;
COORD dwCursorPosition{ 0,0 };
CONSOLE_SCREEN_BUFFER_INFO csbi;
double lastProgressPerDay = 0;
thread printProgressThread;

void progressPrinter::beginPrintProgress() {
    printProgressThread = thread(printProgress);
}

void printProgress() {
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    auto &stState = SearchTree::getSearchTreeState();
    auto &stats = stState.stats;

    cout << setColor(0xF) << "Starting..." << endl << endl << resetColor;

    this_thread::sleep_for(.5s);

    clearScreen();

    bool gcRunning = false;
    while (true) {
        auto gcState = gc::getGCState();
        auto &pmc = gcState.pmc;
        auto &maxMemoryToUse = gcState.maxMemoryToUse;

        if (gcRunning ^ (gcState.runState == GCRunState::running)) {
            clearScreen();
        }

        gcRunning = gcState.runState == GCRunState::running;

        ostringstream ss;

        SetConsoleCursorPosition(hConsoleOutput, dwCursorPosition);
        GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);

        wl("Current process size:", (pmc.PagefileUsage >> 20) << " MB / " << (maxMemoryToUse >> 20) << " MB");
        double progress = 0.0;

#if ET
        for (int i = 0; i <= MAX_THREADS; ++i) {
            progress += stState.threadProgress[i];
        }
#else // ET
        progress = stState.totalProgress;
#endif // ET

        // stats
        const double hitPercent = 100.0 * stats.cacheHits / (stats.cacheHits + stats.cacheMisses);
        const ull totalSymmetryBoards = stats.asymmetricBoards + stats.possiblySymmetricBoards + stats.symmetricBoards;
        const double symmetricBoardPercent = 100.0 * stats.symmetricBoards / totalSymmetryBoards;
        const double possiblySymmetricBoardPercent = 100.0 * stats.possiblySymmetricBoards / totalSymmetryBoards;
        const ull cacheSize = stats.cacheMisses - gcState.totalGarbageCollected;

        wl("Progress:                    ", fixed << setprecision(12) << 100.0 * progress << '%');
        wl("Lowest Move Depth evaluated: ", stState.lowestDepthEvaluated);
        wl("Progress saved:              ", fixed << setprecision(12) << 100 * stState.progressSaved << '%');
        cout << endl;

        cout << setprecision(3);
        wl("Moves evaluated:   ", stState.movesEvaluated);
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
        const ull ms = now - stState.begin_time;
        const ull rms = static_cast<ull>(ms / progress);
        const double progressPerDay = progress * 100 * 24 * 60 * 60 * 1000 / ms;
        const double deltaPPD = progressPerDay - lastProgressPerDay;
        lastProgressPerDay = progressPerDay;
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

        cout << white
            << "Depths finished" << endl
            << "---------------" << endl << resetColor;

        clearStream();
        constexpr int totalWidth = 8;
        const int consoleWidth = csbi.srWindow.Right - csbi.srWindow.Left;
        const int depthsToDisplay = consoleWidth / totalWidth;

        int depthsUsed = 0;

        for (int i = 0; i < DEPTH_COUNTS_TO_TRACK && stState.maxMovesAtDepth[i] != -1; ++i) {
            ++depthsUsed;
        }

        for (int i = max(0, depthsUsed - depthsToDisplay); i < DEPTH_COUNTS_TO_TRACK && stState.maxMovesAtDepth[i] != -1; ++i) {
            cout << setw(totalWidth - 2) << i;
            ss << setw(2) << stState.movesFinishedAtDepth[i] << " / " << stState.maxMovesAtDepth[i];

            if (i < DEPTH_COUNTS_TO_TRACK - 1 && stState.maxMovesAtDepth[i + 1] != -1) {
                cout << " |";
                ss << " |";
            }
        }

        ss << ends;
        cout << endl << white << ss.str().data() << endl << endl << resetColor;

        // GC stats
        const double ppgc = 100.0 * progress / gcState.nextGCRunCount;
        ull currentGCTime = 0;
        if (gcState.runState == GCRunState::running) {
            currentGCTime = now - gcState.gcStartedAt;
        }

        const double timeSpentGCPercent = (gcState.timeSpentGCing + currentGCTime) * 100.0 / ms;

        cout << white
            << "Garbage Collector" << endl
            << "-----------------" << endl << endl << resetColor;

        if (gcRunning) {
            wlc("GC State:", "Running", 0xA);
        }
        else {
            wl("GC State:", "Stopped");
        }

        wl("GCs run:                               ", gcState.nextGCRunCount - 1);
        wl("Total garbage collected:               ", gcState.totalGarbageCollected + (gcRunning ? gcState.lastGarbageCollected : 0));
        wl("Progress per GC:                       ", fixed << setprecision(10) << ppgc << '%');
        wl("Total time spent collecting garbage:   ", printTime(gcState.timeSpentGCing));
        wlc("Percent time spent collecting garbage:", fixed << setprecision(2) << timeSpentGCPercent << '%', 0xB);
        wlc("Lowest Move Depth evaluated since GC: ", stState.lowestDepthEvaluatedSinceLastGC, 0xE);
        cout << endl;

        if (gcState.nextGCRunCount > 1 || gcRunning) {
            if (gcRunning) {
                cout
                    << "Current GC" << endl
                    << "----------" << endl << endl;
            }
            else {
                cout
                    << "Last GC  " << endl
                    << "-------  " << endl << endl;
            }

            wl("Time elapsed:       ", printTime(gcState.lastGCTime + currentGCTime));
            wl("Garbage collected:  ", gcState.lastGarbageCollected);
            wl("% moves removed:    ", fixed << setprecision(5) << gcState.lastPercentMovesDeleted << '%');
            wl("Memory freed:       ", (gcState.lastMemoryFreed >> 20) << " MB / " << (gcState.minMemoryToClear >> 20) << " MB");

            if (gcRunning) {
                wl("Cache depth scoured:", gcState.lastCacheDepthScoured << " -> " << gcState.currentGCDepth);
            }
            else {
                wl("Cache depth scoured:", gcState.lastCacheDepthScoured);
            }

#if VERBOSE
            cout << endl
                << "GC Log" << endl
                << "------" << endl << resetColor;

            for (auto msg : *gcState.messages) {
                cout << setColor(msg.color) << msg.msg;
            }

#endif // VERBOSE
            cout << resetColor << endl << endl;
        }
#if VERBOSE
        static_fail("TODO");
#if ET
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
        printMoveRoute(stState.currentMoveRoute);
#endif // ET
#endif // VERBOSE

        this_thread::sleep_for(PRINT_PROGRESS_INTERVAL);
    }
}


void printMoveRoute(const SearchTree::SearchTreeState &stState, array<int, BOARD_SIZE> &route) {
    for (int i = 0; i < BOARD_SIZE; ++i) {
        if (i == stState.lowestDepthEvaluatedSinceLastGC) {
            cout << setColor(11);
        }
        else if (i == stState.lowestDepthEvaluated) {
            cout << setColor(14);
        }

        if (route[i] == -1) {
            break;
        }

        cout << setfill(' ') << setw(2) << static_cast<int>(route[i]);
    }

    cout << endl << resetColor;
}