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
 *   using specifications from the data definition file
 *   'seqalign.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>

#include <objects/seqloc/Seq_point.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CStd_seg::~CStd_seg(void)
{
}


CStd_seg::TDim CStd_seg::CheckNumRows() const
{
    const size_t& dim = GetDim();
    if (dim != GetLoc().size()) {
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "CStd_seg::CheckNumRows():"
                   " loc.size is inconsistent with dim");
    }
    if (IsSetIds()  &&  dim != GetIds().size()) {
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "CStd_seg::CheckNumRows():"
                   " ids.size is inconsistent with dim");
    }
    _ASSERT(dim <= kMax_Int);
    return (TDim)dim;
}


TSignedSeqPos CStd_seg::GetSeqStart(TDim row) const
{
    TDim row_i = 0;
    ITERATE (CStd_seg::TLoc, i, GetLoc()) {
        if (row_i++ == row) {
            if ((*i)->IsInt()) {
                return (*i)->GetInt().GetFrom();
            } else {
                return -1;
            }
        }
    }
    if (row < 0  ||  row >= GetDim()) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CStd_seg::GetSeqStart():"
                   " Invalid row number");
    }
    NCBI_THROW(CSeqalignException, eInvalidAlignment,
               "CStd_seg::GetSeqStart():"
               " loc.size is inconsistent with dim");
}


TSignedSeqPos CStd_seg::GetSeqStop(TDim row) const
{
    TDim dim = 0;
    ITERATE (CStd_seg::TLoc, i, GetLoc()) {
        if (dim++ == row) {
            if ((*i)->IsInt()) {
                return (*i)->GetInt().GetTo();
            } else {
                return -1;
            }
        }
    }
    if (row < 0  ||  row >= GetDim()) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CStd_seg::GetSeqStop():"
                   " Invalid row number");
    }
    NCBI_THROW(CSeqalignException, eInvalidAlignment,
               "CStd_seg::GetSeqStop():"
               " loc.size is inconsistent with dim");
}


void CStd_seg::Validate(bool /* full_test */) const
{
    CheckNumRows();
}


void CStd_seg::SwapRows(int row0, int row1)
{
    _ASSERT(row0 < (int)GetLoc().size());
    _ASSERT(row1 < (int)GetLoc().size());
    if (row0 < (int)GetLoc().size()  &&  row1 < (int)GetLoc().size()) {
        swap(SetLoc()[row0], SetLoc()[row1]);
        if (IsSetIds()) {
            swap(SetIds()[row0], SetIds()[row1]);
        }
    } else {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CStd_seg::SwapRows():"
                   " Invalid row number");
    }
}


CRange<TSignedSeqPos> CStd_seg::GetSeqRange(TDim row) const
{
    TDim dim = 0;
    ITERATE (CStd_seg::TLoc, i, GetLoc()) {
        if (dim++ == row) {
            if ((*i)->IsInt()) {
                const CSeq_interval& interval = (*i)->GetInt();
                return CRange<TSignedSeqPos>
                    (interval.GetFrom(), interval.GetTo());
            } else {
                return CRange<TSignedSeqPos>(-1, -1);
            }
        }
    }
    if (row < 0  ||  row >= GetDim()) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CStd_seg::GetSeqRange():"
                   " Invalid row number");
    }
    NCBI_THROW(CSeqalignException, eInvalidAlignment,
               "CStd_seg::GetSeqRange():"
               " loc.size is inconsistent with dim");
}


void CStd_seg::OffsetRow(TDim row,
                         TSignedSeqPos offset)

