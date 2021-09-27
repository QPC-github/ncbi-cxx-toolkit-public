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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>

#include "pending_operation.hpp"
#include "http_server_transport.hpp"
#include "psgs_reply.hpp"
#include "pubseq_gateway_utils.hpp"
#include "cass_fetch.hpp"


CPSGS_Reply::~CPSGS_Reply()
{
    if (m_ReplyOwned) {
        delete m_Reply;
    }
}


void CPSGS_Reply::ConnectionCancel(void)
{
    m_ConnectionCanceled = true;
    m_Reply->NotifyClientConnectionDrop();
}


void CPSGS_Reply::Flush(void)
{
    if (m_ConnectionCanceled)
        return;

    while (m_ChunksLock.exchange(true)) {}
    m_Reply->Send(m_Chunks, true);
    m_Chunks.clear();
    m_ChunksLock = false;
}

void CPSGS_Reply::SendAccumulated(void)
{
    if (m_ConnectionCanceled)
        return;

    while (m_ChunksLock.exchange(true)) {}
    if (!m_Chunks.empty()) {
        m_Reply->SendAccumulated(&m_Chunks.front(), m_Chunks.size());
        m_Chunks.clear();
    }
    m_ChunksLock = false;
}

void CPSGS_Reply::Flush(bool  is_last)
{
    if (m_ConnectionCanceled)
        return;

    while (m_ChunksLock.exchange(true)) {}
    m_Reply->Send(m_Chunks, is_last);
    m_Chunks.clear();
    m_ChunksLock = false;

    if (is_last) {
        m_Reply->CancelPending(true);
    }
}


void CPSGS_Reply::Clear(void)
{
    if (m_ConnectionCanceled)
        return;

    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.clear();
    m_Reply = nullptr;
    m_TotalSentReplyChunks = 0;
    m_ChunksLock = false;
}


void CPSGS_Reply::SetContentType(EPSGS_ReplyMimeType  mime_type)
{
    if (m_ConnectionCanceled)
        return;

    m_Reply->SetContentType(mime_type);
}


shared_ptr<CCassDataCallbackReceiver> CPSGS_Reply::GetDataReadyCB(void)
{
    return m_Reply->GetDataReadyCB();
}


bool CPSGS_Reply::IsFinished(void) const
{
    return m_Reply->IsFinished();
}


bool CPSGS_Reply::IsOutputReady(void) const
{
    return m_Reply->IsOutputReady();
}


void CPSGS_Reply::PrepareBioseqMessage(size_t  item_id,
                                       const string &  processor_id,
                                       const string &  msg,
                                       CRequestStatus::ECode  status,
                                       int  err_code,
                                       EDiagSev  severity)
{
    if (m_ConnectionCanceled)
        return;

    string  header = GetBioseqMessageHeader(item_id, processor_id,
                                            msg.size(), status,
                                            err_code, severity);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}



