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

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqtable/SeqTable_sparse_index.hpp>

#include <objects/seqtable/seq_table_exception.hpp>
#include <util/bitset/bmfunc.h>
#include <util/bitset/bmserial.h>

#include <objects/seqtable/impl/delta_cache.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeqTable_sparse_index::~CSeqTable_sparse_index(void)
{
}


#ifdef NCBI_COMPILER_GCC
const size_t CSeqTable_sparse_index::kInvalidRow;
const size_t CSeqTable_sparse_index::kSkipped;
#endif

DEFINE_STATIC_MUTEX(sx_PrepareMutex_sparse_index);


static inline size_t sx_CalcByteBitCount(Uint4 word)
{
    return bm::word_bitcount(word);
}


static inline size_t sx_CalcByteBitCount(Uint1 byte)
{
    return bm::bit_count_table<true>::_count[byte];
}


static inline size_t sx_CalcBlockBitCount(const char* block, size_t size)
{
    const bm::word_t* word_block = reinterpret_cast<const bm::word_t*>(block);
    const bm::word_t* word_block_end = word_block + size/sizeof(bm::word_t);
    size_t ret = bm::bit_count_min_unroll(word_block, word_block_end);

    const char* tail_ptr = reinterpret_cast<const char*>(word_block_end);
    const char* block_end = block + size;
    for ( ; tail_ptr != block_end; ++tail_ptr ) {
        ret += sx_CalcByteBitCount(Uint1(*tail_ptr));
    }

    return ret;
}


// return number of the first set bit in a byte
// bit 0x80 - 0
// bit 0x40 - 1
// ...
// bit 0x01 - 7
// if there are no set bits return kInvalidRow
static inline size_t sx_FindFirstNonZeroBit(Uint1 b)
{
    for ( size_t i = 0; i < 8; ++i, b <<= 1 ) {
        if ( b&0x80 ) {
            return i;
        }
    }
    return CSeqTable_sparse_index::kInvalidRow;
}


// return number of the next set bit in a byte starting with bit prev_i
// bit 0x80 - 0
// bit 0x40 - 1
// ...
// bit 0x01 - 7
// if there are no more set bits return kInvalidRow
static inline size_t sx_FindNextNonZeroBit(Uint1 b, size_t prev_i)
{
    b <<= prev_i+1;
    for ( size_t i = prev_i+1; i < 8; ++i, b <<= 1 ) {
        if ( b&0x80 ) {
            return i;
        }
    }
    return CSeqTable_sparse_index::kInvalidRow;
}


// return number of the first non-zero byte in a range
// if there are no non-zero bytes return kInvalidRow
static inline size_t sx_FindFirstNonZeroByte(const char* beg,
                                             const char* end)
{
    typedef Uint8 TBig; // faster scan using bigger type than char
    const char* ptr = beg;
    // align to bigger type
    for ( ; ptr != end && reinterpret_cast<size_t>(ptr)%sizeof(TBig); ++ptr ) {
        if ( *ptr ) {
            return ptr - beg;
        }
    }
    // scan using bigger type
    for ( ; ptr+sizeof(TBig) <= end; ptr += sizeof(TBig) ) {
        if ( *reinterpret_cast<const TBig*>(ptr) != 0 ) {
            break;
        }
    }
    // scan for exact char position
    for ( ; ptr != end; ++ptr ) {
        if ( *ptr ) {
            return ptr - beg;
        }
    }
    return CSeqTable_sparse_index::kInvalidRow;
}


// return number of the first non-zero byte in a vector, starting with index
// if there are no more non-zero bytes return kInvalidRow
static inline size_t sx_FindFirstNonZeroByte(const vector<char>& bytes,
                                             size_t index)
{
    size_t size = bytes.size();
    const char* ptr = &bytes[0];
    size_t offset = sx_FindFirstNonZeroByte(ptr+index, ptr+size);
    if ( offset == CSeqTable_sparse_index::kInvalidRow ) {
        return CSeqTable_sparse_index::kInvalidRow;
    }
    return index + offset;
}


const size_t CSeqTable_sparse_index::SBitsInfo::kBlockSize = 256;


