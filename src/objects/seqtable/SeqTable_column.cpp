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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'seqtable.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_sparse_index.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/SeqTable_single_data.hpp>
#include <objects/seqtable/CommonString_table.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeqTable_column::~CSeqTable_column(void)
{
}


bool CSeqTable_column::IsSet(size_t row) const
{
    size_t index = row;
    if ( IsSetSparse() ) {
        index = GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            return IsSetSparse_other();
        }
    }
    if ( IsSetData() && GetData().IsSet(index) ) {
        return true;
    }
    return IsSetDefault();
}


const string* CSeqTable_column::GetStringPtr(size_t row) const
{
    size_t index = row;
    if ( IsSetSparse() ) {
        index = GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( IsSetSparse_other() ) {
                return &GetSparse_other().GetString();
            }
            return 0;
        }
    }
    if ( IsSetData() ) {
        if ( const string* ret = GetData().GetStringPtr(index) ) {
            return ret;
        }
    }
    if ( IsSetDefault() ) {
        return &GetDefault().GetString();
    }
    return 0;
}


const vector<char>* CSeqTable_column::GetBytesPtr(size_t row) const
{
    size_t index = row;
    if ( IsSetSparse() ) {
        index = GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( IsSetSparse_other() ) {
                return &GetSparse_other().GetBytes();
            }
            return 0;
        }
    }
    if ( IsSetData() ) {
        if ( const vector<char>* ret = GetData().GetBytesPtr(index) ) {
            return ret;
        }
    }
    if ( IsSetDefault() ) {
        return &GetDefault().GetBytes();
    }
    return 0;
}


CConstRef<CSeq_id> CSeqTable_column::GetSeq_id(size_t row) const
{
    size_t index = row;
    if ( IsSetSparse() ) {
        index = GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( IsSetSparse_other() ) {
                return ConstRef(&GetSparse_other().GetId());
            }
            return null;
        }
    }
    if ( IsSetData() ) {
        const CSeqTable_multi_data::TId& arr = GetData().GetId();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    if ( IsSetDefault() ) {
        return ConstRef(&GetDefault().GetId());
    }
    return null;
}


CConstRef<CSeq_loc> CSeqTable_column::GetSeq_loc(size_t row) const
{
    size_t index = row;
    if ( IsSetSparse() ) {
        index = GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( IsSetSparse_other() ) {
                return ConstRef(&GetSparse_other().GetLoc());
            }
            return null;
        }
    }
    if ( IsSetData() ) {
        const CSeqTable_multi_data::TLoc& arr = GetData().GetLoc();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    if ( IsSetDefault() ) {
        return ConstRef(&GetDefault().GetLoc());
    }
    return null;
}


CConstRef<CSeq_interval> CSeqTable_column::GetSeq_int(size_t row) const
{
    size_t index = row;
    if ( IsSetSparse() ) {
        index = GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( IsSetSparse_other() ) {
                return ConstRef(&GetSparse_other().GetInterval());
            }
            return null;
        }
    }
    if ( IsSetData() ) {
        const CSeqTable_multi_data::TInterval& arr = GetData().GetInterval();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    if ( IsSetDefault() ) {
        return ConstRef(&GetDefault().GetInterval());
    }
    return null;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1744, CRC32: d04e9b34 */
