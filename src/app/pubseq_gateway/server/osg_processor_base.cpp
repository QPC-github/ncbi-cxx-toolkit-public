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
 * Authors: Eugene Vasilchenko
 *
 * File Description: base class for processors which may generate os_gateway
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include "osg_processor_base.hpp"

#include "osg_fetch.hpp"
#include "osg_caller.hpp"
#include "osg_connection.hpp"
#include "cass_processor_base.hpp"

#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Params.hpp>
#include <objects/id2/ID2_Param.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include "pubseq_gateway.hpp"
#include <thread>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


#if 1
# define tLOG_POST(m) LOG_POST(m)
#else
# define tLOG_POST(m) ((void)0)
#endif

#define SEND_TRACE(str) SendTrace(str)
#define SEND_TRACE_FMT(m)                                               \
    do {                                                                \
        if ( NeedTrace() ) {                                            \
            ostringstream str;                                          \
            str << m;                                                   \
            SendTrace(str.str());                                       \
        }                                                               \
    } while(0)

/////////////////////////////////////////////////////////////////////////////
// COSGProcessorRef
/////////////////////////////////////////////////////////////////////////////

COSGProcessorRef::COSGProcessorRef(CPSGS_OSGProcessorBase* ptr)
    : m_ProcessorPtr(ptr),
      m_FinalStatus(IPSGS_Processor::ePSGS_InProgress),
      m_ConnectionPool(ptr->m_ConnectionPool)
{
    _ASSERT(m_ConnectionPool);
}


COSGProcessorRef::~COSGProcessorRef()
{
}


COSGProcessorRef::TFetches COSGProcessorRef::GetFetches()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        return m_ProcessorPtr->GetFetches();
    }
    else {
        return TFetches();
    }
}


void COSGProcessorRef::NotifyOSGCallStart()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->NotifyOSGCallStart();
    }
}


void COSGProcessorRef::NotifyOSGCallReply(const CID2_Reply& reply)
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->NotifyOSGCallReply(reply);
    }
}


void COSGProcessorRef::NotifyOSGCallEnd()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->NotifyOSGCallEnd();
    }
}


void COSGProcessorRef::FinalizeResult(IPSGS_Processor::EPSGS_Status status)
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->FinalizeResult(status);
    }
}


void COSGProcessorRef::SignalEndOfBackgroundProcessing()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->SignalEndOfBackgroundProcessing();
    }
}


IPSGS_Processor::EPSGS_Status COSGProcessorRef::GetStatus() const
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    return m_ProcessorPtr? m_ProcessorPtr->GetStatus(): m_FinalStatus;
}


void COSGProcessorRef::Cancel()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->Cancel();
    }
    else {
        m_FinalStatus = IPSGS_Processor::ePSGS_Canceled;
    }
}


void COSGProcessorRef::Detach()
{
    return;
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_FinalStatus = m_ProcessorPtr->GetStatus();
        m_ProcessorPtr = 0;
    }
}


void COSGProcessorRef::ProcessReplies()
{
    SEND_TRACE("OSG: processing replies");
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->ProcessReplies();
    }
}


void COSGProcessorRef::SetRequestContext()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->GetRequest()->SetRequestContext();
    }
}


void COSGProcessorRef::WaitForOtherProcessors()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->WaitForOtherProcessors();
    }
}


bool COSGProcessorRef::NeedTrace() const
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        return m_ProcessorPtr->NeedTrace();
    }
    return false;
}


void COSGProcessorRef::SendTrace(const string& str)
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr->SendTrace(str);
    }
}


void COSGProcessorRef::s_ProcessReplies(void *data)
{
    CRequestContextResetter     context_resetter;
    shared_ptr<COSGProcessorRef>& ref = *static_cast<shared_ptr<COSGProcessorRef>*>(data);
    try {
        ref->SetRequestContext();
        ref->ProcessReplies();
    }
    catch ( exception& exc ) {
        ERR_POST("OSG: ProcessReplies() failed: "<<exc.what());
        ref->FinalizeResult(IPSGS_Processor::ePSGS_Error);
    }
    ref->SignalEndOfBackgroundProcessing();
    delete &ref;
}


