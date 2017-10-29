//---------------------------------------------------------
// For conditions of distribution and use, see
// https://github.com/preshing/cpp11-on-multicore/blob/master/LICENSE
//---------------------------------------------------------

#ifndef __CPP11OM_BITFIELD_H__
#define __CPP11OM_BITFIELD_H__

#include <cassert>


//---------------------------------------------------------
// BitFieldMember<>: Used internally by ADD_BITFIELD_MEMBER macro.
// All members are public to simplify compliance with sections 9.0.7 and
// 9.5.1 of the C++11 standard, thereby avoiding undefined behavior.
//---------------------------------------------------------
template <typename T, int Offset, int Bits>
struct BitFieldMember
{
    T value;

    static_assert(Offset + Bits <= (int) sizeof(T) * 8, "Member exceeds bitfield boundaries");
    static_assert(Bits < (int) sizeof(T) * 8, "Can't fill entire bitfield with one member");

    static const T Maximum = (T(1) << Bits) - 1;
    static const T Mask = Maximum << Offset;
    T maximum() const { return Maximum; }
    T one() const { return T(1) << Offset; }

    operator T() const
    {
        return (value >> Offset) & Maximum;
    }

    BitFieldMember& operator=(T v)
    {
        assert(v <= Maximum);               // v must fit inside the bitfield member
        value = (value & ~Mask) | (v << Offset);
        return *this;
    }

    BitFieldMember& operator+=(T v)
    {
        assert(T(*this) + v <= Maximum);    // result must fit inside the bitfield member
        value += v << Offset;
        return *this;
    }

    BitFieldMember& operator-=(T v)
    {
        assert(T(*this) >= v);              // result must not underflow
        value -= v << Offset;
        return *this;
    }

    BitFieldMember& operator++() { return *this += 1; }
    BitFieldMember operator++(int)        // postfix form
    {
        BitFieldMember tmp(*this);
        operator++();
        return tmp;
    }
    BitFieldMember& operator--() { return *this -= 1; }
    BitFieldMember operator--(int)       // postfix form
    {
        BitFieldMember tmp(*this);
        operator--();
        return tmp;
    }
};


//---------------------------------------------------------
// BitFieldArray<>: Used internally by ADD_BITFIELD_ARRAY macro.
// All members are public to simplify compliance with sections 9.0.7 and
// 9.5.1 of the C++11 standard, thereby avoiding undefined behavior.
//---------------------------------------------------------
//template <typename T, int BaseOffset, int BitsPerItem, int NumItems, int EmptyBitsPerItem = 0>
template <typename T, int BaseOffset, int BitsPerItem, int NumItems>
struct BitFieldArray
{
    T value;
//#define TotalBitsPerItem (BitsPerItem + EmptyBitsPerItem)
    static_assert(BaseOffset + BitsPerItem * NumItems <= (int) sizeof(T) * 8, "Array exceeds bitfield boundaries");
    //static_assert(BaseOffset + TotalBitsPerItem * NumItems <= (int) sizeof(T) * 8, "Array exceeds bitfield boundaries");
    static_assert(BitsPerItem < (int) sizeof(T) * 8, "Can't fill entire bitfield with one array element");
    //static_assert(TotalBitsPerItem < (int) sizeof(T) * 8, "Can't fill entire bitfield with one array element");

    /*static const T ElementMaximum = (T(1) << TotalBitsPerItem) - 1;
    static const T ValueMaximum = (T(1) << BitsPerItem) - 1;*/
    static const T Maximum = (T(1) << BitsPerItem) - 1;
    //T maximum() const { return ValueMaximum; }
    T maximum() const { return Maximum; }
    int numItems() const { return NumItems; }

    class Element
    {
    private:
        T& value;
        const int offset;

    public:
        Element(T& value, int offset) : value(value), offset(offset) {}
        const T mask = Maximum << offset;
        //const T mask = ValueMaximum << offset;

        operator T() const
        {
            return (value >> offset) & Maximum;
            //return (value >> offset) & ValueMaximum;
        }

        Element& operator=(T v)
        {
            assert(v <= ValueMaximum);               // v must fit inside the bitfield member
            value = (value & ~mask) | (v << offset);
            return *this;
        }

        Element& operator+=(T v)
        {
            //assert(T(*this) + v <= ValueMaximum);    // result must fit inside the bitfield member
            assert(T(*this) + v <= Maximum);    // result must fit inside the bitfield member
            value += v << offset;
            return *this;
        }

        Element& operator-=(T v)
        {
            assert(T(*this) >= v);                  // result must not underflow
            value -= v << offset;
            return *this;
        }

        Element& operator++() { return *this += 1; }
        Element operator++(int)                     // postfix form
        {
            Element tmp(*this);
            operator++();
            return tmp;
        }
        Element& operator--() { return *this -= 1; }
        Element operator--(int)                     // postfix form
        {
            Element tmp(*this);
            operator--();
            return tmp;
        }

        //Element& operator*=(const T v)
        //{
        //    assert(T(*this) * v <= ValueMaximum);   // result must fit inside the bitfield member
        //    return *this = value * v;
        //}

        //Element& operator/=(const T v)
        //{
        //    return *this /= value * v;
        //}

        //Element& operator|=(const T v)
        //{
        //    assert(v <= ValueMaximum);         // v must fit inside the bitfield member
        //    value |= v << offset;
        //    return *this;
        //}

        //Element& operator&=(const T v)
        //{
        //    value &= v << offest;
        //    return *this;
        //}

        //Element& operator<<=(const int shift)
        //{
        //    value = (value & ~mask) | (value << shift) & mask;
        //    return *this;
        //}

        //Element& operator>>=(const int shift)
        //{
        //    value = (value & ~mask) | (value >> shift) & mask;
        //    return *this;
        //}
    };

    Element operator[](int i)
    {
        assert(i / (NumItems + 1) == 0);     // array index must be in range
        return Element(value, BaseOffset + BitsPerItem * i);
        //return Element(value, BaseOffset + TotalBitsPerItem * i);
    }

    const Element operator[](int i) const
    {
        assert(i / (NumItems + 1) == 0);     // array index must be in range
        return Element(value, BaseOffset + BitsPerItem * i);
        //return Element(value, BaseOffset + TotalBitsPerItem * i);
    }
#undef TotalBitsPerItem
};


//---------------------------------------------------------
// Bitfield definition macros.
// For usage examples, see RWLock and LockReducedDiningPhilosophers.
// All members are public to simplify compliance with sections 9.0.7 and
// 9.5.1 of the C++11 standard, thereby avoiding undefined behavior.
//---------------------------------------------------------
#define BEGIN_BITFIELD_TYPE(typeName, T) \
    union typeName \
    { \
        struct Wrapper { T value; }; \
        Wrapper wrapper; \
        typeName(T v = 0) { wrapper.value = v; } \
        typeName& operator=(T v) { wrapper.value = v; return *this; } \
        operator T&() { return wrapper.value; } \
        operator T() const { return wrapper.value; } \
        typedef T StorageType;

#define ADD_BITFIELD_MEMBER(memberName, offset, bits) \
        BitFieldMember<StorageType, offset, bits> memberName;

#define ADD_BITFIELD_ARRAY(memberName, offset, bits, numItems) \
        BitFieldArray<StorageType, offset, bits, numItems> memberName;

//#define ADD_BITFIELD_ARRAY_WITH_EMPTY_BITS(memberName, offset, bits, emptyBits, numItems) \
//        BitFieldArray<StorageType, offset, bits, numItems, emptyBits> memberName;

#define END_BITFIELD_TYPE() \
    };


#endif // __CPP11OM_BITFIELD_H__

