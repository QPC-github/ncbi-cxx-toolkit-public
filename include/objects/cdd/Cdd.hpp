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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'cdd.asn'.
 */

#ifndef OBJECTS_CDD_CDD_HPP
#define OBJECTS_CDD_CDD_HPP


// generated includes
#include <objects/cdd/Cdd_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_CDD_EXPORT CCdd : public CCdd_Base
{
    typedef CCdd_Base Tparent;
public:
    CCdd(void);                                     // constructor
    ~CCdd(void);                                    // destructor

private:
    // Prohibit copy constructor and assignment operator
    CCdd(const CCdd& value);
    CCdd& operator=(const CCdd& value);

};



/////////////////// CCdd inline methods

// constructor
inline
CCdd::CCdd(void)
{
}


/////////////////// end of CCdd inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_CDD_CDD_HPP
/* Original file checksum: lines: 90, chars: 2283, CRC32: e0b16239 */
