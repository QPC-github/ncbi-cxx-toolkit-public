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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <numeric>
#include <unordered_set>
#include <unordered_map>

#include <connect/impl/connect_misc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objcopy.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include "performance.hpp"
#include "processing.hpp"

BEGIN_NCBI_SCOPE

bool CJsonResponse::sm_SetReplyType = true;
bool SParams::verbose = false;

enum EJsonRpcErrors {
    eJsonRpc_ParseError         = -32700,
    eJsonRpc_InvalidRequest     = -32600,
    eJsonRpc_ExceptionOnRead    = -32000,
};

struct SNewRequestContext
{
    SNewRequestContext() :
        m_RequestContext(new CRequestContext)
    {
        m_RequestContext->SetRequestID();
        CDiagContext::SetRequestContext(m_RequestContext);
    }

    ~SNewRequestContext()
    {
        CDiagContext::SetRequestContext(nullptr);
    }

    CRef<CRequestContext> Clone() { return m_RequestContext->Clone(); }

private:
    SNewRequestContext(const SNewRequestContext&) = delete;
    void operator=(const SNewRequestContext&) = delete;

    CRef<CRequestContext> m_RequestContext;
};

struct SInteractiveNewRequestStart : SNewRequestContext
{
    SInteractiveNewRequestStart(CJson_ConstObject params_obj);

private:
    struct SExtra : private CDiagContext_Extra
    {
        SExtra() : CDiagContext_Extra(GetDiagContext().PrintRequestStart()) {}

        void Print(const string& prefix, CJson_ConstValue  json);
        void Print(const string& prefix, CJson_ConstArray  json);
        void Print(const string& prefix, CJson_ConstObject json);
        void Print(const string& prefix, CJson_ConstNode   json);

    private:
        using CDiagContext_Extra::Print;
    };
};

string s_GetId(const CJson_Document& req_doc);
CRequestStatus::ECode s_PsgStatusToRequestStatus(EPSG_Status psg_status);

const char* SMetricType::Name(EType t)
{
    switch (t) {
        case eStart:   return "Start";
        case eSubmit:  return "Submit";
        case eReply:   return "Reply";
        case eDone:    return "Done";
        case eSend:    return "Send";
        case eReceive: return "Receive";
        case eClose:   return "Close";
        case eRetry:   return "Retry";
        case eFail:    return "Fail";

        case eLastType:
        case eError:   break;
    }

    _TROUBLE;
    return "ERROR";
}

const char* s_StrStatus(EPSG_Status status)
{
    switch (status) {
        case EPSG_Status::eSuccess:    return "Success";
        case EPSG_Status::eInProgress: return "InProgress";
        case EPSG_Status::eNotFound:   return "NotFound";
        case EPSG_Status::eCanceled:   return "Canceled";
        case EPSG_Status::eForbidden:  return "Forbidden";
        case EPSG_Status::eError:      break;
    }

    return "Error";
}

SJsonOut& SJsonOut::operator<<(const CJson_Document& doc)
{
    stringstream ss;
    ss << doc;

    unique_lock<mutex> lock(m_Mutex);

    if (m_Pipe) {
        cout << ss.rdbuf() << endl;
    } else {
        cout << m_Separator << '\n' << ss.rdbuf() << flush;
        m_Separator = ',';
    }

    return *this;
}

SJsonOut::~SJsonOut()
{
    // If not in pipe mode and printed some JSON
    if (!m_Pipe && (m_Separator == ',')) {
        cout << "\n]" << endl;
    }
}

template <class TItem>
CJsonResponse::CJsonResponse(EPSG_Status status, TItem item, EExceptionHandling ex_handling) :
    m_JsonObj(SetObject())
{
    if (auto request_id = s_GetReply(item)->GetRequest()->template GetUserContext<string>()) {
        Set("request_id", *request_id);
    }

    try {
        Fill(status, item);
    }
    catch (exception& e) {
        if (ex_handling == eRethrow) {
            throw;
        }

        CJson_Document new_doc;
        new_doc.ResetObject()["data"].AssignCopy(*this);
        AssignCopy(new_doc);
        Set("code",    eJsonRpc_ExceptionOnRead);
        Set("message", e.what());
        m_Error = true;
    }
}

CJsonResponse::CJsonResponse(const string& id, bool result) :
    CJsonResponse(id)
{
    Set("result", result);
}

CJsonResponse::CJsonResponse(const string& id, const CJsonResponse& result) :
    CJsonResponse(id)
{
    m_JsonObj[result.m_Error ? "error" : "result"].AssignCopy(result);
}

CJsonResponse::CJsonResponse(const string& id, int code, const string& message) :
    CJsonResponse(id)
{
    CJson_Object error_obj = m_JsonObj.insert_object("error");
    Set(error_obj["code"],    code);
    Set(error_obj["message"], message);
}

CJsonResponse::CJsonResponse(const string& id) :
    m_JsonObj(SetObject())
{
    Set("jsonrpc", "2.0");

    auto id_value = m_JsonObj["id"].SetValue();

    if (id.empty()) {
        id_value.SetNull();
    } else {
        id_value.SetString(id);
    }
}

const char* s_GetItemName(CPSG_ReplyItem::EType type, bool trouble = true)
{
    switch (type) {
        case CPSG_ReplyItem::eBlobData:       return "BlobData";
        case CPSG_ReplyItem::eBlobInfo:       return "BlobInfo";
        case CPSG_ReplyItem::eSkippedBlob:    return "SkippedBlob";
        case CPSG_ReplyItem::eBioseqInfo:     return "BioseqInfo";
        case CPSG_ReplyItem::eNamedAnnotInfo: return "NamedAnnotInfo";
        case CPSG_ReplyItem::ePublicComment:  return "PublicComment";
        case CPSG_ReplyItem::eProcessor:      return "Processor";
        case CPSG_ReplyItem::eEndOfReply:     if (!trouble) return "Reply"; _TROUBLE;
    }

    return nullptr;
}