{
    if (offset == 0) return;

    CSeq_loc& src_loc = *SetLoc()[row];

    switch (src_loc.Which()) {
    case CSeq_loc::e_Int:
        if (offset < 0) {
            _ASSERT((TSignedSeqPos)src_loc.GetInt().GetFrom() + offset >= 0);
            if ((TSignedSeqPos)src_loc.GetInt().GetFrom() < -offset) {
                NCBI_THROW(CSeqalignException, eOutOfRange,
                           "Negative offset greater than seq position");
            }
        }   
        src_loc.SetInt().SetFrom() += offset;
        src_loc.SetInt().SetTo() += offset;
        break;
    case CSeq_loc::e_Pnt:
        if (offset < 0) {
            _ASSERT((TSignedSeqPos)src_loc.GetPnt().GetPoint() + offset >= 0);
            if ((TSignedSeqPos)src_loc.GetPnt().GetPoint() < -offset) {
                NCBI_THROW(CSeqalignException, eOutOfRange,
                           "Negative offset greater than seq position");
            }
        }
        src_loc.SetPnt().SetPoint() += offset;
        break;
    case CSeq_loc::e_Empty:
        break;
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CStd_seg::OffsetRow only supports pnt and int source seq-locs");
    }
}


/// @deprecated
void CStd_seg::RemapToLoc(TDim row,
                          const CSeq_loc& dst_loc,
                          bool ignore_strand)
{
    // Limit to certain types of locs:
    switch (dst_loc.Which()) {
    case CSeq_loc::e_Whole:
        return;
    case CSeq_loc::e_Int:
        break;
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CStd_seg::RemapToLoc only supports int target seq-locs");
    }

    if (row < 0  ||  row >= GetDim()) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
                   "CStd_seg::RemapToLoc():"
                   " Invalid row number");
    }

    const CSeq_interval& dst_int = dst_loc.GetInt();
    TSeqPos              dst_len = dst_int.GetTo() - dst_int.GetFrom() + 1;

    CSeq_loc&            src_loc  = *SetLoc()[row];
    TSeqPos              src_stop = src_loc.GetStop(eExtreme_Positional);

#if _DEBUG
    // Check ids equality
    const CSeq_id* single_src_id = 0;
    src_loc.CheckId(single_src_id);
    _ASSERT(single_src_id);

    const CSeq_id* single_dst_id = 0;
    dst_loc.CheckId(single_dst_id);
    _ASSERT(single_dst_id);

    if ( !single_src_id->Equals(*single_dst_id)  ||
         (IsSetIds()  &&  !single_dst_id->Equals(*GetIds()[row])) ) {
        NCBI_THROW(CSeqalignException, eInvalidInputData,
                   "CStd_seg::RemapToLoc target seq-loc id does not equal row's id.");
    }
#endif

    // Check the range
    if (dst_len <= src_stop  &&  src_stop != kInvalidSeqPos) {
        string errstr("CStd_seg::RemapToLoc():"
                      " Target Seq-loc is not long enough to"
                      " cover the Std-seg's seq-loc!"
                      " Maximum row seq pos is ");
        errstr += NStr::IntToString(src_stop);
        errstr += ". The total seq-loc len is only ";
        errstr += NStr::IntToString(dst_len);
        errstr += ", it should be at least ";
        errstr += NStr::IntToString(src_stop + 1);
        errstr += " (= max seq pos + 1).";
        NCBI_THROW(CSeqalignException, eOutOfRange, errstr);
    }

    // Actual remapping
    switch (src_loc.Which()) {
    case CSeq_loc::e_Int:
        src_loc.SetInt().SetFrom() += dst_int.GetFrom();
        src_loc.SetInt().SetTo() += dst_int.GetFrom();
        if ( !ignore_strand ) {
            src_loc.SetInt().SetStrand(dst_loc.GetInt().GetStrand());
        }
        break;
    case CSeq_loc::e_Pnt:
        src_loc.SetPnt().SetPoint() += dst_int.GetFrom();
        if ( !ignore_strand ) {
            src_loc.SetPnt().SetStrand(dst_loc.GetInt().GetStrand());
        }
        break;
    case CSeq_loc::e_Empty:
        break;
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "CStd_seg::RemapToLoc only supports pnt and int source seq-locs");
    }
}


CRef<CSeq_loc> CStd_seg::CreateRowSeq_loc(TDim row) const
{
    if (GetDim() <= row) {
        NCBI_THROW(CSeqalignException, eInvalidRowNumber,
            "Invalid row number in CreateRowSeq_interval(): " +
            NStr::IntToString(row));
    }
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->Assign(*GetLoc()[row]);
    return ret;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 64, chars: 1879, CRC32: d01e569e */
