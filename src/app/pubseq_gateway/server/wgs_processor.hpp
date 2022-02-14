#ifndef WGS_PROCESSOR__HPP
#define WGS_PROCESSOR__HPP

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
 * Authors: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description: processor for data from WGS
 *
 */

#include "ipsgs_processor.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include <objects/seq/seq_id_handle.hpp>
#include <thread>


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);
class CID2_Blob_Id;
class CID2_Reply_Data;
class CWGSResolver;
class CSeq_id;
class CAsnBinData;
END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(wgs);


class CPSGS_WGSProcessor;

class CWGSProcessorRef
{
public:
    explicit CWGSProcessorRef(CPSGS_WGSProcessor* ptr);
    ~CWGSProcessorRef();

private:
    friend class CPSGS_WGSProcessor;
    
    void Detach();

    static void ResolveSeqId(shared_ptr<CWGSProcessorRef> ref);
    static void OnResolvedSeqId(void* data);

    static void GetBlobBySeqId(shared_ptr<CWGSProcessorRef> ref);
    static void OnGotBlobBySeqId(void* data);

    static void GetBlobByBlobId(shared_ptr<CWGSProcessorRef> ref);
    static void OnGotBlobByBlobId(void* data);
    
    static void GetChunk(shared_ptr<CWGSProcessorRef> ref);
    static void OnGotChunk(void* data);

    CFastMutex m_ProcessorPtrMutex;
    CPSGS_WGSProcessor* volatile m_ProcessorPtr;
};


class CWGSClient;
class SWGSProcessor_Config;
struct SWGSData;

class CPSGS_WGSProcessor : public IPSGS_Processor
{
public:
    CPSGS_WGSProcessor(void);
    ~CPSGS_WGSProcessor(void) override;

    void LoadConfig(const CNcbiRegistry& registry);

    IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                     shared_ptr<CPSGS_Reply> reply,
                                     TProcessorPriority priority) const override;

    void Process(void) override;
    void Cancel(void) override;
    EPSGS_Status GetStatus(void) override;
    string GetName(void) const override;

    void OnResolvedSeqId(void);
    void OnGotBlobBySeqId(void);
    void OnGotBlobByBlobId(void);
    void OnGotChunk(void);

private:
    friend class CWGSProcessorRef;
    
    CPSGS_WGSProcessor(const shared_ptr<CWGSClient>& client,
                       shared_ptr<CPSGS_Request> request,
                       shared_ptr<CPSGS_Reply> reply,
                       TProcessorPriority priority);

    bool x_IsEnabled(CPSGS_Request& request) const;
    void x_InitClient(void) const;

    void x_ProcessResolveRequest(void);
    void x_ProcessBlobBySeqIdRequest(void);
    void x_ProcessBlobBySatSatKeyRequest(void);
    void x_ProcessTSEChunkRequest(void);

    typedef SPSGS_ResolveRequest::EPSGS_OutputFormat EOutputFormat;
    typedef SPSGS_ResolveRequest::TPSGS_BioseqIncludeData TBioseqInfoFlags;
    typedef int TID2ChunkId;
    typedef vector<string> TBlobIds;

    EOutputFormat x_GetOutputFormat(void);
    void x_SendResult(const string& data_to_send, EOutputFormat output_format);
    void x_SendBioseqInfo(void);
    void x_SendBlobProps(const string& psg_blob_id, CBlobRecord& blob_props);
    void x_SendBlobData(const string& psg_blob_id, const objects::CID2_Reply_Data& data);
    void x_SendChunkBlobProps(const string& id2_info,
                              TID2ChunkId chunk_id,
                              CBlobRecord& blob_props);
    void x_SendChunkBlobData(const string& id2_info,
                             TID2ChunkId chunk_id,
                             const objects::CID2_Reply_Data& data);
    void x_SendSplitInfo(void);
    void x_SendMainEntry(void);
    void x_SendExcluded(void);
    void x_SendBlob(void);
    void x_SendChunk(void);
    void x_WriteData(objects::CID2_Reply_Data& data,
                     const objects::CAsnBinData& obj,
                     bool compress) const;

    template<class C> static int x_GetBlobState(const C& obj) {
        return obj.IsSetBlob_state() ? obj.GetBlob_state() : 0;
    }

    void x_Finish(EPSGS_Status status);
    bool x_IsCanceled();
    bool x_SignalStartProcessing();

    shared_ptr<CWGSProcessorRef> m_ProcessorRef;
    shared_ptr<SWGSProcessor_Config> m_Config;
    mutable shared_ptr<CWGSClient> m_Client;
    EPSGS_Status m_Status;
    bool m_Canceled;
    CRef<objects::CSeq_id> m_SeqId; // requested seq-id
    string m_PSGBlobId; // requested blob-id
    string m_Id2Info; // requested id2-info
    int64_t m_ChunkId; // requested chunk-id
    TBlobIds m_ExcludedBlobs;
    shared_ptr<SWGSData> m_WGSData;
    EOutputFormat m_OutputFormat;
};


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_PROCESSOR__HPP