void CJsonResponse::Fill(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item)
{
    auto reply_item_type = reply_item->GetType();

    if (sm_SetReplyType) {
        Set("reply", s_GetItemName(reply_item_type));
    }

    if (SParams::verbose) {
        Set("processor_id", reply_item->GetProcessorId());
    }

    if (reply_item_status != EPSG_Status::eSuccess) {
        return Fill(reply_item, reply_item_status);
    }

    switch (reply_item_type) {
        case CPSG_ReplyItem::eBlobData:
            return Fill(static_pointer_cast<CPSG_BlobData>(reply_item));

        case CPSG_ReplyItem::eBlobInfo:
            return Fill(static_pointer_cast<CPSG_BlobInfo>(reply_item));

        case CPSG_ReplyItem::eSkippedBlob:
            return Fill(static_pointer_cast<CPSG_SkippedBlob>(reply_item));

        case CPSG_ReplyItem::eBioseqInfo:
            return Fill(static_pointer_cast<CPSG_BioseqInfo>(reply_item));

        case CPSG_ReplyItem::eNamedAnnotInfo:
            return Fill(static_pointer_cast<CPSG_NamedAnnotInfo>(reply_item));

        case CPSG_ReplyItem::ePublicComment:
            return Fill(static_pointer_cast<CPSG_PublicComment>(reply_item));

        case CPSG_ReplyItem::eProcessor:
            return Fill(reply_item, reply_item_status);

        case CPSG_ReplyItem::eEndOfReply:
            _TROUBLE;
            return;
    }
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobData> blob_data)
{
    Set("id", blob_data);
    ostringstream os;
    os << blob_data->GetStream().rdbuf();
    Set("data", NStr::Base64Encode(os.str()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_BlobInfo> blob_info)
{
    Set("id",                 blob_info);
    Set("compression",        blob_info->GetCompression());
    Set("format",             blob_info->GetFormat());
    Set("storage_size",       blob_info->GetStorageSize());
    Set("size",               blob_info->GetSize());
    Set("is_dead",            blob_info->IsDead());
    Set("is_suppressed",      blob_info->IsSuppressed());
    Set("is_withdrawn",       blob_info->IsWithdrawn());
    Set("hup_release_date",   blob_info->GetHupReleaseDate().AsString());
    Set("owner",              blob_info->GetOwner());
    Set("original_load_date", blob_info->GetOriginalLoadDate().AsString());
    Set("class",              objects::CBioseq_set::ENUM_METHOD_NAME(EClass)()->FindName(blob_info->GetClass(), true));
    Set("division",           blob_info->GetDivision());
    Set("username",           blob_info->GetUsername());
    Set("id2_info",           blob_info->GetId2Info());
    Set("n_chunks",           blob_info->GetNChunks());
}

string s_ReasonToString(CPSG_SkippedBlob::EReason reason)
{
    switch (reason) {
        case CPSG_SkippedBlob::eExcluded:   return "Excluded";
        case CPSG_SkippedBlob::eInProgress: return "InProgress";
        case CPSG_SkippedBlob::eSent:       return "Sent";
        case CPSG_SkippedBlob::eUnknown:    return "Unknown";
    };

    _TROUBLE;
    return "";
}

void CJsonResponse::Fill(shared_ptr<CPSG_SkippedBlob> skipped_blob)
{
    Set("id",     skipped_blob->GetId());
    Set("reason", s_ReasonToString(skipped_blob->GetReason()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info)
{
    const auto included_info = bioseq_info->IncludedInfo();

    if (included_info & CPSG_Request_Resolve::fCanonicalId)  Set("canonical_id",  bioseq_info->GetCanonicalId());
    if (included_info & CPSG_Request_Resolve::fOtherIds)     Set("other_ids",     bioseq_info->GetOtherIds());
    if (included_info & CPSG_Request_Resolve::fMoleculeType) Set("molecule_type", objects::CSeq_inst::ENUM_METHOD_NAME(EMol)()->FindName(bioseq_info->GetMoleculeType(), true));
    if (included_info & CPSG_Request_Resolve::fLength)       Set("length",        bioseq_info->GetLength());
    if (included_info & CPSG_Request_Resolve::fChainState)   Set("seq_state",     bioseq_info->GetChainState());
    if (included_info & CPSG_Request_Resolve::fState)        Set("state",         bioseq_info->GetState());
    if (included_info & CPSG_Request_Resolve::fBlobId)       Set("blob_id",       bioseq_info->GetBlobId());
    if (included_info & CPSG_Request_Resolve::fTaxId)        Set("tax_id",        TAX_ID_TO(Int8, bioseq_info->GetTaxId()));
    if (included_info & CPSG_Request_Resolve::fHash)         Set("hash",          bioseq_info->GetHash());
    if (included_info & CPSG_Request_Resolve::fDateChanged)  Set("date_changed",  bioseq_info->GetDateChanged().AsString());
    if (included_info & CPSG_Request_Resolve::fGi)           Set("gi",            GI_TO(Int8, bioseq_info->GetGi()));
}

void CJsonResponse::Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info)
{
    Set("blob_id", named_annot_info->GetBlobId());
    Set("id2_annot_info", named_annot_info->GetId2AnnotInfo());
}

void CJsonResponse::Fill(shared_ptr<CPSG_PublicComment> public_comment)
{
    Set("id",   public_comment);
    Set("text", public_comment->GetText());
}

template <class TItem>
void CJsonResponse::Fill(TItem item, EPSG_Status status)
{
    Set("status", s_StrStatus(status));

    for (;;) {
        auto message = item->GetNextMessage();

        if (message.empty()) return;

        m_JsonObj.insert_array("errors").push_back(message);
    }
}

void CJsonResponse::Set(CJson_Node node, const CPSG_BioId& bio_id)
{
    auto obj = node.ResetObject();
    Set(obj["id"], bio_id.GetId());

    const auto& type = bio_id.GetType();

    if (type != CPSG_BioId::TType::e_not_set) {
        Set(obj["type"], type);
    }
}

void CJsonResponse::Set(CJson_Node node, const vector<CPSG_BioId>& bio_ids)
{
    auto ar = node.ResetArray();

    for (const auto& bio_id : bio_ids) {
        ar.push_back();
        Set(ar.back(), bio_id);
    }
}

void CJsonResponse::Set(CJson_Node node, const CPSG_BlobId& blob_id)
{
    auto obj = node.ResetObject();
    Set(obj["id"], blob_id.GetId());

    const auto& last_modified = blob_id.GetLastModified();

    if (!last_modified.IsNull()) {
        Set(obj["last_modified"], last_modified.GetValue());
    }
}

void CJsonResponse::Set(CJson_Node node, const CPSG_ChunkId& chunk_id)
{
    auto obj = node.ResetObject();
    Set(obj["id2_chunk"], chunk_id.GetId2Chunk());
    Set(obj["id2_info"],  chunk_id.GetId2Info());
}

void SMetrics::OutputItems(ostream& os) const
{
    for (const auto& item : m_Items) {
        os << '\t' << s_GetItemName(item.first, false) << '=' << s_StrStatus(item.second);
    }
}

SParams::SParams(const CArgs& args) :
    service(args["service"].AsString()),
    user_args(args["user-args"].HasValue() ? args["user-args"].AsString() : SPSG_UserArgs())
{
    verbose = args["verbose"].HasValue();
}

SParallelProcessingParams::SParallelProcessingParams(const CArgs& a, bool br, bool e) :
    SParams(a),
    args(a),
    worker_threads(max(1, min(10, args["worker-threads"].AsInteger()))),
    batch_resolve(br),
    echo(e),
    pipe(args[batch_resolve ? "id-file" : "input-file"].AsString() == "-"),
    is(pipe ? cin : args[batch_resolve ? "id-file" : "input-file"].AsInputFile())
{
}

SPerformanceParams::SPerformanceParams(const CArgs& args) :
    SParams(args),
    user_threads(static_cast<size_t>(args["user-threads"].AsInteger())),
    delay(args["delay"].AsDouble()),
    local_queue(args["local-queue"].AsBoolean()),
    report_immediately(args["report-immediately"].AsBoolean()),
    os(args["output-file"].AsOutputFile())
{
}

SIoParams::SIoParams(const CArgs& args) :
    SParams(args),
    start_time(args["START_TIME"].AsInteger()),
    duration(args["DURATION"].AsInteger()),
    user_threads(args["USER_THREADS"].AsInteger()),
    download_size(args["DOWNLOAD_SIZE"].AsInteger())
{
}

template <class TItem, class TStr>
void s_ReportErrors(ostream& os, EPSG_Status status, TItem item, TStr prefix, const char* delim = "\n\t")
{
    os << prefix << s_StrStatus(status);

    for (;;) {
        auto message = item->GetNextMessage();

        if (message.empty()) break;

        os << delim << message;
    }

    os << '\n';
}

struct SDataOnlyCopy
{
    SDataOnlyCopy(const SOneRequestParams::SDataOnly& params) : m_Params(params) {}

    void ItemComplete(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item);
    void ReplyComplete(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply);

private:
    void Process(shared_ptr<CPSG_BlobInfo> blob_info);
    void Process(shared_ptr<CPSG_BlobData> blob_data);
    void Process(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info);
    void Process(shared_ptr<CPSG_BlobInfo> blob_info, shared_ptr<CPSG_BlobData> blob_data);

    const SOneRequestParams::SDataOnly& m_Params;
    unordered_map<string, pair<shared_ptr<CPSG_BlobInfo>, shared_ptr<CPSG_BlobData>>> m_Data;
};

void SDataOnlyCopy::ItemComplete(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    if (status != EPSG_Status::eSuccess) {
        stringstream ss;
        s_ReportErrors(ss, status, item, "Item error: ");
        cerr << ss.rdbuf();

    } else if (item->GetType() == CPSG_ReplyItem::eBlobInfo) {
        Process(static_pointer_cast<CPSG_BlobInfo>(item));

    } else if (item->GetType() == CPSG_ReplyItem::eBlobData) {
        Process(static_pointer_cast<CPSG_BlobData>(item));

    } else if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
        Process(static_pointer_cast<CPSG_NamedAnnotInfo>(item));
    }
}

void SDataOnlyCopy::ReplyComplete(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status != EPSG_Status::eSuccess) {
        stringstream ss;
        s_ReportErrors(ss, status, reply, "Reply error: ");
        cerr << ss.rdbuf();
    }
}

ESerialDataFormat s_GetInputFormat(const string& format)
{
    if (format == "asn1-text") return eSerial_AsnText;
    if (format == "xml")       return eSerial_Xml;
    if (format == "json")      return eSerial_Json;

    return eSerial_AsnBinary;
}

ESerialDataFormat s_GetOutputFormat(const CArgs& args)
{
    if (args.Exist("output-fmt") && args["output-fmt"].HasValue()) {
        const auto& format = args["output-fmt"].AsString();

        if (format == "asn")  return eSerial_AsnText;
        if (format == "asnb") return eSerial_AsnBinary;
        if (format == "xml")  return eSerial_Xml;
        if (format == "json") return eSerial_Json;
    }

    return eSerial_None;
}

TTypeInfo s_GetInputType(const shared_ptr<CPSG_BlobData>& blob_data)
{
    using namespace objects;

    if (auto chunk_id = blob_data->GetId<CPSG_ChunkId>()) {
        return chunk_id->GetId2Chunk() == 999999999 ? CID2S_Split_Info::GetTypeInfo() : CID2S_Chunk::GetTypeInfo();
    }

    return CSeq_entry::GetTypeInfo();
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_BlobInfo> blob_info)
{
    auto& p = m_Data[blob_info->GetId()->Repr()];

    if (p.second) {
        Process(move(blob_info), move(p.second));
    } else {
        p.first = move(blob_info);
    }
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_BlobData> blob_data)
{
    auto& p = m_Data[blob_data->GetId()->Repr()];

    if (p.first) {
        Process(move(p.first), move(blob_data));
    } else {
        p.second = move(blob_data);
    }
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_BlobInfo> blob_info, shared_ptr<CPSG_BlobData> blob_data)
{
    auto& is = blob_data->GetStream();

    if (m_Params.output_format == eSerial_None) {
        cout << is.rdbuf();
        return;
    }

    auto input_format = s_GetInputFormat(blob_info->GetFormat());
    unique_ptr<CObjectIStream> in;

    if (blob_info->GetCompression().find("zip") == string::npos) {
        in.reset(CObjectIStream::Open(input_format, is));
    } else {
        unique_ptr<CZipStreamDecompressor> zip(new CZipStreamDecompressor);
        unique_ptr<CCompressionIStream> compressed_is(new CCompressionIStream(is, zip.release(), CCompressionIStream::fOwnProcessor));
        in.reset(CObjectIStream::Open(input_format, *compressed_is.release(), eTakeOwnership));
    }

    _ASSERT(in);
    in->UseMemoryPool();

    stringstream ss;
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(m_Params.output_format, ss));
    CObjectStreamCopier copier(*in, *out);

    try {
        copier.Copy(s_GetInputType(blob_data));
    }
    catch (CException& ex) {
        cerr << "Failed to process blob '" << blob_data->GetId()->Repr() << "': " << ex.ReportThis() << endl;
        return;
    }

    cout << ss.rdbuf();
}

