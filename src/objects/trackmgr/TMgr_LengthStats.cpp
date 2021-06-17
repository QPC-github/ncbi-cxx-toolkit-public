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
 *   'trackmgr.asn'.
 */

#include <ncbi_pch.hpp>
#include <objects/trackmgr/TMgr_LengthStats.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CTMgr_LengthStats::CTMgr_LengthStats(void)
{
}

CTMgr_LengthStats::~CTMgr_LengthStats(void)
{
}

void
CTMgr_LengthStats::Add(const TSeqPos length, const Uint8 prior_count)
{
    if (prior_count == 0) {
        SetMin(length);
        SetMax(length);
        SetMean(length);
    }
    else {
        SetMin(min(GetMin(), length));
        SetMax(max(GetMax(), length));
        // adjust mean by weighted adjustment of current value
        const TSeqPos curr_mean = GetMean();
        if (length > curr_mean) {
            const TSeqPos len_diff = length - curr_mean;
            SetMean(curr_mean + len_diff / (prior_count + 1));
        }
        else {
            const TSeqPos len_diff = curr_mean - length;
            SetMean(curr_mean - len_diff / (prior_count + 1));
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

