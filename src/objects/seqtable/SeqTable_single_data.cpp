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
 *   using the following specifications:
 *   'seqtable.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/seqtable/SeqTable_single_data.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeqTable_single_data::~CSeqTable_single_data(void)
{
}


void CSeqTable_single_data::ThrowOverflowError(Int8 value,
                                               const char* type_name)
{
    NCBI_THROW_FMT(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_single_data::GetValue("<<type_name<<"&): "
                   "value is too big for requested type: "<<value);
}


void CSeqTable_single_data::ThrowConversionError(const char* type_name) const
{
    NCBI_THROW_FMT(CSeqTableException, eIncompatibleValueType,
                   "CSeqTable_single_data::GetValue("<<type_name<<"&): "
                   <<SelectionName(Which())<<
                   " value cannot be converted to requested type");
}


template<class DstInt, class SrcInt>
static inline
bool sx_DownCast(DstInt& v, const SrcInt& value, const char* type_name)
{
    v = DstInt(value);
    if ( SrcInt(v) != value ) {
        CSeqTable_single_data::ThrowOverflowError(value, type_name);
    }
    return true;
}


void CSeqTable_single_data::GetValue(bool& v) const
{
    static const char* const type_name = "bool";
    switch ( Which() ) {
    case e_Bit:
        v = GetBit();
        break;
    case e_Int:
        sx_DownCast(v, GetInt(), type_name);
        break;
    case e_Int8:
        sx_DownCast(v, GetInt8(), type_name);
        break;
    default:
        ThrowConversionError(type_name);
    }
}


void CSeqTable_single_data::GetValue(Int1& v) const
{
    static const char* const type_name = "Int1";
    switch ( Which() ) {
    case e_Bit:
        v = GetBit();
        break;
    case e_Int:
        sx_DownCast(v, GetInt(), type_name);
        break;
    case e_Int8:
        sx_DownCast(v, GetInt8(), type_name);
        break;
    default:
        ThrowConversionError(type_name);
    }
}


void CSeqTable_single_data::GetValue(Int2& v) const
{
    static const char* const type_name = "Int2";
    switch ( Which() ) {
    case e_Bit:
        v = GetBit();
        break;
    case e_Int:
        sx_DownCast(v, GetInt(), type_name);
        break;
    case e_Int8:
        sx_DownCast(v, GetInt8(), type_name);
        break;
    default:
        ThrowConversionError(type_name);
    }
}


void CSeqTable_single_data::GetValue(int& v) const
{
    static const char* const type_name = "int";
    switch ( Which() ) {
    case e_Bit:
        v = GetBit();
        break;
    case e_Int:
        v = GetInt();
        break;
    case e_Int8:
        sx_DownCast(v, GetInt8(), type_name);
        break;
    default:
        ThrowConversionError(type_name);
    }
}


void CSeqTable_single_data::GetValue(Int8& v) const
{
    switch ( Which() ) {
    case e_Bit:
        v = GetBit();
        break;
    case e_Int:
        v = GetInt();
        break;
    case e_Int8:
        v = GetInt8();
        break;
    default:
        ThrowConversionError("Int8");
    }
}


void CSeqTable_single_data::GetValue(double& v) const
{
    switch ( Which() ) {
    case e_Bit:
        v = GetBit();
        break;
    case e_Int:
        v = GetInt();
        break;
    case e_Int8:
        v = GetInt8();
        break;
    case e_Real:
        v = GetReal();
        break;
    default:
        ThrowConversionError("double");
    }
}


void CSeqTable_single_data::GetValue(string& v) const
{
    if ( IsString() ) {
        v = GetString();
    }
    else {
        ThrowConversionError("string");
    }
}


void CSeqTable_single_data::GetValue(TBytes& v) const
{
    if ( IsBytes() ) {
        v = GetBytes();
    }
    else {
        ThrowConversionError("vector<char>");
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
