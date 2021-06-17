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
 */

/// @Name_std.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'general.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Name_std_.hpp


#ifndef OBJECTS_GENERAL_NAME_STD_HPP
#define OBJECTS_GENERAL_NAME_STD_HPP


// generated includes
#include <objects/general/Name_std_.hpp>

#include <util/static_set.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_GENERAL_EXPORT CName_std : public CName_std_Base
{
    typedef CName_std_Base Tparent;
public:

    typedef CStaticArraySet<string> TSuffixes;

    /// constructor
    CName_std(void);
    /// destructor
    ~CName_std(void);

    /// Get a list of NCBI's standard suffixes.
    static const TSuffixes& GetStandardSuffixes(void);
    static void FixSuffix(string& suffix);

    bool ExtractSuffixFromLastName();

private:
    // Prohibit copy constructor and assignment operator
    CName_std(const CName_std& value);
    CName_std& operator=(const CName_std& value);
    
};

/////////////////// CName_std inline methods

// constructor
inline
CName_std::CName_std(void)
{
}

/////////////////// end of CName_std inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_GENERAL_NAME_STD_HPP
/* Original file checksum: lines: 94, chars: 2559, CRC32: 783940f4 */