void COSGProcessorRef::ProcessRepliesInUvLoop()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        _ASSERT(m_ProcessorPtr->m_ProcessorRef);
        m_ProcessorPtr->GetUvLoopBinder().PostponeInvoke(
            s_ProcessReplies,
            new shared_ptr<COSGProcessorRef>(m_ProcessorPtr->m_ProcessorRef));
    }
}


void COSGProcessorRef::s_Process(shared_ptr<COSGProcessorRef> processor)
{
    processor->Process();
}


void COSGProcessorRef::Process()
{
    SEND_TRACE("OSG: Process() started");
    CRequestContextResetter     context_resetter;
    SetRequestContext();

    try {
        tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() start: "<<GetStatus());
        for ( double retry_count = m_ConnectionPool->GetRetryCount(); retry_count > 0; ) {
            if ( IsCanceled() ) {
                SEND_TRACE("OSG: Process() canceled");
                return;
            }
        
            // We need to distinguish different kinds of communication failures with different
            //   effect on retry logic.
            // 1. stale/disconnected connection failure - there maybe multiple in active connection pool
            // 2. multiple simultaneous failures from concurrent incoming requests
            // 3. repeated failure of specific request at OSG server
            // In the first case we shouldn't account all such failures in the same retry counter -
            //   it will overflow easily, and quite unnecessary.
            // In the first case we shouldn't increase wait time too much -
            //   the failures should be treated as single failure for the sake of waiting before
            //   next connection attempt.
            // In the third case we should make sure we abandon the failing request when retry limit
            //   is reached. It should be detected no matter of concurrent successful requests.
        
            bool last_attempt = retry_count <= 1;
            COSGCaller caller;
            try {
                SEND_TRACE("OSG: allocating connection");
                caller.AllocateConnection(m_ConnectionPool);
                SEND_TRACE_FMT("OSG: allocated connection: "<<caller.GetConnectionID());
            }
            catch ( exception& exc ) {
                if ( last_attempt ) {
                    ERR_POST("OSG: failed opening connection: "<<exc.what());
                    throw;
                }
                else {
                    // failed new connection - consume full retry
                    ERR_POST("OSG: retrying after failure opening connection: "<<exc.what());
                    retry_count -= 1;
                    continue;
                }
            }
        
            if ( IsCanceled() ) {
                SEND_TRACE("OSG: Process() canceled");
                return;
            }
        
            try {
                caller.SendRequest(*this);
                caller.WaitForReplies(*this);
            }
            catch ( exception& exc ) {
                if ( last_attempt ) {
                    ERR_POST("OSG: failed receiving replies: "<<exc.what());
                    throw;
                }
                else {
                    // this may be failure of old connection
                    ERR_POST("OSG: retrying after failure receiving replies: "<<exc.what());
                    if ( caller.GetRequestPacket().Get().front()->GetSerial_number() <= 1 ) {
                        // new connection - consume full retry
                        retry_count -= 1;
                    }
                    else {
                        // old connection from pool - consume part of retry
                        retry_count -= 1./m_ConnectionPool->GetMaxConnectionCount();
                    }
                    continue;
                }
            }
        
            // successful
            break;
        }

        if ( IsCanceled() ) {
            SEND_TRACE("OSG: Process() canceled");
            return;
        }
        tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() got replies: "<<GetStatus());
        WaitForOtherProcessors();
        if ( IsCanceled() ) {
            SEND_TRACE("OSG: Process() canceled");
            return;
        }
        if ( 1 ) {
            SEND_TRACE("OSG: switching to UV loop");
            ProcessRepliesInUvLoop();
            tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() return with async post: "<<GetStatus());
        }
        else {
            SEND_TRACE("OSG: processing replies");
            ProcessReplies();
            tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() finished: "<<GetStatus());
            _ASSERT(GetStatus() != IPSGS_Processor::ePSGS_InProgress);
            tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() return: "<<GetStatus());
            SignalEndOfBackgroundProcessing();
        }
        tLOG_POST("COSGProcessorRef("<<m_ProcessorPtr<<")::Process() done: "<<GetStatus());
    }
    catch ( exception& exc ) {
        ERR_POST("OSG: DoProcess() failed: "<<exc.what());
        FinalizeResult(IPSGS_Processor::ePSGS_Error);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CPSGS_OSGProcessorBase
/////////////////////////////////////////////////////////////////////////////


CPSGS_OSGProcessorBase::CPSGS_OSGProcessorBase(TEnabledFlags enabled_flags,
                                               const CRef<COSGConnectionPool>& pool,
                                               const shared_ptr<CPSGS_Request>& request,
                                               const shared_ptr<CPSGS_Reply>& reply,
                                               TProcessorPriority priority)
    : m_Context(request->GetRequestContext()),
      m_ConnectionPool(pool),
      m_EnabledFlags(enabled_flags),
      m_Status(IPSGS_Processor::ePSGS_InProgress),
      m_BackgroundProcesing(false),
      m_NeedTrace(request->NeedTrace())
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::CPSGS_OSGProcessorBase()");
    m_Request = request;
    m_Reply = reply;
    m_Priority = priority;
    _ASSERT(m_ConnectionPool);
    _ASSERT(m_Request);
    _ASSERT(m_Reply);
}


static int s_DebugLevel = eDebugLevel_default;
static EDiagSev s_DiagSeverity = eDiag_Trace;


int GetDebugLevel()
{
    return s_DebugLevel;
}


void SetDebugLevel(int level)
{
    s_DebugLevel = level;
}


Severity GetDiagSeverity()
{
    return Severity(s_DiagSeverity);
}


void SetDiagSeverity(EDiagSev severity)
{
    s_DiagSeverity = severity;
}


IPSGS_Processor* CPSGS_OSGProcessorBase::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                                         shared_ptr<CPSGS_Reply> reply,
                                                         TProcessorPriority priority) const
{
    return nullptr;
}


