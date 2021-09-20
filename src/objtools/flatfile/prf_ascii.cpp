/* prf_ascii.c
 *
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
 * File Name:  prf_ascii.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Build PRF format entry block.
 *      Parse PRF image in memory to asn.
 */

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seq/Seq_inst.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/PRF_ExtraSrc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/biblio/Cit_art.hpp>


#include "index.h"
#include "prf_index.h"

#include <objtools/flatfile/flatdefn.h>
#include "ftanet.h"
#include <objtools/flatfile/flatfile_parser.hpp>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "utilref.h"
#include "add.h"
#include "utilfun.h"
#include "entry.h"
#include "ref.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "prf_ascii.cpp"

BEGIN_NCBI_SCOPE

static const char *prf_genomes[] = {
    "chloroplast",                      /* 0 -> 2 */
    "chromoplast",                      /* 1 -> 3 */
    "mitochondria",                     /* 2 -> 5 */
    "plastid",                          /* 3 -> 6 */
    "cyanelle",                         /* 4 -> 12 */
    "apicoplast",                       /* 5 -> 16 */
    "leucoplast",                       /* 6 -> 17 */
    "proplastid",                       /* 7 -> 18 */
    NULL
};

/**********************************************************/
static void GetPrfSubBlock(ParserPtr pp, DataBlkPtr entry)
{
    DataBlkPtr dbp;

    dbp = TrackNodeType(entry, ParFlatPRF_NAME);
    if(dbp != NULL)
    {
        BuildSubBlock(dbp, ParFlatPRF_subunit, " subunit");
        BuildSubBlock(dbp, ParFlatPRF_isotype, " isotype");
        BuildSubBlock(dbp, ParFlatPRF_class, " class");
        BuildSubBlock(dbp, ParFlatPRF_gene_name, " gene_name");
        BuildSubBlock(dbp, ParFlatPRF_linkage, " linkage");
        BuildSubBlock(dbp, ParFlatPRF_determine, " determine");
        GetLenSubNode(dbp);
    }

    dbp = TrackNodeType(entry, ParFlatPRF_SOURCE);
    if(dbp != NULL)
    {
        BuildSubBlock(dbp, ParFlatPRF_strain, " strain");
        BuildSubBlock(dbp, ParFlatPRF_org_part, " org_part");
        BuildSubBlock(dbp, ParFlatPRF_org_state, " org_state");
        BuildSubBlock(dbp, ParFlatPRF_host, " host");
        BuildSubBlock(dbp, ParFlatPRF_cname, " cname");
        BuildSubBlock(dbp, ParFlatPRF_taxon, " taxon");
        GetLenSubNode(dbp);
    }

    dbp = TrackNodeType(entry, ParFlatPRF_JOURNAL);
    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlatPRF_JOURNAL)
            continue;
        BuildSubBlock(dbp, ParFlatPRF_AUTHOR, "AUTHOR");
        BuildSubBlock(dbp, ParFlatPRF_TITLE, "TITLE");
        GetLenSubNode(dbp);
    }
}

/**********************************************************/
static CRef<objects::CBioseq> PrfCreateBioseq(IndexblkPtr ibp)
{
    CRef<objects::CBioseq> bioseq(new objects::CBioseq);

    bioseq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    bioseq->SetInst().SetMol(objects::CSeq_inst::eMol_aa);
    bioseq->SetInst().SetTopology(objects::CSeq_inst::eTopology_linear);

    CRef<objects::CSeq_id> id(new objects::CSeq_id);
    objects::CTextseq_id& text_id = id->SetPrf();
    text_id.SetName(ibp->acnum);

    bioseq->SetId().push_back(id);

    return bioseq;
}

