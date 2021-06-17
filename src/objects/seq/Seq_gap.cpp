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
 *   'seq.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seq/Seq_gap.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

namespace
{
    string CanonicalizeString(const CTempString& sValue)
    {
        string newString;
        newString.reserve(sValue.length());

        ITERATE_0_IDX(ii, sValue.length()) {
            const char ch = sValue[ii];
            if (isupper(ch)) {
                newString.push_back((unsigned char)tolower(ch));
            }
            else if (ch == ' ' || ch == '_') {
                newString.push_back('-');
            }
            else {
                newString.push_back(ch);
            }
        }

        return newString;
    }
}

// destructor
CSeq_gap::~CSeq_gap(void)
{
}


void CSeq_gap::ChangeType(TType linkage_type)
{
    SetType(linkage_type);
    if (linkage_type == CSeq_gap::eType_scaffold) {
        SetLinkage(eLinkage_linked);
        if (!IsSetLinkage_evidence() || GetLinkage_evidence().empty()) {
            AddLinkageEvidence(CLinkage_evidence::eType_unspecified);
        }
    } else if (linkage_type == CSeq_gap::eType_repeat) {
        if (IsSetLinkage() && GetLinkage() == eLinkage_linked) {
            if (!IsSetLinkage_evidence() || GetLinkage_evidence().empty()) {
                AddLinkageEvidence(CLinkage_evidence::eType_unspecified);
            }
        } else if (IsSetLinkage_evidence() && !GetLinkage_evidence().empty()) {
            SetLinkage(eLinkage_linked);
        } else {
            SetLinkage(eLinkage_unlinked);
            ResetLinkage_evidence();
        }
    } else {
        ResetLinkage();
        ResetLinkage_evidence();
    }
}


void CSeq_gap::SetLinkageTypeScaffold(CLinkage_evidence::TType evidence_type)
{
    SetType(eType_scaffold);
    SetLinkage(eLinkage_linked);
    ResetLinkage_evidence();
    AddLinkageEvidence(evidence_type);
}


void CSeq_gap::SetLinkageTypeLinkedRepeat(CLinkage_evidence::TType evidence_type)
{
    SetType(eType_repeat);
    SetLinkage(eLinkage_linked);
    ResetLinkage_evidence();
    AddLinkageEvidence(evidence_type);
}


bool CSeq_gap::AddLinkageEvidence(CLinkage_evidence::TType evidence_type)
{
    // only add if type is scaffold or repeat
    if (!IsSetType() || (GetType() != eType_scaffold && GetType() != eType_repeat)) {
        return false;
    }
    bool changed = false;
    if (!IsSetLinkage() || GetLinkage() != eLinkage_linked) {
        SetLinkage(eLinkage_linked);
        changed = true;
    }
    bool found = false;
    if (IsSetLinkage_evidence()) {
        NON_CONST_ITERATE(CSeq_gap::TLinkage_evidence, it, SetLinkage_evidence()) {
            if ((*it)->IsSetType()) {
                if ((*it)->GetType() == evidence_type) {
                    found = true;
                    break;
                } else if ((*it)->GetType() == CLinkage_evidence::eType_unspecified) {
                    (*it)->SetType(evidence_type);
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found) {
        CRef<CLinkage_evidence> ev(new CLinkage_evidence());
        ev->SetType(evidence_type);
        SetLinkage_evidence().push_back(ev);
    }
    return found || changed;
}

const CSeq_gap::SGapTypeInfo *
CSeq_gap::NameToGapTypeInfo(const CTempString & sName)
{
    const CSeq_gap::TGapTypeMap & gapTypeMap =
        GetNameToGapTypeInfoMap();

    TGapTypeMap::const_iterator find_iter =
        gapTypeMap.find(CanonicalizeString(sName).c_str());
    if (find_iter == gapTypeMap.end()) {
        // not found
        return NULL;
    }
    else {
        return &find_iter->second;
    }
}

// static
const CSeq_gap::TGapTypeMap &
CSeq_gap::GetNameToGapTypeInfoMap(void)
{
    // gap-type name to info
    typedef SStaticPair<const char*, CSeq_gap::SGapTypeInfo> TGapTypeElem;
    static const TGapTypeElem sc_gap_type_map[] = {
        { "between-scaffolds", { CSeq_gap::eType_contig, eLinkEvid_Required } },
        { "centromere", { CSeq_gap::eType_centromere, eLinkEvid_Forbidden } },
        { "contamination", { eType_contamination, eLinkEvid_Required } },
        { "heterochromatin", { CSeq_gap::eType_heterochromatin, eLinkEvid_Forbidden } },
        { "repeat-between-scaffolds", { CSeq_gap::eType_repeat, eLinkEvid_Forbidden } },
        { "repeat-within-scaffold", { CSeq_gap::eType_repeat, eLinkEvid_Required } },
        { "short-arm", { CSeq_gap::eType_short_arm, eLinkEvid_Forbidden } },
        { "telomere", { CSeq_gap::eType_telomere, eLinkEvid_Forbidden } },
        { "unknown", { CSeq_gap::eType_unknown, eLinkEvid_UnspecifiedOnly } },
        { "within-scaffold", { CSeq_gap::eType_scaffold, eLinkEvid_Forbidden } },
    };
    DEFINE_STATIC_ARRAY_MAP(TGapTypeMap, sc_GapTypeMap, sc_gap_type_map);
    return sc_GapTypeMap;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1710, CRC32: f3693122 */
