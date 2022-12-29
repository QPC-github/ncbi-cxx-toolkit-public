/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author: Sema
 *
 * File Description:
 *   Main() of Multipattern Search Code Generator
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/multipattern_search.hpp>

USING_NCBI_SCOPE;


class CMultipatternApp : public CNcbiApplication
{
public:
    CMultipatternApp(void);
    virtual void Init(void);
    virtual int  Run (void);
};


CMultipatternApp::CMultipatternApp(void) {}


void CMultipatternApp::Init(void)
{
    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Multipattern Search Code Generator");
    arg_desc->AddFlag("A", "Generate an array/map data");
    arg_desc->AddFlag("D", "Generate DOT graph");
    arg_desc->AddOptionalKey("i", "InFile", "Input File", CArgDescriptions::eInputFile);
    arg_desc->AddExtra(0, kMax_UInt, "Search Patterns as /regex/ or \"plain string\"", CArgDescriptions::eString);
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}


static const pair<string, CMultipatternSearch::TFlags> FlagNames[] = {
    { "#NO_CASE", CMultipatternSearch::fNoCase },
    { "#BEGIN_STRING", CMultipatternSearch::fBeginString },
    { "#END_STRING", CMultipatternSearch::fEndString },
    { "#WHOLE_STRING", CMultipatternSearch::fWholeString },
    { "#BEGIN_WORD", CMultipatternSearch::fBeginWord },
    { "#END_WORD", CMultipatternSearch::fEndWord },
    { "#WHOLE_WORD", CMultipatternSearch::fWholeWord }
};


int CMultipatternApp::Run(void)
{
	vector<pair<string, CMultipatternSearch::TFlags>> input;
	const CArgs& args = GetArgs();
    string fname;
    string params;
    if (args["i"]) {
        fname = args["i"].AsString();
        ifstream file(fname);
        if (!file) {
            cerr << "Cannot open file \'" << fname << "\'\n";
            return 1;
        }
        std::string line;
        size_t m;
        while (std::getline(file, line)) {
            // input line: <query> [<//comment>]
            //   /regex/   // comment ignored
            //   any text  // #NO_CASE #WHOLE_WORD etc...
            string comment;
            if ((m = line.find("//")) != string::npos) {
                comment = line.substr(m);
                line = line.substr(0, m);
            }
            if ((m = line.find_first_not_of(" \t")) != string::npos) {
                line = line.substr(m);
            }
            if ((m = line.find_last_not_of(" \t")) != string::npos) {
                line = line.substr(0, m + 1);
            }
            unsigned int flags = 0;
            for (auto f: FlagNames) {
                if (comment.find(f.first) != string::npos) {
                    flags |= f.second;
                }
            }
            input.push_back(pair<string, unsigned int>(line, flags));
        }
        if ((m = fname.find_last_of("\\/")) != string::npos) {
            fname = fname.substr(m + 1);
        }
    }

	for (size_t i = 1; i <= args.GetNExtra(); i++) {
        string param = args["#" + to_string(i)].AsString();
        params += " " + param;
        input.push_back(pair<string, unsigned int>(param, 0));
	}
    CMultipatternSearch FSM;
    try {
        FSM.AddPatterns(input);
    }
    catch (string s) {
        cerr << s << "\n";
        return 1;
    }
    if (args["D"]) {
        FSM.GenerateDotGraph(cout);
    }
    else if (args["A"]) {
        cout << "//\n// This code was generated by the multipattern application.\n//\n// Command line:\n//   multipattern -A";
        if (!fname.empty()) {
            cout << " -i " << fname;
        }
        cout << "\n//\n";
        FSM.GenerateArrayMapData(cout);
    }
    else {
        cout << "//\n// This code was generated by the multipattern application.\n//\n// Command line:\n//   multipattern";
        if (!fname.empty()) {
            cout << " -i " << fname;
        }
        cout << "\n//\n";
        FSM.GenerateSourceCode(cout);
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CMultipatternApp().AppMain(argc, argv);
}