/**********************************************************/
static CRef<objects::CPRF_block> PrfGetPrfBlock(DataBlkPtr entry)
{
    DataBlkPtr   dbp;

    char*      keywords;
    char*      p;
    char*      q;
    char*      r;
    Char         ch;
    size_t i;

    CRef<objects::CPRF_block> prf_block;
    TKeywordList kwds;

    dbp = TrackNodeType(entry, ParFlatPRF_KEYWORD);
    if(dbp != NULL && dbp->mOffset != NULL && dbp->len > ParFlat_COL_DATA_PRF)
    {
        ch = dbp->mOffset[dbp->len];
        dbp->mOffset[dbp->len] = '\0';
        keywords = StringSave(dbp->mOffset + ParFlat_COL_DATA_PRF);
        dbp->mOffset[dbp->len] = ch;
        for(p = keywords; *p != '\0'; p++)
            if(*p == '\n')
                *p = ' ';
        for(p = keywords; *p == ' ';)
            p++;
        for(q = p;;)
        {
            if(*p == ' ')
            {
                for(r = p, i = 0; *p == ' '; i++)
                    p++;
                if(*p == '\0' || i > 3)
                {
                    ch = *r;
                    *r = '\0';
                    kwds.push_back(q);
                    if(*p == '\0')
                        break;
                    *r = ch;
                    q = p;
                }
            }
            else if(*p == '\0')
            {
                kwds.push_back(q);
                break;
            }
            else
                p++;
        }

        if (!kwds.empty())
        {
            prf_block.Reset(new objects::CPRF_block);
            prf_block->SetKeywords().swap(kwds);
        }
    }

    dbp = TrackNodeType(entry, ParFlatPRF_SOURCE);
    if(dbp == NULL || dbp->mpData == NULL)
        return prf_block;

    if (prf_block.Empty())
        prf_block.Reset(new objects::CPRF_block);

    objects::CPRF_ExtraSrc& extra_src = prf_block->SetExtra_src();

    for(dbp = (DataBlkPtr) dbp->mpData; dbp != NULL; dbp = dbp->mpNext)
    {
        p = dbp->mOffset;
        i = dbp->len;

        if(dbp->mType == ParFlatPRF_strain)
            extra_src.SetStrain(GetBlkDataReplaceNewLine(p, p + i, ParFlat_COL_DATA_PRF));

        else if(dbp->mType == ParFlatPRF_org_part)
            extra_src.SetPart(GetBlkDataReplaceNewLine(p, p + i, ParFlat_COL_DATA_PRF));

        else if(dbp->mType == ParFlatPRF_org_state)
            extra_src.SetState(GetBlkDataReplaceNewLine(p, p + i, ParFlat_COL_DATA_PRF));

        else if(dbp->mType == ParFlatPRF_host)
            extra_src.SetHost(GetBlkDataReplaceNewLine(p, p + i, ParFlat_COL_DATA_PRF));

        else if(dbp->mType == ParFlatPRF_taxon)
            extra_src.SetTaxon(GetBlkDataReplaceNewLine(p, p + i, ParFlat_COL_DATA_PRF));
    }

    if (!extra_src.IsSetStrain() && !extra_src.IsSetPart() && !extra_src.IsSetState() &&
        !extra_src.IsSetHost() && !extra_src.IsSetTaxon())
    {
        prf_block->ResetExtra_src();
        if (!prf_block->IsSetKeywords())
            prf_block.Reset();
    }

    return prf_block;
}

/**********************************************************/
static char* PrfGetStringValue(DataBlkPtr entry, Int2 type)
{
    DataBlkPtr dbp;

    dbp = TrackNodeType(entry, type);
    if(dbp == NULL || dbp->mOffset == NULL || dbp->len <= ParFlat_COL_DATA_PRF)
        return(NULL);

    return(GetBlkDataReplaceNewLine(dbp->mOffset, dbp->mOffset + dbp->len,
                                    ParFlat_COL_DATA_PRF));
}

/**********************************************************/
static char* PrfGetSubStringValue(DataBlkPtr entry, Int2 type, Int2 subtype)
{
    DataBlkPtr dbp;
    char*    res;

    dbp = TrackNodeType(entry, type);
    if(dbp == NULL || dbp->mpData == NULL)
        return(NULL);

    for(res = NULL, dbp = (DataBlkPtr) dbp->mpData; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != subtype)
            continue;

        res = GetBlkDataReplaceNewLine(dbp->mOffset, dbp->mOffset + dbp->len,
                                       ParFlat_COL_DATA_PRF);
        break;
    }
    return(res);
}

