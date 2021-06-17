#ifndef CORELIB___NCBIAPP__HPP
#define CORELIB___NCBIAPP__HPP

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
 * Authors:  Denis Vakatov, Vsevolod Sandomirskiy
 *
 *
 */

/// @file ncbiapp.hpp
/// Defines the CNcbiApplication and CAppException classes for creating
/// NCBI applications.
///
/// The CNcbiApplication class defines the application framework and the high
/// high level behavior of an application, and the CAppException class is used
/// for the exceptions generated by CNcbiApplication.

#include <corelib/ncbiapp_api.hpp>
#include <corelib/version.hpp>

#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
#  define NcbiSys_main        wmain
#else
#  define NcbiSys_main         main
#endif

BEGIN_NCBI_SCOPE

#ifdef CNcbiApplication
# undef CNcbiApplication
#endif

// Set the version number for the program in the form "major.minor.patch" 
// and also record all build information pertaining to building the app itself 
// (and not the C++ Toolkit against which it's built). 
#define NCBI_APP_SET_VERSION(major, minor, patch) \
    SetVersion( CVersionInfo(major,minor,patch, NCBI_TEAMCITY_PROJECT_NAME_PROXY), NCBI_APP_SBUILDINFO_DEFAULT())

// Set the version number for the program in the form "major.minor.teamcity_build_number" 
// and also record all build information pertaining to building the app itself 
// (and not the C++ Toolkit against which it's built). 
// Uses zero patch level if NCBI_TEAMCITY_BUILD_NUMBER is unavailable. 
#define NCBI_APP_SET_VERSION_AUTO(major, minor) NCBI_APP_SET_VERSION(major, minor, NCBI_TEAMCITY_BUILD_NUMBER_PROXY) 


class NCBI_XNCBI_EXPORT CNcbiApplication : public CNcbiApplicationAPI
{
public:
    /// Singleton method.
    ///
    /// Track the instance of CNcbiApplicationAPI, and throw an exception
    /// if an attempt is made to create another instance of the application.
    /// @return
    ///   Current application instance.
    /// @sa
    ///   GetInstanceMutex()
    static CNcbiApplication* Instance(void);

    /// Destructor.
    ///
    /// Clean up the application settings, flush the diagnostic stream.
    virtual ~CNcbiApplication(void);

    /// Constructor.
    ///
    /// Register the application instance, and reset important
    /// application-specific settings to empty values that will
    /// be set later.
    explicit
    CNcbiApplication(const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT());
};


END_NCBI_SCOPE

#endif  /* CORELIB___NCBIAPP__HPP */
