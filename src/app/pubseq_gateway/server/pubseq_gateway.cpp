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
#include <ncbi_pch.hpp>

#include <math.h>
#include <thread>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/services/grid_app_version_info.hpp>
#include <util/random_gen.hpp>

#include <google/protobuf/stubs/common.h>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_huge_report_helper.hpp>

#include "http_request.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
#include "shutdown_data.hpp"
#include "cass_monitor.hpp"
#include "introspection.hpp"
#include "cass_processor_dispatch.hpp"
#include "osg_processor.hpp"
#include "cdd_processor.hpp"
#include "wgs_processor.hpp"
#include "snp_processor.hpp"
#include "dummy_processor.hpp"
#include "favicon.hpp"


USING_NCBI_SCOPE;

const EDiagSev          kDefaultSeverity = eDiag_Critical;
const bool              kDefaultTrace = false;
const float             kSplitInfoBlobCacheSizeMultiplier = 0.8;    // Used to calculate low mark

static const string     kDaemonizeArgName = "daemonize";


// Memorize the configured severity level to check before using ERR_POST.
// Otherwise some expensive operations are executed without a real need.
EDiagSev                g_ConfiguredSeverity = kDefaultSeverity;

// Memorize the configured tracing to check before using ERR_POST.
// Otherwise some expensive operations are executed without a real need.
bool                    g_Trace = kDefaultTrace;

// Memorize the configured log on/off flag.
// It is used in the context resetter to avoid unnecessary context resets
bool                    g_Log = true;

// Memorize the configured on/off flag for the processor timing
// It is used in the processor base class and the dispatcher
bool                    g_AllowProcessorTiming = false;

// Create the shutdown related data. It is used in a few places:
// a URL handler, signal handlers, watchdog handlers
SShutdownData           g_ShutdownData;


CPubseqGatewayApp *     CPubseqGatewayApp::sm_PubseqApp = nullptr;


CPubseqGatewayApp::CPubseqGatewayApp() :
    m_MappingIndex(0),
    m_CassConnection(nullptr),
    m_CassConnectionFactory(CCassConnectionFactory::s_Create()),
    m_StartTime(GetFastLocalTime()),
    m_ExcludeBlobCache(nullptr),
    m_SplitInfoCache(nullptr),
    m_StartupDataState(ePSGS_NoCassConnection),
    m_LogFields("http")
{
    sm_PubseqApp = this;
    m_HelpMessageJson = GetIntrospectionNode().Repr(CJsonNode::fStandardJson);
    m_HelpMessageHtml =
        "\n\n\n<!DOCTYPE html>\n"
        #include "introspection_html.hpp"
        ;
}


CPubseqGatewayApp::~CPubseqGatewayApp()
{}


void CPubseqGatewayApp::Init(void)
{
    unique_ptr<CArgDescriptions>    argdesc(new CArgDescriptions());

    argdesc->AddFlag(kDaemonizeArgName,
                     "Turn on daemonization of Pubseq Gateway at the start.");

    argdesc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Daemon to service Accession.Version Cache requests");
    SetupArgDescriptions(argdesc.release());

    // Memorize the configured severity
    g_ConfiguredSeverity = GetDiagPostLevel();

    // Memorize the configure trace
    g_Trace = GetDiagTrace();
}


void CPubseqGatewayApp::ParseArgs(void)
{
    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   registry = GetConfig();

    m_Settings.Read(GetConfig(), m_Alerts);
    g_Log = m_Settings.m_Log;
    g_AllowProcessorTiming = m_Settings.m_AllowProcessorTiming;

    m_CassConnectionFactory->AppParseArgs(args);
    m_CassConnectionFactory->LoadConfig(registry, "");
    m_CassConnectionFactory->SetLogging(GetDiagPostLevel());

    m_OSGConnectionPool = new psg::osg::COSGConnectionPool();
    m_OSGConnectionPool->AppParseArgs(args);
    m_OSGConnectionPool->SetLogging(GetDiagPostLevel());
    m_OSGConnectionPool->LoadConfig(registry);


    // It throws an exception in case of inability to start
    m_Settings.Validate(m_Alerts);
}