/**********************************************************/
static CRef<objects::CBioSource> PrfGetBioSource(ParserPtr pp, DataBlkPtr entry)
{
    CRef<objects::CBioSource> bio_src;

    DataBlkPtr   dbp;
    const char   **b;

    char*      taxname;
    char*      lineage;
    char*      org_part;
    Int4         i;
    Int4         gen;
    Int4         count;

    dbp = TrackNodeType(entry, ParFlatPRF_SOURCE);
    if(dbp == NULL || dbp->mOffset == NULL || dbp->len <= ParFlat_COL_DATA_PRF)
        return bio_src;

    taxname = GetBlkDataReplaceNewLine(dbp->mOffset, dbp->mOffset + dbp->len,
                                       ParFlat_COL_DATA_PRF);
    if(taxname == NULL)
        return bio_src;

    lineage = NULL;
    org_part = NULL;
    gen = 1;
    if(dbp->mpData != NULL)
    {
        for(dbp = (DataBlkPtr) dbp->mpData; dbp != NULL; dbp = dbp->mpNext)
        {
            if(dbp->mType == ParFlatPRF_taxon && lineage == NULL)
            {
                lineage = GetBlkDataReplaceNewLine(dbp->mOffset,
                                                   dbp->mOffset + dbp->len,
                                                   ParFlat_COL_DATA_PRF);
            }
            else if(dbp->mType == ParFlatPRF_org_part && org_part == NULL)
            {
                org_part = GetBlkDataReplaceNewLine(dbp->mOffset,
                                                    dbp->mOffset + dbp->len,
                                                    ParFlat_COL_DATA_PRF);
                if(org_part == NULL)
                    continue;

                for(b = prf_genomes, count = 0, i = 0; *b != NULL; b++, i++)
                {
                    if(StringIStr(org_part, *b) != NULL)
                    {
                        count++;
                        gen = i;
                    }
                }
                if(count > 1)
                {
                    gen = 0;
                    ErrPostEx(SEV_ERROR, ERR_SOURCE_ConflictingGenomes,
                              "Multiple types of genomes are indicated for this record; using \"unknown\" instead.");
                }
                else if(count == 1)
                {
                    if(gen == 0)
                        gen = 2;        /* chloroplast */
                    else if(gen == 1)
                        gen = 3;        /* chromoplast */
                    else if(gen == 2)
                        gen = 5;        /* mitochondrion */
                    else if(gen == 3)
                        gen = 6;        /* plastid */
                    else if(gen == 4)
                        gen = 12;       /* cyanelle */
                    else if(gen == 5)
                        gen = 16;       /* apicoplast */
                    else if(gen == 6)
                        gen = 17;       /* leucoplast */
                    else if(gen == 7)
                        gen = 18;       /* proplastid */
                    else
                        gen = 1;        /* genomic */
                }
            }
            if(lineage != NULL && org_part != NULL)
                break;
        }
    }

    if(org_part != NULL)
        MemFree(org_part);

    bio_src.Reset(new objects::CBioSource);

    objects::COrg_ref& org_ref = bio_src->SetOrg();
    org_ref.SetTaxname(taxname);
    MemFree(taxname);

    if(lineage != NULL)
    {
        org_ref.SetOrgname().SetLineage(lineage);
        MemFree(lineage);
    }

    bio_src->SetGenome(gen);
    fta_fix_orgref(pp, org_ref, &pp->entrylist[pp->curindx]->drop, NULL);

    fta_sort_biosource(*bio_src);

    return bio_src;
}

