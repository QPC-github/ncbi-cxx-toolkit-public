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

/// @Pubdesc.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'seq.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Pubdesc_.hpp


#ifndef OBJECTS_SEQ_PUBDESC_HPP
#define OBJECTS_SEQ_PUBDESC_HPP


// generated includes
#include <objects/seq/Pubdesc_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_SEQ_EXPORT CPubdesc : public CPubdesc_Base
{
    typedef CPubdesc_Base Tparent;
public:
    // constructor
    CPubdesc(void);
    // destructor
    ~CPubdesc(void);

private:
    // Prohibit copy constructor and assignment operator
    CPubdesc(const CPubdesc& value);
    CPubdesc& operator=(const CPubdesc& value);

};

/////////////////// CPubdesc inline methods

// constructor
inline
CPubdesc::CPubdesc(void)
{
}


/////////////////// end of CPubdesc inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_SEQ_PUBDESC_HPP
/* Original file checksum: lines: 94, chars: 2516, CRC32: 69e228f3 */
