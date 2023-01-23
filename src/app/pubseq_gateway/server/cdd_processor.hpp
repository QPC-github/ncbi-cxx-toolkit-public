#ifndef CDD_PROCESSOR__HPP
#define CDD_PROCESSOR__HPP

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
 * Authors: Aleksey Grichenko
 *
 * File Description: processor for data from CDD
 *
 */

#include "ipsgs_processor.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "timing.hpp"
#include <objects/seq/seq_id_handle.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>


BEGIN_NCBI_NAMESPACE;

class CThreadPool;

BEGIN_NAMESPACE(objects);

class CCDD_Reply_Get_Blob_Id;
class CSeq_annot;
class CID2_Blob_Id;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(cdd);

const string    kCDDProcessorEvent = "CDD";

class CPSGS_CDDProcessor : public IPSGS_Processor
{
public:
    CPSGS_CDDProcessor(void);
    ~CPSGS_CDDProcessor(void) override;

    vector<string> WhatCanProcess(shared_ptr<CPSGS_Request> request,
                                  shared_ptr<CPSGS_Reply> reply) const override;
    bool CanProcess(shared_ptr<CPSGS_Request> request,
                    shared_ptr<CPSGS_Reply> reply) const override;
    IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                     shared_ptr<CPSGS_Reply> reply,
                                     TProcessorPriority priority) const override;
    void Process(void) override;
    void Cancel(void) override;
    EPSGS_Status GetStatus(void) override;
    string GetName(void) const override;
    string GetGroupName(void) const override;

    void GetBlobId(void);
    void OnGotBlobId(void);

    void GetBlobBySeqId(void);
    void OnGotBlobBySeqId(void);

    void GetBlobByBlobId(void);
    void OnGotBlobByBlobId(void);

private:
    CPSGS_CDDProcessor(shared_ptr<objects::CCDDClientPool> client_pool,
                       shared_ptr<ncbi::CThreadPool> thread_pool,
                       shared_ptr<CPSGS_Request> request,
                       shared_ptr<CPSGS_Reply> reply,
                       TProcessorPriority priority);

    bool x_IsEnabled(CPSGS_Request& request) const;
    bool x_CanProcessSeq_id(const string& psg_id) const;
    bool x_CanProcessAnnotRequestIds(SPSGS_AnnotRequest& annot_request) const;
    bool x_CanProcessAnnotRequest(SPSGS_AnnotRequest& annot_request,
                                  TProcessorPriority priority) const;
    bool x_CanProcessBlobRequest(SPSGS_BlobBySatSatKeyRequest& blob_request) const;
    bool x_NameIncluded(const vector<string>& names) const;
    void x_Finish(EPSGS_Status status);
    void x_ProcessResolveRequest(void);
    void x_ProcessGetBlobRequest(void);
    void x_SendAnnotInfo(const objects::CCDD_Reply_Get_Blob_Id& blob_info);
    void x_RegisterTiming(EPSGOperation operation,
                          EPSGOperationStatus status,
                          size_t size);
    void x_RegisterTimingNotFound(EPSGOperation operation);
    void x_SendAnnot(const objects::CID2_Blob_Id& id2_blob_id, CRef<objects::CSeq_annot>& annot);
    static void x_SendError(shared_ptr<CPSGS_Reply> reply,
                            const string& msg);
    static void x_SendError(shared_ptr<CPSGS_Reply> reply,
                            const string& msg, const exception& exc);
    void x_SendError(const string& msg);
    void x_SendError(const string& msg, const exception& exc);
    void x_ReportResultStatus(SPSGS_AnnotRequest::EPSGS_ResultStatus status);
    void x_UnlockRequest(void);
    bool x_IsCanceled();
    bool x_SignalStartProcessing();

    CFastMutex m_Mutex;
    shared_ptr<objects::CCDDClientPool> m_ClientPool;
    psg_time_point_t m_Start;
    EPSGS_Status m_Status;
    bool m_Canceled;
    vector<objects::CSeq_id_Handle> m_SeqIds;
    CRef<objects::CCDDClientPool::TBlobId> m_BlobId;
    objects::CCDDClientPool::SCDDBlob m_CDDBlob;
    string m_Error;
    bool m_Unlocked;
    shared_ptr<ncbi::CThreadPool> m_ThreadPool;
};


END_NAMESPACE(cdd);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_PROCESSOR__HPP
