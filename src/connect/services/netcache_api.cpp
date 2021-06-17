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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the NetCache client API.
 *
 */

#include <ncbi_pch.hpp>

#ifdef _MSC_VER
// Disable warning C4191: 'type cast' :
//     unsafe conversion from 'ncbi::CDll::FEntryPoint'
//     to 'void (__cdecl *)(...)' [comes from plugin_manager.hpp]
#pragma warning (disable: 4191)
#endif

#include "netschedule_api_impl.hpp"
#include "netcache_api_impl.hpp"

#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/services/impl/netcache_api_int.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/rwstream.hpp>

#include <memory>


#define NCBI_USE_ERRCODE_X  ConnServ_NetCache


BEGIN_NCBI_SCOPE

struct SFallbackServer
{
    using TAddress = SSocketAddress;

    static const TAddress* Get()
    {
        static const TAddress address(Init());
        return address.host ? &address : nullptr;
    }

private:
    static TAddress Init();
};

SSocketAddress SFallbackServer::Init()
{
    try {
        return TAddress::Parse(TCGI_NetCacheFallbackServer::GetDefault());
    } catch (...) {
    }

    return TAddress(0, 0);
}

INetServerConnectionListener::TPropCreator CNetCacheServerListener::GetPropCreator() const
{
    return [] { return new SNetCacheServerProperties; };
}

INetServerConnectionListener* CNetCacheServerListener::Clone()
{
    return new CNetCacheServerListener(*this);
}

void SNetCacheAPIImpl::Init(CSynRegistry& registry, const SRegSynonyms& sections)
{
    GetListener()->SetAuthString(m_Service->MakeAuthString());

    if (m_Service->GetClientName().length() < 3) {
        NCBI_THROW(CNetCacheException,
            eAuthenticationError, "Client name is too short or empty");
    }

    m_TempDir =                            registry.Get(sections, { "tmp_dir", "tmp_path" }, ".");
    m_CacheInput =                         registry.Get(sections, "cache_input", false);
    m_CacheOutput =                        registry.Get(sections, "cache_output", false);
    const bool prolong_on_write =          registry.Get(sections, "prolong_blob_lifetime_on_write", true);
    const bool create_on_write =           registry.Get(sections, "create_blob_on_write", true);

    m_DefaultParameters.SetMirroringMode(  registry.Get(sections, "enable_mirroring", kEmptyStr));
    m_DefaultParameters.SetServerCheck(    registry.Get(sections, "server_check", kEmptyStr));
    m_DefaultParameters.SetServerCheckHint(registry.Get(sections, "server_check_hint", kEmptyStr));
    m_DefaultParameters.SetUseCompoundID(  registry.Get(sections, "use_compound_id", false));

    const auto allowed_services =          registry.Get(sections, "allowed_services", kEmptyStr);

    m_FlagsOnWrite = (prolong_on_write ? 0 : 1) | (create_on_write ? 0 : 2);

    if (allowed_services.empty()) return;

    m_ServiceMap.Restrict();

    vector<string> services;
    NStr::Split(allowed_services, ", ", services,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    for (auto& service : services) {
        // Do not add configured service, it is always allowed
        if (NStr::CompareNocase(service,
                    m_Service.GetServiceName())) {
            m_ServiceMap.AddToAllowed(service);
        }
    }
}

void CNetCacheServerListener::OnConnected(CNetServerConnection& connection)
{
    auto server_props = connection->m_Server->Get<SNetCacheServerProperties>();

    CFastMutexGuard guard(server_props->m_Mutex);

    if (server_props->mirroring_checked) {
        guard.Release();
        connection->WriteLine(m_Auth);
    } else {
        string version_info(connection.Exec(m_Auth + "\r\nVERSION", false));

        server_props->mirroring_checked = true;

        try {
            CUrlArgs url_parser(version_info);

            ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
                if (field->name == "mirrored" && field->value == "true")
                    server_props->mirrored = true;
            }
        }
        catch (CException&) {
        }
    }
}

