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
* Author:  Sergiy Gotvyanskyy
* File Description:
*   Utility class for processing ASN.1 files using Huge Files approach
*
*/
#include <ncbi_pch.hpp>
#include "new_gbreleasefile.hpp"

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <corelib/ncbiapp_api.hpp>
#include <chrono>

#include <objtools/edit/huge_file.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/format/flat_file_generator.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(edit);

class CNewGBReleaseFileImpl
{
public:
    CNewGBReleaseFileImpl(CFlatFileGenerator& generator, const string& file_name)
    : m_file{new CHugeFile},
      m_FFGenerator{generator},
      m_reader{new CHugeAsnReader}
    {
        static set<TTypeInfo> supported_types =
        {
            CBioseq_set::GetTypeInfo(),
            CBioseq::GetTypeInfo(),
            CSeq_entry::GetTypeInfo(),
            CSeq_submit::GetTypeInfo(),
        };
        m_file->m_supported_types = &supported_types;

        m_file->Open(file_name);
        m_reader->Open(m_file.get(), nullptr);
    }

    CRef<CSeq_entry> GetNextEntry()
    {
        if (m_current == m_flattened.end())
        {
            m_flattened.clear();
            m_current = m_flattened.end();
            return {};
        }
        else
        {
            return m_reader->LoadSeqEntry(*m_current++);
        }
    }

    void FlattenGenbankSet()
    {
        m_flattened.clear();

        for (auto& rec: m_reader->GetBioseqs())
        {
            auto parent = rec.m_parent_set;
            auto _class = parent->m_class;
            if ((_class == CBioseq_set::eClass_not_set) ||
                (_class == CBioseq_set::eClass_genbank))
            { // create fake bioseq_set
                m_flattened.push_back({rec.m_pos, m_reader->GetBiosets().end(), objects::CBioseq_set::eClass_not_set, {} });
            } else {
                if (m_flattened.empty() || (m_flattened.back().m_pos != parent->m_pos))
                    m_flattened.push_back(*parent);
            }
        }



        if ((m_flattened.size() == 1) && (m_reader->GetBiosets().size()>1))
        {// exposing the whole top entry
            auto top = m_reader->GetBiosets().begin();
            top++;
            if (m_reader->GetSubmit().NotEmpty()
                || (top->m_class != CBioseq_set::eClass_genbank)
                || top->m_descr.NotEmpty())
            {
                m_flattened.clear();
                m_flattened.push_back(*top);
            }
        }
        else if (m_reader->GetSubmit() && m_reader->GetSubmit()->IsSetSub()) {
            m_FFGenerator.SetSubmit(m_reader->GetSubmit()->GetSub());
        }   

        m_current = m_flattened.begin();
    }

private:
    unique_ptr<CHugeFile> m_file;
    CHugeAsnReader::TBioseqSetIndex m_flattened;
    CHugeAsnReader::TBioseqSetIndex::iterator m_current;
    CFlatFileGenerator& m_FFGenerator;
public:
    unique_ptr<CHugeAsnReader> m_reader;
};

CNewGBReleaseFile::CNewGBReleaseFile(CFlatFileGenerator& generator, const string& file_name, bool propagate)
    :m_Impl{new CNewGBReleaseFileImpl(generator, file_name)}
{
}

CNewGBReleaseFile::~CNewGBReleaseFile(void)
{
}

void CNewGBReleaseFile::Read(THandler handler)
{
    size_t i = 0;
    auto& cfg = CNcbiApplicationAPI::Instance()->GetConfig();
    auto thresold = cfg.GetInt("asn2flat", "report_thresold", -1);
    auto s_allowed  = cfg.GetString("asn2flat", "allowed_indices", "");
    std::vector<CTempString> v_allowed;
    std::set<size_t> allowed;
    NStr::Split(s_allowed, ",", v_allowed);
    for (auto& rec: v_allowed)
    {
        size_t v;
        NStr::StringToNumeric(rec, &v);
        allowed.insert(v);
    }

    while (m_Impl->m_reader->GetNextBlob())
    {
        m_Impl->FlattenGenbankSet();
        CRef<objects::CSeq_entry> entry;

        do
        {
            ++i;
            entry.Reset();
            auto started = std::chrono::system_clock::now();
            entry = m_Impl->GetNextEntry();
            if (entry)
            {
                if (allowed.empty() || allowed.find(i) != allowed.end())
                {
                    handler(entry);
                    auto stopped = std::chrono::system_clock::now();

                    auto lapsed = (stopped - started)/1ms;
                    if (thresold >=0 && lapsed >= thresold)
                        std::cerr << i << ":" << lapsed << "ms\n";
                }

            }
        }
        while ( entry );
    }
    std::cerr << "Total seqs:" << i << "\n";
}


END_NCBI_SCOPE
