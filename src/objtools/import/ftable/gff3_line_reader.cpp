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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seq/so_map.hpp>

#include <objtools/import/import_error.hpp>
#include "gff_util.hpp"

#include "gff3_line_reader.hpp"
#include "gff3_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGff3LineReader::CGff3LineReader(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatLineReader(errorReporter),
    mColumnDelimiter(""),
    mSplitFlags(0)
{
    CSoMap::GetSupportedSoTerms(mValidFeatureTypes);
}

//  ============================================================================
bool
CGff3LineReader::GetNextRecord(
    CStreamLineReader& lineReader,
    CFeatImportData& record)
//  ============================================================================
{
    xReportProgress();

    string nextLine = "";
    while (!lineReader.AtEOF()) {
        nextLine = *(++lineReader);
        ++mLineCount;
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        vector<string> columns;
        xSplitLine(nextLine, columns);
        xInitializeRecord(columns, record);
        ++mRecordCount;
        return true;
    }
    return false;
}

//  ============================================================================
void
CGff3LineReader::xInitializeRecord(
    const vector<string>& columns,
    CFeatImportData& record_)
//  ============================================================================
{
    CImportError errorInvalidScore(
        CImportError::WARNING, 
        "Invalid score value - assuming \".\"",
        LineCount());

    CImportError errorInvalidPhase(
        CImportError::WARNING, 
        "Bad phase value - assuming \".\"", 
        LineCount());

    assert(dynamic_cast<CGff3ImportData*>(&record_));
    CGff3ImportData& record = static_cast<CGff3ImportData&>(record_);

    string seqId;
    TSeqPos seqStart, seqStop;
    ENa_strand seqStrand;
    xInitializeLocation(columns, seqId, seqStart, seqStop, seqStrand);

    string source;
    xInitializeSource(columns, source);

    string featType;
    xInitializeType(columns, featType);

    bool scoreIsValid;
    double score;
    if (!GffUtil::InitializeScore(columns, scoreIsValid, score)) {
        scoreIsValid = false;
        mErrorReporter.ReportError(errorInvalidScore);
    }

    string phase;
    if (!GffUtil::InitializeFrame(columns, phase)) {
        phase = ".";
        mErrorReporter.ReportError(errorInvalidPhase);
    }

    vector<pair<string, string>> attributes;
    xInitializeAttributes(columns, attributes);

    record.Initialize(seqId, source, featType, seqStart, seqStop, 
        scoreIsValid, score, seqStrand, phase, attributes);

}

//  ============================================================================
void
CGff3LineReader::xSplitLine(
    const string& line,
    vector<string>& columns)
//  ============================================================================
{
    CImportError errorInvalidColumnCount(
        CImportError::CRITICAL, "Invalid column count");

    columns.clear();
    NStr::Split(line, "\t", columns, 0);
    if (columns.size() == 9) {
        return;
    }
    if (mColumnDelimiter == " \t"  &&  columns.size() > 9) {
        vector<string> attributes(columns.begin() + 8, columns.end());
        columns[8] = NStr::Join(attributes, " ");
        columns.erase(columns.begin() + 9, columns.end());
        return;
    }
    throw errorInvalidColumnCount;
}

//  ============================================================================
void
CGff3LineReader::xInitializeType(
    const vector<string>& columns,
    string& featType)
//  ============================================================================
{
    featType = columns[2];
}

//  ============================================================================
void
CGff3LineReader::xInitializeLocation(
    const vector<string>& columns,
    string& seqId,
    TSeqPos& seqStart,
    TSeqPos& seqStop,
    ENa_strand& seqStrand)
//  ============================================================================
{
    CImportError errorInvalidSeqStartValue(
        CImportError::ERROR, "Invalid seqStart value",
        LineCount());
    CImportError errorInvalidSeqStopValue(
        CImportError::ERROR, "Invalid seqStop value",
        LineCount());
    CImportError errorInvalidSeqStrandValue(
        CImportError::ERROR, "Invalid seqStrand value",
        LineCount());

    seqId = columns[0];

    try {
        seqStart = NStr::StringToInt(columns[3])-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStartValue;
    }
    try {
        seqStop = NStr::StringToInt(columns[4])-1;
    }
    catch(std::exception&) {
        throw errorInvalidSeqStopValue;
    }

    vector<string> strandLegals = {".", "+", "-"};
    if (find(strandLegals.begin(), strandLegals.end(), columns[6]) ==
            strandLegals.end()) {
        throw errorInvalidSeqStrandValue;
    }
    seqStrand = ((columns[6] == "-") ? eNa_strand_minus : eNa_strand_plus);
}

//  ============================================================================
void
CGff3LineReader::xInitializeSource(
    const vector<string>& columns,
    string& source)
    //  ============================================================================
{
    source = columns[1];
}

//  ============================================================================
void
CGff3LineReader::xInitializeAttributes(
    const vector<string>& columns,
    vector<pair<string, string>>& attributes)
//  ============================================================================
{
    CImportError errorInvalidAttributeFormat(
        CImportError::ERROR, "Invalid attribute formatting", 
        LineCount());

    string attributesStr = columns[8];
    string featType = columns[2];
    NStr::ToLower(featType);

    vector<string> attributesVec;
    xSplitAttributeStringBySemicolons(attributesStr, attributesVec);

    attributes.clear();
    for (auto singleAttr: attributesVec) {
        string key, value;
        if (!NStr::SplitInTwo(singleAttr, "=", key, value)) {
            throw errorInvalidAttributeFormat;
        }
        NStr::TruncateSpacesInPlace(value);
        if (NStr::StartsWith(value, "\"")  &&  NStr::EndsWith(value, "\"")) {
            value = value.substr(1, value.size() -2);
        }
        attributes.push_back(pair<string, string>(key, value));
    }
}

//  ============================================================================
void
CGff3LineReader::xSplitAttributeStringBySemicolons(
    const std::string& attrStr,
    std::vector<std::string>& attrVec)
//  ============================================================================
{
    string strCurrAttrib;
    bool inQuotes = false;

    for (auto curChar: attrStr) {
        if (inQuotes) {
            if (curChar == '\"') {
                inQuotes = false;
            }  
            strCurrAttrib += curChar;
        } else { // not in quotes
            if (curChar == ';') {
                NStr::TruncateSpacesInPlace( strCurrAttrib );
                if(!strCurrAttrib.empty())
                    attrVec.push_back(strCurrAttrib);
                strCurrAttrib.clear();
            } else {
                if(curChar == '\"') {
                    inQuotes = true;
                }
                strCurrAttrib += curChar;
            }
        }
    }

    NStr::TruncateSpacesInPlace( strCurrAttrib );
    if (!strCurrAttrib.empty()) {
        attrVec.push_back(strCurrAttrib);
    }
}

