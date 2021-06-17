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

/// @file DocSum.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'esummary.dtd'.
///
/// New methods or data members can be added to it if needed.
/// See also: DocSum_.hpp


#ifndef esummary__OBJTOOLS_EUTILS_ESUMMARY_DOCSUM_HPP
#define esummary__OBJTOOLS_EUTILS_ESUMMARY_DOCSUM_HPP

#include <objtools/eutils/esummary/Item.hpp>

// generated includes
#include <objtools/eutils/esummary/DocSum_.hpp>

// generated classes

BEGIN_esummary_SCOPE // namespace esummary::

NCBI_USING_NAMESPACE_STD;

/////////////////////////////////////////////////////////////////////////////
class NCBI_EUTILS_EXPORT CDocSum : public CDocSum_Base
{
    typedef CDocSum_Base Tparent;
public:
    // constructor
    CDocSum(void);
    // destructor
    ~CDocSum(void);

    /// Return string value of the item with the given key or empty string if
    /// the key does not exist. If the item contains multiple strings only the
    /// first one is returned.
    const string& FindValue(const string& key) const;

    /// Return item for the key or NULL if the key does not exist.
    const CItem* FindItem(const string& key) const;

private:
    // Prohibit copy constructor and assignment operator
    CDocSum(const CDocSum& value);
    CDocSum& operator=(const CDocSum& value);

};

/////////////////// CDocSum inline methods

// constructor
inline
CDocSum::CDocSum(void)
{
}


/////////////////// end of CDocSum inline methods


END_esummary_SCOPE // namespace esummary::


#endif // esummary__OBJTOOLS_EUTILS_ESUMMARY_DOCSUM_HPP
/* Original file checksum: lines: 82, chars: 2387, CRC32: 82a6fe10 */