/**********************************************************/
static CRef<objects::CPubdesc> PrfGetPub(ParserPtr pp, DataBlkPtr entry)
{
    DataBlkPtr  dbp;

    char*     title;
    char*     author;
    char*     jour;

    CRef<objects::CPubdesc> ret;

    if(entry == NULL || entry->mOffset == NULL ||
       entry->len <= ParFlat_COL_DATA_PRF)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                  "JOURNAL line is missing or empty, reference dropped.");
        return ret;
    }

    jour = GetBlkDataReplaceNewLine(entry->mOffset, entry->mOffset + entry->len,
                                    ParFlat_COL_DATA_PRF);
    if(jour == NULL || *jour == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                  "JOURNAL line is missing or empty, reference dropped.");
        if(jour != NULL)
            MemFree(jour);

        return ret;
    }

    title = NULL;
    author = NULL;
    CRef<objects::CTitle::C_E> title_art(new objects::CTitle::C_E);
    CRef<objects::CAuth_list> auth_list;

    for(dbp = (DataBlkPtr) entry->mpData; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType == ParFlatPRF_TITLE)
        {
            title = GetBlkDataReplaceNewLine(dbp->mOffset,
                                             dbp->mOffset + dbp->len,
                                             ParFlat_COL_DATA_PRF);
            if (title != NULL && *title != '\0')
                title_art->SetName(title);
        }
        else if(dbp->mType == ParFlatPRF_AUTHOR)
        {
            author = GetBlkDataReplaceNewLine(dbp->mOffset,
                                              dbp->mOffset + dbp->len,
                                              ParFlat_COL_DATA_PRF);
            if(author != NULL)
            {
                get_auth(author, GB_REF, NULL, auth_list);
                if (auth_list.Empty())
                {
                    MemFree(author);
                    author = NULL;
                }
            }
        }
    }

    if(title == NULL || *title == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                  "TITLE line is missing or empty, reference dropped.");
        MemFree(jour);
        if(title != NULL)
            MemFree(title);
        if(author != NULL)
            MemFree(author);

        return ret;
    }

    MemFree(title);
    if (author == NULL || *author == '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                  "AUTHOR line is missing or incorrect, reference dropped.");
        MemFree(jour);
        if(author != NULL)
            MemFree(author);

        return ret;
    }

    MemFree(author);

    CRef<objects::CCit_art> empty_cit_art;
    CRef<objects::CPub> pub_ref = journal(pp, jour, jour + StringLen(jour), auth_list, title_art, false, empty_cit_art, 0);
    if (pub_ref.Empty())
    {
        MemFree(jour);
        return ret;
    }
    MemFree(jour);

    ret.Reset(new objects::CPubdesc);
    ret->SetPub().Set().push_back(pub_ref);

    return ret;
}

/**********************************************************/
static CRef<objects::CMolInfo> PrfGetMolInfo(DataBlkPtr entry, char* comm)
{
    CRef<objects::CMolInfo> mol_info(new objects::CMolInfo);

    char*    p;
    char*    q;
    bool       left;
    bool       right;

    mol_info->SetBiomol(objects::CMolInfo::eBiomol_peptide);
    mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_complete);

    p = PrfGetSubStringValue(entry, ParFlatPRF_NAME, ParFlatPRF_determine);
    if(p != NULL)
    {
        if(NStr::CompareNocase(p, "protein") == 0)
            mol_info->SetTech(objects::CMolInfo::eTech_seq_pept);
        else if(NStr::CompareNocase(p, "gene") == 0 || NStr::CompareNocase(p, "mRNA") == 0)
            mol_info->SetTech(objects::CMolInfo::eTech_concept_trans);
        else
            ErrPostEx(SEV_ERROR, ERR_FORMAT_UnknownDetermineField,
                      "The \"determine\" subfield of NAME contains a value other than protein, mRNA or gene.");

        MemFree(p);
    }

    if(comm == NULL)
        return mol_info;

    left = false;
    right = false;
    for(p = comm, q = p;;)
    {
        p = StringChr(q, ';');
        if(p != NULL)
           *p = '\0';
        if(NStr::CompareNocase(q, "N=incomplete") == 0)
            left = true;
        else if(NStr::CompareNocase(q, "C=incomplete") == 0)
            right = true;
        else
        {
            if(p == NULL)
                break;
            *p = ';';
            q = p + 1;
            continue;
        }
        if(p == NULL)
        {
            if(q > comm)
               q--;
            *q = '\0';
            break;
        }
        *p++ = ';';
        fta_StringCpy(q, p);
    }

    for(p = comm; *p == ' ' || *p == '\t' || *p == ';';)
        p++;
    if(*p == '\0')
        *comm = '\0';

    if(left && right)
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_no_ends);
    else if(left)
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_no_left);
    else if(right)
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_no_right);

    return mol_info;
}

