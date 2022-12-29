#ifndef PUBSEQ_GATEWAY__HPP
#define PUBSEQ_GATEWAY__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <string>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/messages.hpp>

#include "pending_operation.hpp"
#include "http_daemon.hpp"
#include "pubseq_gateway_version.hpp"
#include "pubseq_gateway_stat.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"
#include "exclude_blob_cache.hpp"
#include "split_info_cache.hpp"
#include "alerts.hpp"
#include "timing.hpp"
#include "psgs_dispatcher.hpp"
#include "osg_connection.hpp"
#include "psgs_uv_loop_binder.hpp"


USING_NCBI_SCOPE;


const long  kMaxTestIOSize = 1000000000;


// Cassandra mapping needs to be switched atomically so it is first read into
// the corresponding structure and then switched.
struct SCassMapping
{
    string                              m_BioseqKeyspace;
    vector<pair<string, int32_t>>       m_BioseqNAKeyspaces;

    bool operator==(const SCassMapping &  rhs) const
    {
        return m_BioseqKeyspace == rhs.m_BioseqKeyspace &&
               m_BioseqNAKeyspaces == rhs.m_BioseqNAKeyspaces;
    }

    bool operator!=(const SCassMapping &  rhs) const
    { return !this->operator==(rhs); }

    vector<string> validate(const string &  root_keyspace) const
    {
        vector<string>      errors;

        if (m_BioseqKeyspace.empty())
            errors.push_back("Cannot find the resolver keyspace (where SI2CSI "
                             "and BIOSEQ_INFO tables reside) in the " +
                             root_keyspace + ".SAT2KEYSPACE table.");

        if (m_BioseqNAKeyspaces.empty())
            errors.push_back("No bioseq named annotation keyspaces found "
                             "in the " + root_keyspace + " keyspace.");

        return errors;
    }

    void clear(void)
    {
        m_BioseqKeyspace.clear();
        m_BioseqNAKeyspaces.clear();
    }
};