CPSGS_OSGProcessorBase::~CPSGS_OSGProcessorBase()
{
    StopAsyncThread();
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() status: "<<m_Status);
    _ASSERT(m_Status != IPSGS_Processor::ePSGS_InProgress);
    _ASSERT(!m_BackgroundProcesing);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() return: "<<m_Status);
    if ( m_ProcessorRef ) {
        m_ProcessorRef->Detach();
    }
}


void CPSGS_OSGProcessorBase::WaitForOtherProcessors()
{
}


void CPSGS_OSGProcessorBase::WaitForCassandra()
{
    SendTrace("OSG: waiting for Cassandra results");
    GetRequest()->WaitFor(kCassandraProcessorEvent);
}


void CPSGS_OSGProcessorBase::SendTrace(const string& str)
{
    if ( NeedTrace() ) {
        m_Reply->SendTrace(str, m_Request->GetStartTimestamp());
    }
}


void CPSGS_OSGProcessorBase::StopAsyncThread()
{
    if ( m_ProcessorRef ) {
        m_ProcessorRef->Detach();
    }
    /*
    if ( m_Thread ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() joining status: "<<m_Status);
        m_Thread->join();
        m_Thread.reset();
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::~CPSGS_OSGProcessorBase() joined status: "<<m_Status);
    }
    */
}


void CPSGS_OSGProcessorBase::Process()
{
    SEND_TRACE("OSG: Process() called");
    CRequestContextResetter     context_resetter;
    GetRequest()->SetRequestContext();

    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process(): "<<m_Status);
    if ( IsCanceled() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() canceled: "<<m_Status);
        return;
    }
    if ( GetFetches().empty() ) {
        CreateRequests();
        if ( m_Context ) {
            for ( auto& f : m_Fetches ) {
                f->SetContext(*m_Context);
            }
        }
    }
    _ASSERT(!GetFetches().empty());
    m_ProcessorRef = make_shared<COSGProcessorRef>(this);
    if ( m_ConnectionPool->GetAsyncProcessing() ) {
        SEND_TRACE("OSG: switching Process() to background thread");
        ProcessAsync();
    }
    else {
        ProcessSync();
    }
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Process() return: "<<m_Status);
}