void CPSGS_Reply::PrepareBioseqData(
                    size_t  item_id,
                    const string &  processor_id,
                    const string &  content,
                    SPSGS_ResolveRequest::EPSGS_OutputFormat  output_format)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetBioseqInfoHeader(item_id, processor_id,
                                             content.size(), output_format);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBioseqCompletion(size_t  item_id,
                                          const string &  processor_id,
                                          size_t  chunk_count)
{
    if (m_ConnectionCanceled)
        return;

    string      bioseq_meta = GetBioseqCompletionHeader(item_id,
                                                        processor_id,
                                                        chunk_count);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_meta.data()),
                bioseq_meta.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobPropMessage(size_t                 item_id,
                                         const string &         processor_id,
                                         const string &         msg,
                                         CRequestStatus::ECode  status,
                                         int                    err_code,
                                         EDiagSev               severity)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetBlobPropMessageHeader(item_id, processor_id,
                                                  msg.size(), status, err_code,
                                                  severity);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::x_PrepareTSEBlobPropMessage(size_t                 item_id,
                                              const string &         processor_id,
                                              int64_t                id2_chunk,
                                              const string &         id2_info,
                                              const string &         msg,
                                              CRequestStatus::ECode  status,
                                              int                    err_code,
                                              EDiagSev               severity)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetTSEBlobPropMessageHeader(
                                item_id, processor_id, id2_chunk, id2_info,
                                msg.size(), status, err_code, severity);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobPropMessage(CCassBlobFetch *       fetch_details,
                                         const string &         processor_id,
                                         const string &         msg,
                                         CRequestStatus::ECode  status,
                                         int                    err_code,
                                         EDiagSev               severity)
{
    if (m_ConnectionCanceled)
        return;

    PrepareBlobPropMessage(fetch_details->GetBlobPropItemId(this),
                           processor_id, msg, status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobPropMessage(CCassBlobFetch *       fetch_details,
                                            const string &         processor_id,
                                            int64_t                id2_chunk,
                                            const string &         id2_info,
                                            const string &         msg,
                                            CRequestStatus::ECode  status,
                                            int                    err_code,
                                            EDiagSev               severity)
{
    if (m_ConnectionCanceled)
        return;

    x_PrepareTSEBlobPropMessage(fetch_details->GetBlobPropItemId(this),
                                processor_id, id2_chunk, id2_info, msg,
                                status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareBlobPropData(size_t                   item_id,
                                      const string &           processor_id,
                                      const string &           blob_id,
                                      const string &           content,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    string  header = GetBlobPropHeader(item_id,
                                       processor_id,
                                       blob_id,
                                       content.size(),
                                       last_modified);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobPropData(CCassBlobFetch *         fetch_details,
                                      const string &           processor_id,
                                      const string &           content,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    PrepareBlobPropData(fetch_details->GetBlobPropItemId(this),
                        processor_id,
                        fetch_details->GetBlobId().ToString(),
                        content,
                        last_modified);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobPropData(CCassBlobFetch *  fetch_details,
                                         const string &    processor_id,
                                         int64_t           id2_chunk,
                                         const string &    id2_info,
                                         const string &    content)
{
    if (m_ConnectionCanceled)
        return;

    PrepareTSEBlobPropData(fetch_details->GetBlobPropItemId(this),
                           processor_id, id2_chunk, id2_info, content);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobPropData(size_t  item_id,
                                         const string &    processor_id,
                                         int64_t           id2_chunk,
                                         const string &    id2_info,
                                         const string &    content)
{
    if (m_ConnectionCanceled)
        return;

    string  header = GetTSEBlobPropHeader(item_id,
                                          processor_id,
                                          id2_chunk, id2_info,
                                          content.size());
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(content.data()),
                    content.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobData(size_t                   item_id,
                                  const string &           processor_id,
                                  const string &           blob_id,
                                  const unsigned char *    chunk_data,
                                  unsigned int             data_size,
                                  int                      chunk_no,
                                  CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    ++m_TotalSentReplyChunks;

    string  header = GetBlobChunkHeader(
                            item_id,
                            processor_id,
                            blob_id,
                            data_size, chunk_no,
                            last_modified);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));

    if (data_size > 0 && chunk_data != nullptr)
        m_Chunks.push_back(m_Reply->PrepareChunk(chunk_data, data_size));
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobData(CCassBlobFetch *         fetch_details,
                                  const string &           processor_id,
                                  const unsigned char *    chunk_data,
                                  unsigned int             data_size,
                                  int                      chunk_no,
                                  CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    fetch_details->IncrementTotalSentBlobChunks();
    PrepareBlobData(fetch_details->GetBlobChunkItemId(this),
                    processor_id,
                    fetch_details->GetBlobId().ToString(),
                    chunk_data, data_size, chunk_no,
                    last_modified);
}


void CPSGS_Reply::PrepareTSEBlobData(size_t                 item_id,
                                     const string &         processor_id,
                                     const unsigned char *  chunk_data,
                                     unsigned int           data_size,
                                     int                    chunk_no,
                                     int64_t                id2_chunk,
                                     const string &         id2_info)
{
    if (m_ConnectionCanceled)
        return;

    ++m_TotalSentReplyChunks;

    string  header = GetTSEBlobChunkHeader(
                            item_id,
                            processor_id,
                            data_size, chunk_no,
                            id2_chunk, id2_info);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(header.data()),
                    header.size()));

    if (data_size > 0 && chunk_data != nullptr)
        m_Chunks.push_back(m_Reply->PrepareChunk(chunk_data, data_size));
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareTSEBlobData(CCassBlobFetch *  fetch_details,
                                     const string &  processor_id,
                                     const unsigned char *  chunk_data,
                                     unsigned int  data_size,
                                     int  chunk_no,
                                     int64_t  id2_chunk,
                                     const string &  id2_info)
{
    if (m_ConnectionCanceled)
        return;

    fetch_details->IncrementTotalSentBlobChunks();
    PrepareTSEBlobData(fetch_details->GetBlobChunkItemId(this),
                       processor_id,
                       chunk_data, data_size, chunk_no,
                       id2_chunk, id2_info);
}


void CPSGS_Reply::PrepareBlobPropCompletion(size_t  item_id,
                                            const string &  processor_id,
                                            size_t  chunk_count)
{
    if (m_ConnectionCanceled)
        return;

    string      blob_prop_meta = GetBlobPropCompletionHeader(item_id,
                                                             processor_id,
                                                             chunk_count);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::x_PrepareTSEBlobPropCompletion(size_t          item_id,
                                                 const string &  processor_id,
                                                 size_t          chunk_count)
{
    if (m_ConnectionCanceled)
        return;

    string      blob_prop_meta = GetTSEBlobPropCompletionHeader(item_id,
                                                                processor_id,
                                                                chunk_count);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(blob_prop_meta.data()),
                    blob_prop_meta.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobPropCompletion(CCassBlobFetch *  fetch_details,
                                            const string &  processor_id)
{
    if (m_ConnectionCanceled)
        return;

    // +1 is for the completion itself
    PrepareBlobPropCompletion(fetch_details->GetBlobPropItemId(this),
                              processor_id,
                              fetch_details->GetTotalSentBlobChunks() + 1);

    // From now the counter will count chunks for the blob data
    fetch_details->ResetTotalSentBlobChunks();
    fetch_details->SetBlobPropSent();
}


void CPSGS_Reply::PrepareTSEBlobPropCompletion(CCassBlobFetch *  fetch_details,
                                               const string &  processor_id)
{
    if (m_ConnectionCanceled)
        return;

    // +1 is for the completion itself
    x_PrepareTSEBlobPropCompletion(fetch_details->GetBlobPropItemId(this),
                                   processor_id,
                                   fetch_details->GetTotalSentBlobChunks() + 1);

    // From now the counter will count chunks for the blob data
    fetch_details->ResetTotalSentBlobChunks();
    fetch_details->SetBlobPropSent();
}


void CPSGS_Reply::PrepareBlobMessage(size_t                   item_id,
                                     const string &           processor_id,
                                     const string &           blob_id,
                                     const string &           msg,
                                     CRequestStatus::ECode    status,
                                     int                      err_code,
                                     EDiagSev                 severity,
                                     CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetBlobMessageHeader(item_id, processor_id,
                                              blob_id, msg.size(),
                                              status, err_code, severity,
                                              last_modified);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobMessage(CCassBlobFetch *         fetch_details,
                                     const string &           processor_id,
                                     const string &           msg,
                                     CRequestStatus::ECode    status,
                                     int                      err_code,
                                     EDiagSev                 severity,
                                     CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    PrepareBlobMessage(fetch_details->GetBlobChunkItemId(this),
                       processor_id,
                       fetch_details->GetBlobId().ToString(),
                       msg, status, err_code, severity, last_modified);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::x_PrepareTSEBlobMessage(size_t  item_id,
                                          const string &  processor_id,
                                          int64_t  id2_chunk,
                                          const string &  id2_info,
                                          const string &  msg,
                                          CRequestStatus::ECode  status,
                                          int  err_code,
                                          EDiagSev  severity)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetTSEBlobMessageHeader(item_id, processor_id,
                                                 id2_chunk, id2_info, msg.size(),
                                                 status, err_code, severity);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareTSEBlobMessage(CCassBlobFetch *  fetch_details,
                                        const string &  processor_id,
                                        int64_t  id2_chunk,
                                        const string &  id2_info,
                                        const string &  msg,
                                        CRequestStatus::ECode  status, int  err_code,
                                        EDiagSev  severity)
{
    if (m_ConnectionCanceled)
        return;

    x_PrepareTSEBlobMessage(fetch_details->GetBlobChunkItemId(this),
                            processor_id, id2_chunk, id2_info,
                            msg, status, err_code, severity);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareBlobCompletion(size_t                   item_id,
                                        const string &           processor_id,
                                        size_t                   chunk_count)
{
    if (m_ConnectionCanceled)
        return;

    string completion = GetBlobCompletionHeader(item_id, processor_id,
                                                chunk_count);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(completion.data()),
                    completion.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareTSEBlobCompletion(CCassBlobFetch *  fetch_details,
                                           const string &  processor_id)
{
    if (m_ConnectionCanceled)
        return;

    // +1 is for the completion itself
    PrepareTSEBlobCompletion(fetch_details->GetBlobChunkItemId(this),
                             processor_id,
                             fetch_details->GetTotalSentBlobChunks() + 1);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareTSEBlobCompletion(size_t  item_id,
                                           const string &  processor_id,
                                           size_t  chunk_count)
{
    if (m_ConnectionCanceled)
        return;

    string completion = GetTSEBlobCompletionHeader(item_id, processor_id,
                                                   chunk_count);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(completion.data()),
                    completion.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobExcluded(const string &           blob_id,
                                      const string &           processor_id,
                                      EPSGS_BlobSkipReason     skip_reason,
                                      CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    string  exclude = GetBlobExcludeHeader(GetItemId(), processor_id,
                                           blob_id, skip_reason, last_modified);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobExcluded(size_t                item_id,
                                      const string &        processor_id,
                                      const string &        blob_id,
                                      EPSGS_BlobSkipReason  skip_reason)
{
    if (m_ConnectionCanceled)
        return;

    string  exclude = GetBlobExcludeHeader(item_id, processor_id,
                                           blob_id, skip_reason);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                    (const unsigned char *)(exclude.data()),
                    exclude.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareBlobCompletion(CCassBlobFetch *  fetch_details,
                                        const string &    processor_id)
{
    if (m_ConnectionCanceled)
        return;

    // +1 is for the completion itself
    PrepareBlobCompletion(fetch_details->GetBlobChunkItemId(this),
                          processor_id,
                          fetch_details->GetTotalSentBlobChunks() + 1);
    fetch_details->IncrementTotalSentBlobChunks();
}


void CPSGS_Reply::PrepareReplyMessage(const string &         msg,
                                      CRequestStatus::ECode  status,
                                      int                    err_code,
                                      EDiagSev               severity)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetReplyMessageHeader(msg.size(),
                                               status, err_code, severity);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareProcessorMessage(size_t                 item_id,
                                          const string &         processor_id,
                                          const string &         msg,
                                          CRequestStatus::ECode  status,
                                          int                    err_code,
                                          EDiagSev               severity)
{
    if (m_ConnectionCanceled)
        return;

    string      header = GetProcessorMessageHeader(item_id, processor_id,
                                                   msg.size(),
                                                   status, err_code, severity);
    string      completion = GetProcessorMessageCompletionHeader(item_id,
                                                                 processor_id,
                                                                 2);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()), completion.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PreparePublicComment(const string &  processor_id,
                                       const string &  public_comment,
                                       const string &  blob_id,
                                       CBlobRecord::TTimestamp  last_modified)
{
    if (m_ConnectionCanceled)
        return;

    auto        item_id = GetItemId();
    string      header = GetPublicCommentHeader(item_id, processor_id, blob_id,
                                                last_modified, public_comment.size());
    string      completion = GetPublicCommentCompletionHeader(item_id,
                                                              processor_id,
                                                              2);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(public_comment.data()), public_comment.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()), completion.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PreparePublicComment(const string &  processor_id,
                                       const string &  public_comment,
                                       int64_t  id2_chunk,
                                       const string &  id2_info)
{
    if (m_ConnectionCanceled)
        return;

    auto        item_id = GetItemId();
    string      header = GetPublicCommentHeader(item_id, processor_id, id2_chunk,
                                                id2_info, public_comment.size());
    string      completion = GetPublicCommentCompletionHeader(item_id,
                                                              processor_id,
                                                              2);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(public_comment.data()), public_comment.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(completion.data()), completion.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareNamedAnnotationData(const string &  annot_name,
                                             const string &  processor_id,
                                             const string &  content)
{
    if (m_ConnectionCanceled)
        return;

    size_t      item_id = GetItemId();
    string      header = GetNamedAnnotationHeader(item_id, processor_id,
                                                  annot_name, content.size());
    // There are always 2 chunks
    string      bioseq_na_meta = GetNamedAnnotationCompletionHeader(item_id,
                                                                    processor_id,
                                                                    2);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(bioseq_na_meta.data()),
                bioseq_na_meta.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareAccVerHistoryData(const string &  processor_id,
                                           const string &  content)
{
    if (m_ConnectionCanceled)
        return;

    size_t      item_id = GetItemId();
    string      header = GetAccVerHistoryHeader(item_id, processor_id,
                                                content.size());

    // There are always 2 chunks
    string      acc_ver_hist_meta = GetAccVerHistCompletionHeader(item_id,
                                                                  processor_id,
                                                                  2);
    while (m_ChunksLock.exchange(true)) {}
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(content.data()), content.size()));
    ++m_TotalSentReplyChunks;

    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(acc_ver_hist_meta.data()),
                acc_ver_hist_meta.size()));
    ++m_TotalSentReplyChunks;
    m_ChunksLock = false;
}


void CPSGS_Reply::PrepareReplyCompletion(void)
{
    if (m_ConnectionCanceled)
        return;
    if (m_Reply->IsClosed())
        return;

    while (m_ChunksLock.exchange(true)) {}
    ++m_TotalSentReplyChunks;

    string  reply_completion = GetReplyCompletionHeader(m_TotalSentReplyChunks);
    m_Chunks.push_back(m_Reply->PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));
    m_ChunksLock = false;
}


void CPSGS_Reply::SendTrace(const string &  msg,
                            const TPSGS_HighResolutionTimePoint &  create_timestamp)
{
    if (m_ConnectionCanceled)
        return;

    auto            now = chrono::high_resolution_clock::now();
    uint64_t        mks = chrono::duration_cast<chrono::microseconds>
                                            (now - create_timestamp).count();
    string          timestamp = "Timestamp (mks): " + to_string(mks) + "\n";

    PrepareReplyMessage(timestamp + msg,
                        CRequestStatus::e200_Ok, 0, eDiag_Trace);
}


void CPSGS_Reply::SendData(const string &  data_to_send,
                           EPSGS_ReplyMimeType  mime_type)
{
    if (m_ConnectionCanceled)
        return;

    m_Reply->SetContentType(mime_type);
    m_Reply->SetContentLength(data_to_send.length());
    m_Reply->SendOk(data_to_send.data(), data_to_send.length(), false);
}

