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

/// @file Entrezgene.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'entrezgene.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Entrezgene_.hpp


#ifndef OBJECTS_ENTREZGENE_ENTREZGENE_HPP
#define OBJECTS_ENTREZGENE_ENTREZGENE_HPP


// generated includes
#include <objects/entrezgene/Entrezgene_.hpp>

#include <objects/seqfeat/Gene_nomenclature.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_ENTREZGENE_EXPORT CEntrezgene : public CEntrezgene_Base
{
    typedef CEntrezgene_Base Tparent;
public:
    // constructor
    CEntrezgene(void);
    // destructor
    ~CEntrezgene(void);

public:

    // Find or assemble as much nomenclature information as we can from the
    // data that are present.
    CRef<CGene_nomenclature> GetNomenclature() const;

    // Produce a standard description of the gene.
    string GetDescription() const;

    // Try to find a root-level comment with the given heading.
    CRef<CGene_commentary> FindComment(const string& heading) const; 

private:
    // Prohibit copy constructor and assignment operator
    CEntrezgene(const CEntrezgene& value);
    CEntrezgene& operator=(const CEntrezgene& value);

};

/////////////////// CEntrezgene inline methods

// constructor
inline
CEntrezgene::CEntrezgene(void)
{
}


/////////////////// end of CEntrezgene inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_ENTREZGENE_ENTREZGENE_HPP
/* Original file checksum: lines: 140737443291734, chars: 2453, CRC32: 62e5311c */