class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp();
    ~CPubseqGatewayApp();

    virtual void Init(void);
    void ParseArgs(void);
    void OpenCache(void);
    bool OpenCass(void);
    bool PopulateCassandraMapping(bool  need_accept_alert=false);
    void PopulatePublicCommentsMapping(void);
    void CheckCassMapping(void);
    void CloseCass(void);
    bool SatToKeyspace(int  sat, string &  sat_name);

    void MaintainSplitInfoBlobCache(void) {
        if (m_SplitInfoCache)
            m_SplitInfoCache->Maintain();
    }

    string GetBioseqKeyspace(void) const
    {
        return m_CassMapping[m_MappingIndex].m_BioseqKeyspace;
    }

    vector<pair<string, int32_t>> GetBioseqNAKeyspaces(void) const
    {
        return m_CassMapping[m_MappingIndex].m_BioseqNAKeyspaces;
    }

    CPubseqGatewayCache *  GetLookupCache(void)
    {
        return m_LookupCache.get();
    }

    CExcludeBlobCache *  GetExcludeBlobCache(void)
    {
        return m_ExcludeBlobCache.get();
    }

    CSplitInfoCache *  GetSplitInfoCache(void)
    {
        return m_SplitInfoCache.get();
    }

    CPSGMessages *  GetPublicCommentsMapping(void)
    {
        return m_PublicComments.get();
    }

    unsigned long GetSendBlobIfSmall(void) const
    {
        return m_SendBlobIfSmall;
    }

    int OnBadURL(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGet(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetBlob(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnResolve(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetTSEChunk(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetNA(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnAccessionVersionHistory(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnIPGResolve(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnHealth(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnConfig(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnInfo(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnStatus(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnShutdown(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnGetAlerts(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnAckAlert(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnStatistics(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnDispatcherStatus(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);
    int OnTestIO(CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply);

    virtual int Run(void);

    void NotifyRequestFinished(size_t  request_id);

    static CPubseqGatewayApp *  GetInstance(void);

    COperationTiming & GetTiming(void)
    {
        return *m_Timing.get();
    }

    EPSGS_StartupDataState GetStartupDataState(void) const
    {
        return m_StartupDataState;
    }

    map<string, tuple<string, string>>  GetIdToNameAndDescriptionMap(void) const
    {
        return m_IdToNameAndDescription;
    }

    shared_ptr<CCassConnection> GetCassandraConnection(void)
    {
        return m_CassConnection;
    }

    unsigned int GetCassandraTimeout(void) const
    {
        return m_TimeoutMs;
    }

    unsigned int GetCassandraMaxRetries(void) const
    {
        return m_MaxRetries;
    }

    bool GetOSGProcessorsEnabled() const
    {
        return m_OSGProcessorsEnabled;
    }

    bool GetCDDProcessorsEnabled() const
    {
        return m_CDDProcessorsEnabled;
    }

    bool GetWGSProcessorsEnabled() const
    {
        return m_WGSProcessorsEnabled;
    }

    bool GetSNPProcessorsEnabled() const
    {
        return m_SNPProcessorsEnabled;
    }

    const CRef<psg::osg::COSGConnectionPool>& GetOSGConnectionPool() const
    {
        return m_OSGConnectionPool;
    }

    bool GetCassandraProcessorsEnabled(void) const
    {
        return m_CassandraProcessorsEnabled;
    }

    IPSGS_Processor::EPSGS_StartProcessing
    SignalStartProcessing(IPSGS_Processor *  processor)
    {
        return m_RequestDispatcher->SignalStartProcessing(processor);
    }

    size_t GetProcessorMaxConcurrency(const string &  processor_id);

    void SignalFinishProcessing(IPSGS_Processor *  processor,
                                CPSGS_Dispatcher::EPSGS_SignalSource  signal_source)
    {
        m_RequestDispatcher->SignalFinishProcessing(processor, signal_source);
    }

    void SignalConnectionCanceled(size_t  request_id)
    {
        m_RequestDispatcher->SignalConnectionCanceled(request_id);
    }

    bool GetSSLEnable(void) const
    {
        return m_SSLEnable;
    }

    string GetSSLCertFile(void) const
    {
        return m_SSLCertFile;
    }

    string GetSSLKeyFile(void) const
    {
        return m_SSLKeyFile;
    }

    string GetSSLCiphers(void) const
    {
        return m_SSLCiphers;
    }

    size_t GetShutdownIfTooManyOpenFD(void) const
    {
        return m_ShutdownIfTooManyOpenFD;
    }

    int GetPageSize(void) const
    {
        return m_IPGPageSize;
    }

    string GetIPGKeyspace(void) const
    {
        return m_IPGKeyspace;
    }

    CPSGAlerts &  GetAlerts(void)
    {
        return m_Alerts;
    }

    CPSGSCounters &  GetCounters(void)
    {
        return m_Counters;
    }

    void RegisterUVLoop(uv_thread_t  uv_thread, uv_loop_t *  uv_loop)
    {
        lock_guard<mutex>   guard(m_ThreadToBinderGuard);

        m_ThreadToUVLoop[uv_thread] = uv_loop;
        m_ThreadToBinder[uv_thread] =
                unique_ptr<CPSGS_UvLoopBinder>(new CPSGS_UvLoopBinder(uv_loop));
    }

    void UnregisterUVLoop(uv_thread_t  uv_thread)
    {
        lock_guard<mutex>   guard(m_ThreadToBinderGuard);
        m_ThreadToBinder[uv_thread]->x_Unregister();
    }

    void RegisterDaemonUVLoop(uv_thread_t  uv_thread, uv_loop_t *  uv_loop)
    {
        lock_guard<mutex>   guard(m_ThreadToBinderGuard);

        m_ThreadToUVLoop[uv_thread] = uv_loop;
    }

    uv_loop_t *  GetUVLoop(void)
    {
        auto    it = m_ThreadToUVLoop.find(uv_thread_self());
        if (it == m_ThreadToUVLoop.end()) {
            return nullptr;
        }
        return it->second;
    }

    void CancelAllProcessors(void)
    {
        m_RequestDispatcher->CancelAll();
    }

    CPSGS_Dispatcher *  GetProcessorDispatcher(void)
    {
        return m_RequestDispatcher.get();
    }

    CPSGS_UvLoopBinder &  GetUvLoopBinder(uv_thread_t  uv_thread_id);

private:
    struct SRequestParameter
    {
        bool            m_Found;
        CTempString     m_Value;

        SRequestParameter() : m_Found(false)
        {}
    };

    void x_SendMessageAndCompletionChunks(
        shared_ptr<CPSGS_Reply>  reply,
        const psg_time_point_t &  now,
        const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity);

    bool x_ProcessCommonGetAndResolveParams(
        CHttpRequest &  req,
        shared_ptr<CPSGS_Reply>  reply,
        const psg_time_point_t &  now,
        CTempString &  seq_id, int &  seq_id_type,
        SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache,
        bool  seq_id_is_optional=false);

    // Common URL parameters for all ../ID/.. requests
    bool x_GetCommonIDRequestParams(
            CHttpRequest &  req,
            shared_ptr<CPSGS_Reply>  reply,
            const psg_time_point_t &  now,
            SPSGS_RequestBase::EPSGS_Trace &  trace,
            int &  hops,
            vector<string> &  enabled_processors,
            vector<string> &  disabled_processors,
            bool &  processor_events);

private:
    void x_ValidateArgs(void);
    string  x_GetCmdLineArguments(void) const;
    CRef<CRequestContext>  x_CreateRequestContext(CHttpRequest &  req) const;
    void x_PrintRequestStop(CRef<CRequestContext>  context,
                            CPSGS_Request::EPSGS_Type  request_type,
                            CRequestStatus::ECode  status,
                            size_t  bytes_sent);

    SRequestParameter  x_GetParam(CHttpRequest &  req,
                                  const string &  name) const;
    bool  x_GetSendBlobIfSmallParameter(CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const psg_time_point_t &  now,
                                        int &  send_blob_if_small);
    bool x_IsBoolParamValid(const string &  param_name,
                            const CTempString &  param_value,
                            string &  err_msg) const;
    bool x_ConvertIntParameter(const string &  param_name,
                               const CTempString &  param_value,
                               int &  converted,
                               string &  err_msg) const;
    bool x_ConvertIntParameter(const string &  param_name,
                               const CTempString &  param_value,
                               int64_t &  converted,
                               string &  err_msg) const;
    bool x_ConvertDoubleParameter(const string &  param_name,
                                  const CTempString &  param_value,
                                  double &  converted,
                                  string &  err_msg) const;
    bool x_GetUseCacheParameter(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply,
                                const psg_time_point_t &  now,
                                SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache);
    bool x_GetOutputFormat(CHttpRequest &  req,
                           shared_ptr<CPSGS_Reply>  reply,
                           const psg_time_point_t &  now,
                           SPSGS_ResolveRequest::EPSGS_OutputFormat &  output_format);
    bool x_GetTSEOption(CHttpRequest &  req,
                        shared_ptr<CPSGS_Reply>  reply,
                        const psg_time_point_t &  now,
                        SPSGS_BlobRequestBase::EPSGS_TSEOption &  tse_option);
    bool x_GetLastModified(CHttpRequest &  req,
                           shared_ptr<CPSGS_Reply>  reply,
                           const psg_time_point_t &  now,
                           int64_t &  last_modified);
    bool x_GetBlobId(CHttpRequest &  req,
                     shared_ptr<CPSGS_Reply>  reply,
                     const psg_time_point_t &  now,
                     SPSGS_BlobId &  blob_id);
    bool x_GetResolveFlags(CHttpRequest &  req,
                           shared_ptr<CPSGS_Reply>  reply,
                           const psg_time_point_t &  now,
                           SPSGS_ResolveRequest::TPSGS_BioseqIncludeData &  include_data_flags);
    bool x_GetId2Chunk(CHttpRequest &  req,
                       shared_ptr<CPSGS_Reply>  reply,
                       const psg_time_point_t &  now,
                       int64_t &  id2_chunk);
    vector<string> x_GetExcludeBlobs(CHttpRequest &  req) const;
    unsigned long x_GetDataSize(const IRegistry &  reg,
                                const string &  section,
                                const string &  entry,
                                unsigned long  default_val);
    bool x_GetAccessionSubstitutionOption(CHttpRequest &  req,
                                          shared_ptr<CPSGS_Reply>  reply,
                                          const psg_time_point_t &  now,
                                          SPSGS_RequestBase::EPSGS_AccSubstitutioOption & acc_subst_option);
    bool x_GetTraceParameter(CHttpRequest &  req,
                             shared_ptr<CPSGS_Reply>  reply,
                             const psg_time_point_t &  now,
                             SPSGS_RequestBase::EPSGS_Trace &  trace);
    bool x_GetProcessorEventsParameter(CHttpRequest &  req,
                                       shared_ptr<CPSGS_Reply>  reply,
                                       const psg_time_point_t &  now,
                                       bool &  processor_events);
    bool x_GetHops(CHttpRequest &  req,
                   shared_ptr<CPSGS_Reply>  reply,
                   const psg_time_point_t &  now,
                   int &  hops);
    bool x_GetResendTimeout(CHttpRequest &  req,
                            shared_ptr<CPSGS_Reply>  reply,
                            const psg_time_point_t &  now,
                            double &  resend_timeout);
    bool x_GetSeqIdResolveParameter(CHttpRequest &  req,
                                    shared_ptr<CPSGS_Reply>  reply,
                                    const psg_time_point_t &  now,
                                    bool &  auto_blob_skipping);
    bool x_GetAutoBlobSkippingParameter(CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const psg_time_point_t &  now,
                                        bool &  seq_id_resolve);
    bool x_GetEnabledAndDisabledProcessors(CHttpRequest &  req,
                                           shared_ptr<CPSGS_Reply>  reply,
                                           const psg_time_point_t &  now,
                                           vector<string> &  enabled_processors,
                                           vector<string> &  disabled_processors);
    bool x_GetNames(CHttpRequest &  req,
                    shared_ptr<CPSGS_Reply>  reply,
                    const psg_time_point_t &  now,
                    vector<string> &  names);
    bool x_GetProtein(CHttpRequest &  req,
                      shared_ptr<CPSGS_Reply>  reply,
                      const psg_time_point_t &  now,
                      CTempString &  protein);
    bool x_GetIPG(CHttpRequest &  req,
                  shared_ptr<CPSGS_Reply>  reply,
                  const psg_time_point_t &  now,
                  int64_t &  ipg);
    bool x_GetNucleotide(CHttpRequest &  req,
                         shared_ptr<CPSGS_Reply>  reply,
                         const psg_time_point_t &  now,
                         CTempString &  nucleotide);
    bool x_GetTimeSeries(CHttpRequest &  req,
                         shared_ptr<CPSGS_Reply>  reply,
                         const psg_time_point_t &  now,
                         vector<pair<int, int>> &  time_series);

private:
    void x_InsufficientArguments(shared_ptr<CPSGS_Reply>  reply,
                                 const psg_time_point_t &  now,
                                 const string &  err_msg);
    void x_MalformedArguments(shared_ptr<CPSGS_Reply>  reply,
                              const psg_time_point_t &  now,
                              const string &  err_msg);
    void x_Finish500(shared_ptr<CPSGS_Reply>  reply,
                     const psg_time_point_t &  now,
                     EPSGS_PubseqGatewayErrorCode  code,
                     const string &  err_msg);
    bool x_IsShuttingDown(shared_ptr<CPSGS_Reply>  reply,
                          const psg_time_point_t &  now);
    void x_ReadIdToNameAndDescriptionConfiguration(const IRegistry &  reg,
                                                   const string &  section);
    void x_RegisterProcessors(void);
    bool x_DispatchRequest(CRef<CRequestContext>   context,
                           shared_ptr<CPSGS_Request>  request,
                           shared_ptr<CPSGS_Reply>  reply);
    void x_InitSSL(void);

private:
    string                              m_Si2csiDbFile;
    string                              m_BioseqInfoDbFile;
    string                              m_BlobPropDbFile;

    // Bioseq and named annotations keyspaces can be updated dynamically.
    // The index controls the active set.
    // The sat names cannot be updated dynamically - they are read once.
    size_t                              m_MappingIndex;
    SCassMapping                        m_CassMapping[2];
    vector<string>                      m_SatNames;
    vector<int32_t>                     m_BioseqNAKeyspaces;
    unique_ptr<CPSGMessages>            m_PublicComments;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;
    unsigned int                        m_MaxRetries;

    unsigned int                        m_ExcludeCacheMaxSize;
    unsigned int                        m_ExcludeCachePurgePercentage;
    unsigned int                        m_ExcludeCacheInactivityPurge;
    unsigned long                       m_SmallBlobSize;
    unsigned long                       m_MinStatValue;
    unsigned long                       m_MaxStatValue;
    unsigned long                       m_NStatBins;
    string                              m_StatScaleType;
    unsigned long                       m_TickSpan;

    CTime                               m_StartTime;
    string                              m_RootKeyspace;
    string                              m_AuthToken;

    bool                                m_AllowIOTest;
    unique_ptr<char []>                 m_IOTestBuffer;

    unsigned long                       m_SendBlobIfSmall;
    int                                 m_MaxHops;
    double                              m_ResendTimeoutSec;
    double                              m_RequestTimeoutSec;

    bool                                m_CassandraProcessorsEnabled;
    string                              m_TestSeqId;
    bool                                m_TestSeqIdIgnoreError;

    unique_ptr<CPubseqGatewayCache>     m_LookupCache;
    unique_ptr<CHttpDaemon>             m_TcpDaemon;

    unique_ptr<CExcludeBlobCache>       m_ExcludeBlobCache;
    unique_ptr<CSplitInfoCache>         m_SplitInfoCache;
    size_t                              m_SplitInfoBlobCacheSize;

    CPSGAlerts                          m_Alerts;
    unique_ptr<COperationTiming>        m_Timing;
    CPSGSCounters                       m_Counters;

    EPSGS_StartupDataState              m_StartupDataState;
    CNcbiLogFields                      m_LogFields;

    // Serialized JSON introspection message
    string                              m_HelpMessage;

    // Configured counter/statistics ID to name/description
    map<string, tuple<string, string>>  m_IdToNameAndDescription;

    bool                                m_OSGProcessorsEnabled;
    CRef<psg::osg::COSGConnectionPool>  m_OSGConnectionPool;

    bool                                m_CDDProcessorsEnabled;

    bool                                m_WGSProcessorsEnabled;

    bool                                m_SNPProcessorsEnabled;

    // Requests dispatcher
    unique_ptr<CPSGS_Dispatcher>        m_RequestDispatcher;
    size_t                              m_ProcessorMaxConcurrency;

    // https support
    bool                                m_SSLEnable;
    string                              m_SSLCertFile;
    string                              m_SSLKeyFile;
    string                              m_SSLCiphers;

    size_t                              m_ShutdownIfTooManyOpenFD;

    // IPG settings
    string                              m_IPGKeyspace;
    int                                 m_IPGPageSize;
    bool                                m_EnableHugeIPG;

    // Mapping between the libuv thread id and the binder associated with the
    // libuv worker thread loop
    map<uv_thread_t,
        unique_ptr<CPSGS_UvLoopBinder>> m_ThreadToBinder;
    mutex                               m_ThreadToBinderGuard;
    map<uv_thread_t, uv_loop_t *>       m_ThreadToUVLoop;

private:
    static CPubseqGatewayApp *          sm_PubseqApp;
};


#endif
