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
 * Author:  Kamen Todorov
 *
 * File Description:
 *   Sparse-align
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'seqalign.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/seqalign_exception.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSparse_align::~CSparse_align(void)
{
}


void CSparse_align::Validate(bool /*full_test*/) const
{
    CheckNumRows();
    CheckNumSegs();
}


CSparse_align::TNumseg CSparse_align::CheckNumSegs(void) const {
    size_t numseg = GetNumseg();
    _SEQALIGN_ASSERT(GetFirst_starts().size() == numseg);
    _SEQALIGN_ASSERT(GetSecond_starts().size() == numseg);
    _SEQALIGN_ASSERT(GetLens().size() == numseg);
    _SEQALIGN_ASSERT(IsSetSecond_strands() ? GetSecond_strands().size() == numseg : true);
    _SEQALIGN_ASSERT(IsSetSeg_scores() ? GetSeg_scores().size() == numseg : true);
    _SEQALIGN_ASSERT(numseg <= kMax_Int);
    return (TNumseg)numseg;
}    


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1735, CRC32: ca1dcbf8 */
