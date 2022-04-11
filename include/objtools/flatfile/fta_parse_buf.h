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
 * File Name:  fta_parse_buf.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------

 */

#ifndef FTA_PARSE_BUF_H
#define FTA_PARSE_BUF_H

#include <list>
#include <objects/seqset/Seq_entry.hpp>
BEGIN_NCBI_SCOPE
// some forward declarations
//struct parser_vals;
//typedef struct parser_vals Parser;

struct Parser;

typedef std::list<CRef<objects::CSeq_entry>> TEntryList;

TEntryList& fta_parse_buf(Parser& pp, const char* buf);
bool        fta_set_format_source(Parser& pp, const std::string& format, const std::string& source);

END_NCBI_SCOPE

#endif // FTA_PARSE_BUF_H