void CPSGS_OSGProcessorBase::ProcessSync()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessSync(): "<<m_Status);
    m_ProcessorRef->Process();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessSync() return: "<<m_Status);
    _ASSERT(m_Status != ePSGS_InProgress);
}


void CPSGS_OSGProcessorBase::ProcessAsync()
{
    if ( !SignalStartOfBackgroundProcessing() ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessAsync(): don't start thread: "<<m_Status);
        return;
    }
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessAsync(): "<<m_Status);
    thread(bind(&COSGProcessorRef::s_Process, m_ProcessorRef)).detach();
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::ProcessAsync() started: "<<m_Status);
}


void CPSGS_OSGProcessorBase::s_ProcessReplies(void* proc)
{
    auto processor = static_cast<CPSGS_OSGProcessorBase*>(proc);
    tLOG_POST("CPSGS_OSGProcessorBase("<<proc<<")::ProcessReplies() start: "<<processor->m_Status);
    processor->ProcessReplies();
    tLOG_POST("CPSGS_OSGProcessorBase("<<proc<<")::ProcessReplies() done: "<<processor->m_Status);
}


void CPSGS_OSGProcessorBase::Cancel()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::Cancel()");
    SetFinalStatus(ePSGS_Canceled);
    FinalizeResult();
}


void CPSGS_OSGProcessorBase::ResetReplies()
{
}


void CPSGS_OSGProcessorBase::NotifyOSGCallStart()
{
}


void CPSGS_OSGProcessorBase::NotifyOSGCallReply(const CID2_Reply& /*reply*/)
{
}


void CPSGS_OSGProcessorBase::NotifyOSGCallEnd()
{
}


void CPSGS_OSGProcessorBase::AddRequest(const CRef<CID2_Request>& req0)
{
    CRef<CID2_Request> req = req0;
    if ( 1 ) {
        // set hops
        auto hops = GetRequest()->GetRequest<SPSGS_RequestBase>().m_Hops + 1;
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("hops");
        param->SetValue().push_back(to_string(hops));
        req->SetParams().Set().push_back(param);
    }
    m_Fetches.push_back(Ref(new COSGFetch(req)));
}


IPSGS_Processor::EPSGS_Status CPSGS_OSGProcessorBase::GetStatus()
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::GetStatus(): "<<m_Status);
    return m_Status;
}


void CPSGS_OSGProcessorBase::SetFinalStatus(EPSGS_Status status)
{
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SetFinalStatus(): "<<m_Status<<" -> "<<status);
    _ASSERT(m_Status == ePSGS_InProgress || status == m_Status ||
            m_Status == ePSGS_Canceled || status == ePSGS_Canceled);
    m_Status = status;
}


void CPSGS_OSGProcessorBase::FinalizeResult()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::FinalizeResult(): "<<m_Status);
    _ASSERT(m_Status != ePSGS_InProgress);
    if ( !m_BackgroundProcesing ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::FinalizeResult(): signal "<<m_Status);
        SignalFinishProcessing();
    }
}


void CPSGS_OSGProcessorBase::FinalizeResult(EPSGS_Status status)
{
    SetFinalStatus(status);
    FinalizeResult();
}


bool CPSGS_OSGProcessorBase::SignalStartOfBackgroundProcessing()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalStartOfBackgroundProcessing(): signal "<<m_Status);
    _ASSERT(!m_BackgroundProcesing);
    if ( GetStatus() == IPSGS_Processor::ePSGS_Canceled ) {
        return false;
    }
    m_BackgroundProcesing = true;
    return true;
}


void CPSGS_OSGProcessorBase::SignalEndOfBackgroundProcessing()
{
    CMutexGuard guard(m_StatusMutex);
    tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalEndOfBackgroundProcessing(): signal "<<m_Status);
    _ASSERT(m_BackgroundProcesing);
    m_BackgroundProcesing = false;
    if ( GetStatus() == IPSGS_Processor::ePSGS_Canceled ) {
        tLOG_POST("CPSGS_OSGProcessorBase("<<this<<")::SignalEndOfBackgroundProcessing(): signal "<<m_Status);
        SignalFinishProcessing();
    }
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
