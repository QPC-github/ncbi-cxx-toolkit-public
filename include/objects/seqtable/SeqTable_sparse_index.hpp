/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Support for SeqTable-sparse-index ASN.1 type
 *   API to work with different representations of sparse index
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'seqtable.asn'.
 */

/// @file SeqTable_sparse_index.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'seqtable.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: SeqTable_sparse_index_.hpp


#ifndef OBJECTS_SEQTABLE_SEQTABLE_SPARSE_INDEX_HPP
#define OBJECTS_SEQTABLE_SEQTABLE_SPARSE_INDEX_HPP


// generated includes
#include <objects/seqtable/SeqTable_sparse_index_.hpp>

#include <util/bitset/ncbi_bitset.hpp>

// generated classes


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


class CIndexDeltaSumCache;


/////////////////////////////////////////////////////////////////////////////
// CSeqTable_sparse_index is a class that helps representing columns
// that has values only in a subset of rows. So this class virtually tells
// if a row has a value and what is the sequential number of the value.
// The sequential number is treated as an index of the final row's value
// in a separate value array.
// There are several possible representations:
//  1. indexes
//     - list of indexes of rows having values
//  2. indexes-delta
//     - deltas between sequential rows having values, and the first element
//       is the index of the first row with value
//  3. bit-set
//     - bit octet array with one bit per row in ASN.1 order, if a bit is set
//       it mean this row has a value
//  4. bit-set-bvector
//     - serialized bvector<> instance with total bit size stored separately,
//       similarly to bit-set if a bit is set it mean this row has a value
class NCBI_SEQ_EXPORT CSeqTable_sparse_index : public CSeqTable_sparse_index_Base
{
    typedef CSeqTable_sparse_index_Base Tparent;
public:
    // constructor
    CSeqTable_sparse_index(void);
    // destructor
    ~CSeqTable_sparse_index(void);

    static const size_t kSkipped    = size_t(0)-1;
    static const size_t kInvalidRow = size_t(0)-1;

    // return number of all represented rows (with or without value)
    size_t GetSize(void) const;

    // return index of a value for the argument row
    // return kSkipped if the row doesn't have value
    size_t GetIndexAt(size_t row) const;

    // return true if the row has value
    bool HasValueAt(size_t row) const;

    // const_iterator iterates only rows with values
    class const_iterator {
    public:
        const_iterator(void)
            : m_Row(kInvalidRow),
              m_ValueIndex(0)
            {
            }
        
        DECLARE_OPERATOR_BOOL(m_Row != kInvalidRow);

        bool operator==(const const_iterator& iter) const {
            return m_Row == iter.m_Row;
        }

        bool operator!=(const const_iterator& iter) const {
            return !(*this == iter);
        }

        // current bit is always set
        bool operator*(void) const {
            return true;
        }

        size_t GetRow(void) const {
            return m_Row;
        }

        size_t GetValueIndex(void) const {
            return m_ValueIndex;
        }

        // go to the next set bit
        const_iterator& operator++(void) {
            m_Row = m_Obj->x_GetNextRowWithValue(m_Row, m_ValueIndex);
            ++m_ValueIndex;
            return *this;
        }

    protected:
        friend class CSeqTable_sparse_index;

        const_iterator(const CSeqTable_sparse_index* obj,
                       size_t row,
                       size_t value_index = 0)
            : m_Obj(obj),
              m_Row(row),
              m_ValueIndex(value_index)
            {
            }

    private:
        CConstRef<CSeqTable_sparse_index> m_Obj;
        size_t m_Row, m_ValueIndex;
    };

    // iterators over rows with values
    const_iterator begin(void) const {
        return const_iterator(this, x_GetFirstRowWithValue());
    }
    const_iterator end(void) const {
        return const_iterator();
    }

    
    // change the representation of index
    void ChangeTo(E_Choice type);
    void ChangeToIndexes(void);
    void ChangeToIndexes_delta(void);
    void ChangeToBit_set(void);
    void ChangeToBit_set_bvector(void);

    // Overload base setters to reset extra data fields
    void Reset(void) {
        x_ResetCache();
        Tparent::Reset();
    }
    TIndexes& SetIndexes(void) {
        x_ResetCache();
        return Tparent::SetIndexes();
    }
    TIndexes_delta& SetIndexes_delta(void) {
        x_ResetCache();
        return Tparent::SetIndexes_delta();
    }
    TBit_set& SetBit_set(void) {
        x_ResetCache();
        return Tparent::SetBit_set();
    }
    TBit_set_bvector& SetBit_set_bvector(void) {
        x_ResetCache();
        return Tparent::SetBit_set_bvector();
    }

    void PostRead(void) {
        x_ResetCache();
    }

protected:
    friend class const_iterator;

    void x_ResetCache(void) {
        m_Cache.Reset();
    }

    // iterator support
    size_t x_GetFirstRowWithValue(void) const;
    size_t x_GetNextRowWithValue(size_t row, size_t value_index) const;

    // bits counting support
    size_t x_GetBitSetCache(size_t byte_count) const;

    struct SBitsInfo : public CObject {
        SBitsInfo(void)
            : m_BlocksFilled(0),
              m_CacheBlockIndex(size_t(0)-1)
            {
            }

        // size of byte blocks for bit counting
        static const size_t kBlockSize;

        // accumulated bits counts per block of bytes
        // size = (totalbytes+kBlockSize-1)/kBlockSize
        // m_Blocks[0] = bits in the 1st block (0..kBlockSize-1)
        // m_Blocks[1] = bits in first 2 blocks (0..2*kBlockSize-1)
        AutoArray<size_t> m_Blocks;
        // number of calculated entries in m_Block
        size_t m_BlocksFilled;

        // cached accumulated bit counts per byte within a block
        // size = kBlockSize
        // m_CacheBlockInfo[0] = bits in the first byte of cached block
        // m_CacheBlockInfo[1] = bits in first 2 bytes of cached block
        // ...
        AutoArray<size_t> m_CacheBlockInfo;
        // index of the block with cached bit counts (or size_t(-1))
        size_t m_CacheBlockIndex;
    };

    CIndexDeltaSumCache& x_GetDeltaCache(void) const;
    size_t x_GetDeltaSum(size_t index) const;
    size_t x_FindDeltaSum(size_t sum) const;

    mutable CRef<CObject> m_Cache;

private:
    // Prohibit copy constructor and assignment operator
    CSeqTable_sparse_index(const CSeqTable_sparse_index& value);
    CSeqTable_sparse_index& operator=(const CSeqTable_sparse_index& value);
};

/////////////////// CSeqTable_sparse_index inline methods

// constructor
inline
CSeqTable_sparse_index::CSeqTable_sparse_index(void)
{
}


/////////////////// end of CSeqTable_sparse_index inline methods


NCBISER_HAVE_POST_READ(CSeqTable_sparse_index)


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQTABLE_SEQTABLE_SPARSE_INDEX_HPP
