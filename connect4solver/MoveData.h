#pragma once
#include <atomic>
#include <memory>
#include "Constants.h"
#include "Piece.h"
#include "RedHashes.h"

namespace connect4solver {

    class MoveData;
    typedef std::shared_ptr<MoveData> MovePtr;

    class BlackMoveData;
    typedef std::shared_ptr<BlackMoveData> BlackMovePtr;

    class RedMoveData;
    typedef std::shared_ptr<RedMoveData> RedMovePtr;
    typedef std::unique_ptr<BoardHash[]> NextHashArrayPtr;

#pragma pack(push, 1)

    class MoveData
    {
    protected:
        /*
            bitmask:
                0-6:   # moves to win
                7-11:  # of references to this move
                12-14: worker thread id
                15:    is finished
                16:    is symmetric
        */
        std::atomic<ushort> m_data;

    public:
        // used when initializing array
        MoveData();
        MoveData(const bool isSymmetric);

        // returns true if the thread successfully acquired access to the move data,
        // else false (either the move has already been finished or another thread got to it first
        const bool acquire(const int threadId);

        // sets the threadId back to default value without setting the finished bit
        // used when another thread's resultant move makes this move unnecessary
        void releaseWithoutFinish();

        //void setIsFinished(const bool isFinished);

        // sets isFinished = true, threadId = THREAD_ID_MASK (no thread), and movesToWin
        void finish(const int movesTilWin);

        const ushort incRefCount();
        const ushort decRefCount();

        const int getMovesToWin() const;
        const int getRefCount() const;
        const bool getIsFinished() const;
        const bool getIsSymmetric() const;
        const int getWorkerThread() const;
    };

    class BlackMoveData : public MoveData {
    private:
        BoardHash m_next;

    public:
        BlackMoveData();
        BlackMoveData(const bool isSymmetric);

        void setNextHash(const BoardHash hash);
        const BoardHash getNextHash() const;
    };

    class RedMoveData : public MoveData {
    private:
        uchar m_hashesUsed;
        NextHashArrayPtr m_next;

    public:
        RedMoveData();
        RedMoveData(const bool isSymmetric);

        void setNextHash(const uint x, const BoardHash hash);
        const RedHashes getNextHashes() const;

        // minor memory optimization, saving only the hashes used after the move has been finished
        // called during garbage collection
        // returns the number of empty hashes removed
        const int condense();
    };

#pragma pack(pop)

}