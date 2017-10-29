#pragma once
#include <bitfield.h>
#include "Constants.h"
#include "Constants.MoveData.h"
#include "Piece.h"

#if USE_SMART_POINTERS
#include <memory>
#endif

#if ENABLE_THREADING
#include <atomic>
#endif

namespace connect4solver {

#if PACK_MOVE_DATA
#pragma pack(push, 1)
#endif

    class MoveData
    {
    private:
        MoveData(const MoveData &md);
        MoveData(const MoveDataBase data);
        static const MoveData createBlackLost(MoveDataInternal data);

#if !BF
        // Helper functions
        inline static const bool maskEquals(const MoveDataBase data, const MoveDataBase mask);
        inline const bool maskEquals(const MoveDataBase mask) const;
#endif // !BF

    protected:
        /*
            bitmask:
                0-5:    # moves to win
                6:      is symmetric flag
                7:      is finished flag
                8-10:   # of references to this move
                11-14:  worker thread id
                15:  unused
        */
        MoveDataInternal m_data;

    public:
        // used when initializing array
        MoveData();
        MoveData(const bool isSymmetric);

        // sets isFinished = true, threadId = THREAD_ID_MASK (no thread), and movesToWin
        void finish(const int movesTilWin);

        const ushort incRefCount();
        const ushort decRefCount();

        const int getMovesToWin() const;
        const int getRefCount() const;
        const bool getIsFinished() const;
        const bool getIsSymmetric() const;
        const bool getBlackLost() const;

        static void setBlackLost(MovePtr* mptr);

#if ENABLE_THREADING
        // returns true if the thread successfully acquired access to the move data,
        // else false (either the move has already been finished or another thread got to it first
        const bool acquire(const int threadId);

        // sets the threadId back to default value without setting the finished bit
        // used when another thread's resultant move makes this move unnecessary
        void releaseWithoutFinish();

        const int getWorkerThread() const;
#endif

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
#if USE_DYNAMIC_SIZE_RED_NEXT
        NextHashArrayPtr m_next;
#else
        BoardHash m_next[BOARD_WIDTH];
#endif

    public:
        RedMoveData();
        RedMoveData(const bool isSymmetric);

#if !USE_SMART_POINTERS & USE_DYNAMIC_SIZE_RED_NEXT
        ~RedMoveData();
#endif

        static void setBlackLost(RedMovePtr* ptr);

        void setNextHash(const int x, const BoardHash hash);
        const BoardHash* getNextHashes(int &size) const;
    };

#if PACK_MOVE_DATA
#pragma pack(pop)
#endif

}