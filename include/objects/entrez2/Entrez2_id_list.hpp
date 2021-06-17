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
 *   'entrez2.asn'.
 *
 */

#ifndef OBJECTS_ENTREZ2_ENTREZ2_ID_LIST_HPP
#define OBJECTS_ENTREZ2_ENTREZ2_ID_LIST_HPP

#include <util/resize_iter.hpp>

// generated includes
#include <objects/entrez2/Entrez2_id_list_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_ENTREZ2_EXPORT CEntrez2_id_list : public CEntrez2_id_list_Base
{
    typedef CEntrez2_id_list_Base Tparent;
public:
    typedef CConstResizingIterator<TUids> TConstUidIterator;
    typedef CResizingIterator<TUids>      TUidIterator;
    typedef TIntId TUid;

    // constructor
    CEntrez2_id_list(void);
    // destructor
    ~CEntrez2_id_list(void);

    // so apps don't have to (de)marshal the list
    TUidIterator GetUidIterator();
    TConstUidIterator GetConstUidIterator() const;

    // resize the container to hold a certain number of UIDs
    void Resize(size_t size);

    // set the container to a set of integer UIDs
    void AssignUids(const vector<TUid>& uids);

    static const size_t sm_UidSize; // bytes

private:
    // Prohibit copy constructor and assignment operator
    CEntrez2_id_list(const CEntrez2_id_list& value);
    CEntrez2_id_list& operator=(const CEntrez2_id_list& value);

};



/////////////////// CEntrez2_id_list inline methods

// constructor
inline
CEntrez2_id_list::CEntrez2_id_list(void)
{
}


/////////////////// end of CEntrez2_id_list inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_ENTREZ2_ENTREZ2_ID_LIST_HPP
/* Original file checksum: lines: 90, chars: 2507, CRC32: 92bb06a4 */
