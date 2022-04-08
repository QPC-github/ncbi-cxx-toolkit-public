/* $Id $
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
 * File Name: fta_src.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *
 */

#ifndef FTA_SRC_H
#define FTA_SRC_H
BEGIN_NCBI_SCOPE

void ParseSourceFeat(ParserPtr pp, DataBlkPtr dbp, TSeqIdList& seqids,
                                Int2 type, objects::CBioseq& bioseq, TSeqFeatList& seq_feats);
bool fta_if_special_org(const char* name);

END_NCBI_SCOPE

#endif // FTA_SRC_H
