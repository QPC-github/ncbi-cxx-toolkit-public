/*  $Id$
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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for structured comments for sequences
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <util/line_reader.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>

#include "struc_cmt_reader.hpp"
#include "table2asn_context.hpp"
#include "visitors.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CTable2AsnStructuredCommentsReader::CTable2AsnStructuredCommentsReader(const std::string& filename, ILineErrorListener* logger)
    : CStructuredCommentsReader(logger)
{
    CRef<ILineReader> reader{ILineReader::New(filename)};
    m_vertical = IsVertical(*reader);
    if (m_vertical) {
        m_comments.push_back({});
        LoadCommentsByRow(*reader, m_comments.front());
        _CheckStructuredCommentsSuffix(m_comments.front());
    } else {
        LoadComments(*reader, m_comments, CSeq_id::fParse_AnyLocal);
        for (CStructComment& comment : m_comments) {
            _CheckStructuredCommentsSuffix(comment);
        }
    }
}

CTable2AsnStructuredCommentsReader::~CTable2AsnStructuredCommentsReader()
{
}

void CTable2AsnStructuredCommentsReader::ProcessComments(CSeq_entry& entry) const
{
    if (m_vertical) {
        VisitAllSeqDesc(entry, true, [this](CBioseq* bioseq, CSeq_descr& descr)
        {
            if (bioseq && !bioseq->IsNa())
                return;

            _AddStructuredComments(descr, m_comments.front());
        });
    } else {
        for (const CStructComment& comment : m_comments) {
            _AddStructuredComments(entry, comment);
        }
    }
}

void CTable2AsnStructuredCommentsReader::_AddStructuredComments(CSeq_descr& descr, const CStructComment& comments)
{
    for (const auto& new_desc : comments.m_descs)
    {
        bool append_desc = true;

        const string& index = CStructComment::GetPrefix(*new_desc);
        //if (index.empty()) continue;

        for (auto& desc : descr.Set()) // push to create setdescr
        {
            if (!desc->IsUser()) continue;

            auto& user = desc->SetUser();

            const string& other = CStructComment::GetPrefix(*desc);
            //if (other.empty()) continue;

            if (NStr::Equal(other, index))
            {
                append_desc = false;
                // Merge
                for (const auto& field : new_desc->GetUser().GetData())
                {
                    user.SetFieldRef(field->GetLabel().GetStr())->SetValue(field->GetData().GetStr());
                }
            }
        }
        if (append_desc)
        {
            CRef<CSeqdesc> add_desc(new CSeqdesc);
            add_desc->Assign(*new_desc);
            descr.Set().push_back(add_desc);
        }
    }
}

bool sSeqIdMatchesCommentId(const CSeq_id& seqID, const CSeq_id& commentID)
{
    // idea: try match the raw text of the commentID with the "money" part of the given ID
 
    if (seqID.Compare(commentID) == CSeq_id::e_YES) {
        return true;
    }
    if (!commentID.IsLocal()) {
        return false;
    }

    const auto& commentIdText = commentID.GetLocal().GetStr();
    const CTextseq_id* pTsid = seqID.GetTextseq_Id();
    if (pTsid) {
        if (pTsid->IsSetAccession()) {
            return (pTsid->GetAccession() == commentIdText);
        }
        if (pTsid->IsSetName()) {
            return (pTsid->GetName() == commentIdText);
        }
        return false;
    }

    string seqIdText;
    switch (seqID.Which()) {
        default:
            return false;
        case CSeq_id::e_Gibbsq:
            seqIdText = NStr::IntToString(seqID.GetGibbsq());
            break;
        case CSeq_id::e_Gibbmt:
            seqIdText = NStr::IntToString(seqID.GetGibbmt());
            break;
        case CSeq_id::e_Giim:
            seqIdText = NStr::IntToString(seqID.GetGiim().GetId());
            break;
        case CSeq_id::e_General: {
            const auto& general = seqID.GetGeneral();
            if (general.IsSetTag()) {
                if (general.GetTag().IsStr()) {
                    seqIdText = general.GetTag().GetStr();
                }
                else {
                    seqIdText = NStr::IntToString(general.GetTag().GetId());
                }
            }
            break;
        }
        case CSeq_id::e_Patent: {
            const CId_pat& idp = seqID.GetPatent().GetCit();
            seqIdText = idp.GetId().IsNumber() ?
                idp.GetId().GetNumber() : idp.GetId().GetApp_number();
            seqIdText += '_';
            seqIdText += NStr::IntToString(seqID.GetPatent().GetSeqid());
            break;
        }
        case CSeq_id::e_Gi:
            seqIdText = NStr::NumericToString(seqID.GetGi());
            break;
        case CSeq_id::e_Pdb: {
            const CPDB_seq_id& pid = seqID.GetPdb();
            seqIdText = pid.GetMol().Get();
            if (pid.IsSetChain_id()) {
                seqIdText += '_';
                seqIdText += pid.GetChain_id();
            }
            else if (pid.IsSetChain()) {
                unsigned char chain = static_cast<unsigned char>(pid.GetChain());
                if (chain > ' ') {
                    seqIdText += '_';
                    seqIdText += static_cast<char>(chain);
                }
            }
            break;
        }
    }
    return (seqIdText == commentIdText);
}



void CTable2AsnStructuredCommentsReader::_AddStructuredComments(CSeq_entry& entry, const CStructComment& comments)
{
    VisitAllBioseqs(entry, [comments](CBioseq& bioseq)
    {
        if (!bioseq.IsNa())
            return;

        if (comments.m_id.NotEmpty())
        {
            bool matched = false;
            for (const auto& id : bioseq.GetId())
            {
                if (sSeqIdMatchesCommentId(*id, *comments.m_id))
                {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                return;
        }

        _AddStructuredComments(bioseq.SetDescr(), comments);
    });
}

void CTable2AsnStructuredCommentsReader::_CheckStructuredCommentsSuffix(CStructComment& comments)
{
    // assumption: all descriptors are structural comments
    for (auto& desc : comments.m_descs) {
        string prefix, suffix;
        const auto& user = desc->GetUser();
        for (const auto& data : user.GetData()) {
            if (data->IsSetLabel() && data->GetLabel().IsStr()) {
                const string& label = data->GetLabel().GetStr();
                if (label == "StructuredCommentPrefix") {
                    prefix = data->GetData().GetStr();
                }
                else if (label == "StructuredCommentSuffix") {
                    suffix = data->GetData().GetStr();
                }
            }
        }
        if (!prefix.empty() && suffix.empty()) {
            desc->SetUser().AddField("StructuredCommentSuffix", prefix);
        }
    }
}

bool CTable2AsnStructuredCommentsReader::IsVertical(ILineReader& reader)
{
    CTempString line;
    reader.ReadLine();
    bool vert = false;
    if (!reader.AtEOF())
    {
        line = reader.GetCurrentLine();
        vector<CTempString> values;
        NStr::Split(line, "\t", values);
        vert = values.size()<=2;
        reader.UngetLine();
    }
    return vert;
}

END_NCBI_SCOPE
