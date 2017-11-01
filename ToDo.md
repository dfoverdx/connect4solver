connect4solver
==============

General
-------

*   Figure out if there is a way to place restrictions on template parameters in current VC++ version.  (May need to
    scrap this--looks `concept` and `require` keywords are c\++20, and current version is c\++17.)  Ideally, it would
    be similar to C#'s `where T : IMyInterface` so that IntelliSense can do more in templated functions (e.g.
    `SearchTree::search()`, which currently has no idea that `CurMPtr` is always a `MovePtr`; getting a
    `MoveManager` "interface" would be helpful, too).
    -   [How can I simulate interfaces in C++?](https://stackoverflow.com/questions/1216750/how-can-i-simulate-interfaces-in-c)
    -   Generics *might* be the answer?  [Generics and Templates (Visual C++)](https://msdn.microsoft.com/en-us/library/sbh15dya.aspx)


MoveData
--------

*   Fix up MoveData.Bitfield.cpp (hasn't been used since refactoring into `.Mask` and `.Bitfield` files)
*   ~~*Possibly*--but almost certainly don't--consider making the destructors of `BlackMoveData` and `RedMoveData`
    be responsible for decrementing ref counts of referenced moves.  Feels like absolutely terrible coupling, but could
    simplify the GC code.  Since `MoveData` don't know their own depth, it would likely be slow to calculate it from
    the hash, and so having it determine which `moveCache` to remove references from would be ineffiecient compared to
    current method (inside GC).  Also, I haven't put too much thought into this, but it *may* introduce deadlock race
    conditions when locking the cache mutexes.  No, seriously, this is a terrible idea.~~


Board
-----

*   Think some more about whether a black heuristic score of 0 (aside from empty board) is a guaranteed loss/cat's
    game.  I'm pretty sure it is, but still.
*   Ideally make the heuristic not count empty spaces in the middle of a set of four as two separate possible wins.
    i.e. `rbb bbr` should only count as one possible win, but right now it counts as two: `bb b`  and `b bb`.  It's a
    minor improvement, but might make some difference toward short-circuiting.
*   If implementing the change to [bitfield.h](#bitfieldh) suggested below, change `Board::SymmetryBitField` to use the
    bitfield macros.
*   <a name="altSearchArchBoard"></a>If implementing the
    [Alternative Search Architecture](#alternative-search-architecture), add `const char numPieces` to `Board`.  Then
    add

    ```cpp
    // value used when accessing a cache
    // TODO: figure out if it should be -1 or -2.  Currently, I think it's -1, since SearchTree::run() starts at depth 0
    //       with one black piece on the board
    inline const int Board::moveDepth() const { return numPieces - 1; }
    ```

    Note: If adding a byte for numPieces pushes the `Board` into the next compiled size (look at `sizeof(Board)` with
    and without the added field), put `numPieces` in what is currently the `SymmetryBitField` and then rename that
    bitfield.


SearchTree
----------

*   Consider further templatizing `SearchTree::search()` to include a `DEPTH` and `THREADED` template variables.
    -   This would require moving `search()` into a templatized `struct` since C++ doesn't support incomplete
        template declarations of functions. (see [How to unroll a short loop in C++](https://stackoverflow.com/questions/2382137/how-to-unroll-a-short-loop-in-c))

        ```cpp
        template<typename CurMPtr, int DEPTH, bool THREADED>
        struct search {
            static bool run(const uint threadId, const Board &board, double progress, const double progressChunk, CurMPtr &curMovePtr) {
                // ...
                bool finished = search<NextMPtr, DEPTH + 1, THREADED>::run(/*...*/);
                // ...
            }
        }
        ```

    -   For parallelism, this requires a `bool THREADED` template variable so that the added simplicity of
        defining a specialized `search<CurMPtr, THREAD_AT_DEPTH>()` can distinguish between the function that kicks
        off the threads, and the function that searches on each of those threads.
    -   Would need to redeclare `SearchTree::searchLast()` to just be a specialized
        `SearchTree::search<BlackMoveData, LAST_MOVE_DEPTH, THREADED>()` and remove the call from current
        `search()`
    -   (Note: don't bother trying to pull out the `threadId` argument--I've given this more thought than it deserves
        and there is absolutely no point.)
*   Consider renaming `SearchTree::status` to `SearchTree::state` as it seems more appropriate.
*   Consider locking the `moveCache` with a reader/writer lock rather than a basic mutex.  This would allow multiple 
    readers or a single writer to access the cache at a time.  See [How to make a multiple-read/single-write lock from more basic synchronization primitives?](https://stackoverflow.com/questions/27860685/how-to-make-a-multiple-read-single-write-lock-from-more-basic-synchronization-pr).


#### Alternative Search Architecture ####

Rather than using a depth first search as is currently implemented, use a priority queue.  This search method ensures
that all threads are always doing work, while the DFS implementation only starts threads at a certain depth, and those
threads wait for each other to finish.  It also handles *some* of the deletion of unreferenced moves, reducing the
frequency of GC (i.e. it deletes a move if a thread starts working on a move that is unreferenced; it does not delete
moves automatically when a parent move is set to `BLACK_LOST`, which is where the GC comes in).

Requires reworking or abandoining `MoveManager`.

Need to add `condition_variable` that is notified when all the depth 0 moves are done and queue is empty.  Have 
`run()` wait on that cv.  Also add cv for moveQueue that threads will wait on if they find the queue empty.  Call 
`.notify_one()` whenever pushing to the queue.

This implementation works best if `MAX_THREADS` is equal to the number of processor cores.  See
[Programmatically find the number of cores on a machine](https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine).

```cpp
// Constants.h
// ...
#include <thread>
// ...

// change from constexpr to const
const int NUM_CORES = std::thread::hardware_concurrency(); // may return 0 if function is unavailable
const int MAX_THREADS = NUM_CORES == 0 ? /* some default value... 4? */ : NUM_CORES;
```

If implementing this, be sure to make the change to [Board](#altSearchArchBoard).

```cpp
// BoardMove.h
template <typename CurMPtr>
struct BoardMove {
    const Board board;
    const double progressChunk;
    CurMPtr movePtr;
    bool boardsEnumerated = false;
    BoardHash nextHashes[BOARD_WIDTH];

    BoardMove() : board(Board()), progressChunk(1.0), movePtr(nullptr) {}
    BoardMove(const Board &b, const double progressChunk, const MovePtr &ptr) : 
        board(b), progressChunk(progressChunk), movePtr(ptr) {}

    friend bool operator<(const BoardMove &l, const BoardMove &r) {
        if (l.board.numPieces != r.board.numPieces) {
            return l.board.numPieces < r.board.numPieces;
        }

        if (l.board.numPieces % 2 == 1) {
            // red turn -- higher priority moves have lower heuristics
            return l.board.heuristic > r.board.heuristic;
        }

        // black turn
        return l.board.heuristic < r.board.heuristic;
    }

    friend bool operator<=(const BoardMove &l, const BoardMove &r) { /* ... */ }
    friend bool operator>(const BoardMove &l, const BoardMove &r) { /* ... */ }
    friend bool operator>=(const BoardMove &l, const BoardMove &r) { /* ... */ }

    // I think we need to leave operator== and operator!= undefined.  I don't believe std::priority_queue needs them,
    // and we will never call it.
}
```

```cpp
// SearchTree.h
//...
#include <priority_queue>
//...

class SearchTree {
// ...
private:
    // ...
    static std::priority_queue<BoardMove> moveQueue;
    // ...
    
    // simply loops, popping the moveQueue, determines whether it's a red move or a black move and calls 
    // the proper processMove<CurMPtr>()
    static void doWork();
    
    template<typename CurMPtr>
    static void processMove(BoardMove<CurMPtr> &bm);

    // returns true if move is finished because a next board is a short-circuit condition
    // sets bestMoves to number of moves (or BLACK_LOST) if returning true, else -1
    //
    // otherwise, generates boards and MoveData* and pushes BoardMoves into moveQueue and returns false
    template<typename CurMPtr>
    inline static bool enumerateBoardMoves(BoardMove<CurMPtr> &bm, int &bestMoves);

public:
    // ...
    static struct CacheAndLocks {
        // ...
    private:
        // ...
        std::mutex moveQueueLock;
    } cal;
    // ...
}
```

```cpp
// SearchTree.cpp
// ...
#include <vector>
// ...

vector<thread> workerThreads(MAX_THREADS);
// ...

priority_queue<BoardMove> SearchTree::moveQueue;
// ...

void run() {
    // generate first board, push a BoardMove into the moveQueue
    // kick off MAX_THREADS threads that run doWork()
    // wait on cv
    // upload tree to DB
}

template<typename CurMPtr>
void processMove(BoardMove<CurMPtr> &bm) {
    if (bm.movePtr->getRefCount() == 0) {
        // erase move from cache
        // delete *mptr;
        return;
    }

    int movesTilWin;
    if (!bm.boardMovesEnumerated, movesTilWin) {
        if (enumerateBoards(bm) {
            // increment boardsEvaluated
           
            if (movesTilWin == BLACK_LOST) {
                {
                    // lGuard(moveCacheLock[bm.board.moveDepth + 1])
                    // decrement references to next move(s) (see MoveManager)
                }

                // lGuard(moveCacheLock[bm.board.moveDepth])
                // pick proper MoveData::setBlackLost()
                // MoveData::setBlackLost(&bm.movePtr);
            }

            // lGuard(moveCacheLock[bm.board.moveDepth])
            bm.movePtr->finish(movesTilWin);
        }
        else {
            bm.boardMovesEnumerated = true;
            // lGuard(moveQueueLock)
            moveQueue.push(bm);
        }

        return;
    }

    // increment boardsEvaluated
    // search next moves from cache by looking at bm.nextHashes
    // find the best movesTilWin -- may short-circuit, so don't just stop if discovering a move that is 
    // not finished
    // if a next move is not finished and didn't short-circuit
    //    assert that the move is being processed (via thread id)
    //    lGuard(moveQueueLock)
    //    push bm back into moveQueue
    //    return;
    //
    // bm.movePtr->setFinish(movesTilWin) or setBlackLost(&bm.movePtr) as appropriate
}
```


#### SearchTree.cpp ####

*   ~~Consider moving the `search()` parts of SearchTree.cpp into separate SearchTree.Search.cpp file leaving really
    the only things in SearchTree.cpp to be `SearchTree::run()` and possibly `SearchTree::setMaxMemoryToUse()`~~
*   Add parallelism
    -   Consider writing version of code that doesn't use cpp11-on-multicore's `bitfield`, and instead uses masks
        with `atomic<T>::compare_exchange_strong()`.  This may, in fact, be necessary, as I'm not entirely sure why
        `bitfield` is atomic in the first place.  The best solution, if it turns out it's not atomic, might be to
        edit the class so that it *is* atomic, rather than using complicated masks.  Look at 
        [Safe Bitfields in C++](http://preshing.com/20150324/safe-bitfields-in-cpp/) for reference on how to do
        things atomically.
*   Debug issue where `status.movesFinishedAtDepth` and `status.maxMovesAtDepth` aren't being set when the
    `LowestDepthManager` completes more than one depth of moves.  The bug is either in **SearchTree.h**:147
    (`SearchTree::LowestDepthmanager::~LowestDepthManager()`) or **SearchTree.cpp**:396 (`SearchTree::search()`)
*   Figure out how to store the route taken in order to save/load progress between runs--may or may not be in the DB


Garbage Collector (currently in SearchTree.cpp)
-----------------------------------------------

*   Pull GC out of SearchTree.cpp into its own class, then add `GarbageCollector` as a friend to `SearchTree`
    -   Pull all memory management out of `SearchTree` and into new class.
    -   Pull GC struct out of `SearchTree::status` and place as static `struct` in `GarbageCollector`.
*   Probably should delete `USE_SMART_GC` code since, as the majaority of moves get removed from a cache during GC,
    it is slower to maintain the hashset of unreferenced moves and takes more memory than simply iterating through the
    whole cache each time
    -   If not, need to add `MoveManager` as friend to `GarbageCollector`
    -   Consider a hybrid approach that uses elements of smart GC within dumb GC code so that if we have to make
        multiple passes through a certain depth, we've tracked just the nodes that were recently set to 0-references
        on the last pass.  This may or may not actually reduce performance.
*   Theoretically, the GC should be able to run in parallel to the search since it won't delete any referenced moves
    and moves are re-referenced as soon as they're determined to be a move that follows a board state.  As long as
    both the GC and the search lock the depth's cache's mutex when cleaning/enumerating boards, the GC could be
    abstracted to run on its own thread.  This optimization (if it even *is* an optimzation) may very well be far more
    trouble than it is worth--but might be fun.
*   Add state of GC (running, not running, progress?) to ~~`SearchTree::status`~~ `GarbageCollector`
*   Consider changing `SearchTree::cal.moveCache` from type `MoveCache[]` to `MoveCache*[]`
    -   This allows the GC, if enough moves are deleted from a cache, to create a new cache, copy the items from the
        old one into the new one, delete the old one, and set the pointer to the new one.  Since a cache only stores
        pointers to `MoveData`, copying the items into the new cache shouldn't even interrupt processing of
        moves--only the insertion and retrieval of moves to and from the cache itself (which requires a lock anyway).
*   Remove ioLock from GC (SearchTree.cpp `SearchTree::manageMemory()`)
    -   Theoretically this should remove race condition dining philosophers problem between GC, IO, and
        `SearchTree::setCheckMemoryAtInterval()`
    -   If moving GC into its own thread, `SearchTree::setCheckMemoryAtInterval()` becomes unnecessary anyway.
*   If implementing [this feature](#CacheSizeDetail), update `estimatedDataDeleted` algorithm.

<a name="ProgressPrinter"></a>
Progress Printer (currently in SearchTree.IO.cpp)
-------------------------------------------------

*   Create `ProgressPrinter` class, add it as a friend to `SearchTree` and `GarbageCollector`, and refactor all 
    code out of **SearchTree.IO.cpp**.
*   Figure out how to hide the console cursor (currently code is in **main.cpp**, but it doesn't appear to be working)
*   Auto-size console to be large enough to display whole of `printProgress()` output when process starts
*   Move all IO operations out of `#if VERBOSE` sections and add a set of messages/warnings to 
    `SearchTree::status`.
*   Add functionality to `SearchTree::printProgress()` to handle display of GC.
*   When implementing [Connect4SolverGUI](#connect4SolverGui), add use of named pipes ([C++ docs](https://msdn.microsoft.com/en-us/library/windows/desktop/aa365592%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396))
    to corralle and expose progress data.
    -   Increase refresh rate from 2Hz to 60Hz since it no longer needs to print anything to `cout` which is the major
        performance hit.


Add database support: `DatabaseManager` class
-----------------------------------------------

*   DB schema:
    -   `tbl_move`:
        *   `depth` (PK)
        *   `hash` (PK)
        *   `col0_height` - `col6_height`
        *   `col0_pieces` - `col6_peices`
        *   `is_symmetric`
        *   `moves_to_win`
    -   `tbl_move_move`:
        *   `depth` (PK)
        *   `hash` (PK)
        *   `move_col` (PK)
        *   `next_hash` (FK)

        Note: since each red move is going to have exactly one black move, consider somehow storing `black_move_col`
        and `black_move_hash` inside `tbl_move_move` and enforcing that `depth` always be even (for red turn
        depths)
*   Figure out how to determine if a move can possibly follow from a previous board state
    -   Probably consists of figuring out if you can use bit manipulation in T-SQL (looks like you can)
        *   [Comparing two bitmasks in SQL to see if any of the bits match](https://stackoverflow.com/questions/143712/comparing-two-bitmasks-in-sql-to-see-if-any-of-the-bits-match)
        *   [Bit-flipping operations in T-SQL](https://stackoverflow.com/questions/1110155/bit-flipping-operations-in-t-sql)
    -   For each column in root board, apply root column's `~openSpaces` as mask to next board's piece data, then
        compare root board's piece data to next board's piece data
    -   Could also generate or store string represenation in `tbl_move` of each board's columns' piece data
        (`nvarchar(7)` consisting of `r` or `b` and stopping at board height), then search future boards'
        columns' piece data string by `like root_col_piece_str + '%'` -- would be incredibly slow, but may be
        only option if bit manipulation isn't available
*   Figure out how to efficiently blit tens of millions of moves into (and out of) a DB
    -   [Increasing mass import speed to MS SQL 2008 database from client application](https://stackoverflow.com/questions/9801017/increasing-mass-import-speed-to-ms-sql-2008-database-from-client-application)
    -   [Bulk Copy Data from Program Variables (ODBC)](https://docs.microsoft.com/en-us/sql/relational-databases/native-client-odbc-how-to/bulk-copy/bulk-copy-data-from-program-variables-odbc)
*   Process:
    1.  When cache is full (see `SearchTree::status.cacheFullAt`), run GC to remove *all* unreferenced moves
    2.  Blit *finshed* moves from cache/tree to the DB
    3.  (optional) Delete moves from DB that are unreferenced by other moves
    4.  Delete all moves from cache
    5.  Regenerate the moves from depth `0` to depth `cacheFullAt` and add them to cache
    6.  Load all moves from DB such that the move from the DB *could* follow from the board at depth `cacheFullAt`
        *   Probably need to stop after x-million moves (or, depending on how many moves are BLACK_LOST, x-amount of
            memory) have been loaded


NextMoveDescriptor
------------------

*   Detemplatize.
*   Change `move` type to `MovePtr*`.
*   Update `MoveManager` (and subtypes) and `SearchTree` functions to accomodate change using 
    `dynamic_pointer_cast<NextMPtr*>` and `dynamic_pointer_cast<NextMPtr>` when using smart pointers or simple 
    `(NextMPtr*)` and `(NextMPtr)` when not.  (Add macros to **SearchTree.Macros.h**.)


MoveManager
-----------

*   Make `MoveManager`'s methods IntelliSense-accessible in `SearchTree::search()`.  Here are some ideas:
    -   Create a base class `MoveManagerBase` with public `virtual` method(s) used in `MoveManager<CurMPtr>`.
        *   Define `operator[]`'s return type as `NextMoveDescriptor`.
        *   Update `SearchTree::enumerateBoards()`
            -   Remove `MM` template parameter, and replace with 
                ```cpp
                typedef conditional<is_same<...>, BlackMoveManager, RedMoveManager>::type MM;
                ```
            -   Update `MM &mManager` argument to `MoveManagerBase &mManager`
            -   Cast `mManager` as `(MM)mManager` when necessary
        *   Update `SearchTree::search()`
            -   After initializing `mManager`, create a second variable `MoveManagerBase &mmb = (MoveManagerBase)mManager`.
        *   Unless we implement using smart pointers for the `MoveManager`s, should not need `dynamic_cast` or 
            `dynamic_pointer_cast`.
    -   Create a wrapper template struct with a function that returns `NextMoveDescriptor` from 
        `MoveManager.moves` so that IntelliSense at least knows it's getting a `NextMoveDescriptor`.
*   Add two custom `iterator`s (one for threaded and one for not) that return all the data necessary for evaluating
    the next move.
    -   Look at currently-commented-out `acquireMove()` in SearchTree.cpp for threaded approach
    -   To be used rather than the current for-loop in `SearchTree::search()`
    -   This is an abstraction that makes it easy/clean to find the next move, whether in parallel or in serial
*   <a name="CacheSizeDetail"></a>Possibly keep track of number of `RedMovePtr`s and `BlackMovePtr`s vs the number of `MovePtr`s (`BLACK_LOST`
    moves) at each level.
    -   Interesting stat to display in [Connect4SolverGUI](#connect4SolverGui).
    -   Gives better estimate for which cache the GC should start with.


connect4solverTests
===================

*   Figure out how to abstract tests in order to generate same tests for various combinations of CompilerFlags without
    recompiling
*   Add new test classes as friend classes using the `unitTestClass` and `unitTestFriend` macros
*   Add test class stubs
    -   DatabaseManager stub
    -   Others maybe?


BoardTest
---------

*   Add tests for `Board::calcSymmetry()` that tests both `isSymmetric` and `possiblySymmetric`.


Add unit test classes
---------------------

*   MoveManagerTest
*   MoveDataTest
    -   MoveData
    -   RedMoveData
    -   BlackMoveData
    -   Tests for black lost MoveData
*   SearchTreeTest
    -   Would be easiest to start a test from a certain point, x-moves in, and call `SearchTree::search()` from that
        depth/board.
    -   Absolutely needs to test GC and that pointers get cleaned up--though I'm not really sure how to do that...
    -   Test save and load between runs
    -   Attempt to write parallelism tests...?
*   DatabaseManagerTest
    -   Test saving and loading to/from the db
*   Test that bitfields from **bitfield.h** are in fact thread-safe
*   ([optional](#alternative-search-architecture)) BoardMoveTest


Add performance tests
---------------------

*   Test performance of bitfields vs masks
*   Test performance of regular pointers vs smart pointers
*   Test performance of various thread depths when parallelism is implemented
*   Test performance of smart gc vs not vs hybrid
*   Test performance of various max memory usage thresholds
*   Test performance of locking moveCache with simple mutext vs reader/writer lock


cpp11-on-multicore
==================

bitfield.h
----------

*   Consider adding specialization for `BitFieldMember<T, Offset, 1>` that returns a `bool` rather than a `T`
    -   See specialization of `BitFieldArray<T, Offset, 1, NumItems>::Element` that I already made
*   Determine if `Bitfield` is, in fact, thread-safe as is claimed by the creator of it


<a name="connect4SolverGui"></a>
Connect4SolverGUI (needs a better name)
=======================================

A new WPF C# project that replaces the [`ProgressPrinter`](#ProgressPrinter) in Connect4Solver.

Add an optional argument to connect4solver.exe `main()` that enables or disables use of named pipes ([C# docs](https://docs.microsoft.com/en-us/dotnet/standard/io/how-to-use-named-pipes-for-network-interprocess-communication))
in the `ProgressPrinter`.

Connect4SolverGui kicks off the connect4solver.exe *process* (rather than running the C++ code itself) and uses named 
pipes to share progress data between the two processes.

Features
--------

*   Display stats already exposed by `ProgressPrinter`.
*   Logarithmic graph of time remaining.
*   Progress bars for depths?
*   Memory usage.
*   Sizes of each depth's cache.


Long term
=========

*   Write visualizer of entire computed search tree in 3 dimensions... ha. ha. ha.
*   Write program that allows you to play against the completed search tree telling you just how many moves you have
    left to live.  Might write this program in C#/WPF since performance is not an issue.