void CNetCacheServerListener::OnErrorImpl(const string& err_msg, CNetServer& server)
{
    static const char s_BlobNotFoundMsg[] = "BLOB not found";
    if (NStr::strncmp(err_msg.c_str(), s_BlobNotFoundMsg,
            sizeof(s_BlobNotFoundMsg) - 1) == 0) {
        if (strstr(err_msg.c_str(), "AGE=") != NULL) {
            CONNSERV_THROW_FMT(CNetCacheBlobTooOldException, eBlobTooOld,
                    server, err_msg);
        } else {
            CONNSERV_THROW_FMT(CNetCacheException, eBlobNotFound,
                    server, err_msg);
        }
    }

    static const char s_AccessDenied[] = "Access denied";
    if (NStr::strncmp(err_msg.c_str(), s_AccessDenied,
        sizeof(s_AccessDenied) - 1) == 0)
        CONNSERV_THROW_FMT(CNetCacheException, eAccessDenied, server, err_msg);

    static const char s_UnknownCommandMsg[] = "Unknown command";
    if (NStr::strncmp(err_msg.c_str(), s_UnknownCommandMsg,
        sizeof(s_UnknownCommandMsg) - 1) == 0)
        CONNSERV_THROW_FMT(CNetCacheException, eUnknownCommand, server, err_msg);

    CONNSERV_THROW_FMT(CNetCacheException, eServerError, server, err_msg);
}

void CNetCacheServerListener::OnWarningImpl(const string& warn_msg,
        CNetServer& server)
{
        ERR_POST(Warning << "NetCache server at "
                << server->m_ServerInPool->m_Address.AsString() <<
                ": WARNING: " << warn_msg);
}

const char* const kNetCacheAPIDriverName = "netcache_api";

SNetCacheAPIImpl::SNetCacheAPIImpl(CSynRegistryBuilder registry_builder, const string& section,
        const string& service, const string& client_name,
        CNetScheduleAPI::TInstance ns_api) :
    m_NetScheduleAPI(ns_api),
    m_DefaultParameters(eVoid)
{
    SRegSynonyms sections{ section, kNetCacheAPIDriverName, "netcache_client", "netcache" };

    string ns_client_name;

    if (ns_api) {
        ns_client_name = ns_api->m_Service->GetClientName();

        CNetScheduleConfigLoader loader(registry_builder, sections, false);
        loader(ns_api);
    }

    m_Service = SNetServiceImpl::Create("NetCacheAPI", service, client_name,
            new CNetCacheServerListener,
            registry_builder, sections, ns_client_name);
    Init(registry_builder, sections);
}

SNetCacheAPIImpl::SNetCacheAPIImpl() :
    m_DefaultParameters(eVoid)
{
}

SNetCacheAPIImpl::SNetCacheAPIImpl(SNetServerInPool* server,
        SNetCacheAPIImpl* parent) :
    m_Service(SNetServiceImpl::Clone(server, parent->m_Service)),
    m_ServiceMap(parent->m_ServiceMap),
    m_TempDir(parent->m_TempDir),
    m_CacheInput(parent->m_CacheInput),
    m_CacheOutput(parent->m_CacheOutput),
    m_NetScheduleAPI(parent->m_NetScheduleAPI),
    m_DefaultParameters(parent->m_DefaultParameters)
{
}

void SNetCacheAPIImpl::AppendClientIPSessionID(string* cmd, CRequestContext& req)
{
    _ASSERT(cmd);

    // XXX: A workaround for IP always required
    // TODO: Remove after all NetCache servers upgraded to include CXX-9580 and CXX-9720
    if (!req.IsSetClientIP()) *cmd += " ip=\"\"";

    g_AppendClientIPAndSessionID(*cmd, req);
}

void SNetCacheAPIImpl::AppendClientIPSessionIDPasswordAgeHitID(string* cmd,
        const CNetCacheAPIParameters* parameters)
{
    _ASSERT(cmd);

    CRequestContext& req = CDiagContext::GetRequestContext();
    AppendClientIPSessionID(cmd, req);

    string password(parameters->GetPassword());
    if (!password.empty()) {
        cmd->append(" pass=\"");
        cmd->append(NStr::PrintableString(password));
        cmd->append(1, '\"');
    }

    unsigned max_age = parameters->GetMaxBlobAge();
    if (max_age > 0) {
        cmd->append(" age=");
        cmd->append(NStr::NumericToString(max_age));
    }

    AppendHitID(cmd, req);
}