size_t CSeqTable_sparse_index::x_GetBitSetCache(size_t byte_count) const
{
    const TBit_set& bytes = GetBit_set();
    size_t size = bytes.size();
    CMutexGuard guard(sx_PrepareMutex_sparse_index);
    if ( !m_Cache ) {
        m_Cache.Reset(new SBitsInfo);
    }
    SBitsInfo& info= dynamic_cast<SBitsInfo&>(*m_Cache);
    static const size_t kBlockSize = SBitsInfo::kBlockSize;
    size_t block_index  = byte_count / kBlockSize;
    size_t block_offset = byte_count % kBlockSize;
    for ( ; block_index > info.m_BlocksFilled; ) {
        if ( !info.m_Blocks ) {
            size_t block_count = size / kBlockSize;
            info.m_Blocks.reset(new size_t[block_count]);
        }
        size_t next_index = info.m_BlocksFilled;
        size_t count = sx_CalcBlockBitCount(&bytes[next_index*kBlockSize],
                                            kBlockSize);
        if ( next_index > 0 ) {
            count += info.m_Blocks[next_index-1];
        }
        info.m_Blocks[next_index] = count;
        info.m_BlocksFilled = next_index+1;
    }
    size_t ret = block_index? info.m_Blocks[block_index-1]: 0;
    if ( block_offset ) {
        if ( block_index != info.m_CacheBlockIndex ) {
            if ( !info.m_CacheBlockInfo ) {
                info.m_CacheBlockInfo.reset(new size_t[kBlockSize]);
            }
            size_t count = 0;
            size_t block_pos = block_index*kBlockSize;
            size_t block_size = min(kBlockSize, size-block_pos);
            const char* block = &bytes[block_pos];
            for ( size_t i = 0; i < block_size; ++i ) {
                count += sx_CalcByteBitCount(Uint1(block[i]));
                info.m_CacheBlockInfo[i] = count;
            }
            info.m_CacheBlockIndex = block_index;
        }
        ret += info.m_CacheBlockInfo[block_offset-1];
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CIndexDeltaSumCache
/////////////////////////////////////////////////////////////////////////////

//#define USE_DELTA_CACHE
const size_t CIndexDeltaSumCache::kBlockSize = 128;
const size_t CIndexDeltaSumCache::kInvalidRow = CSeqTable_sparse_index::kInvalidRow;
const size_t CIndexDeltaSumCache::kBlockTooLow = CIndexDeltaSumCache::kInvalidRow-1;


CIndexDeltaSumCache::CIndexDeltaSumCache(size_t size)
    : m_Blocks(new TValue[(size+kBlockSize-1)/kBlockSize]),
      m_BlocksFilled(0),
#ifdef USE_DELTA_CACHE
      m_CacheBlockInfo(new TValue[kBlockSize]),
#endif
      m_CacheBlockIndex(size_t(0)-1)
{
}


CIndexDeltaSumCache::~CIndexDeltaSumCache(void)
{
}


inline
CIndexDeltaSumCache::TValue
CIndexDeltaSumCache::x_GetDeltaSum2(const TDeltas& deltas,
                                    size_t block_index,
                                    size_t block_offset)
{
#ifdef USE_DELTA_CACHE
    _ASSERT(block_index <= m_BlocksFilled);
    if ( block_index != m_CacheBlockIndex ) {
        size_t size = deltas.size();
        size_t block_pos = block_index*kBlockSize;
        _ASSERT(block_pos < size);
        size_t block_size = min(kBlockSize, size-block_pos);
        _ASSERT(block_offset < block_size);
        TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
        const TDeltas::value_type* block = &deltas[block_pos];
        for ( size_t i = 0; i < block_size; ++i ) {
            sum += block[i];
            m_CacheBlockInfo[i] = sum;
        }
        m_CacheBlockIndex = block_index;
        if ( block_index == m_BlocksFilled ) {
            m_Blocks[block_index] = sum;
            m_BlocksFilled = block_index+1;
        }
    }
    return m_CacheBlockInfo[block_offset];
#else
    size_t size = deltas.size();
    size_t block_pos = block_index*kBlockSize;
    _ASSERT(block_pos < size);
    size_t block_size = min(kBlockSize, size-block_pos);
    _ASSERT(block_offset < block_size);
    TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
    const TDeltas::value_type* block = &deltas[block_pos];
    for ( size_t i = 0; i <= block_offset; ++i ) {
        sum += block[i];
    }
    if ( block_index == m_BlocksFilled ) {
        TValue sum2 = sum;
        for ( size_t i = block_offset+1; i < block_size; ++i ) {
            sum2 += block[i];
        }
        m_Blocks[block_index] = sum2;
        m_BlocksFilled = block_index+1;
    }
    return sum;
#endif
}


inline
size_t CIndexDeltaSumCache::x_FindDeltaSum2(const TDeltas& deltas,
                                            size_t block_index,
                                            TValue find_sum)
{
    _ASSERT(block_index<=m_BlocksFilled);
    _ASSERT(block_index==m_BlocksFilled || find_sum<=m_Blocks[block_index]);
    _ASSERT(block_index==0 || find_sum>m_Blocks[block_index-1]);
    size_t size = deltas.size();
    size_t block_pos = block_index*kBlockSize;
    _ASSERT(block_pos < size);
    size_t block_size = min(kBlockSize, size-block_pos);
#ifdef USE_DELTA_CACHE
    if ( block_index < m_BlocksFilled && find_sum > m_Blocks[block_index] ) {
        return kBlockTooLow;
    }
    x_GetDeltaSum2(deltas, block_index, 0);
    _ASSERT(block_index < m_BlocksFilled);
    if ( find_sum > m_Blocks[block_index] ) {
        return kBlockTooLow;
    }
    _ASSERT(find_sum <= m_Blocks[block_index]);
    size_t value_index = lower_bound(&m_CacheBlockInfo[0],
                                     &m_CacheBlockInfo[block_size],
                                     find_sum) - &m_CacheBlockInfo[0];
    _ASSERT(value_index < block_size);
    _ASSERT(find_sum <= m_CacheBlockInfo[value_index]);
    _ASSERT(value_index==0 || find_sum > m_CacheBlockInfo[value_index-1]);
    if ( find_sum != m_CacheBlockInfo[value_index] ) {
        return kInvalidRow;
    }
    return block_pos + value_index;
#else
    TValue sum = block_index == 0? 0: m_Blocks[block_index-1];
    const TDeltas::value_type* block = &deltas[block_pos];
    for ( size_t i = 0; i < block_size; ++i ) {
        sum += block[i];
        if ( sum >= find_sum ) {
            if ( block_index == m_BlocksFilled ) {
                TValue sum2 = sum;
                for ( size_t j = i+1; j < block_size; ++j ) {
                    sum2 += block[j];
                }
                m_Blocks[block_index] = sum2;
                m_BlocksFilled = block_index+1;
            }
            return sum > find_sum? kInvalidRow: block_pos+i;
        }
    }
    if ( block_index == m_BlocksFilled ) {
        m_Blocks[block_index] = sum;
        m_BlocksFilled = block_index+1;
    }
    return kBlockTooLow;
#endif
}


CIndexDeltaSumCache::TValue
CIndexDeltaSumCache::GetDeltaSum(const TDeltas& deltas,
                                 size_t index)
{
    _ASSERT(index < deltas.size());
    size_t block_index  = index / kBlockSize;
    size_t block_offset = index % kBlockSize;
    while ( block_index >= m_BlocksFilled ) {
        x_GetDeltaSum2(deltas, m_BlocksFilled, 0);
    }
    return x_GetDeltaSum2(deltas, block_index, block_offset);
}


size_t CIndexDeltaSumCache::FindDeltaSum(const TDeltas& deltas,
                                         TValue find_sum)
{
    size_t size = deltas.size();
    size_t block_index;
    if ( m_BlocksFilled > 0 && find_sum <= m_Blocks[m_BlocksFilled-1] ) {
        // sum is within already preprocessed block, locate the block
        block_index = lower_bound(&m_Blocks[0],
                                  &m_Blocks[m_BlocksFilled],
                                  find_sum) - &m_Blocks[0];
        size_t row = x_FindDeltaSum2(deltas, block_index, find_sum);
        _ASSERT(row != kBlockTooLow);
        return row;
    }
    else {
        // preprocess blocks sequentially until row will be within block range
        for ( ;; ) {
            block_index = m_BlocksFilled;
            if ( block_index*kBlockSize >= size ) {
                // last block is processed but the required sum is still bigger
                return kInvalidRow;
            }
            size_t row = x_FindDeltaSum2(deltas, block_index, find_sum);
            if ( row != kBlockTooLow ) {
                return row;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////


CIndexDeltaSumCache& CSeqTable_sparse_index::x_GetDeltaCache(void) const
{
    CIndexDeltaSumCache* info =
        dynamic_cast<CIndexDeltaSumCache*>(m_Cache.GetNCPointerOrNull());
    if ( !info ) {
        m_Cache = info = new CIndexDeltaSumCache(GetIndexes_delta().size());
    }
    return *info;
}


inline
size_t CSeqTable_sparse_index::x_GetDeltaSum(size_t index) const
{
    CMutexGuard guard(sx_PrepareMutex_sparse_index);
    return x_GetDeltaCache().GetDeltaSum(GetIndexes_delta(), index);
}


inline
size_t CSeqTable_sparse_index::x_FindDeltaSum(size_t sum) const
{
    CMutexGuard guard(sx_PrepareMutex_sparse_index);
    return x_GetDeltaCache().FindDeltaSum(GetIndexes_delta(), sum);
}


size_t CSeqTable_sparse_index::GetSize(void) const
{
    switch ( Which() ) {
    case e_Indexes:
    {
        const TIndexes& indexes = GetIndexes();
        return indexes.empty()? 0: indexes.back()+1;
    }
    case e_Indexes_delta:
    {
        const TIndexes_delta& deltas = GetIndexes_delta();
        return deltas.empty()? 0: x_GetDeltaSum(deltas.size()-1)+1;
    }
    case e_Bit_set:
        return GetBit_set().size()*8;
    case e_Bit_set_bvector:
        return GetBit_set_bvector().GetSize();
    default:
        return 0;
    }
}


size_t CSeqTable_sparse_index::x_GetFirstRowWithValue(void) const
{
    switch ( Which() ) {
    case e_Indexes:
    {
        const TIndexes& indexes = GetIndexes();
        return indexes.empty()? kInvalidRow: indexes.front();
    }
    case e_Indexes_delta:
    {
        const TIndexes_delta& deltas = GetIndexes_delta();
        return deltas.empty()? kInvalidRow: deltas.front();
    }
    case e_Bit_set:
    {
        const TBit_set& bytes = GetBit_set();
        size_t byte_index = sx_FindFirstNonZeroByte(bytes, 0);
        if ( byte_index == kInvalidRow ) {
            return kInvalidRow;
        }
        return byte_index*8 + sx_FindFirstNonZeroBit(Uint1(bytes[byte_index]));
    }
    case e_Bit_set_bvector:
        return GetBit_set_bvector().GetBitVector().get_first();
    default:
        return kInvalidRow;
    }
}


size_t CSeqTable_sparse_index::x_GetNextRowWithValue(size_t row,
                                                     size_t value_index) const
{
    switch ( Which() ) {
    case e_Indexes:
    {
        const TIndexes& indexes = GetIndexes();
        return ++value_index >= indexes.size()?
            kInvalidRow: indexes[value_index];
    }
    case e_Indexes_delta:
    {
        const TIndexes_delta& deltas = GetIndexes_delta();
        return ++value_index >= deltas.size()?
            kInvalidRow: row + deltas[value_index];
    }
    case e_Bit_set:
    {
        const TBit_set& bytes = GetBit_set();
        size_t byte_index = row / 8;
        size_t bit_index = row % 8;
        bit_index = sx_FindNextNonZeroBit(Uint1(bytes[byte_index]), bit_index);
        if ( bit_index != kInvalidRow ) {
            return byte_index*8 + bit_index;
        }
        byte_index = sx_FindFirstNonZeroByte(bytes, byte_index + 1);
        if ( byte_index == kInvalidRow ) {
            return kInvalidRow;
        }
        return byte_index*8 + sx_FindFirstNonZeroBit(Uint1(bytes[byte_index]));
    }
    case e_Bit_set_bvector:
    {
        row = GetBit_set_bvector().GetBitVector().get_next(row);
        return row == 0? kInvalidRow: row;
    }
    default:
        return kInvalidRow;
    };
}


size_t CSeqTable_sparse_index::GetIndexAt(size_t row) const
{
    switch ( Which() ) {
    case e_Indexes:
    {
        const TIndexes& indexes = GetIndexes();
        TIndexes::const_iterator iter =
            lower_bound(indexes.begin(), indexes.end(), row);
        if ( iter != indexes.end() && *iter == row ) {
            return iter - indexes.begin();
        }
        else {
            return kSkipped;
        }
    }
    case e_Indexes_delta:
        return x_FindDeltaSum(row);
    case e_Bit_set:
    {
        const TBit_set& bytes = GetBit_set();
        size_t byte_index = row/8;
        if ( byte_index >= bytes.size() ) {
            return kSkipped;
        }
        Uint1 byte = bytes[byte_index];
        size_t bit_index = row%8; // most significant bit has index 0
        if ( !((byte<<bit_index)&0x80) ) {
            return kSkipped;
        }
        size_t count = sx_CalcByteBitCount(Uint1(byte>>(8-bit_index)));
        if ( byte_index ) {
            count += x_GetBitSetCache(byte_index);
        }
        return count;
    }
    case e_Bit_set_bvector:
    {
        const bm::bvector<>& bv = GetBit_set_bvector().GetBitVector();
        if ( row >= bv.size() || !bv.get_bit(row) ) {
            return kSkipped;
        }
        return row == 0? 0: bv.count_range(0, row-1);
    }
    default:
        return kSkipped;
    }
}


bool CSeqTable_sparse_index::HasValueAt(size_t row) const
{
    switch ( Which() ) {
    case e_Indexes:
    {
        const TIndexes& indexes = GetIndexes();
        TIndexes::const_iterator iter =
            lower_bound(indexes.begin(), indexes.end(), row);
        return iter != indexes.end() && *iter == row;
    }
    case e_Indexes_delta:
        return x_FindDeltaSum(row) != kInvalidRow;
    case e_Bit_set:
    {
        const TBit_set& bits = GetBit_set();
        size_t i = row/8, j = row%8;
        return i < bits.size() && ((bits[i]<<j)&0x80) != 0;
    }
    case e_Bit_set_bvector:
    {
        const bm::bvector<>& bv = GetBit_set_bvector().GetBitVector();
        return row < bv.size() && bv.get_bit(row);
    }
    default:
        return false;
    }
}


void CSeqTable_sparse_index::ChangeTo(E_Choice type)
{
    if ( Which() == type ) {
        return;
    }
    switch ( type ) {
    case e_Indexes:
        ChangeToIndexes();
        break;
    case e_Indexes_delta:
        ChangeToIndexes_delta();
        break;
    case e_Bit_set:
        ChangeToBit_set();
        break;
    case e_Bit_set_bvector:
        ChangeToBit_set_bvector();
        break;
    default:
        NCBI_THROW(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_sparse_index::ChangeTo(): "
                   "requested sparse index type is invalid");
    }
}


void CSeqTable_sparse_index::ChangeToIndexes(void)
{
    if ( IsIndexes() ) {
        return;
    }
    TIndexes indexes;
    if ( IsIndexes_delta() ) {
        // convert delta to running sum
        indexes.swap(SetIndexes_delta());
        size_t row = 0;
        NON_CONST_ITERATE ( TIndexes, it, indexes ) {
            row += *it;
            *it = row;
        }
    }
    else {
        for ( const_iterator it = begin(); it; ++it ) {
            indexes.push_back(it.GetRow());
        }
    }
    SetIndexes().swap(indexes);
}


void CSeqTable_sparse_index::ChangeToIndexes_delta(void)
{
    if ( IsIndexes_delta() ) {
        return;
    }
    TIndexes_delta indexes;
    if ( IsIndexes() ) {
        // convert to delta
        indexes.swap(SetIndexes());
        size_t prev_row = 0;
        NON_CONST_ITERATE ( TIndexes_delta, it, indexes ) {
            size_t row = *it;
            *it = row - prev_row;
            prev_row = row;
        }
    }
    else {
        size_t prev_row = 0;
        for ( const_iterator it = begin(); it; ++it ) {
            size_t row = it.GetRow();
            indexes.push_back(row-prev_row);
            prev_row = row;
        }
    }
    SetIndexes_delta().swap(indexes);
}



void CSeqTable_sparse_index::ChangeToBit_set(void)
{
    if ( IsBit_set() ) {
        return;
    }
    TBit_set bytes;
    size_t size = GetSize();
    if ( size != kInvalidRow ) {
        bytes.reserve((GetSize()+7)/8);
    }
    size_t last_byte_index = 0;
    Uint1 last_byte = 0;
    for ( const_iterator it = begin(); it; ++it ) {
        size_t row = it.GetRow();
        size_t byte_index = row / 8;
        if ( byte_index != last_byte_index ) {
            if ( bytes.capacity() < byte_index+1 ) {
                bytes.reserve(max(bytes.capacity(), byte_index+1)*2);
            }
            bytes.resize(last_byte_index);
            bytes.push_back(last_byte);
            last_byte_index = byte_index;
            last_byte = 0;
        }
        size_t bit_index = row % 8;
        last_byte |= 0x80 >> bit_index;
    }
    if ( last_byte ) {
        bytes.reserve(last_byte_index+1);
        bytes.resize(last_byte_index);
        bytes.push_back(last_byte);
    }
    SetBit_set().swap(bytes);
}



void CSeqTable_sparse_index::ChangeToBit_set_bvector(void)
{
    if ( IsBit_set_bvector() ) {
        return;
    }
    AutoPtr<bm::bvector<> > bv(new bm::bvector<>(GetSize()));
    for ( const_iterator it = begin(); it; ++it ) {
        size_t row = it.GetRow();
        bv->set_bit(row);
    }
    bv->optimize();
    SetBit_set_bvector().SetBitVector(bv.release());
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
