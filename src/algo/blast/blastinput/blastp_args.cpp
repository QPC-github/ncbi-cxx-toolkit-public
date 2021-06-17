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
 * Author: Christiam Camacho
 *
 */

/** @file blastp_args.cpp
 * Implementation of the BLASTP command line arguments
 */
#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/blastp_args.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CBlastpAppArgs::CBlastpAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    static const string kProgram("blastp");
    arg.Reset(new CProgramDescriptionArgs(kProgram, "Protein-Protein BLAST"));
    const bool kQueryIsProtein = true;
    bool const kFilterByDefault = false;
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    static const char kDefaultTask[] = "blastp";
    SetTask(kDefaultTask);
    set<string> tasks
        (CBlastOptionsFactory::GetTasks(CBlastOptionsFactory::eProtProt));
    arg.Reset(new CTaskCmdLineArgs(tasks, kDefaultTask));
    m_Args.push_back(arg);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs);
    m_BlastDbArgs->SetDatabaseMaskingSupport(true);
    m_BlastDbArgs->SetIPGFilteringSupport(true);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein, false, false,
                                     false, false, true));
    m_Args.push_back(arg);

    arg.Reset(new CFilteringArgs(kQueryIsProtein, kFilterByDefault));
    m_Args.push_back(arg);

    arg.Reset(new CMatrixNameArg);
    m_Args.push_back(arg);

    arg.Reset(new CWordThresholdArg);
    m_Args.push_back(arg);

    m_HspFilteringArgs.Reset(new CHspFilteringArgs);
    arg.Reset(m_HspFilteringArgs);
    m_Args.push_back(arg);

    arg.Reset(new CWindowSizeArg);
    m_Args.push_back(arg);

    m_QueryOptsArgs.Reset(new CQueryOptionsArgs(kQueryIsProtein));
    arg.Reset(m_QueryOptsArgs);
    m_Args.push_back(arg);

    m_FormattingArgs.Reset(new CFormattingArgs);
    arg.Reset(m_FormattingArgs);
    m_Args.push_back(arg);

    m_MTArgs.Reset(new CMTArgs(CThreadable::kMinNumThreads, CMTArgs::eSplitByDB));
    arg.Reset(m_MTArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGappedArgs);
    m_Args.push_back(arg);

    m_RemoteArgs.Reset(new CRemoteArgs);
    arg.Reset(m_RemoteArgs);
    m_Args.push_back(arg);

    arg.Reset(new CCompositionBasedStatsArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle> 
CBlastpAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality, 
                                      const CArgs& args)
{
    _ASSERT(args.Exist(kTask));
    _ASSERT(args[kTask].HasValue());
    return x_CreateOptionsHandleWithTask(locality, args[kTask].AsString());
}

int
CBlastpAppArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(eBlastp, m_IsUngapped, is_remote);
}

/// Get the input stream
CNcbiIstream&
CBlastpNodeArgs::GetInputStream()
{
	if ( !m_InputStream ) {
		abort();
	}
	return *m_InputStream;
}
/// Get the output stream
CNcbiOstream&
CBlastpNodeArgs::GetOutputStream()
{
	return m_OutputStream;
}

CBlastpNodeArgs::CBlastpNodeArgs(const string & input)
{
	m_InputStream = new CNcbiIstrstream(input.c_str(), input.length());
}

CBlastpNodeArgs::~CBlastpNodeArgs()
{
	if (m_InputStream) {
		free(m_InputStream);
		m_InputStream = NULL;
	}
}

int
CBlastpNodeArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(eBlastp, m_IsUngapped, is_remote);
}

CRef<CBlastOptionsHandle>
CBlastpNodeArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality, const CArgs& args)
{
    return CBlastpAppArgs::x_CreateOptionsHandle(locality, args);
}

END_SCOPE(blast)
END_NCBI_SCOPE