void SNetCacheAPIImpl::AppendHitID(string* cmd, CRequestContext& req)
{
    _ASSERT(cmd);

    g_AppendHitID(*cmd, req);
}

void SNetCacheAPIImpl::AppendClientIPSessionIDHitID(string* cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    AppendClientIPSessionID(cmd, req);
    AppendHitID(cmd, req);
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd_base, const CNetCacheKey& key,
        const CNetCacheAPIParameters* parameters)
{
    string result(cmd_base + key.StripKeyExtensions());
    AppendClientIPSessionIDPasswordAgeHitID(&result, parameters);
    return result;
}

unsigned SNetCacheAPIImpl::x_ExtractBlobAge(
        const CNetServer::SExecResult& exec_result, const char* cmd_name)
{
    string::size_type pos = exec_result.response.find("AGE=");

    if (pos == string::npos) {
        CONNSERV_THROW_FMT(CNetCacheException, eInvalidServerResponse,
                exec_result.conn->m_Server,
                       "No AGE field in " << cmd_name <<
                       " output: \"" << exec_result.response << "\"");
    }

    return NStr::StringToUInt(exec_result.response.c_str() + pos +
            sizeof("AGE=") - 1, NStr::fAllowTrailingSymbols);
}

CNetServer SNetCacheMirrorTraversal::BeginIteration()
{
    if (m_PrimaryServerCheck)
        return *(m_Iterator = m_Service.Iterate(m_PrimaryServer));

    m_Iterator = NULL;
    return m_PrimaryServer;
}

CNetServer SNetCacheMirrorTraversal::NextServer()
{
    if (!m_Iterator) {
        m_Iterator = m_Service.Iterate(m_PrimaryServer);
        CNetServer first_server = *m_Iterator;
        if (first_server->m_ServerInPool != m_PrimaryServer->m_ServerInPool)
            return first_server;
    }
    return ++m_Iterator ? *m_Iterator : CNetServer();
}

