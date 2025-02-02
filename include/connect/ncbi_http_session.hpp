#ifndef CONNECT___NCBI_HTTP_SESSION__HPP
#define CONNECT___NCBI_HTTP_SESSION__HPP

/* $Id$
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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   CHttpSession class supporting different types of requests/responses,
 *   headers/cookies/sessions management etc.
 *
 */

#include <corelib/ncbi_cookies.hpp>
#include <connect/ncbi_conn_stream.hpp>


/** @addtogroup HttpSession
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Default retries value.
struct SGetHttpDefaultRetries
{
    unsigned short operator()(void) const;
};

/// Nullable retries for CHttpRequest
typedef CNullable<unsigned short, SGetHttpDefaultRetries> THttpRetries;


class NCBI_XCONNECT_EXPORT CHttpHeaders : public CObject
{
public:
    /// Create empty headers list.
    CHttpHeaders(void) {}

    /// Some standard HTTP headers.
    enum EHeaderName {
        eCacheControl = 0,
        eContentLength,
        eContentType,
        eCookie,
        eDate,
        eExpires,
        eLocation,
        eRange,
        eReferer,
        eSetCookie,
        eUserAgent,
        eHost
    };

    /// Helper class allowing to use both strings and enums as header names.
    /// The class should not be used explicitly - all conversions are automatic.
    class CHeaderNameConverter {
    public:
        CHeaderNameConverter(const char* name)
            : m_Name(name) {}
        CHeaderNameConverter(const string& name)
            : m_Name(name) {}
        CHeaderNameConverter(CTempString name)
            : m_Name(name) {}
        CHeaderNameConverter(CHttpHeaders::EHeaderName name)
            : m_Name(CHttpHeaders::GetHeaderName(name)) {}
        CTempString GetName(void) { return m_Name; }
    private:
        CTempString m_Name;
    };

    /// List of header values (may be required for headers with multiple values
    /// like Cookie).
    typedef vector<string> THeaderValues;
    /// Map of header name-values, case-insensitive.
    typedef map<string, THeaderValues, PNocase> THeaders;

    /// Check if there's at least one header with the name.
    bool HasValue(CHeaderNameConverter name) const;

    /// Get number of values with the given name.
    size_t CountValues(CHeaderNameConverter name) const;

    /// Get value of the header or empty string. If multiple values
    /// exist, return the last one.
    const string& GetValue(CHeaderNameConverter name) const;

    /// Get all values with the given name.
    const THeaderValues& GetAllValues(CHeaderNameConverter name) const;

    /// Remove all existing values with the name, set the new value.
    /// Throw exception if the name is a reserved one and can not be
    /// set directly (NCBI-SID, NCBI-PHID). These values should be
    /// set through CRequestContext, the headers will be added by
    /// CConn_HttpStream automatically.
    void SetValue(CHeaderNameConverter name, CTempString value);

    /// Add new value with the name (multiple values are allowed with
    /// the same name, the order is preserved).
    /// Throw exception if the name is a reserved one and can not be
    /// set directly (NCBI-SID, NCBI-PHID). These values should be
    /// set through CRequestContext, the headers will be added by
    /// CConn_HttpStream automatically.
    void AddValue(CHeaderNameConverter name, CTempString value);

    /// Remove all values with the given name.
    void Clear(CHeaderNameConverter name);

    /// Remove all headers.
    void ClearAll(void);

    /// Clear any existing values and copy all headers from 'headers'
    /// to this object.
    void Assign(const CHttpHeaders& headers);

    /// Add values from 'headers' to this object. When merging values
    /// with the same name, the new values are added after the existing
    /// ones, so that the new values have higher priority.
    void Merge(const CHttpHeaders& headers);

    virtual ~CHttpHeaders(void) {}

private:
    friend class CHttpRequest;
    friend class CHttpResponse;

    // Prohibit copying headers.
    CHttpHeaders(const CHttpHeaders&);
    CHttpHeaders& operator=(const CHttpHeaders&);

    // Check if the name is a reserved one (NCBI-SID, NCBI-PHID).
    // If yes, log error - these headers must be set in
    // CRequestContext, not directly.
    // Return 'false' if the header name is not reserved and a value can
    // be set safely.
    bool x_IsReservedHeader(CTempString name) const;

    THeaders m_Headers;

public:
    /// Parse headers from the string (usually provided by a stream callback).
    /// The new headers are added to the existing ones.
    void ParseHttpHeader(const CTempString& headers);

    /// Get all headers as a single string as required by CConn_HttpStream.
    /// Each header line is terminated by a single HTTP_EOL.
    string GetHttpHeader(void) const;

    const THeaders& Get() const { return m_Headers; }

    /// Get string representation of the given name.
    static const char* GetHeaderName(EHeaderName name);
};


/// Per-request proxy settings.
class NCBI_XCONNECT_EXPORT CHttpProxy
{
public:
    CHttpProxy(void) : m_Port(0) {}
    CHttpProxy(string host, unsigned short port)
        : m_Host(move(host)), m_Port(port) {}
    CHttpProxy(string host, unsigned short port, string user, string password)
        : m_Host(move(host)), m_Port(port), m_User(move(user)), m_Password(move(password)) {}

    bool IsEmpty(void) const { return m_Host.empty(); }
    const string& GetHost(void) const { return m_Host; }
    unsigned short GetPort(void) const { return m_Port; }
    const string& GetUser(void) const { return m_User; }
    const string& GetPassword(void) const { return m_Password; }

private:
    string         m_Host;
    unsigned short m_Port;
    string         m_User;
    string         m_Password;
};


class CHttpResponse;
class CTlsCertCredentials;


/// CHttpSession and CHttpRequest parameters.
class NCBI_XCONNECT_EXPORT CHttpParam
{
public:
    CHttpParam(void);

    /// Add all HTTP headers to request.
    CHttpParam& SetHeaders(const CHttpHeaders& headers);
    /// Set or replace a single HTTP header, @sa CHttpHeaders::SetValue().
    CHttpParam& SetHeader(CHttpHeaders::EHeaderName header, CTempString value);
    /// Add a single HTTP header, @sa CHttpHeaders::AddValue().
    CHttpParam& AddHeader(CHttpHeaders::EHeaderName header, CTempString value);
    const CHttpHeaders& GetHeaders(void) const { return *m_Headers; }

    CHttpParam& SetTimeout(const CTimeout& timeout);
    const CTimeout& GetTimeout(void) const { return m_Timeout; }

    CHttpParam& SetRetries(THttpRetries retries);
    THttpRetries GetRetries(void) const { return m_Retries; }

    CHttpParam& SetCredentials(shared_ptr<CTlsCertCredentials> credentials);
    shared_ptr<CTlsCertCredentials> GetCredentials(void) const { return m_Credentials; }

    CHttpParam& SetProxy(const CHttpProxy& proxy) { m_Proxy = proxy; return *this; }
    const CHttpProxy& GetProxy(void) const { return m_Proxy; }

    const CTimeout& GetDeadline() const { return m_Deadline; }
    CHttpParam& SetDeadline(const CTimeout& deadline) { m_Deadline = deadline; return *this; }

    ESwitch GetRetryProcessing() const { return m_RetryProcessing; }
    CHttpParam& SetRetryProcessing(ESwitch on_off) { m_RetryProcessing = on_off; return *this; }

private:
    CRef<CHttpHeaders>              m_Headers;
    CTimeout                        m_Timeout;
    THttpRetries                    m_Retries;
    shared_ptr<CTlsCertCredentials> m_Credentials;
    CHttpProxy                      m_Proxy;
    CTimeout                        m_Deadline;
    ESwitch                         m_RetryProcessing;
};


/// Shortcut for GET request. Each request uses a separate session,
/// no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Get()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpGet(const CUrl& url, const CHttpParam& param);

/// Shortcut for GET request. Each request uses a separate session,
/// no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Get()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpGet(const CUrl&     url,
                        const CTimeout& timeout = CTimeout(CTimeout::eDefault),
                        THttpRetries    retries = null);

/// Shortcut for GET request with custom headers. Each request uses a separate
/// session, no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Get()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpGet(const CUrl&         url,
                        const CHttpHeaders& headers,
                        const CTimeout&     timeout = CTimeout(CTimeout::eDefault),
                        THttpRetries        retries = null);

/// Shortcut for POST request. Each request uses a separate session,
/// no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Post()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpPost(const CUrl&       url,
                         CTempString       data,
                         const CHttpParam& param = CHttpParam());

/// Shortcut for POST request. Each request uses a separate session,
/// no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Post()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpPost(const CUrl&     url,
                         CTempString     data,
                         CTempString     content_type,
                         const CTimeout& timeout = CTimeout(CTimeout::eDefault),
                         THttpRetries    retries = null);

/// Shortcut for POST request with custom headers. Each request uses a separate
/// session, no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Post()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpPost(const CUrl&         url,
                         const CHttpHeaders& headers,
                         CTempString         data,
                         CTempString         content_type = CTempString(),
                         const CTimeout&     timeout = CTimeout(CTimeout::eDefault),
                         THttpRetries        retries = null);

/// Shortcut for PUT request. Each request uses a separate session,
/// no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Put()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpPut(const CUrl&       url,
                        CTempString       data,
                        const CHttpParam& param = CHttpParam());

/// Shortcut for PUT request. Each request uses a separate session,
/// no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Put()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpPut(const CUrl&     url,
                        CTempString     data,
                        CTempString     content_type,
                        const CTimeout& timeout = CTimeout(CTimeout::eDefault),
                        THttpRetries    retries = null);

/// Shortcut for PUT request with custom headers. Each request uses a separate
/// session, no data like cookies is shared between multiple requests.
/// @sa CHttpSession::Put()
NCBI_XCONNECT_EXPORT
CHttpResponse g_HttpPut(const CUrl&         url,
                        const CHttpHeaders& headers,
                        CTempString         data,
                        CTempString         content_type = CTempString(),
                        const CTimeout&     timeout = CTimeout(CTimeout::eDefault),
                        THttpRetries        retries = null);


/// Interface for custom form data providers.
class NCBI_XCONNECT_EXPORT CFormDataProvider_Base : public CObject
{
public:
    /// Get content type. Returns empty string by default, indicating
    /// no Content-Type header should be used for the part.
    virtual string GetContentType(void) const { return string(); }

    /// Get optional filename to be shown in Content-Disposition header.
    virtual string GetFileName(void) const { return string(); }

    /// Write user data to the stream.
    virtual void WriteData(CNcbiOstream& out) const = 0;

    virtual ~CFormDataProvider_Base(void) {}
};


/// POST request data
class NCBI_XCONNECT_EXPORT CHttpFormData : public CObject
{
public:
    /// Supported content types for POST requests.
    enum EContentType {
        eFormUrlEncoded,      ///< 'application/x-www-form-urlencoded', default
        eMultipartFormData    ///< 'multipart/form-data'
    };

    /// Add name=value pair. The data of this type can be sent using either
    /// eFormUrlEncoded or eMultipartFormData content types.
    /// @param entry_name
    ///   Name of the form input. The name must not be empty, otherwise an
    ///   exception is thrown. Multiple values with the same name are allowed.
    /// @param value
    ///   Value for the input. If the form content type is eFormUrlEncoded,
    ///   the value will be URL encoded properly. Otherwise the value is sent as-is.
    ///   URL-encoded form data allows only one value per entry, otherwise
    ///   exception will be thrown when sending the data.
    /// @param content_type
    ///   Content type for the entry used if the form content type is
    ///   eMultipartFormData. If not set, the protocol assumes 'text/plain'
    ///   content type. Not used when sending eFormUrlEncoded content.
    void AddEntry(CTempString entry_name,
                  CTempString value,
                  CTempString content_type = CTempString());

    /// Add file entry. The form content type is automatically set to
    /// eMultipartFormData and can not be changed later.
    /// @param entry_name
    ///   Name of the form input responsible for selecting files. Multiple
    ///   files can be added with the same entry name. The name must not be
    ///   empty, otherwise an exception is thrown.
    /// @param file_name
    ///   Path to the local file to be sent. The file must exist and be
    ///   readable during the call to WriteFormData, otherwise an exception
    ///   will be thrown.
    /// @param content_type
    ///   Can be used to set content type for each file. If not set, the
    ///   protocol assumes it to be 'application/octet-stream'.
    void AddFile(CTempString entry_name,
                 CTempString file_name,
                 CTempString content_type = CTempString());

    /// Add custom data provider. The data written by the provider is
    /// properly prefixed with Content-Disposition, boundary, Content-Type etc.
    /// The form content type is automatically set to eMultipartFormData and
    /// can not be changed later.
    /// @param entry_name
    ///   Name of the form input responsible for the data. Multiple providers
    ///   can be added with the same entry name. The name must not be empty,
    ///   otherwise an exception is thrown.
    /// @param provider
    ///   An instance of CFormDataProvider_Base derived class. The object must
    ///   be created in heap.
    void AddProvider(CTempString             entry_name,
                     CFormDataProvider_Base* provider);

    /// Check if the form data is empty (no entries have been added).
    bool IsEmpty(void) const;

    /// Clear all form data, reset content type to the default eFormUrlEncoded.
    void Clear(void);

    /// Write form data to the stream using the selected form content type.
    /// If the data is not valid (e.g. a file does not exist or can not be
    /// read), throw CHttpSessionException.
    void WriteFormData(CNcbiOstream& out) const;

    /// Get current content type.
    EContentType GetContentType(void) const { return m_ContentType; }
    /// Set content type for the form data. If the new content type conflicts
    /// with the types of entries already added, throw an exception (e.g.
    /// files/providers require eMultipartFormData content type).
    /// @note Content types for individual entries can be set when adding
    /// an entry (see AddEntry, AddFile, AddProvider).
    void SetContentType(EContentType content_type);

    virtual ~CHttpFormData(void) {}

private:
    friend class CHttpRequest;

    // Create new data object. The default content type is eFormUrlEncoded.
    CHttpFormData(void);

    // Prohibit copying.
    CHttpFormData(const CHttpFormData&);
    CHttpFormData& operator=(const CHttpFormData&);

    struct SFormData {
        string m_Value;
        string m_ContentType;
    };

    typedef vector<SFormData>   TValues;
    typedef map<string, TValues> TEntries;
    typedef vector< CRef<CFormDataProvider_Base> > TProviders;
    typedef map<string, TProviders> TProviderEntries;

    EContentType     m_ContentType;
    TEntries         m_Entries;   // Normal entries
    TProviderEntries m_Providers; // Files and custom providers
    mutable string   m_Boundary;  // Main boundary string

public:
    /// Get the form content type as a string.
    string GetContentTypeStr(void) const;

    /// Generate a new random string to be used as multipart boundary.
    static string CreateBoundary(void);
};


class CHttpRequest;
class CHttpSession_Base;


/// HTTP response
class NCBI_XCONNECT_EXPORT CHttpResponse : public CObject
{
public:
    /// Get incoming HTTP headers.
    const CHttpHeaders& Headers(void) const { return *m_Headers; }

    /// Get input stream. If the status code indicates that there's
    /// no content to be read, throw CHttpSessionException. The actual
    /// request body (e.g. error page) can be read using ErrorStream().
    CNcbiIstream& ContentStream(void) const;

    /// Get input stream containing error message (e.g. 404 page)
    /// If the status code indicates that valid content is available
    /// the method throws CHttpSessionException.
    CNcbiIstream& ErrorStream(void) const;

    /// Get actual resource location. This may be different from the
    /// requested URL in case of redirects.
    const CUrl& GetLocation(void) const { return m_Location; }

    /// Get response status code.
    int GetStatusCode(void) const { return m_StatusCode; }

    /// Get response status text.
    const string& GetStatusText(void) const { return m_StatusText; }

    /// Check if the requested content can be read from the content stream.
    /// This is true for status codes 2xx. If there's no content available,
    /// ContentStream() will throw an exception and response body can be
    /// accessed using ErrorStream().
    /// @note This method returns true even if the content stream is empty
    /// (e.g. after HEAD request), it's based on status code only.
    bool CanGetContentStream(void) const;

    virtual ~CHttpResponse(void) {}

private:
    friend class CHttpRequest;

    CHttpResponse(CHttpSession_Base& session, const CUrl& url, shared_ptr<iostream> stream = {});

    // Update response headers and location, parse cookies and store them in the session.
    void x_Update(CHttpHeaders::THeaders headers, int status_code, string status_text);

    CRef<CHttpSession_Base> m_Session;
    CUrl                    m_Url;      // Original URL
    CUrl                    m_Location; // Redirection or the original URL.
    shared_ptr<iostream>    m_Stream;
    CRef<CHttpHeaders>      m_Headers;
    int                     m_StatusCode;
    string                  m_StatusText;
};


/// Wrapper class for NCBI_CRED
class NCBI_XCONNECT_EXPORT CTlsCertCredentials : virtual protected CConnIniter
{
public:
    /// Initialize credentials. The arguments are passed to NcbiCreateTlsCertCredentials.
    /// @sa
    ///  NcbiCreateTlsCertCredentials
    CTlsCertCredentials(const CTempStringEx& cert, const CTempStringEx& pkey);
    ~CTlsCertCredentials(void);

    NCBI_CRED GetNcbiCred(void) const;
    const string& GetCert(void) const { return m_Cert; }
    const string& GetPKey(void) const { return m_PKey; }

private:
    CTlsCertCredentials(const CTlsCertCredentials&);
    CTlsCertCredentials& operator=(const CTlsCertCredentials&);

    string m_Cert;
    string m_PKey;
    mutable NCBI_CRED m_Cred = nullptr;
};


/// HTTP request
class NCBI_XCONNECT_EXPORT CHttpRequest
{
public:
    /// Get HTTP headers to be sent.
    CHttpHeaders& Headers(void) { return *m_Headers; }

    /// Get form data to be sent with POST request. Throw an exception
    /// if the selected method is not POST or if the request is already
    /// being executed (after calling ContentStream() but before Execute()).
    CHttpFormData& FormData(void);

    /// Get output stream to write user data.
    /// Headers are sent automatically when opening the stream.
    /// No changes can be made to the request after getting the stream
    /// until it is completed by calling Execute().
    /// Throws an exception if the selected request method does not
    /// support sending data or if the existing form data is not empty.
    /// @note This method automatically adds cookies to the request headers.
    CNcbiOstream& ContentStream(void);

    /// Send the request, initialize and return the response. The output
    /// content stream is flushed and closed by this call.
    /// After executing a request it can be modified (e.g. more headers
    /// and/or form data added) and re-executed.
    /// @note This method automatically adds cookies to the request headers.
    CHttpResponse Execute(void);

    /// Get current timeout. If set to CTimeout::eDefault, the global
    /// default value is used (or the one from $CONN_TIMEOUT).
    const CTimeout& GetTimeout(void) const { return m_Timeout; }
    /// Set new timeout.
    CHttpRequest& SetTimeout(const CTimeout& timeout);
    /// Set new timeout in seconds/microseconds.
    CHttpRequest& SetTimeout(unsigned int sec, unsigned int usec = 0);

    /// Get number of retries. If not set returns the global default
    /// value ($CONN_MAX_TRY - 1).
    THttpRetries GetRetries(void) const { return m_Retries; }
    /// Set number of retries. If not set, the global default
    /// value is used ($CONN_MAX_TRY - 1).
    void SetRetries(THttpRetries retries) { m_Retries = retries; }

    /// Get current deadline for Execute().
    /// For now, effective only if waiting for actual response (as opposed to
    /// a recognized 'retry later' response), infinite by default.
    /// @sa Execute() GetRetryProcessing()
    const CTimeout& GetDeadline() const { return m_Deadline; }
    /// Set new deadline for Execute().
    /// @sa Execute() SetRetryProcessing()
    CHttpRequest& SetDeadline(const CTimeout& deadline);

    /// Return whether Execute() will wait for actual response.
    /// If on, will wait for deadline expired or actual response
    /// (as opposed to a recognized 'retry later' response), off by default.
    /// @sa Execute() GetDeadline()
    ESwitch GetRetryProcessing() const { return m_RetryProcessing; }
    /// Set whether Execute() should wait for actual response.
    /// @sa Execute() SetDeadline()
    CHttpRequest& SetRetryProcessing(ESwitch on_off);

    /// Set callback to adjust URL after resolving service location.
    /// The callback must take a CUrl reference and return bool:
    /// bool AdjustUrlCallback(CUrl& url);
    /// The callback should return true for the adjusted URL to be used to
    /// make the request, or false if the changes should be discarded.
    template<class TCallback>
    void SetAdjustUrlCallback(TCallback callback) {
        m_AdjustUrl.Reset(new CAdjustUrlCallback<TCallback>(callback));
    }

    /// Set proxy.
    void SetProxy(const CHttpProxy& proxy) { m_Proxy = proxy; }
    const CHttpProxy& GetProxy(void) const { return m_Proxy; }

    /// Set request parameters.
    /// @sa CHttpParam
    void SetParam(const CHttpParam& param);

private:
    friend class CHttpSession_Base;
    friend class CHttpSession;
    friend class CHttp2Session;

    CHttpRequest(CHttpSession_Base& session, const CUrl& url, EReqMethod method, const CHttpParam& param = {});

    class CAdjustUrlCallback_Base : public CObject {
    public:
        virtual ~CAdjustUrlCallback_Base(void) {}
        virtual bool AdjustUrl(CUrl& url) = 0;
    };

    template<class TCallback>
    class CAdjustUrlCallback : public CAdjustUrlCallback_Base {
    public:
        CAdjustUrlCallback(TCallback& callback) : m_Callback(callback) {}
        virtual bool AdjustUrl(CUrl& url) { return m_Callback(url); }
    private:
        TCallback m_Callback;
    };

    // Open connection, initialize response.
    void x_InitConnection(bool use_form_data);
    void x_InitConnection2(shared_ptr<iostream> stream);

    bool x_CanSendData(void) const;

    // Find cookies matching the url, add or replace 'Cookie' header with the
    // new values.
    void x_AddCookieHeader(const CUrl& url, bool initial);

    void x_AdjustHeaders(bool use_form_data);
    void x_UpdateResponse(CHttpHeaders::THeaders headers, int status_code, string status_text);
    void x_SetProxy(SConnNetInfo& net_info);

    // CConn_HttpStream callback for parsing headers.
    // 'user_data' must point to a CHttpRequest object.
    static EHTTP_HeaderParse sx_ParseHeader(const char* headers,
                                            void*       user_data,
                                            int         server_error);
    // CConn_HttpStream callback for handling retries and redirects.
    // 'user_data' must point to a CHttpRequest object.
    static int sx_Adjust(SConnNetInfo* net_info,
                         void*         user_data,
                         unsigned int  failure_count);

    CRef<CHttpSession_Base> m_Session;
    CUrl                    m_Url;
    EReqMethod              m_Method;
    CRef<CHttpHeaders>      m_Headers;
    CRef<CHttpFormData>     m_FormData;
    shared_ptr<iostream>    m_Stream;
    CRef<CHttpResponse>     m_Response; // current response or null
    CTimeout                m_Timeout;
    THttpRetries            m_Retries;
    CTimeout                m_Deadline;
    ESwitch                 m_RetryProcessing;
    CRef<CAdjustUrlCallback_Base> m_AdjustUrl;
    // Store credentials so that they are not destroyed while request is being executed.
    shared_ptr<CTlsCertCredentials> m_Credentials;
    CHttpProxy              m_Proxy;
};


/// HTTP session class, holding common data for multiple requests.
class NCBI_XCONNECT_EXPORT CHttpSession_Base : public CObject,
                                               virtual protected CConnIniter
{
public:
    /// Supported request methods, proxy for EReqMethod.
    /// @sa EReqMethod
    enum ERequestMethod {
        eHead   = eReqMethod_Head,
        eGet    = eReqMethod_Get,
        ePost   = eReqMethod_Post,
        ePut    = eReqMethod_Put,
		ePatch  = eReqMethod_Patch,
        eDelete = eReqMethod_Delete
    };

    /// Initialize request. This does not open connection to the server.
    /// A user can set headers/form-data before opening the stream and
    /// sending the actual request.
    /// The default request method is GET.
    CHttpRequest NewRequest(const CUrl& url, ERequestMethod method = eGet, const CHttpParam& param = {});

    /// Shortcut for GET requests.
    /// @param url
    ///   URL to send request to.
    /// @sa NewRequest() CHttpRequest
    CHttpResponse Get(const CUrl&     url,
                      const CTimeout& timeout = CTimeout(CTimeout::eDefault),
                      THttpRetries    retries = null);

    /// Shortcut for POST requests.
    /// @param url
    ///   URL to send request to.
    /// @param data
    ///   Data to be sent with the request. The data is sent as-is,
    ///   any required encoding must be performed by the caller.
    /// @param content_type
    ///   Content-type. If empty, application/x-www-form-urlencoded
    ///   is used.
    /// @sa NewRequest() CHttpRequest
    CHttpResponse Post(const CUrl&     url,
                       CTempString     data,
                       CTempString     content_type = CTempString(),
                       const CTimeout& timeout = CTimeout(CTimeout::eDefault),
                       THttpRetries    retries = null);

    /// Shortcut for PUT requests.
    /// @param url
    ///   URL to send request to.
    /// @param data
    ///   Data to be sent with the request. The data is sent as-is,
    ///   any required encoding must be performed by the caller.
    /// @param content_type
    ///   Content-type. If empty, application/x-www-form-urlencoded
    ///   is used.
    /// @sa NewRequest() CHttpRequest
    CHttpResponse Put(const CUrl&     url,
                      CTempString     data,
                      CTempString     content_type = CTempString(),
                      const CTimeout& timeout = CTimeout(CTimeout::eDefault),
                      THttpRetries    retries = null);

    /// Get all stored cookies.
    const CHttpCookies& Cookies(void) const { return m_Cookies; }
    /// Get all stored cookies, non-const.
    CHttpCookies& Cookies(void) { return m_Cookies; }

    /// HTTP protocol version.
    enum EProtocol {
        eHTTP_10, ///< HTTP/1.0
        eHTTP_11, ///< HTTP/1.1
        eHTTP_2   ///< HTTP/2
    };

    /// Get protocol version.
    EProtocol GetProtocol(void) const { return m_Protocol; }
    void SetProtocol(EProtocol protocol) { m_Protocol = protocol; }

    /// Get flags passed to CConn_HttpStream.
    /// @sa SetHttpFlags
    THTTP_Flags GetHttpFlags(void) const { return m_HttpFlags; }
    /// Set flags passed to CConn_HttpStream. When sending request,
    /// fHTTP_AdjustOnRedirect is always added to the flags.
    /// @sa GetHttpFlags
    void SetHttpFlags(THTTP_Flags flags) { m_HttpFlags = flags; }

    /// Set TLS credentials.
    /// @sa CTlsCertCredentials
    void SetCredentials(shared_ptr<CTlsCertCredentials> cred);
    shared_ptr<CTlsCertCredentials> GetCredentials(void) const { return m_Credentials; }

    CHttpSession_Base(EProtocol protocol);
    virtual ~CHttpSession_Base(void) {}

    void SetProxy(const CHttpProxy& proxy) { m_Proxy = proxy; }
    const CHttpProxy& GetProxy(void) const { return m_Proxy; }
private:
    friend class CHttpRequest;
    friend class CHttpResponse;

    virtual void x_StartRequest(EProtocol protocol, CHttpRequest& req, bool use_form_data) = 0;
    virtual bool x_Downgrade(CHttpResponse& resp, EProtocol& protocol) const = 0;

    // Save cookies returned by a response.
    void x_SetCookies(const CHttpHeaders::THeaderValues& cookies,
                      const CUrl*                        url);
    // Get a single 'Cookie' header line for the url.
    string x_GetCookies(const CUrl& url) const;

    EProtocol    m_Protocol;
    THTTP_Flags  m_HttpFlags;
    CHttpCookies m_Cookies;
    shared_ptr<CTlsCertCredentials> m_Credentials;
    CHttpProxy   m_Proxy;
};


/// @sa CHttpSession_Base
class NCBI_XCONNECT_EXPORT CHttpSession : public CHttpSession_Base
{
public:
    CHttpSession() : CHttpSession_Base(CHttpSession_Base::eHTTP_10) {}

private:
    void x_StartRequest(EProtocol _DEBUG_ARG(protocol), CHttpRequest& req, bool use_form_data) override
    {
        _ASSERT(protocol <= CHttpSession_Base::eHTTP_11);
        req.x_InitConnection(use_form_data);
    }

    bool x_Downgrade(CHttpResponse&, EProtocol&) const override
    {
        return false;
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CHttpSessionException --
///
///   Exceptions used by CHttpSession, CHttpRequest and CHttpResponse
///   classes.

class NCBI_XCONNECT_EXPORT CHttpSessionException : public CException
{
public:
    enum EErrCode {
        eConnFailed,      ///< Failed to open connection.
        eBadRequest,      ///< Error initializing or sending a request.
        eBadContentType,  ///< Content-type conflicts with the data.
        eBadFormDataName, ///< Empty or bad name in form data.
        eBadFormData,     ///< Bad form data (e.g. unreadable file).
        eBadStream,       ///< Wrong stream used to read content or error.
        eOther
    };

    virtual const char* GetErrCodeString(void) const override;

    NCBI_EXCEPTION_DEFAULT(CHttpSessionException, CException);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_HTTP_SESSION__HPP */
