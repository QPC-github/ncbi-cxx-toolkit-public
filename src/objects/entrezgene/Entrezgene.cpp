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
 *   'entrezgene.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/entrezgene/Entrezgene.hpp>

#include <objects/entrezgene/Gene_track.hpp>
#include <objects/entrezgene/Gene_commentary.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <serial/enumvalues.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CEntrezgene::~CEntrezgene(void)
{
}

// Locate or create the nomenclature element.  Ensure that the gene symbol is
// always filled in, even if it is not present in the structured elements and
// has to be derived.

CRef<objects::CGene_nomenclature> CEntrezgene::GetNomenclature() const
{
    CRef<CGene_nomenclature> nomen(new CGene_nomenclature);
    nomen->SetStatus(CGene_nomenclature::eStatus_unknown);

    const CGene_ref& gene_ref = GetGene();

    // First choice: the formal-name element.

    if (gene_ref.IsSetFormal_name()) {
        nomen->Assign(gene_ref.GetFormal_name());
    }

    // Second choice: look in the properties

    if ( ! nomen->IsSetSymbol() ) {

        for ( const auto& root_properties : GetProperties() ) {

            if ((root_properties->GetType() == CGene_commentary::eType_comment) &&
                 root_properties->IsSetLabel() && (root_properties->GetLabel() == "Nomenclature" )) {

                // found it!

                if (root_properties->IsSetProperties()) {

                    // Look for the symbol and full name.

                    for ( const auto& nomen_properties : root_properties->GetProperties() ) {

                        if ((nomen_properties->GetType() == CGene_commentary::eType_property) &&
                             nomen_properties->IsSetLabel() && nomen_properties->IsSetText()) {

                            const string& label(nomen_properties->GetLabel());
                            const string& text(nomen_properties->GetText());

                            if (text.empty()) {
                                continue;
                            }

                            if (label == "Official Symbol") {

                                nomen->SetSymbol(text);
                                nomen->SetStatus(CGene_nomenclature::eStatus_official);

                            } else if (label == "Official Full Name") {

                                nomen->SetName(text);
                                nomen->SetStatus(CGene_nomenclature::eStatus_official);

                            } else if (label == "Interim Symbol") {

                                nomen->SetSymbol(text);
                                nomen->SetStatus(CGene_nomenclature::eStatus_interim);

                            } else if (label == "Interim Full Name") {

                                nomen->SetName(text);
                                nomen->SetStatus(CGene_nomenclature::eStatus_interim);
                            }
                        }

                    } // for each nomenclature property element
                }

                break; // we found it
            }

        } // for each property element
    }

    // Ensure that the symbol is filled in.

    if ( ! nomen->IsSetSymbol() ) {

        // Third choice: get from EG.locus

        if (gene_ref.IsSetLocus()) {
            nomen->SetSymbol(gene_ref.GetLocus());
        }

        // Fourth choice: get from EG.locus_tag

        else
        if (gene_ref.IsSetLocus_tag()) {
            nomen->SetSymbol(gene_ref.GetLocus_tag());
        }

        // Fifth choice: get from EG.gene.syn[0]

        else
        if (gene_ref.IsSetSyn() && ( ! gene_ref.GetSyn().empty() ) ) {
            nomen->SetSymbol(*(gene_ref.GetSyn().begin()));
        }

        // Last choice: construct it as "LOC" + geneid

        else
        {
            nomen->SetSymbol( "LOC" + NStr::IntToString(GetTrack_info().GetGeneid()) );
        }
    }

    return nomen;
}

// Produce a standard description of the gene.

string CEntrezgene::GetDescription() const 
{
    string desc;

    // Description:
    //
    //     first of:
    //         Eg.gene.desc or
    //         Eg.prot.desc or
    //         first Eg.prot.name or
    //         Eg.rna.ext.name or
    //         Eg.type

    if (GetGene().IsSetDesc()) {
        desc = GetGene().GetDesc();
    }
    else if (IsSetProt() && GetProt().IsSetDesc()) {
        desc = GetProt().GetDesc();
    }
    else if (IsSetProt() && GetProt().IsSetName() && ( ! GetProt().GetName().empty() ) ) {
        desc = *(GetProt().GetName().begin());
    }
    else if (IsSetRna() && GetRna().IsSetExt() && GetRna().GetExt().IsName()) {
        desc = GetRna().GetExt().GetName();
    }
    else {
        const bool allowBadValue = true;
        desc = GetTypeInfo_enum_EType()->FindName(GetType(), allowBadValue);
    }

    if (desc == "other") {
        desc.clear();
    }

    return desc;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 140737443291791, chars: 1733, CRC32: c6c80683 */