CNetServer::SExecResult SNetCacheAPIImpl::ExecMirrorAware(
    const CNetCacheKey& key, const string& cmd,
    bool multiline_output,
    const CNetCacheAPIParameters* parameters,
    SNetServiceImpl::EServerErrorHandling error_handling)
{
    const string& key_service_name = key.GetServiceName();

    bool key_has_service_name = !key_service_name.empty();

    CNetService service(m_Service);

    if (key_has_service_name && key_service_name != service.GetServiceName()) {
        // NB: Configured service is always allowed

        if (!m_ServiceMap.IsAllowed(key_service_name)) {
            NCBI_THROW_FMT(CNetCacheException, eAccessDenied, "Service " <<
                    key_service_name << " is not in the allowed services");
        }

        service = m_ServiceMap.GetServiceByName(key_service_name, m_Service);
    }

    bool mirroring_allowed =
            !key.GetFlag(CNetCacheKey::fNCKey_SingleServer) &&
            parameters->GetMirroringMode() != CNetCacheAPI::eMirroringDisabled;

    if (key.GetVersion() == 3) {
        /* Version 3 - no server address, only CRC32 of it: */
        if (!service.IsLoadBalanced()) {
            NCBI_THROW_FMT(CNetSrvConnException, eLBNameNotFound,
                key.GetKey() << ": NetCache key version 3 "
                "requires an LBSM service name.");
        }

        Uint4 crc32 = key.GetHostPortCRC32();

        for (CNetServiceIterator it(service.Iterate(CNetService::eRandomize));
                it; ++it) {
            CNetServer server(*it);

            if (CNetCacheKey::CalculateChecksum(
                    CSocketAPI::ntoa(server.GetHost()),
                            server.GetPort()) == crc32) {
                // The server with the checksum from the key
                // has been found in the service.

                // TODO: cache the calculated checksums to resolve
                // them into host:port immediately.

                if (!mirroring_allowed)
                    return server.ExecWithRetry(cmd, multiline_output);

                CNetServer::SExecResult exec_result;

                SNetCacheMirrorTraversal mirror_traversal(service,
                        server, eOff /* turn off server check since
                                        the server is discovered
                                        through this service */);

                service->IterateUntilExecOK(cmd, multiline_output,
                        exec_result, &mirror_traversal, error_handling);

                return exec_result;
            }
        }

        if (mirroring_allowed) {
            // Original server either is down or has a different port (thus crc32 mismatch),
            // will use any server from this service
            return service->FindServerAndExec(cmd, multiline_output);
        }

        NCBI_THROW_FMT(CNetSrvConnException, eServerNotInService,
                key.GetKey() << ": unable to find a NetCache server "
                "by the checksum from this key.");
    }

    CNetServer primary_server(service.GetServer(key.GetHost(), key.GetPort()));

    ESwitch server_check = eDefault;
    parameters->GetServerCheck(&server_check);
    if (server_check == eDefault)
        server_check = key.GetFlag(CNetCacheKey::fNCKey_NoServerCheck) ?
                eOff : eOn;

    if (key_has_service_name && mirroring_allowed) {
        CNetServer::SExecResult exec_result;

        SNetCacheMirrorTraversal mirror_traversal(service,
                primary_server, server_check);

        service->IterateUntilExecOK(cmd, multiline_output, exec_result,
                &mirror_traversal, error_handling);

        return exec_result;
    }

    // If enabled, check if the server belongs to the selected service
    if (server_check != eOff && !service->IsInService(primary_server)) {

        // Service name is known, no need to check other services
        if (key_has_service_name) {
            NCBI_THROW_FMT(CNetSrvConnException, eServerNotInService,
                    key.GetKey() << ": NetCache server " <<
                    primary_server.GetServerAddress() << " could not be "
                    "accessed because it is not registered for the service.");

        // Service name is not known,
        // check if the server belongs to one of the explicitly allowed services
        } else if (!m_ServiceMap.IsAllowed(primary_server, m_Service)) {
            NCBI_THROW_FMT(CNetCacheException, eAccessDenied,
                    key.GetKey() << ": NetCache server " <<
                    primary_server.GetServerAddress() << " could not be "
                    "accessed because it is not registered for the allowed "
                    "services.");
        }

    }

    return primary_server.ExecWithRetry(cmd, multiline_output);
}