void SDataOnlyCopy::Process(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info)
{
    if (m_Params.output_format == eSerial_None) {
        cout << NStr::Base64Decode(named_annot_info->GetId2AnnotInfo());
        return;
    }

    for (const auto& info : named_annot_info->GetId2AnnotInfoList() ) {
        cout << MSerial_Format(m_Params.output_format) << *info;
    }
}

bool s_GetDataOnly(const CArgs& args)
{
    return (args.Exist("blob-only") && args["blob-only"].HasValue()) ||
        (args.Exist("annot-only") && args["annot-only"].HasValue());
}

SOneRequestParams::SOneRequestParams(const CArgs& args) :
    SParams(args),
    latency({args["latency"].HasValue(), args["debug-printout"].HasValue()}),
    data_only({s_GetDataOnly(args), s_GetOutputFormat(args)})
{
}

void CProcessing::ItemComplete(SJsonOut& output, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    CJsonResponse result_doc(status, item);
    output << result_doc;
}

void CProcessing::ReplyComplete(SJsonOut& output, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status != EPSG_Status::eSuccess) {
        CJsonResponse result_doc(status, reply);
        output << result_doc;
    }
}

int CProcessing::OneRequest(const SOneRequestParams params, shared_ptr<CPSG_Request> request)
{
    SDataOnlyCopy data_only_copy(params.data_only);
    CLogLatencyReport latency_report{
        "/objtools/pubseq_gateway/client/",
        R"(\S+: (\S+:[0-9]+)/\S+?\S+&client_id=\S+)",
        R"(\S+: Closed with status \S+)"
    };

    if (params.latency.enabled) {
        latency_report.Start();
        latency_report.SetDebug(params.latency.debug);
        TPSG_DebugPrintout::SetDefault(TPSG_DebugPrintout::TValue::eSome);
    }

    CPSG_EventLoop queue;
    SJsonOut json_out;

    using namespace placeholders;

    if (params.data_only.enabled) {
        auto item_complete = bind(&SDataOnlyCopy::ItemComplete, &data_only_copy, _1, _2);
        auto reply_complete = bind(&SDataOnlyCopy::ReplyComplete, &data_only_copy, _1, _2);
        queue = CPSG_EventLoop(params.service, item_complete, reply_complete);
    } else {
        auto item_complete = bind(&CProcessing::ItemComplete, ref(json_out), _1, _2);
        auto reply_complete = bind(&CProcessing::ReplyComplete, ref(json_out), _1, _2);
        queue = CPSG_EventLoop(params.service, item_complete, reply_complete);
    }

    queue.SetUserArgs(params.user_args);

    _VERIFY(queue.SendRequest(request, CDeadline::eInfinite));
    queue.Stop();
    _VERIFY(queue.Run(CDeadline::eInfinite));
    return 0;
}