/**********************************************************/
static void PrfGetDescr(ParserPtr pp, DataBlkPtr entry, TSeqdescList& descrs)
{
    DataBlkPtr   dbp;

    char*      comm;

    comm = PrfGetStringValue(entry, ParFlatPRF_COMMENT);
    CRef<objects::CBioSource> bio_src = PrfGetBioSource(pp, entry);
    CRef<objects::CMolInfo> mol_info = PrfGetMolInfo(entry, comm);

    if(comm != NULL && *comm == '\0')
    {
        MemFree(comm);
        comm = NULL;
    }

    if (comm != NULL)
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetComment(comm);
        descrs.push_back(descr);
    }
    MemFree(comm);

    if (bio_src.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetSource(*bio_src);
        descrs.push_back(descr);
    }

    if (mol_info.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetMolinfo(*mol_info);
        descrs.push_back(descr);
    }

    dbp = TrackNodeType(entry, ParFlatPRF_JOURNAL);
    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlatPRF_JOURNAL)
            continue;

        CRef<objects::CPubdesc> pubdesc = PrfGetPub(pp, dbp);
        if (pubdesc.NotEmpty())
        {
            CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
            descr->SetPub(*pubdesc);
            descrs.push_back(descr);
        }
    }

    CRef<objects::CPRF_block> prf = PrfGetPrfBlock(entry);
    if (prf.NotEmpty())
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetPrf(*prf);
        descrs.push_back(descr);
    }
}

/**********************************************************/
static CRef<objects::CSeq_loc> PrfGetFullSeqInt(objects::CSeq_id& id, Int4 len)
{
    CRef<objects::CSeq_loc> loc(new objects::CSeq_loc);
    objects::CSeq_interval& interval = loc->SetInt();

    interval.SetFrom(0);
    interval.SetTo(len - 1);
    interval.SetId(id);

    return loc;
}

/**********************************************************/
static bool PrfValidateEcNumber(std::string& ec_str)
{
    char* temp;
    char* p;
    char* q;
    char* r;
    bool    good;
    bool    lastdot;
    Char    ch;
    Int4    i;

    if (ec_str.empty())
        return false;

    temp = StringSave(ec_str.c_str());
    for(p = temp, q = temp; *p != '\0'; p++)
        if(*p != ' ')
            *q++ = *p;
    *q = '\0';

    if(StringNICmp(temp, "EC", 2) == 0)
        q = temp + 2;
    else
        q = temp;

    good = true;
    lastdot = false;
    for(p = q, i = 0; (*p >= '0' && *p <= '9') || *p == '.' || *p == '-'; p++)
    {
        if(p == q)
            continue;
        if(*p == '-')
        {
            r = p - 1;
            if(*r != '.' || (p[1] != '\0' && p[1] != '.'))
                good = false;
        }
        else if(*p == '.')
        {
            if(p[1] == '\0')
                lastdot = true;
            r = p - 1;
            if(*r != '-' && (*r < '0' || *r > '9'))
                good = false;
            else if(p[1] != '\0' && p[1] != '-' && (p[1] < '0' || p[1] > '9'))
                good = false;
            else
                i++;
        }
    }

    if(*p != '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_InvalidECNumber,
                  "EC number \"%s\" has been ignored, because it contains a value other than a number or a single dash character.",
                  ec_str.c_str());
        MemFree(temp);
        return false;
    }

    if(!good)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_InvalidECNumber,
                  "Badly formatted EC number \"%s\" has been ignored", ec_str.c_str());
        MemFree(temp);
        return false;
    }

    for(p = q; *p >= '0' && *p <= '9';)
        p++;
    if(p == q || (*p != '\0' && *p != '.'))
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_InvalidECNumber,
                  "EC number \"%s\" has been ignored, because it does not start with a number.",
                  ec_str.c_str());
        MemFree(temp);
        return false;
    }

    ch = *p;
    *p = '\0';
    if(atoi(q) > 6)
        ErrPostEx(SEV_WARNING, ERR_FORMAT_UnusualECNumber,
                  "The first value of EC number \"%s\" exceeds known maximum of 6.",
                  ec_str.c_str());
    *p = ch;

    if(i > 3)
    {
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LongECNumber,
                  "EC number \"%s\" contains more than 4 code numbers.", ec_str.c_str());
        if(lastdot)
            ErrPostEx(SEV_WARNING, ERR_FORMAT_UnusualECNumber,
                      "EC number \"%s\" is not comprised of 4 numeric codes.",
                      ec_str.c_str());
    }

    if(i > 3)
    {
        if(q > temp)
            ec_str = q;
        MemFree(temp);
        return true;
    }

    ec_str = q;
    if(lastdot)
        ec_str += '-';

    for(; i < 3; i++)
        ec_str += ".-";

    MemFree(temp);
    return true;
}

