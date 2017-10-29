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
*   If you make the change to bitfield.h suggested below, change `Board::SymmetryBitField` to use the bitfield 
    macros.


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
    -   (Note: don't bother trying to pull out the `threadId` parameter--I've given this thought and there is
        absolutely no point)
*   Consider renaming `SearchTree::status` to `SearchTree::state` as it seems more appropriate.

#### SearchTree.cpp ####

*   ~~Consider moving the `search()` parts of SearchTree.cpp into separate SearchTree.Search.cpp file leaving really
    the only things in SearchTree.cpp to be `SearchTree::run()` and possibly `SearchTree::setMaxMemoryToUse()`~~
*   Add parallelism
    -   Consider writing version of code that doesn't use cpp11-on-multicore's `bitfield`, and instead uses masks
        with `atomic<T>::compare_exchange_strong()`.  This may, in fact, be necessary, as I'm not entirely sure why
        `bitfield` is atomic in the first place.  The best solution, if it turns out it's not atomic, might be to
        edit the class so that it *is* atomic, rather than using complicated masks.
*   Debug issue where `status.movesFinishedAtDepth` and `status.maxMovesAtDepth` aren't being set when the 
    `LowestDepthManager` completes more than one depth of moves.  The bug is either in SearchTree.h:147 
    (`SearchTree::LowestDepthmanager::~LowestDepthManager()`) or SearchTree.cpp:396 (`SearchTree::search()`)
*   Figure out how to store the route taken in order to save/load progress between runs--may or may not be in the DB


Garbage Collector (currently in SearchTree.cpp)
-----------------------------------------------

*   Pull GC out of SearchTree.cpp into its own class, then add `GarbageCollector` as a friend to `SearchTree`
    -   Pull all memory management out of `SearchTree` and into new class
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
*   Theoretically this should remove race condition dining philosophers problem between GC, IO, and 
    `SearchTree::setCheckMemoryAtInterval()`
*   If moving GC into its own thread, `SearchTree::setCheckMemoryAtInterval()` becomes unnecessary anyway


Progress Printer (currently in SearchTree.IO.cpp)
-------------------------------------------------

*   Create `ProgressPrinter` class, add it as a friend to `SearchTree`, and refactor all code out of 
    SearchTree.IO.cpp.
*   Figure out how to hide the console cursor (currently code is in main.cpp, but it doesn't appear to be working)
*   Auto-size console to be large enough to display whole of `printProgress()` output when process starts
*   Move all IO operations out of `#if VERBOSE` sections and add a set of messages/warnings to `SearchTree::status`.
*   Add functionality to `SearchTree::printProgress()` to handle display of GC


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


MoveManager
-----------

*   Add two custom `iterator`s (one for threaded and one for not) that return all the data necessary for evaluating 
    the next move
    -   Look at currently-commented-out `acquireMove()` in SearchTree.cpp for threaded approach
    -   To be used rather than the current for-loop in `SearchTree::search()`
    -   This is an abstraction that makes it easy/clean to find the next move, whether in parallel or in serial


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

*   Add tests for `Board::calcSymmetry()` that tests both `isSymmetric` and `possiblySymmetric`


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
*   Test that bitfields from bitfield.h are in fact thread-safe


Add performance tests
---------------------

*   Test performance of bitfields vs masks
*   Test performance of regular pointers vs smart pointers
*   Test performance of various thread depths when parallelism is implemented
*   Test performance of smart gc vs not vs hybrid
*   Test performance of various max memory usage thresholds


cpp11-on-multicore
==================

bitfield.h
----------

*   Consider adding specialization for `BitFieldMember<T, Offset, 1>` that returns a `bool` rather than a `T`
    -   See specialization of `BitFieldArray<T, Offset, 1, NumItems>::Element` that I already made
*   Determine if `Bitfield` is, in fact, thread-safe as is claimed by the creator of it


Long term
=========

*   Write visualizer of entire computed search tree in 3 dimensions... ha. ha. ha.
*   Write program that allows you to play against the completed search tree telling you just how many moves you have 
    left to live.  Might write this program in C#/WPF since performance is not an issue.