void CPubseqGatewayApp::OpenCache(void)
{
    // It was decided to work with and without the cache even if the wrapper
    // has not been created. So the cache initialization is called once and
    // always succeed.
    m_StartupDataState = ePSGS_StartupDataOK;

    try {
        // NB. It was decided that the configuration may ommit the cache file
        // paths. In this case the server should not use the corresponding
        // cache at all. This is covered in the CPubseqGatewayCache class.
        m_LookupCache.reset(
                new CPubseqGatewayCache(m_Settings.m_BioseqInfoDbFile,
                                        m_Settings.m_Si2csiDbFile,
                                        m_Settings.m_BlobPropDbFile));

        // The format of the sat ids is different
        set<int>        sat_ids;
        for (size_t  index = 0; index < m_SatNames.size(); ++index) {
            if (!m_SatNames[index].empty()) {
                // Need to exclude the bioseq NA keyspaces because they are not
                // in cache. They use a different schema
                if (find(m_BioseqNAKeyspaces.begin(),
                         m_BioseqNAKeyspaces.end(),
                         index) == m_BioseqNAKeyspaces.end()) {
                    sat_ids.insert(index);
                }
            }
        }

        m_LookupCache->Open(sat_ids);
        const auto        errors = m_LookupCache->GetErrors();
        if (!errors.empty()) {
            string  msg = "Error opening the LMDB cache:";
            for (const auto &  err : errors) {
                msg += "\n" + err.message;
            }

            PSG_ERROR(msg);
            m_Alerts.Register(ePSGS_OpenCache, msg);
            m_LookupCache->ResetErrors();
        }
    } catch (const exception &  exc) {
        string      msg = "Error initializing the LMDB cache: " +
                          string(exc.what()) +
                          ". The server continues without cache.";
        PSG_ERROR(exc);
        m_Alerts.Register(ePSGS_OpenCache, msg);
        m_LookupCache.reset(nullptr);
    } catch (...) {
        string      msg = "Unknown initializing LMDB cache error. "
                          "The server continues without cache.";
        PSG_ERROR(msg);
        m_Alerts.Register(ePSGS_OpenCache, msg);
        m_LookupCache.reset(nullptr);
    }
}


bool CPubseqGatewayApp::OpenCass(void)
{
    static bool need_logging = true;

    try {
        if (!m_CassConnection)
            m_CassConnection = m_CassConnectionFactory->CreateInstance();
        m_CassConnection->Connect();

        // Next step in the startup data initialization
        m_StartupDataState = ePSGS_NoValidCassMapping;
    } catch (const exception &  exc) {
        string      msg = "Error connecting to Cassandra: " +
                          string(exc.what());
        if (need_logging)
            PSG_CRITICAL(exc);

        need_logging = false;
        m_Alerts.Register(ePSGS_OpenCassandra, msg);
        return false;
    } catch (...) {
        string      msg = "Unknown Cassandra connecting error";
        if (need_logging)
            PSG_CRITICAL(msg);

        need_logging = false;
        m_Alerts.Register(ePSGS_OpenCassandra, msg);
        return false;
    }

    need_logging = false;
    return true;
}


void CPubseqGatewayApp::CloseCass(void)
{
    m_CassConnection = nullptr;
    m_CassConnectionFactory = nullptr;
}


bool CPubseqGatewayApp::SatToKeyspace(int  sat, string &  sat_name)
{
    if (sat >= 0) {
        if (sat < static_cast<int>(m_SatNames.size())) {
            sat_name = m_SatNames[sat];
            return !sat_name.empty();
        }
    }
    return false;
}