/**********************************************************/
static void PrfParseEcNumber(char* str, objects::CProt_ref::TEc& ecs)
{
    char*    p;
    char*    q;

    if(str == NULL || *str == '\0')
        return;

    for(p = str, q = str; *p != '\0'; p++)
    {
        if(*p != '/' && *p != ';' && *p != ',')
            continue;
        for(*p++ = '\0'; *p == '/' || *p == ';' || *p == ',';)
            p++;
        if(*q != '\0')
            ecs.push_back(q);
        q = p--;
    }
    if(*q != '\0')
        ecs.push_back(q);

    for (objects::CProt_ref::TEc::iterator it = ecs.begin(); it != ecs.end(); )
    {
        if (PrfValidateEcNumber(*it))
        {
            ++it;
            continue;
        }

        it = ecs.erase(it);
    }
}

/**********************************************************/
static void PrfGetProtRefEc(DataBlkPtr entry, objects::CProt_ref::TEc& ecs)
{
    char*    p;

    p = PrfGetSubStringValue(entry, ParFlatPRF_NAME, ParFlatPRF_class);
    if (p == NULL)
        return;

    PrfParseEcNumber(p, ecs);
    MemFree(p);
}

/**********************************************************/
static CRef<objects::CProt_ref> PrfGetProtRef(DataBlkPtr entry)
{
    CRef<objects::CProt_ref> prot_ref(new objects::CProt_ref);

    char*    p;

    p = PrfGetStringValue(entry, ParFlatPRF_NAME);
    if(p != NULL)
        prot_ref->SetName().push_back(p);

    PrfGetProtRefEc(entry, prot_ref->SetEc());
    if (prot_ref->GetEc().empty())
        prot_ref->ResetEc();

    if (!prot_ref->IsSetName() && !prot_ref->IsSetEc())
        prot_ref.Reset();

    return prot_ref;
}

/**********************************************************/
static CRef<objects::CGene_ref> PrfGetGeneRef(DataBlkPtr entry)
{
    CRef<objects::CGene_ref> gene_ref;

    char* p = PrfGetSubStringValue(entry, ParFlatPRF_NAME, ParFlatPRF_gene_name);
    if(p == NULL)
        return gene_ref;

    gene_ref.Reset(new objects::CGene_ref);
    gene_ref->SetLocus(p);

    return gene_ref;
}

/**********************************************************/
static void PrfGetAnnot(ParserPtr pp, DataBlkPtr entry, objects::CBioseq& bioseq)
{
    CRef<objects::CSeq_loc> loc = PrfGetFullSeqInt(*(*bioseq.SetId().begin()), bioseq.GetLength());
    if (loc.Empty())
        return;

    CRef<objects::CProt_ref> prot_ref = PrfGetProtRef(entry);
    CRef<objects::CGene_ref> gene_ref = PrfGetGeneRef(entry);
    
    if (prot_ref.Empty() && gene_ref.Empty())
        return;

    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot);
    objects::CSeq_annot::C_Data::TFtable& feats = annot->SetData().SetFtable();

    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
    feat->SetLocation(*loc);

    if (prot_ref.NotEmpty())
    {
        feat->SetData().SetProt(*prot_ref);
        feats.push_back(feat);

        if (gene_ref.NotEmpty())
        {
            feat.Reset(new objects::CSeq_feat);
            feat->SetLocation(*loc);
            feat->SetData().SetGene(*gene_ref);

            feats.push_back(feat);
        }
    }
    else
    {
        feat->SetData().SetGene(*gene_ref);
        feats.push_back(feat);
    }

    bioseq.SetAnnot().push_back(annot);
}