CNetCacheAPI::CNetCacheAPI(CNetCacheAPI::EAppRegistry /* use_app_reg */,
        const string& conf_section /* = kEmptyStr */,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(NULL, conf_section,
            kEmptyStr, kEmptyStr, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(const IRegistry& reg, const string& conf_section,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(reg, conf_section,
            kEmptyStr, kEmptyStr, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(CConfig* conf, const string& conf_section,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(conf, conf_section,
            kEmptyStr, kEmptyStr, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(const string& client_name,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(NULL,
            kEmptyStr, kEmptyStr, client_name, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(const string& service_name,
        const string& client_name, CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(NULL,
            kEmptyStr, service_name, client_name, ns_api))
{
}

void CNetCacheAPI::SetDefaultParameters(const CNamedParameterList* parameters)
{
    m_Impl->m_DefaultParameters.LoadNamedParameters(parameters);
}

string CNetCacheAPI::PutData(const void* buf, size_t size,
        const CNamedParameterList* optional)
{
    return PutData(kEmptyStr, buf, size, optional);
}

CNetServerConnection SNetCacheAPIImpl::InitiateWriteCmd(
    CNetCacheWriter* nc_writer, const CNetCacheAPIParameters* parameters)
{
    string cmd("PUT3 ");
    cmd.append(NStr::IntToString(parameters->GetTTL()));

    const string& blob_id(nc_writer->GetBlobID());

    const bool write_existing_blob = !blob_id.empty();
    CNetCacheKey key;
    string stripped_blob_id;

    if (write_existing_blob) {
        key.Assign(blob_id, m_CompoundIDPool);
        cmd.push_back(' ');
        stripped_blob_id = key.StripKeyExtensions();
        cmd.append(stripped_blob_id);
    }

    AppendClientIPSessionIDPasswordAgeHitID(&cmd, parameters);
    if (m_FlagsOnWrite) cmd.append(" flags=").append(to_string(m_FlagsOnWrite));

    CNetServer::SExecResult exec_result;

    if (write_existing_blob)
        exec_result = ExecMirrorAware(key, cmd, false, parameters,
                SNetServiceImpl::eIgnoreServerErrors);
    else {
        try {
            exec_result = m_Service.FindServerAndExec(cmd, false);
        } catch (CNetSrvConnException& e) {
            const SSocketAddress* backup = SFallbackServer::Get();

            if (backup == NULL) {
                LOG_POST(Info << "Fallback server address is not configured.");
                throw;
            }

            ERR_POST_X(3, "Could not connect to " <<
                m_Service.GetServiceName() << ":" << e.what() <<
                ". Connecting to backup server " << backup->AsString() << ".");

            exec_result =
                    m_Service->GetServer(*backup).ExecWithRetry(cmd, false);
        }
    }

    if (NStr::FindCase(exec_result.response, "ID:") != 0) {
        // Answer is not in the "ID:....." format
        exec_result.conn->Abort();
        CONNSERV_THROW_FMT(CNetServiceException, eCommunicationError,
            exec_result.conn->m_Server,
            "Unexpected server response: " << exec_result.response);
    }
    exec_result.response.erase(0, 3);

    if (exec_result.response.empty()) {
        exec_result.conn->Abort();
        CONNSERV_THROW_FMT(CNetServiceException, eCommunicationError,
            exec_result.conn->m_Server,
            "Invalid server response. Empty key.");
    }

    if (write_existing_blob) {
        if (exec_result.response != stripped_blob_id) {
            exec_result.conn->Abort();
            CONNSERV_THROW_FMT(CNetCacheException, eInvalidServerResponse,
                exec_result.conn->m_Server,
                "Server created " << exec_result.response <<
                " in response to PUT3 \"" << stripped_blob_id << "\"");
        }
    } else {
        if (m_Service.IsLoadBalanced()) {
            CNetCacheKey::TNCKeyFlags key_flags = 0;

            switch (parameters->GetMirroringMode()) {
            case CNetCacheAPI::eMirroringDisabled:
                key_flags |= CNetCacheKey::fNCKey_SingleServer;
                break;
            case CNetCacheAPI::eMirroringEnabled:
                break;
            default:
                if (!exec_result.conn->m_Server->Get<SNetCacheServerProperties>()->mirrored)
                    key_flags |= CNetCacheKey::fNCKey_SingleServer;
            }

            bool server_check_hint = true;
            parameters->GetServerCheckHint(&server_check_hint);
            if (!server_check_hint)
                key_flags |= CNetCacheKey::fNCKey_NoServerCheck;

            CNetCacheKey::AddExtensions(exec_result.response,
                    m_Service.GetServiceName(), key_flags);
        }

        if (parameters->GetUseCompoundID())
            exec_result.response = CNetCacheKey::KeyToCompoundID(
                    exec_result.response, m_CompoundIDPool);

        nc_writer->SetBlobID(exec_result.response);
    }

    return exec_result.conn;
}


string  CNetCacheAPI::PutData(const string& key,
                              const void*   buf,
                              size_t        size,
                              const CNamedParameterList* optional)
{
    string actual_key(key);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);
    parameters.SetCachingMode(eCaching_Disable);

    CNetCacheWriter writer(m_Impl, &actual_key, kEmptyStr,
            eNetCache_Wait, &parameters);

    writer.WriteBufferAndClose(reinterpret_cast<const char*>(buf), size);

    return actual_key;
}

CNcbiOstream* CNetCacheAPI::CreateOStream(string& key,
        const CNamedParameterList* optional)
{
    return new CWStream(PutData(&key, optional), 0, NULL,
        CRWStreambuf::fOwnWriter | CRWStreambuf::fLeakExceptions);
}

IEmbeddedStreamWriter* CNetCacheAPI::PutData(string* key,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return new CNetCacheWriter(m_Impl, key, kEmptyStr,
            eNetCache_Wait, &parameters);
}


bool CNetCacheAPI::HasBlob(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return m_Impl->ExecMirrorAware(key,
            m_Impl->MakeCmd("HASB ", key, &parameters),
            false,
            &parameters).response[0] == '1';
}


size_t CNetCacheAPI::GetBlobSize(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return CheckBlobSize(NStr::StringToUInt8(
            m_Impl->ExecMirrorAware(key,
                    m_Impl->MakeCmd("GSIZ ", key, &parameters),
                    false,
                    &parameters).response));
}


void CNetCacheAPI::Remove(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    try {
        m_Impl->ExecMirrorAware(key,
                m_Impl->MakeCmd("RMV2 ", key, &parameters),
                false,
                &parameters);
    }
    catch (std::exception& e) {
        ERR_POST("Could not remove blob \"" << blob_id << "\": " << e.what());
    }
    catch (...) {
        ERR_POST("Could not remove blob \"" << blob_id << "\"");
    }
}

CNetServerMultilineCmdOutput CNetCacheAPI::GetBlobInfo(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    string cmd("GETMETA " + key.StripKeyExtensions());
    cmd.append(m_Impl->m_Service->m_ServerPool->m_EnforcedServer.host == 0 ?
            " 0" : " 1");

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    m_Impl->AppendClientIPSessionIDHitID(&cmd);

    CNetServerMultilineCmdOutput output(m_Impl->ExecMirrorAware(key,
            cmd, true, &parameters));

    output->SetNetCacheCompatMode();

    return output;
}

void CNetCacheAPI::PrintBlobInfo(const string& blob_key,
        const CNamedParameterList* optional)
{
    CNetServerMultilineCmdOutput output(GetBlobInfo(blob_key, optional));

    string line;

    if (output.ReadLine(line)) {
        if (!NStr::StartsWith(line, "SIZE="))
            NcbiCout << line << NcbiEndl;
        while (output.ReadLine(line))
            NcbiCout << line << NcbiEndl;
    }
}

void CNetCacheAPI::ProlongBlobLifetime(const string& blob_key, unsigned ttl,
        const CNamedParameterList* optional)
{
    CNetCacheKey key_obj(blob_key, m_Impl->m_CompoundIDPool);

    string cmd("PROLONG \"\" " + key_obj.StripKeyExtensions());

    cmd += " \"\" ttl=";
    cmd += NStr::NumericToString(ttl);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    m_Impl->AppendClientIPSessionIDPasswordAgeHitID(&cmd, &parameters);

    m_Impl->ExecMirrorAware(key_obj, cmd, false, &parameters);
}

IReader* CNetCacheAPI::GetReader(const string& key, size_t* blob_size,
    const CNamedParameterList* optional)
{
    return m_Impl->GetPartReader(key, 0, 0, blob_size, optional);
}

IReader* CNetCacheAPI::GetPartReader(const string& key,
    size_t offset, size_t part_size, size_t* blob_size,
    const CNamedParameterList* optional)
{
    return m_Impl->GetPartReader(key, offset, part_size,
        blob_size, optional);
}

void CNetCacheAPI::ReadData(const string& key, string& buffer,
        const CNamedParameterList* optional)
{
    ReadPart(key, 0, 0, buffer, optional);
}

void CNetCacheAPI::ReadPart(const string& key,
    size_t offset, size_t part_size, string& buffer,
    const CNamedParameterList* optional)
{
    size_t blob_size;
    unique_ptr<IReader> reader(GetPartReader(key, offset, part_size,
        &blob_size, optional));

    buffer.resize(blob_size);

    m_Impl->ReadBuffer(*reader, const_cast<char*>(buffer.data()),
        blob_size, NULL, blob_size);
}

IReader* CNetCacheAPI::GetData(const string& key, size_t* blob_size,
    const CNamedParameterList* optional)
{
    try {
        return GetReader(key, blob_size, optional);
    }
    catch (CNetCacheBlobTooOldException&) {
        return NULL;
    }
    catch (CNetCacheException& e) {
        switch (e.GetErrCode()) {
        case CNetCacheException::eBlobNotFound:
        case CNetCacheException::eAccessDenied:
            return NULL;
        }
        throw;
    }
}

CNetCacheReader* SNetCacheAPIImpl::GetPartReader(const string& blob_id,
    size_t offset, size_t part_size,
    size_t* blob_size_ptr, const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_CompoundIDPool);

    string stripped_blob_id(key.StripKeyExtensions());

    const char* cmd_name;
    string cmd;

    if (offset == 0 && part_size == 0) {
        cmd_name = "GET2 ";
        cmd = cmd_name + stripped_blob_id;
    } else {
        cmd_name = "GETPART ";
        cmd = cmd_name + stripped_blob_id + ' ' +
            NStr::UInt8ToString((Uint8) offset) + ' ' +
            NStr::UInt8ToString((Uint8) part_size);
    }

    CNetCacheAPIParameters parameters(&m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    AppendClientIPSessionIDPasswordAgeHitID(&cmd, &parameters);

    unsigned max_age = parameters.GetMaxBlobAge();
    if (max_age > 0) {
        cmd += " age=";
        cmd += NStr::NumericToString(max_age);
    }

    CNetServer::SExecResult exec_result;

    try {
        exec_result = ExecMirrorAware(key, cmd, false, &parameters);
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound) {
            e.AddToMessage(", ID=");
            e.AddToMessage(blob_id);
        }
        throw;
    }

    unsigned* actual_age_ptr = parameters.GetActualBlobAgePtr();
    if (max_age > 0 && actual_age_ptr != NULL)
        *actual_age_ptr = x_ExtractBlobAge(exec_result, cmd_name);

    return new CNetCacheReader(this, blob_id,
            exec_result, blob_size_ptr, &parameters);
}

CNetCacheAPI::EReadResult CNetCacheAPI::GetData(const string&  key,
                      void*          buf,
                      size_t         buf_size,
                      size_t*        n_read,
                      size_t*        blob_size,
                      const CNamedParameterList* optional)
{
    _ASSERT(buf && buf_size);

    size_t x_blob_size = 0;

    unique_ptr<IReader> reader(GetData(key, &x_blob_size, optional));
    if (reader.get() == 0)
        return eNotFound;

    if (blob_size)
        *blob_size = x_blob_size;

    return m_Impl->ReadBuffer(*reader,
        (char*) buf, buf_size, n_read, x_blob_size);
}

CNetCacheAPI::EReadResult CNetCacheAPI::GetData(
    const string& key, CSimpleBuffer& buffer,
    const CNamedParameterList* optional)
{
    size_t x_blob_size = 0;

    unique_ptr<IReader> reader(GetData(key, &x_blob_size, optional));
    if (reader.get() == 0)
        return eNotFound;

    buffer.resize_mem(x_blob_size);
    return m_Impl->ReadBuffer(*reader,
        (char*) buffer.data(), x_blob_size, NULL, x_blob_size);
}

CNcbiIstream* CNetCacheAPI::GetIStream(const string& key, size_t* blob_size,
        const CNamedParameterList* optional)
{
    return new CRStream(GetReader(key, blob_size, optional), 0, NULL,
        CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions);
}

CNetCacheAdmin CNetCacheAPI::GetAdmin()
{
    return new SNetCacheAdminImpl(m_Impl);
}

CNetService CNetCacheAPI::GetService()
{
    return m_Impl->m_Service;
}

CNetCacheAPIExt CNetCacheAPIExt::GetServer(CNetServer::TInstance server)
{
    return new SNetCacheAPIImpl(server->m_ServerInPool, m_Impl);
}

CCompoundIDPool CNetCacheAPI::GetCompoundIDPool()
{
    return m_Impl->m_CompoundIDPool;
}

void CNetCacheAPI::SetCompoundIDPool(
        CCompoundIDPool::TInstance compound_id_pool)
{
    m_Impl->m_CompoundIDPool = compound_id_pool;
}

/* static */
CNetCacheAPI::EReadResult SNetCacheAPIImpl::ReadBuffer(
    IReader& reader,
    char* buf_ptr,
    size_t buf_size,
    size_t* n_read,
    size_t blob_size)
{
    size_t bytes_read;
    size_t total_bytes_read = 0;

    while (buf_size > 0) {
        ERW_Result rw_res = reader.Read(buf_ptr, buf_size, &bytes_read);
        if (rw_res == eRW_Success) {
            total_bytes_read += bytes_read;
            buf_ptr += bytes_read;
            buf_size -= bytes_read;
        } else if (rw_res == eRW_Eof) {
            break;
        } else {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                       "Error while reading BLOB");
        }
    }

    if (n_read != NULL)
        *n_read = total_bytes_read;

    return total_bytes_read == blob_size ?
        CNetCacheAPI::eReadComplete : CNetCacheAPI::eReadPart;
}


///////////////////////////////////////////////////////////////////////////////

/// @internal
class CNetCacheAPICF : public IClassFactory<SNetCacheAPIImpl>
{
public:

    typedef SNetCacheAPIImpl TDriver;
    typedef SNetCacheAPIImpl IFace;
    typedef IFace TInterface;
    typedef IClassFactory<SNetCacheAPIImpl> TParent;
    typedef TParent::SDriverInfo TDriverInfo;
    typedef TParent::TDriverList TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetCacheAPICF(const string& driver_name = kNetCacheAPIDriverName,
                   int patch_level = -1)
        : m_DriverVersionInfo
        (ncbi::CInterfaceVersion<IFace>::eMajor,
         ncbi::CInterfaceVersion<IFace>::eMinor,
         patch_level >= 0 ?
            patch_level : ncbi::CInterfaceVersion<IFace>::ePatchLevel),
          m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    /// Create instance of TDriver
    virtual TInterface*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(IFace),
                   const TPluginManagerParamTree* params = 0) const
    {
        if (params && (driver.empty() || driver == m_DriverName) &&
                version.Match(NCBI_INTERFACE_VERSION(IFace)) !=
                    CVersionInfo::eNonCompatible) {
            CConfig config(params);
            return new SNetCacheAPIImpl(&config, m_DriverName,
                kEmptyStr, kEmptyStr, NULL);
        }
        return NULL;
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
    }
protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;

};


void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetcacheapi(
     CPluginManager<SNetCacheAPIImpl>::TDriverInfoList&   info_list,
     CPluginManager<SNetCacheAPIImpl>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CNetCacheAPICF>::NCBI_EntryPointImpl(info_list, method);
}

//////////////////////////////////////////////////////////////////////////////
//

CBlobStorage_NetCache::CBlobStorage_NetCache()
{
    _ASSERT(0 && "Cannot create an unitialized CBlobStorage_NetCache object.");
}


CBlobStorage_NetCache::~CBlobStorage_NetCache()
{
    try {
        Reset();
    }
    NCBI_CATCH_ALL("CBlobStorage_NetCache_Impl::~CBlobStorage_NetCache()");
}

bool CBlobStorage_NetCache::IsKeyValid(const string& str)
{
    return CNetCacheKey::IsValidKey(str, m_NCClient.GetCompoundIDPool());
}

CNcbiIstream& CBlobStorage_NetCache::GetIStream(const string& key,
                                             size_t* blob_size,
                                             ELockMode /*lockMode*/)
{
    m_IStream.reset(m_NCClient.GetIStream(key, blob_size));
    return *m_IStream;
}

string CBlobStorage_NetCache::GetBlobAsString(const string& data_id)
{
    string buf;
    m_NCClient.ReadData(data_id, buf);
    return buf;
}

CNcbiOstream& CBlobStorage_NetCache::CreateOStream(string& key,
                                                   ELockMode /*lockMode*/)
{
    m_OStream.reset(m_NCClient.CreateOStream(key));
    return *m_OStream;
}

string CBlobStorage_NetCache::CreateEmptyBlob()
{
    return m_NCClient.PutData((const void*) NULL, 0);
}

void CBlobStorage_NetCache::DeleteBlob(const string& data_id)
{
    m_NCClient.Remove(data_id);
}

void CBlobStorage_NetCache::Reset()
{
    m_IStream.reset();
    m_OStream.reset();
}

END_NCBI_SCOPE
