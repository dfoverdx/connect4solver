#include <cassert>
#include "PopCount.h"
#include "RedHashes.h"

using namespace std;
using namespace connect4solver;

RedHashes::RedHashes(uchar usedIndices, const BoardHash* hashes) :
    m_hashes(unordered_map<int, BoardHash>(static_cast<size_t>(popCountByte(usedIndices)))) {
    int hashIdx = 0;
    for (int i = 0; usedIndices > 0; ++i, usedIndices >>= 1) {
        if (usedIndices & 1) {
            assert(hashes[hashIdx] != 0);
            m_hashes.insert({ i, hashes[hashIdx] });
            ++hashIdx;
        }
    }
}

RedHashes::RedHashes(uchar usedIndices, const BoardHash* hashes, const int size) :
    m_hashes(unordered_map<int, BoardHash>(static_cast<size_t>(popCountByte(usedIndices)))) {
    for (int i = 0; i < size; ++i, usedIndices >>= 1) {
        if (usedIndices & 1) {
            assert(hashes[i] != 0);
            m_hashes.insert({ i, hashes[i] });
        }
    }
}

const BoardHash RedHashes::getMoveHash(int x) const {
    return m_hashes.at(x);
}

const RedHashes::iterator RedHashes::begin() const {
    return m_hashes.cbegin();
}

const RedHashes::iterator connect4solver::RedHashes::end() const
{
    return m_hashes.cbegin();
}