/**********************************************************/
static void PrfCreateDescrTitle(objects::CBioseq& bioseq, DataBlkPtr entry)
{
    char*      p;

    p = PrfGetStringValue(entry, ParFlatPRF_NAME);
    if(p == NULL)
        return;

    std::string taxname;
    ITERATE(objects::CSeq_descr::Tdata, desc, bioseq.GetDescr().Get())
    {
        if (!(*desc)->IsSource())
            continue;

        const objects::CBioSource& bio_src = (*desc)->GetSource();
        if (bio_src.IsSetOrg())
        {
            taxname = bio_src.GetOrg().IsSetTaxname() ? bio_src.GetOrg().GetTaxname() : "";
            break;
        }
    }

    std::string defline;
    if (taxname.empty())
        defline = p;
    else
    {
        defline = p;
        defline += " [";
        defline += taxname;
        defline += "]";
    }
    MemFree(p);

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetTitle(defline);
    bioseq.SetDescr().Set().push_back(descr);
}

/**********************************************************/
static CRef<objects::CSeq_entry> PrfPrepareEntry(ParserPtr pp, DataBlkPtr entry, unsigned char* protconv,
                                                             IndexblkPtr ibp)
{
    Int2        curkw;
    char*     ptr;
    char*     eptr;
    EntryBlkPtr ebp;

    ebp = (EntryBlkPtr) entry->mpData;
    ptr = entry->mOffset;
    eptr = ptr + entry->len;
    curkw = ParFlatPRF_ref;
    while(curkw != ParFlatPRF_END)
    {
        ptr = GetPrfBlock(&ebp->chain, ptr, &curkw, eptr);
    }
    GetPrfSubBlock(pp, entry);

    CRef<objects::CBioseq> bioseq = PrfCreateBioseq(ibp);
    GetScope().AddBioseq(*bioseq);

    CRef<objects::CSeq_entry> seq_entry(new objects::CSeq_entry);
    seq_entry->SetSeq(*bioseq);

    GetSeqData(pp, entry, *bioseq, ParFlatPRF_SEQUENCE, protconv, objects::CSeq_data::e_Iupacaa);

    PrfGetDescr(pp, entry, bioseq->SetDescr().Set());
    PrfGetAnnot(pp, entry, *bioseq);

    PrfCreateDescrTitle(*bioseq, entry);

    TEntryList entries;
    entries.push_back(seq_entry);
    fta_find_pub_explore(pp, entries);

    PackEntries(entries);

    return seq_entry;
}

/**********************************************************
 *
 *   bool PrfAscii(pp):
 *
 *      Return FALSE if allocate entry block failed.
 *
 **********************************************************/
bool PrfAscii(ParserPtr pp)
{
    DataBlkPtr  entry;

    Int4        total;
    Int4        i;
    IndexblkPtr ibp;
    Int4        imax;

    auto protconv = GetProteinConv();

    for(total = 0, i = 0, imax = pp->indx; i < imax; i++)
    {
        pp->curindx = i;
        ibp = pp->entrylist[i];

        err_install(ibp, pp->accver);

        if(ibp->drop != 1)
        {
            entry = LoadEntry(pp, ibp->offset, ibp->len);
            if(entry == NULL)
            {
                FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
                return false;
            }

            CRef<objects::CSeq_entry> cur_entry = PrfPrepareEntry(pp, entry, protconv.get(), ibp);
            if (ibp->drop != 1 && cur_entry.NotEmpty())
            {
                pp->entries.push_back(cur_entry);
                cur_entry.Reset();
            }

            FreeEntry(entry);
        }
        if(ibp->drop != 1)
        {
            total++;
            ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                      "OK - entry \"%s|%s\" parsed successfully.",
                      ibp->locusname, ibp->acnum);
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry \"%s|%s\" skipped.",
                      ibp->locusname, ibp->acnum);
        }
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete,
              "Parsing completed, %d entr%s parsed.",
              total, (total == 1) ? "y" : "ies");
    return true;
}

END_NCBI_SCOPE
// LCOV_EXCL_STOP
