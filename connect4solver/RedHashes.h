#pragma once
#include <unordered_map>
#include "Typedefs.h"

namespace connect4solver {
    class RedHashes
    {
        friend class RedMoveData;

    private:
        std::unordered_map<int, BoardHash> m_hashes;

        // used when RedMove is finished
        RedHashes(uchar usedIndices, const BoardHash* hashes);

        // used when RedMove is not finished
        RedHashes(uchar usedIndices, const BoardHash* hashes, const int size);

    public:
        typedef std::unordered_map<int, BoardHash>::const_iterator RedHashes::iterator;

        const BoardHash getMoveHash(int x) const;
        const RedHashes::iterator begin() const;
        const RedHashes::iterator end() const;
    };

}