int CPubseqGatewayApp::Run(void)
{
    srand(time(NULL));

    try {
        ParseArgs();
    } catch (const exception &  exc) {
        PSG_CRITICAL(exc.what());
        return 1;
    } catch (...) {
        PSG_CRITICAL("Unknown argument parsing error");
        return 1;
    }

    if (GetArgs()[kDaemonizeArgName]) {
        // NOTE: if the stdin/stdout are not kept (default daemonize behavior)
        // then libuv fails to close its events loop. There are some asserts
        // with fds, one of them fails so a core dump is generated.
        // With stdin/stdout kept open libuv stays happy and does not crash
        bool    is_good = CCurrentProcess::Daemonize(kEmptyCStr,
                                                     CCurrentProcess::fDF_KeepCWD |
                                                     CCurrentProcess::fDF_KeepStdin |
                                                     CCurrentProcess::fDF_KeepStdout);
        if (!is_good)
            NCBI_THROW(CPubseqGatewayException, eDaemonizationFailed,
                       "Error during daemonization");
    }

    bool    connected = OpenCass();
    bool    populated = false;
    if (connected) {
        PopulatePublicCommentsMapping();
        populated = PopulateCassandraMapping();
    }

    if (populated)
        OpenCache();

    m_RequestDispatcher.reset(new CPSGS_Dispatcher(m_Settings.m_RequestTimeoutSec));
    x_RegisterProcessors();


    // m_IdToNameAndDescription was populated at the time of
    // dealing with arguments
    m_Counters.UpdateConfiguredNameDescription(m_Settings.m_IdToNameAndDescription);

    auto purge_size = round(float(m_Settings.m_ExcludeCacheMaxSize) *
                            float(m_Settings.m_ExcludeCachePurgePercentage) / 100.0);
    m_ExcludeBlobCache.reset(
        new CExcludeBlobCache(m_Settings.m_ExcludeCacheInactivityPurge,
                              m_Settings.m_ExcludeCacheMaxSize,
                              m_Settings.m_ExcludeCacheMaxSize - static_cast<size_t>(purge_size)));

    m_SplitInfoCache.reset(new CSplitInfoCache(m_Settings.m_SplitInfoBlobCacheSize,
                                               m_Settings.m_SplitInfoBlobCacheSize * kSplitInfoBlobCacheSizeMultiplier));

    m_Timing.reset(new COperationTiming(m_Settings.m_MinStatValue,
                                        m_Settings.m_MaxStatValue,
                                        m_Settings.m_NStatBins,
                                        m_Settings.m_StatScaleType,
                                        m_Settings.m_SmallBlobSize,
                                        m_Settings.m_OnlyForProcessor,
                                        m_Settings.m_LogTimingThreshold));

    // Setup IPG huge report
    ipg::CPubseqGatewayHugeIpgReportHelper::SetHugeIpgDisabled(
                                            !m_Settings.m_EnableHugeIPG);

    vector<CHttpHandler>        http_handler;
    CHttpGetParser              get_parser;

    http_handler.emplace_back(
            "/ID/getblob",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetBlob(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGet(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/resolve",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnResolve(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_tse_chunk",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetTSEChunk(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_na",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetNA(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ID/get_acc_ver_history",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnAccessionVersionHistory(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/IPG/resolve",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnIPGResolve(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/health",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnHealth(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/deep-health",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                // For the time being 'deep-health' matches 'health'
                return OnHealth(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/config",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnConfig(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/info",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnInfo(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/status",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnStatus(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/shutdown",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnShutdown(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/get_alerts",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnGetAlerts(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/ack_alert",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnAckAlert(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/statistics",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnStatistics(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/ADMIN/dispatcher_status",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnDispatcherStatus(req, reply);
            }, &get_parser, nullptr);
    http_handler.emplace_back(
            "/favicon.ico",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                // It's a browser, most probably admin request
                reply->SetContentType(ePSGS_ImageMime);
                reply->SetContentLength(sizeof(favicon));
                reply->SendOk((const char *)(favicon), sizeof(favicon), true);
                return 0;
            }, &get_parser, nullptr);

    if (m_Settings.m_AllowIOTest) {
        m_IOTestBuffer.reset(new char[kMaxTestIOSize]);
        CRandom     random;
        char *      current = m_IOTestBuffer.get();
        for (size_t  k = 0; k < kMaxTestIOSize; k += 8) {
            Uint8   random_val = random.GetRandUint8();
            memcpy(current, &random_val, 8);
            current += 8;
        }

        http_handler.emplace_back(
                "/TEST/io",
                [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
                {
                    return OnTestIO(req, reply);
                }, &get_parser, nullptr);
    }

    http_handler.emplace_back(
            "/",
            [this](CHttpRequest &  req, shared_ptr<CPSGS_Reply>  reply)->int
            {
                return OnBadURL(req, reply);
            }, &get_parser, nullptr);


    x_InitSSL();
    m_TcpDaemon.reset(
            new CHttpDaemon(http_handler, "0.0.0.0",
                            m_Settings.m_HttpPort,
                            m_Settings.m_HttpWorkers,
                            m_Settings.m_ListenerBacklog,
                            m_Settings.m_TcpMaxConn));

    // Run the monitoring thread
    int             ret_code = 0;
    std::thread     monitoring_thread(CassMonitorThreadedFunction);

    try {
        m_TcpDaemon->Run([this](CTcpDaemon &  tcp_daemon)
                {
                    // This lambda is called once per second.
                    // Earlier implementations printed counters on stdout.

                    static unsigned long   tick_no = 0;
                    if (++tick_no % m_Settings.m_TickSpan == 0) {
                        tick_no = 0;
                        this->m_Timing->Rotate();
                    }

                    static unsigned long    tick_no_for_1_min = 0;
                    if (++tick_no_for_1_min % 60 == 0) {
                        tick_no_for_1_min = 0;
                        this->m_Timing->RotateRequestStat();
                    }
                });
    } catch (const CException &  exc) {
        ERR_POST(Critical << exc);
        ret_code = 1;
    } catch (const exception &  exc) {
        ERR_POST(Critical << exc);
        ret_code = 1;
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while running TCP daemon");
        ret_code = 1;
    }

    // To stop the monitoring thread a shutdown flag needs to be set.
    // It is going to take no more than 0.1 second because the period of
    // checking the shutdown flag in the monitoring thread is 100ms
    g_ShutdownData.m_ShutdownRequested = true;
    monitoring_thread.join();

    CloseCass();
    return ret_code;
}



CPubseqGatewayApp *  CPubseqGatewayApp::GetInstance(void)
{
    return sm_PubseqApp;
}


string CPubseqGatewayApp::x_GetCmdLineArguments(void) const
{
    const CNcbiArguments &  arguments = CNcbiApplication::Instance()->
                                                            GetArguments();
    size_t                  args_size = arguments.Size();
    string                  cmdline_args;

    for (size_t index = 0; index < args_size; ++index) {
        if (index != 0)
            cmdline_args += " ";
        cmdline_args += arguments[index];
    }
    return cmdline_args;
}


static string       kNcbiSidHeader = "HTTP_NCBI_SID";
static string       kNcbiPhidHeader = "HTTP_NCBI_PHID";
static string       kXForwardedForHeader = "X-Forwarded-For";
static string       kUserAgentHeader = "User-Agent";
static string       kUserAgentApplog = "USER_AGENT";
static string       kRequestPathApplog = "request_path";

// If log is off, each log_sampling_ratio-th request needs to be logged anyway
// so there is a sequential request counter
static uint64_t     s_request_number = 0;
CRef<CRequestContext> CPubseqGatewayApp::x_CreateRequestContext(
                                                CHttpRequest &  req) const
{
    bool                    need_log_sampling = false;
    CRef<CRequestContext>   context;

    if (!g_Log) {
        if (m_Settings.m_LogSamplingRatio > 0) {
            ++s_request_number;
            if (s_request_number >= m_Settings.m_LogSamplingRatio) {
                need_log_sampling = true;
                s_request_number = 0;
            }
        }
    }

    if (g_Log || need_log_sampling) {
        context.Reset(new CRequestContext());
        context->SetRequestID();

        // NCBI SID may come from the header
        string      sid = req.GetHeaderValue(kNcbiSidHeader);
        if (!sid.empty())
            context->SetSessionID(sid);
        else
            context->SetSessionID();

        // NCBI PHID may come from the header
        string      phid = req.GetHeaderValue(kNcbiPhidHeader);
        if (!phid.empty())
            context->SetHitID(phid);
        else
            context->SetHitID();

        // Client IP may come from the headers
        string      client_ip = req.GetHeaderValue(kXForwardedForHeader);
        if (!client_ip.empty()) {
            vector<string>      ip_addrs;
            NStr::Split(client_ip, ",", ip_addrs);
            if (!ip_addrs.empty()) {
                // Take the first one, there could be many...
                context->SetClientIP(ip_addrs[0]);
            }
        } else {
            client_ip = req.GetPeerIP();
            if (!client_ip.empty()) {
                context->SetClientIP(client_ip);
            }
        }

        // It was decided not to use the standard C++ Toolkit function because
        // it searches in the CGI environment and does quite a bit of
        // unnecessary things. The PSG server only needs X-Forwarded-For
        // TNCBI_IPv6Addr  client_address = req.GetClientIP();
        // if (!NcbiIsEmptyIPv6(&client_address)) {
        //     char        buf[256];
        //     if (NcbiIPv6ToString(buf, sizeof(buf), &client_address) != 0) {
        //         context->SetClientIP(buf);
        //     }
        // }

        CDiagContext::SetRequestContext(context);
        CDiagContext_Extra  extra = GetDiagContext().PrintRequestStart();

        // This is the URL path
        extra.Print(kRequestPathApplog, req.GetPath());

        string      user_agent = req.GetHeaderValue(kUserAgentHeader);
        if (!user_agent.empty())
            extra.Print(kUserAgentApplog, user_agent);

        req.PrintParams(extra);
        req.PrintLogFields(m_LogFields);

        // If extra is not flushed then it picks read-only even though it is
        // done after...
        extra.Flush();

        // Just in case, avoid to have 0
        context->SetRequestStatus(CRequestStatus::e200_Ok);
        context->SetReadOnly(true);
    }
    return context;
}


void CPubseqGatewayApp::x_PrintRequestStop(CRef<CRequestContext>   context,
                                           CPSGS_Request::EPSGS_Type  request_type,
                                           CRequestStatus::ECode  status, size_t  bytes_sent)
{
    GetCounters().IncrementRequestStopCounter(status);

    if (request_type != CPSGS_Request::ePSGS_UnknownRequest)
        GetTiming().RegisterForTimeSeries(request_type, status);

    if (context.NotNull()) {
        CDiagContext::SetRequestContext(context);
        context->SetReadOnly(false);
        context->SetRequestStatus(status);
        context->SetBytesWr(bytes_sent);
        GetDiagContext().PrintRequestStop();
        context.Reset();
        CDiagContext::SetRequestContext(NULL);
    }
}


bool CPubseqGatewayApp::PopulateCassandraMapping(bool  need_accept_alert)
{
    static bool need_logging = true;

    size_t      vacant_index = m_MappingIndex + 1;
    if (vacant_index >= 2)
        vacant_index = 0;
    m_CassMapping[vacant_index].clear();

    try {
        string      err_msg;
        if (!FetchSatToKeyspaceMapping(
                    m_Settings.m_RootKeyspace, m_CassConnection,
                    m_SatNames, eBlobVer2Schema,
                    m_CassMapping[vacant_index].m_BioseqKeyspace, eResolverSchema,
                    m_CassMapping[vacant_index].m_BioseqNAKeyspaces, eNamedAnnotationsSchema,
                    err_msg)) {
            err_msg = "Error populating cassandra mapping: " + err_msg;
            if (need_logging)
                PSG_CRITICAL(err_msg);
            need_logging = false;
            m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
            return false;
        }
    } catch (const exception &  exc) {
        string      err_msg = "Cannot populate keyspace mapping from Cassandra.";
        if (need_logging) {
            PSG_CRITICAL(exc);
            PSG_CRITICAL(err_msg);
        }
        need_logging = false;
        m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg + " " + exc.what());
        return false;
    } catch (...) {
        string      err_msg = "Unknown error while populating "
                              "keyspace mapping from Cassandra.";
        if (need_logging)
            PSG_CRITICAL(err_msg);
        need_logging = false;
        m_Alerts.Register(ePSGS_NoValidCassandraMapping, err_msg);
        return false;
    }

    auto    errors = m_CassMapping[vacant_index].validate(m_Settings.m_RootKeyspace);
    if (m_SatNames.empty())
        errors.push_back("No sat to keyspace resolutions found in the '" +
                         m_Settings.m_RootKeyspace + "' keyspace.");

    if (errors.empty()) {

        m_BioseqNAKeyspaces.clear();
        for (auto & item : m_CassMapping[vacant_index].m_BioseqNAKeyspaces) {
            m_BioseqNAKeyspaces.push_back(item.second);
        }

        m_MappingIndex = vacant_index;

        // Next step in the startup data initialization
        m_StartupDataState = ePSGS_NoCassCache;

        if (need_accept_alert) {
            // We are not the first time here; It means that the alert about
            // the bad mapping has been set before. So now we accepted the
            // configuration so a change config alert is needed for the UI
            // visibility
            m_Alerts.Register(ePSGS_NewCassandraMappingAccepted,
                              "Keyspace mapping (sat names, bioseq info and named "
                              "annotations) has been populated");
        }
    } else {
        string      combined_error("Invalid Cassandra mapping:");
        for (const auto &  err : errors) {
            combined_error += "\n" + err;
        }
        if (need_logging)
            PSG_CRITICAL(combined_error);

        m_Alerts.Register(ePSGS_NoValidCassandraMapping, combined_error);
    }

    need_logging = false;
    return errors.empty();
}


void CPubseqGatewayApp::PopulatePublicCommentsMapping(void)
{
    if (m_PublicComments.get() != nullptr)
        return;     // We have already been here: the mapping needs to be
                    // populated once

    try {
        string      err_msg;

        m_PublicComments.reset(new CPSGMessages);

        if (!FetchMessages(m_Settings.m_RootKeyspace, m_CassConnection,
                           *m_PublicComments.get(), err_msg)) {
            // Allow another try later
            m_PublicComments.reset(nullptr);

            err_msg = "Error populating cassandra public comments mapping: " + err_msg;
            PSG_ERROR(err_msg);
            m_Alerts.Register(ePSGS_NoCassandraPublicCommentsMapping, err_msg);
        }
    } catch (const exception &  exc) {
        // Allow another try later
        m_PublicComments.reset(nullptr);

        string      err_msg = "Cannot populate public comments mapping from Cassandra";
        PSG_ERROR(exc);
        PSG_ERROR(err_msg);
        m_Alerts.Register(ePSGS_NoCassandraPublicCommentsMapping,
                          err_msg + ". " + exc.what());
    } catch (...) {
        // Allow another try later
        m_PublicComments.reset(nullptr);

        string      err_msg = "Unknown error while populating "
                              "public comments mapping from Cassandra";
        PSG_ERROR(err_msg);
        m_Alerts.Register(ePSGS_NoCassandraPublicCommentsMapping, err_msg);
    }
}


void CPubseqGatewayApp::CheckCassMapping(void)
{
    size_t      vacant_index = m_MappingIndex + 1;
    if (vacant_index >= 2)
        vacant_index = 0;
    m_CassMapping[vacant_index].clear();

    vector<string>      sat_names;
    try {
        string      err_msg;
        if (!FetchSatToKeyspaceMapping(
                    m_Settings.m_RootKeyspace, m_CassConnection,
                    sat_names, eBlobVer2Schema,
                    m_CassMapping[vacant_index].m_BioseqKeyspace,
                    eResolverSchema,
                    m_CassMapping[vacant_index].m_BioseqNAKeyspaces,
                    eNamedAnnotationsSchema,
                    err_msg)) {
            m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                              "Error checking cassandra mapping: " + err_msg);
            return;
        }

    } catch (const exception &  exc) {
        m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                          "Cannot check keyspace mapping from Cassandra. " +
                          string(exc.what()));
        return;
    } catch (...) {
        m_Alerts.Register(ePSGS_InvalidCassandraMapping,
                          "Unknown error while checking "
                          "keyspace mapping from Cassandra.");
        return;
    }

    auto    errors = m_CassMapping[vacant_index].validate(m_Settings.m_RootKeyspace);
    if (sat_names.empty())
        errors.push_back("No sat to keyspace resolutions found in the " +
                         m_Settings.m_RootKeyspace + " keyspace.");

    if (errors.empty()) {
        // No errors detected in the DB; let's compare with what we use now
        if (sat_names != m_SatNames)
            m_Alerts.Register(ePSGS_NewCassandraSatNamesMapping,
                              "Cassandra has new sat names mapping in  the " +
                              m_Settings.m_RootKeyspace + " keyspace. The server needs "
                              "to restart to use it.");

        if (m_CassMapping[0] != m_CassMapping[1]) {
            m_MappingIndex = vacant_index;
            m_Alerts.Register(ePSGS_NewCassandraMappingAccepted,
                              "Keyspace mapping (bioseq info and named "
                              "annotations) has been updated");
        }
    } else {
        string      combined_error("Invalid Cassandra mapping detected "
                                   "during the periodic check:");
        for (const auto &  err : errors) {
            combined_error += "\n" + err;
        }
        m_Alerts.Register(ePSGS_InvalidCassandraMapping, combined_error);
    }
}


// Prepares the chunks for the case when it is a client error so only two
// chunks are required:
// - a message chunk
// - a reply completion chunk
void  CPubseqGatewayApp::x_SendMessageAndCompletionChunks(
        shared_ptr<CPSGS_Reply>  reply,
        const psg_time_point_t &  create_timestamp,
        const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity)
{
    if (reply->IsFinished()) {
        // This is the case when a reply is already formed and sent to
        // the client.
        return;
    }

    reply->SetContentType(ePSGS_PSGMime);
    reply->PrepareReplyMessage(message, status, code, severity);
    reply->PrepareReplyCompletion(status, create_timestamp);
    reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
    reply->SetCompleted();
}


void CPubseqGatewayApp::x_InitSSL(void)
{
    if (m_Settings.m_SSLEnable) {
        SSL_load_error_strings();
        SSL_library_init();
        OpenSSL_add_all_algorithms();
    }
}


void CPubseqGatewayApp::x_RegisterProcessors(void)
{
    // Note: the order of adding defines the priority.
    //       Earleir added - higher priority
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new CPSGS_CassProcessorDispatcher()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::osg::CPSGS_OSGProcessor()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::cdd::CPSGS_CDDProcessor()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::wgs::CPSGS_WGSProcessor()));
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new psg::snp::CPSGS_SNPProcessor()));

    #if 0
    // For testing during development. The processor code does nothing and can
    // be switched on to receive requests.
    m_RequestDispatcher->AddProcessor(
        unique_ptr<IPSGS_Processor>(new CPSGS_DummyProcessor()));
    #endif
}


CPSGS_UvLoopBinder &
CPubseqGatewayApp::GetUvLoopBinder(uv_thread_t  uv_thread_id)
{
    auto it = m_ThreadToBinder.find(uv_thread_id);
    if (it == m_ThreadToBinder.end()) {
        // Binding is suppposed only for the processors (which work in their
        // threads, which in turn have their thread id registered at the moment
        // they start)
        PSG_ERROR("Binding is supported only for the worker threads");
        NCBI_THROW(CPubseqGatewayException, eLogic,
                   "Binding is supported only for the worker threads");
    }
    return *(it->second.get());
}



int main(int argc, const char* argv[])
{
    srand(time(NULL));
    CThread::InitializeMainThreadId();


    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetAllowedSessionIDFormat(CRequestContext::eSID_Other);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);


    int ret = CPubseqGatewayApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}