CParallelProcessing::CParallelProcessing(const SParallelProcessingParams& params) :
    m_JsonOut(params.pipe)
{
    using namespace placeholders;
    auto item_complete = bind(params.batch_resolve ? &CProcessing::ItemComplete : &Interactive::ItemComplete, ref(m_JsonOut), _1, _2);
    auto reply_complete = bind(params.batch_resolve ? &CProcessing::ReplyComplete : &Interactive::ReplyComplete, ref(m_JsonOut), _1, _2);

    for (int n = params.worker_threads; n > 0; --n) {
        m_PsgQueues.emplace_back(params.service, item_complete, reply_complete);
        auto& queue = m_PsgQueues.back();
        queue.SetUserArgs(params.user_args);
        m_Threads.emplace_back(&CPSG_EventLoop::Run, ref(queue), CDeadline::eInfinite);

        if (params.batch_resolve) {
            m_Threads.emplace_back(&BatchResolve::Submitter, ref(m_InputQueue), ref(queue), cref(params.args));
        } else {
            m_Threads.emplace_back(&Interactive::Submitter, ref(m_InputQueue), ref(queue), ref(m_JsonOut), params.echo);
        }
    }
}

CParallelProcessing::~CParallelProcessing()
{
    m_InputQueue.Stop(m_InputQueue.eDrain);
}

void CParallelProcessing::BatchResolve::Submitter(TInputQueue& input, CPSG_Queue& output, const CArgs& args)
{
    auto request_context = CDiagContext::GetRequestContext().Clone();
    auto type(args["type"].HasValue() ? SRequestBuilder::GetBioIdType(args["type"].AsString()) : CPSG_BioId::TType());
    auto include_info(SRequestBuilder::GetIncludeInfo(SRequestBuilder::GetSpecified<CPSG_Request_Resolve>(args)));

    string id;

    while (input.Pop(id)) {
        _ASSERT(!id.empty()); // ReadLine makes sure it's not empty
        auto bio_id = CPSG_BioId(id, type);
        auto user_context = make_shared<string>(move(id));
        auto request = make_shared<CPSG_Request_Resolve>(move(bio_id), move(user_context), move(request_context));

        request->IncludeInfo(include_info);

        _VERIFY(output.SendRequest(move(request), CDeadline::eInfinite));
    }

    output.Stop();
}

void CParallelProcessing::Interactive::Submitter(TInputQueue& input, CPSG_Queue& output, SJsonOut& json_out, bool echo)
{
    CJson_Document json_schema_doc(CProcessing::RequestSchema());
    // cout << boolalpha << json_schema_doc.ReadSucceeded() << '|' << json_schema_doc.GetReadError() << endl;
    // cout << json_schema_doc.ToString() << endl;

    CJson_Schema json_schema(json_schema_doc);
    string line;

    while (input.Pop(line)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty

        CJson_Document json_doc;

        if (!json_doc.ParseString(line)) {
            json_out << CJsonResponse(s_GetId(json_doc), eJsonRpc_ParseError, json_doc.GetReadError());
        } else if (!json_schema.Validate(json_doc)) {
            json_out << CJsonResponse(s_GetId(json_doc), eJsonRpc_InvalidRequest, json_schema.GetValidationError());
        } else {
            if (echo) json_out << json_doc;

            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto id = json_obj["id"].GetValue().GetString();
            auto params_obj = json_obj["params"].GetObject();
            auto user_args = params_obj.has("user_args") ? params_obj["user_args"].GetValue().GetString() : string();
            auto user_context = make_shared<string>(id);

            SInteractiveNewRequestStart new_request_start(params_obj);
            auto request_context = new_request_start.Clone();

            if (auto request = SRequestBuilder::Build(method, params_obj, user_args, move(user_context), move(request_context))) {
                _VERIFY(output.SendRequest(move(request), CDeadline::eInfinite));
            }
        }
    }

    output.Stop();
}

void CParallelProcessing::Interactive::ReplyComplete(SJsonOut& output, EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status != EPSG_Status::eSuccess) {
        const auto request = reply->GetRequest();
        CRequestContextGuard_Base guard(request->GetRequestContext());
        guard.SetStatus(s_PsgStatusToRequestStatus(status));

        const auto& request_id = *request->GetUserContext<string>();

        CJsonResponse result_doc(status, reply);
        output << CJsonResponse(request_id, result_doc);
    }
}

void CParallelProcessing::Interactive::ItemComplete(SJsonOut& output, EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    const auto request = item->GetReply()->GetRequest();
    const auto& request_id = *request->GetUserContext<string>();

    CJsonResponse result_doc(status, item);
    output << CJsonResponse(request_id, result_doc);
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_ReplyItem>& item)
{
    return item->GetReply();
}

shared_ptr<CPSG_Reply> s_GetReply(shared_ptr<CPSG_Reply>& reply)
{
    return reply;
}

CRequestStatus::ECode s_PsgStatusToRequestStatus(EPSG_Status psg_status)
{
    switch (psg_status) {
        case EPSG_Status::eSuccess:  return CRequestStatus::e200_Ok;
        case EPSG_Status::eNotFound: return CRequestStatus::e404_NotFound;
        case EPSG_Status::eCanceled: return CRequestStatus::e499_BrokenConnection;
        case EPSG_Status::eForbidden:return CRequestStatus::e403_Forbidden;
        case EPSG_Status::eError:    return CRequestStatus::e400_BadRequest;
        case EPSG_Status::eInProgress: _TROUBLE; break;
    }

    return CRequestStatus::e500_InternalServerError;
}

string s_GetId(const CJson_Document& req_doc)
{
    string id;

    if (req_doc.IsObject()) {
        auto req_obj = req_doc.GetObject();

        if (req_obj.has("id")) {
            auto id_node = req_obj["id"];

            if (id_node.IsValue()) {
                auto id_value = id_node.GetValue();

                if (id_value.IsString()) {
                    id = id_value.GetString();
                }
            }
        }
    }

    return id;
}

template <class TCreateContext>
vector<shared_ptr<CPSG_Request>> CProcessing::ReadCommands(TCreateContext create_context, size_t report_progress_after)
{
    static CJson_Schema json_schema(RequestSchema());
    string line;
    vector<shared_ptr<CPSG_Request>> requests;
    unordered_set<string> ids;

    // Read requests from cin
    while (ReadLine(line)) {
        CJson_Document json_doc;

        if (!json_doc.ParseString(line)) {
            cerr << "Error in request '" << line << "': " << json_doc.GetReadError() << endl;
            return {};
        }

        auto id = s_GetId(json_doc);

        if (id.empty()) {
            cerr << "Error in request '" << line << "': no id or id is empty" << endl;
            return {};
        } else if (!json_schema.Validate(json_doc)) {
            cerr << "Error in request '" << id << "': " << json_schema.GetValidationError() << endl;
            return {};
        } else if (!ids.insert(id).second) {
            cerr << "Error in request '" << id << "': duplicate ID" << endl;
            return {};
        } else {
            CJson_ConstObject json_obj(json_doc.GetObject());
            auto method = json_obj["method"].GetValue().GetString();
            auto params_obj = json_obj["params"].GetObject();
            auto user_args = params_obj.has("user_args") ? params_obj["user_args"].GetValue().GetString() : string();
            auto user_context = create_context(id, params_obj);

            if (!user_context) return {};

            if (auto request = SRequestBuilder::Build(method, params_obj, user_args, move(user_context))) {
                requests.emplace_back(move(request));
                if (report_progress_after && (requests.size() % report_progress_after == 0)) cerr << '.';
            }
        }
    }

    return requests;
}

int CProcessing::Performance(const SPerformanceParams params)
{
    if (params.delay < 0.0) {
        cerr << "DELAY must be non-negative" << endl;
        return -1;
    }

    const size_t kReportProgressAfter = 2000;

    using TReplyStorage = deque<shared_ptr<CPSG_Reply>>;
    SIoRedirector io_redirector(cout, params.os);

    if (SParams::verbose) cerr << "Preparing requests: ";
    auto requests = ReadCommands([](string id, CJson_ConstNode&){ return make_shared<SMetrics>(move(id)); }, SParams::verbose ? kReportProgressAfter : 0);

    if (requests.empty()) return -1;

    atomic_size_t start(params.user_threads);
    atomic_int to_submit(static_cast<int>(requests.size()));
    auto wait = [&]() { while (start > 0) this_thread::sleep_for(chrono::microseconds(1)); };

    auto l = [&](CPSG_Queue& queue, TReplyStorage& replies) {
        start--;
        wait();

        for (;;) {
            auto i = to_submit--;

            if (i <= 0) break;

            // Submit and get response
            auto& request = requests[requests.size() - i];
            auto metrics = request->GetUserContext<SMetrics>();

            metrics->Set(SMetricType::eStart);
            auto reply = queue.SendRequestAndGetReply(request, CDeadline::eInfinite);
            metrics->Set(SMetricType::eSubmit);

            _ASSERT(reply);

            metrics->Set(SMetricType::eReply);

            for (;;) {
                auto reply_item = reply->GetNextItem(CDeadline::eInfinite);
                _ASSERT(reply_item);

                const auto type = reply_item->GetType();

                if (type == CPSG_ReplyItem::eEndOfReply) break;

                const auto status = reply_item->GetStatus(CDeadline::eInfinite);
                metrics->AddItem({type, status});
            }

            const auto status = reply->GetStatus(CDeadline::eInfinite);
            metrics->Set(SMetricType::eDone);
            metrics->AddItem({CPSG_ReplyItem::eEndOfReply, status});

            if (params.report_immediately) {
                // Metrics are reported on destruction
                metrics.reset();
                request.reset();
                reply.reset();
            } else {
                // Store the reply for now to prevent metrics from being written to cout (affects performance)
                replies.emplace_back(reply);
            }

            if (params.delay) {
                this_thread::sleep_for(chrono::duration<double>(params.delay));
            }
        }
    };

    vector<CPSG_Queue> queues;
    vector<TReplyStorage> replies(params.user_threads);

    for (size_t i = 0; i < (params.local_queue ? params.user_threads : 1); ++i) {
        queues.emplace_back(params.service);
        queues.back().SetUserArgs(params.user_args);
    }

    vector<thread> threads;
    threads.reserve(params.user_threads);

    // Start threads in advance so it won't affect metrics
    for (size_t i = 0; i < params.user_threads; ++i) {
        threads.emplace_back(l, ref(queues[params.local_queue ? i : 0]), ref(replies[i]));
    }

    wait();

    // Start processing replies
    if (SParams::verbose) {
        cerr << "\nSubmitting requests: ";
        size_t previous = requests.size() / kReportProgressAfter;

        while (to_submit > 0) {
            size_t current = to_submit / kReportProgressAfter;

            for (auto i = current; i < previous; ++i) {
                cerr << '.';
            }

            previous = current;
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        cerr << "\nWaiting for threads: " << params.user_threads << '\n';
    }

    for (auto& t : threads) {
        t.join();
    }

    // Release all replies held in queues, if any
    queues.clear();

    // Output metrics
    if (SParams::verbose) cerr << "Outputting metrics: ";
    requests.clear();
    size_t output = 0;

    for (auto& thread_replies : replies) {
        for (auto& reply : thread_replies) {
            reply.reset();
            if (SParams::verbose && (++output % kReportProgressAfter == 0)) cerr << '.';
        }
    }

    if (SParams::verbose) cerr << '\n';
    return 0;
}

struct STestingContext : string
{
    enum EExpected { eSuccess, eFailure };

    EExpected expected;

    STestingContext(string id, EExpected e) : string(move(id)), expected(e) {}

    static shared_ptr<STestingContext> CreateContext(string id, CJson_ConstObject params_obj);
};

shared_ptr<STestingContext> STestingContext::CreateContext(string id, CJson_ConstObject params_obj)
{
    string error;

    try {
        if (params_obj.has("expected_result")) {
            auto expected = params_obj["expected_result"].GetValue().GetString();
            auto result = expected == "success" ? eSuccess : eFailure;
            return make_shared<STestingContext>(move(id), result);
        } else {
            error = "no 'expected_result' found";
        }
    }
    catch (exception& e) {
        error = e.what();
    }

    cerr << "Error in request '" << id << "': " << error << endl;
    return {};
}

struct SExitCode
{
    enum { eSuccess = 0, eRunError = -1, eTestFail = -2, };

    // eRunError has the highest priority and eSuccess - the lowest
    void operator =(int rv) { if ((m_RV != eRunError) && (rv != eSuccess)) m_RV = rv; }

    operator int() const { return m_RV; }

private:
    int m_RV = eSuccess;
};

int s_CheckItems(bool expect_errors, const string& request_id, shared_ptr<CPSG_Reply> reply)
{
    bool no_errors = true;

    for (;;) {
        auto reply_item = reply->GetNextItem(CDeadline::eInfinite);
        _ASSERT(reply_item);

        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

        const auto status = reply_item->GetStatus(CDeadline::eInfinite);

        if (status == EPSG_Status::eSuccess) {
            try {
                CJsonResponse check(status, reply_item, CJsonResponse::eRethrow);
            }
            catch (exception& e) {
                cerr << "Error on reading reply item for request '" << request_id << "': " << e.what() << endl;
                return SExitCode::eRunError;
            }

        } else if (!expect_errors) {
            cerr << "Fail on getting item for request '" << request_id << "' expected to succeed" << endl;
            return SExitCode::eTestFail;

        } else {
            no_errors = false;
        }
    }

    if (expect_errors && no_errors) {
        cerr << "Success on getting all items for request '" << request_id << "' expected to fail" << endl;
        return SExitCode::eTestFail;
    }

    return SExitCode::eSuccess;
}

int CProcessing::Testing(const SParams params)
{
    const TPSG_RequestTimeout kDefaultTimeout(TPSG_RequestTimeout::eGetDefault);
    CPSG_Queue queue(params.service);
    queue.SetUserArgs(params.user_args);
    ifstream input_file("psg_client_test.json");
    SIoRedirector ior(cin, input_file);

    if (!input_file) {
        cerr << "Failed to read 'psg_client_test.json'" << endl;
        return SExitCode::eRunError;
    }

    auto requests = ReadCommands(&STestingContext::CreateContext);

    if (requests.empty()) return SExitCode::eRunError;

    SExitCode rv;

    for (const auto& request : requests) {
        const CDeadline deadline(kDefaultTimeout);
        auto expected_result = request->GetUserContext<STestingContext>();
        const auto& request_id = *expected_result;

        auto reply = queue.SendRequestAndGetReply(request, deadline);
        _ASSERT(reply);

        const bool expect_errors = expected_result->expected == STestingContext::eFailure;
        auto reply_status = reply->GetStatus(deadline);

        if (reply_status == EPSG_Status::eInProgress) {
            rv = SExitCode::eTestFail;
            cerr << "Timeout for request '" << request_id << "' expected to " << (expect_errors ? "fail" : "succeed") << '\n';

        } else if (reply_status != EPSG_Status::eSuccess) {
            if (!expect_errors) {
                rv = SExitCode::eTestFail;
                string prefix("Fail for request '" + request_id + "' expected to succeed: ");
                stringstream ss;
                s_ReportErrors(ss, reply_status, reply, prefix, ", ");
                cerr << ss.rdbuf();
            }

        } else {
            rv = s_CheckItems(expect_errors, request_id, reply);
        }
    }

    return rv;
}

bool CProcessing::ReadLine(string& line, istream& is)
{
    for (;;) {
        if (!getline(is, line)) {
            return false;
        } else if (!line.empty()) {
            return true;
        }
    }
}

SInteractiveNewRequestStart::SInteractiveNewRequestStart(CJson_ConstObject params_obj)
{
    // All JSON types have already been validated with the scheme

    auto context = params_obj.find("context");
    auto& ctx = CDiagContext::GetRequestContext();

    if (context != params_obj.end()) {
        auto context_obj = context->value.GetObject();

        auto sid = context_obj.find("sid");

        if (sid != context_obj.end()) {
            ctx.SetSessionID(sid->value.GetValue().GetString());
        }

        auto phid = context_obj.find("phid");

        if (phid != context_obj.end()) {
            ctx.SetHitID(phid->value.GetValue().GetString());
        }

        auto client_ip = context_obj.find("client_ip");

        if (client_ip != context_obj.end()) {
            ctx.SetClientIP(client_ip->value.GetValue().GetString());
        }
    }

    if (!ctx.IsSetSessionID()) ctx.SetSessionID();
    if (!ctx.IsSetHitID())     ctx.SetHitID();

    SExtra extra;
    extra.Print("params", params_obj);
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstValue json)
{
    _ASSERT(json.IsNumber());

    if (json.IsInt4()) {
        Print(prefix, json.GetInt4());
    } else if (json.IsUint4()) {
        Print(prefix, json.GetUint4());
    } else if (json.IsInt8()) {
        Print(prefix, json.GetInt8());
    } else if (json.IsUint8()) {
        Print(prefix, json.GetUint8());
    } else if (json.IsDouble()) {
        Print(prefix, json.GetDouble());
    } else {
        _TROUBLE;
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstArray json)
{
    for (size_t i = 0; i < json.size(); ++i) {
        Print(prefix + '[' + to_string(i) + ']', json[i]);
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstObject json)
{
    for (const auto& pair : json) {
        Print(prefix + '.' + pair.name, pair.value);
    }
}

void SInteractiveNewRequestStart::SExtra::Print(const string& prefix, CJson_ConstNode json)
{
    switch (json.GetType()) {
        case CJson_ConstNode::eNull:
            Print(prefix, "<null>");
            break;

        case CJson_ConstNode::eBool:
            Print(prefix, json.GetValue().GetBool() ? "true" : "false");
            break;

        case CJson_ConstNode::eString:
            Print(prefix, json.GetValue().GetString());
            break;

        case CJson_ConstNode::eNumber:
            Print(prefix, json.GetValue());
            break;

        case CJson_ConstNode::eArray:
            Print(prefix, json.GetArray());
            break;

        case CJson_ConstNode::eObject:
            Print(prefix, json.GetObject());
            break;
    };
}

int CProcessing::ParallelProcessing(const SParallelProcessingParams params)
{
    CParallelProcessing parallel_processing(params);
    string line;

    while (ReadLine(line, params.is)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty
        parallel_processing(move(line));
    }

    return 0;
}

class CPSG_Request_Io : public CPSG_Request
{
public:
    CPSG_Request_Io(size_t size) :
        m_AbsPathRef("/TEST/io?return_data_size=" + to_string(size))
    {}

private:
    string x_GetType() const override { return "io"; }
    string x_GetId() const override { return ""; }
    void x_GetAbsPathRef(ostream& os) const override { os << m_AbsPathRef; }

    const string m_AbsPathRef;
};

struct SIoContext
{
    const SIoParams& params;
    mutex m;
    condition_variable cv;

    SIoContext(const SIoParams& p) : params(p), m_Work(true) {}

    bool Work() const { return m_Work; }
    void Stop() { m_Work = false; }

private:
    atomic<bool> m_Work;
};

struct SIoWorker
{
    size_t errors = 0;

    SIoWorker(SIoContext& context) :
        m_Context(context),
        m_Thread(&SIoWorker::Do, this)
    {}

    void Do();
    void Join() { m_Thread.join(); }

private:
    SIoContext& m_Context;
    thread m_Thread;
    deque<shared_ptr<CPSG_Reply>> m_Replies;
};

void SIoWorker::Do()
{
    const CDeadline kInfinite = CDeadline::eInfinite;

    CPSG_Queue queue(m_Context.params.service);
    queue.SetUserArgs(m_Context.params.user_args);
    auto request = make_shared<CPSG_Request_Io>(m_Context.params.download_size);
    ostringstream err_stream;

    // Wait
    {
        unique_lock<mutex> lock(m_Context.m);
        m_Context.cv.wait(lock);
    }

    // Submit requests and receive response
    while (m_Context.Work()) {
        // Submit and get response
        auto reply = queue.SendRequestAndGetReply(request, kInfinite);
        _ASSERT(reply);

        // Store the reply for now to prevent internal metrics from being written to cout (affects performance)
        m_Replies.emplace_back(reply);

        auto reply_status = reply->GetStatus(kInfinite);
        bool success = reply_status == EPSG_Status::eSuccess;

        if (!success) {
            s_ReportErrors(err_stream, reply_status, reply, "Warning: Reply error ");
        }

        // Items
        while (m_Context.Work()) {
            auto reply_item = reply->GetNextItem(kInfinite);
            _ASSERT(reply_item);

            if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;

            auto item_status = reply_item->GetStatus(kInfinite);

            if (item_status != EPSG_Status::eSuccess) {
                success = false;
                s_ReportErrors(err_stream, reply_status, reply, "Warning: Item error ");
            }
        }

        if (!m_Context.Work()) break;

        if (!success) ++errors;
    }

    cerr << err_stream.str();
};

struct SIoOutput : stringstream, SIoRedirector
{
    SIoOutput() : SIoRedirector(cout, *this) {}

    void Reset() { SIoRedirector::Reset(); stringstream::seekg(0); }
    void Output(size_t errors);
};

void SIoOutput::Output(size_t errors)
{
    map<size_t, vector<SMessage>> raw_data;

    while (*this) {
        size_t request;
        SMessage message;

        if ((*this >> request >> message) && (message.type != SMetricType::eError)) {
            raw_data[request].emplace_back(std::move(message));
        }
    }

    cout << "Requests: " << raw_data.size() << endl;
    cout << "Errors: " << errors << endl;

    vector<double> stats;
    stats.reserve(raw_data.size());

    for (const auto& pair: raw_data) {
        const auto& messages = pair.second;
        auto send = find_if(messages.begin(), messages.end(), SMessage::IsSameType<SMetricType::eSend>);

        if (send == messages.end()) {
            cerr << "Warning: Cannot find event 'Send' for request '" << pair.first << endl;
            continue;
        }

        auto close = find_if(messages.begin(), messages.end(), SMessage::IsSameType<SMetricType::eClose>);

        if (close == messages.end()) {
            cerr << "Warning: Cannot find event 'Close' for request '" << pair.first << endl;
            continue;
        }

        stats.emplace_back(close->milliseconds - send->milliseconds);
    }


    const auto size = static_cast<double>(stats.size());
    const double avg = accumulate(stats.begin(), stats.end(), 0.0) / size;
    sort(stats.begin(), stats.end());

    cout << "Avg: " << avg << endl;
    cout << "Min: " << stats.front() << endl;
    cout << " 1%: " << stats[size_t(max(0.0, 0.01 * size - 1))] << endl;
    cout << "10%: " << stats[size_t(max(0.0, 0.10 * size - 1))] << endl;
    cout << "25%: " << stats[size_t(max(0.0, 0.25 * size - 1))] << endl;
    cout << "50%: " << stats[size_t(max(0.0, 0.50 * size - 1))] << endl;
    cout << "75%: " << stats[size_t(max(0.0, 0.75 * size - 1))] << endl;
    cout << "90%: " << stats[size_t(max(0.0, 0.90 * size - 1))] << endl;
    cout << "99%: " << stats[size_t(max(0.0, 0.99 * size - 1))] << endl;
    cout << "Max: " << stats.back() << endl;
}

int CProcessing::Io(const SIoParams params)
{
    SIoOutput io_output;

    // Half a second delay between threads start and actual work
    chrono::milliseconds kWarmUpDelay(500);

    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eIo);

    auto now = chrono::system_clock::now();
    auto start = chrono::system_clock::from_time_t(params.start_time);
    auto sleep = chrono::duration_cast<chrono::milliseconds>(start - now) - kWarmUpDelay;

    if (sleep.count() <= 0) {
        cerr << "Warning: Start time (" << params.start_time << ") has already passed or too close\n";
        sleep = chrono::milliseconds::zero();
    }

    this_thread::sleep_for(sleep);

    SIoContext context(params);

    vector<SIoWorker> threads;
    threads.reserve(params.user_threads);

    // Start threads in advance so it won't affect metrics
    for (int i = 0; i < params.user_threads; ++i) {
        threads.emplace_back(context);
    }

    this_thread::sleep_for(kWarmUpDelay);
    context.cv.notify_all();

    if (params.duration < 1) {
        cerr << "Warning: Duration (" << params.duration << ") is less that a second\n";
    } else {
        this_thread::sleep_for(chrono::seconds(params.duration));
    }

    context.Stop();

    size_t errors = 0;

    for (auto& t : threads) {
        t.Join();

        errors += t.errors;
    }

    // Make internal metrics be written to (redirected) cout
    threads.clear();

    // Report statistics
    auto start_format = CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeFrac);
    CTime start_ctime(params.start_time);
    auto start_ctime_str = start_ctime.GetLocalTime().AsString(start_format);

    io_output.Reset();

    cout << "Start: " << start_ctime_str << " = " << start_ctime.GetTimeT() << "." << setfill('0') << setw(3) << start_ctime.MilliSecond() << endl;
    cout << "Duration: " << static_cast<double>(params.duration) << endl;
    cout << "Threads: " << params.user_threads << endl;
    cout << "Size: " << params.download_size << endl;

    io_output.Output(errors);
    return 0;
}

int CProcessing::JsonCheck(istream* schema_is, istream& doc_is)
{
    CJson_Document schema_doc;

    if (!schema_is) {
        schema_doc = RequestSchema();

    } else if (!schema_doc.Read(*schema_is)) {
        cerr << "Error on reading JSON schema: " << schema_doc.GetReadError() << endl;
        return -1;
    }

    CJson_Schema schema(schema_doc);
    string line;
    size_t line_no = 0;
    int rv = 0;

    while (ReadLine(line, doc_is)) {
        CJson_Document input_doc;
        ++line_no;

        if (!input_doc.ParseString(line)) {
            cerr << "Error on reading JSON document (" << line_no << "): " << input_doc.GetReadError() << endl;
            if (rv == 0) rv = -2;
        }

        if (schema.Validate(input_doc)) {
        } else {
            cerr << "Error on validating JSON document (" << line_no << "): " << schema.GetValidationError() << endl;
            if (rv == 0) rv = -3;
        }
    }

    cout << "JSON documents (" << line_no << ") are valid" << endl;
    return rv;
}

CPSG_BioId::TType SRequestBuilder::GetBioIdType(string type)
{
    CObjectTypeInfo info(objects::CSeq_id::GetTypeInfo());

    if (auto index = info.FindVariantIndex(type)) return static_cast<CPSG_BioId::TType>(index);
    if (auto value = objects::CSeq_id::WhichInverseSeqId(type)) return value;

    return static_cast<CPSG_BioId::TType>(atoi(type.c_str()));
}

CPSG_BioId SRequestBuilder::GetBioId(const CArgs& input)
{
    const auto& id = input["ID"].AsString();

    if (!input["type"].HasValue()) return CPSG_BioId(id);

    const auto type = GetBioIdType(input["type"].AsString());
    return CPSG_BioId(id, type);
}

CPSG_BioId SRequestBuilder::GetBioId(const CJson_ConstObject& input)
{
    auto array = input["bio_id"].GetArray();
    auto id = array[0].GetValue().GetString();

    if (array.size() == 1) return CPSG_BioId(id);

    auto value = array[1].GetValue();
    auto type = value.IsString() ? GetBioIdType(value.GetString()) : static_cast<CPSG_BioId::TType>(value.GetInt4());
    return CPSG_BioId(id, type);
}

CPSG_BlobId SRequestBuilder::GetBlobId(const CArgs& input)
{
    const auto& id = input["ID"].AsString();
    const auto& last_modified = input["last-modified"];
    return last_modified.HasValue() ? CPSG_BlobId(id, last_modified.AsInt8()) : id;
}

CPSG_BlobId SRequestBuilder::GetBlobId(const CJson_ConstObject& input)
{
    auto array = input["blob_id"].GetArray();
    auto id = array[0].GetValue().GetString();
    return array.size() > 1 ? CPSG_BlobId(move(id), array[1].GetValue().GetInt8()) : move(id);
}

CPSG_ChunkId SRequestBuilder::GetChunkId(const CArgs& input)
{
    return { input["ID2_CHUNK"].AsInteger(), input["ID2_INFO"].AsString() };
}

CPSG_ChunkId SRequestBuilder::GetChunkId(const CJson_ConstObject& input)
{
    auto array = input["chunk_id"].GetArray();
    return { static_cast<int>(array[0].GetValue().GetInt4()), array[1].GetValue().GetString() };
}

vector<string> SRequestBuilder::GetNamedAnnots(const CJson_ConstObject& input)
{
    auto na_array = input["named_annots"].GetArray();
    CPSG_Request_NamedAnnotInfo::TAnnotNames names;

    for (const auto& na : na_array) {
        names.push_back(na.GetValue().GetString());
    }

    return names;
}

CPSG_Request_Resolve::TIncludeInfo SRequestBuilder::GetIncludeInfo(TSpecified specified)
{
    const auto& info_flags = GetInfoFlags();

    auto i = info_flags.begin();
    bool all_info_except = specified(i->name);
    CPSG_Request_Resolve::TIncludeInfo include_info = all_info_except ? CPSG_Request_Resolve::fAllInfo : CPSG_Request_Resolve::TIncludeInfo(0);

    for (++i; i != info_flags.end(); ++i) {
        if (specified(i->name)) {
            if (all_info_except) {
                include_info &= ~i->value;
            } else {
                include_info |= i->value;
            }
        }
    }

    // Provide all info if nothing is specified explicitly
    return include_info ? include_info : CPSG_Request_Resolve::fAllInfo;
}

void SRequestBuilder::ExcludeTSEs(shared_ptr<CPSG_Request_Biodata> request, const CArgs& input)
{
    if (!input["exclude-blob"].HasValue()) return;

    auto blob_ids = input["exclude-blob"].GetStringList();

    for (const auto& blob_id : blob_ids) {
        request->ExcludeTSE(blob_id);
    }
}

void SRequestBuilder::ExcludeTSEs(shared_ptr<CPSG_Request_Biodata> request, const CJson_ConstObject& input)
{
    if (!input.has("exclude_blobs")) return;

    auto blob_ids = input["exclude_blobs"].GetArray();

    for (const auto& blob_id : blob_ids) {
        request->ExcludeTSE(blob_id.GetValue().GetString());
    }
}

const initializer_list<SDataFlag> kDataFlags =
{
    { "no-tse",    "Return only the info",                                  CPSG_Request_Biodata::eNoTSE    },
    { "slim-tse",  "Return split info blob if available, or nothing",       CPSG_Request_Biodata::eSlimTSE  },
    { "smart-tse", "Return split info blob if available, or original blob", CPSG_Request_Biodata::eSmartTSE },
    { "whole-tse", "Return all split blobs if available, or original blob", CPSG_Request_Biodata::eWholeTSE },
    { "orig-tse",  "Return original blob",                                  CPSG_Request_Biodata::eOrigTSE  },
};

const initializer_list<SDataFlag>& SRequestBuilder::GetDataFlags()
{
    return kDataFlags;
}

const initializer_list<SInfoFlag> kInfoFlags =
{
    { "all-info-except", "Return all info except explicitly specified by other flags", CPSG_Request_Resolve::fAllInfo },
    { "canonical-id",    "Return canonical ID info",                    CPSG_Request_Resolve::fCanonicalId  },
    { "name",            "Use name for canonical ID info, if returned", CPSG_Request_Resolve::fName         },
    { "other-ids",       "Return other IDs info",                       CPSG_Request_Resolve::fOtherIds     },
    { "molecule-type",   "Return molecule type info",                   CPSG_Request_Resolve::fMoleculeType },
    { "length",          "Return length info",                          CPSG_Request_Resolve::fLength       },
    { "chain-state",     "Return chain state info (in seq_state pair)", CPSG_Request_Resolve::fChainState   },
    { "state",           "Return state info",                           CPSG_Request_Resolve::fState        },
    { "blob-id",         "Return blob ID info",                         CPSG_Request_Resolve::fBlobId       },
    { "tax-id",          "Return tax ID info",                          CPSG_Request_Resolve::fTaxId        },
    { "hash",            "Return hash info",                            CPSG_Request_Resolve::fHash         },
    { "date-changed",    "Return date changed info",                    CPSG_Request_Resolve::fDateChanged  },
    { "gi",              "Return GI",                                   CPSG_Request_Resolve::fGi           },
};

const initializer_list<SInfoFlag>& SRequestBuilder::GetInfoFlags()
{
    return kInfoFlags;
}

CJson_Document CProcessing::RequestSchema()
{
    return CJson_Document(R"REQUEST_SCHEMA(
{
    "type": "object",
    "definitions": {
        "jsonrpc": {
            "$id": "#jsonrpc",
            "enum": [ "2.0" ]
        },
        "id": {
            "$id": "#id",
            "type": "string"
        },
        "bio_id": {
            "$id": "#bio_id",
            "type": "array",
            "items": [
                {
                    "type": "string"
                },
                {
                    "oneOf": [
                        {
                            "type": "string"
                        },
                        {
                            "type": "number"
                        }
                    ]
                }
            ],
            "minItems": 1,
            "maxItems": 2
        },
        "blob_id": {
            "$id": "#blob_id",
            "type": "array",
            "items": [
                {
                    "type": "string"
                },
                {
                    "type": "number"
                }
            ],
            "minItems": 1,
            "maxItems": 2
        },
        "chunk_id": {
            "$id": "#chunk_id",
            "type": "array",
            "items": [
                {
                    "type": "number"
                },
                {
                    "type": "string"
                }
            ],
            "minItems": 2,
            "maxItems": 2
        },
        "include_data": {
            "$id": "#include_data",
            "enum": [
                "no-tse",
                "slim-tse",
                "smart-tse",
                "whole-tse",
                "orig-tse"
            ]
        },
        "include_info": {
            "$id": "#include_info",
            "type": "array",
            "items": {
                "type": "string",
                "enum": [
                    "all-info-except",
                    "canonical-id",
                    "name",
                    "other-ids",
                    "molecule-type",
                    "length",
                    "chain-state",
                    "state",
                    "blob-id",
                    "tax-id",
                    "hash",
                    "date-changed",
                    "gi"
                ]
            },
            "uniqueItems": true
        },
        "named_annots": {
            "$id": "#named_annots",
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "exclude_blobs": {
            "$id": "#exclude_blobs",
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "acc_substitution": {
            "$id": "#acc_substitution",
            "enum": [
                "default",
                "limited",
                "never"
            ]
        },
        "context": {
            "$id": "#context",
            "type": "object",
            "items": {
                "sid": { "type": "string" },
                "phid": { "type": "string" },
                "client_ip": { "type": "string" }
            }
        },
        "user_args": {
            "$id": "#user_args",
            "type": "string"
        }
    },
    "oneOf": [
        {
            "properties": {
                "jsonrpc": { "$ref": "#/definitions/jsonrpc" },
                "method": { "enum": [ "biodata" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#/definitions/bio_id" },
                        "include_data": { "$ref": "#/definitions/include_data" },
                        "exclude_blobs": { "$ref": "#/definitions/exclude_blobs" },
                        "acc_substitution": { "$ref": "#/definitions/acc_substitution" },
                        "auto_blob_skipping": { "type": "boolean" },
                        "context": { "$ref": "#/definitions/context" },
                        "user_args": { "$ref": "#/definitions/user_args" }
                    },
                    "required": [ "bio_id" ]
                },
                "id": { "$ref": "#/definitions/id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$ref": "#/definitions/jsonrpc" },
                "method": { "enum": [ "blob" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "blob_id": { "$ref": "#/definitions/blob_id" },
                        "include_data": { "$ref": "#/definitions/include_data" },
                        "context": { "$ref": "#/definitions/context" },
                        "user_args": { "$ref": "#/definitions/user_args" }
                    },
                    "required": [ "blob_id" ]
                },
                "id": { "$ref": "#/definitions/id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$ref": "#/definitions/jsonrpc" },
                "method": { "enum": [ "resolve" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#/definitions/bio_id" },
                        "include_info": { "$ref": "#/definitions/include_info" },
                        "acc_substitution": { "$ref": "#/definitions/acc_substitution" },
                        "context": { "$ref": "#/definitions/context" },
                        "user_args": { "$ref": "#/definitions/user_args" }
                    },
                    "required": [ "bio_id" ]
                },
                "id": { "$ref": "#/definitions/id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$ref": "#/definitions/jsonrpc" },
                "method": { "enum": [ "named_annot" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "bio_id" : { "$ref": "#/definitions/bio_id" },
                        "named_annots": { "$ref": "#/definitions/named_annots" },
                        "acc_substitution": { "$ref": "#/definitions/acc_substitution" },
                        "context": { "$ref": "#/definitions/context" },
                        "user_args": { "$ref": "#/definitions/user_args" }
                    },
                    "required": [ "bio_id","named_annots" ]
                },
                "id": { "$ref": "#/definitions/id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        },
        {
            "properties": {
                "jsonrpc": { "$ref": "#/definitions/jsonrpc" },
                "method": { "enum": [ "chunk" ] },
                "params": {
                    "type": "object",
                    "properties": {
                        "chunk_id": { "$ref": "#/definitions/chunk_id" },
                        "context": { "$ref": "#/definitions/context" },
                        "user_args": { "$ref": "#/definitions/user_args" }
                    },
                    "required": [ "chunk_id" ]
                },
                "id": { "$ref": "#/definitions/id" }
            },
            "required": [ "jsonrpc", "method", "params", "id" ]
        }
    ]
}
        )REQUEST_SCHEMA");
}

END_NCBI_SCOPE
