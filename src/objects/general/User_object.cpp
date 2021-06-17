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
 *   'general.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


SAFE_CONST_STATIC_STRING(kUnverifiedOrganism,     "Organism");
SAFE_CONST_STATIC_STRING(kUnverifiedMisassembled, "Misassembled");
SAFE_CONST_STATIC_STRING(kUnverifiedFeature,      "Features");
SAFE_CONST_STATIC_STRING(kUnverifiedContaminant, "Contaminant");


// destructor
CUser_object::~CUser_object(void)
{
}


//
// retrieve a named field.  The named field can recurse, depending
// on a set of user-defined delimiters
//
const CUser_field& CUser_object::GetField(const string& str,
                                          const string& delim,
                                          NStr::ECase use_case) const
{
    CConstRef<CUser_field> ref(GetFieldRef(str, delim, use_case));
    if (ref.Empty()) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "Unable to find User-field " + str);
    }
    return *ref;
}


CConstRef<CUser_field> CUser_object::GetFieldRef(const string& str,
                                                 const string& delim,
                                                 NStr::ECase use_case) const
{
    list<string> toks;
    NStr::Split(str, delim, toks, NStr::fSplit_Tokenize);
    if ( !toks.size() ) {
        return CConstRef<CUser_field>();
    }

    ///
    /// step 1: scan one level deep for a nested object
    /// we scan only with the first token
    ///
    string sub;
    string first;
    {{
        list<string>::iterator iter = toks.begin();
        first = *iter;
        for (++iter;  iter != toks.end();  ++iter) {
            if ( !sub.empty() ) {
                sub += delim;
            }
            sub += *iter;
        }
    }}

    ITERATE(TData, field_iter, GetData()) {
        bool found = false;
        const CUser_field& field = **field_iter;
        if (field.IsSetLabel()  &&  field.GetLabel().IsStr()) {
            const string& this_label = field.GetLabel().GetStr();
            if ( NStr::Equal(this_label, first, use_case) ) {
                found = true;
            }
        }
        if ( !found ) {
            continue;
        }

        if ( !sub.empty() ) {
            CConstRef<CUser_field> field_ref =
                (*field_iter)->GetFieldRef(sub, delim, use_case);
            if (field_ref) {
                return field_ref;
            }
        } else {
            return CConstRef<CUser_field>(&field);
        }
    }
    return CConstRef<CUser_field>();
}


bool CUser_object::HasField(const string& str,
                            const string& delim,
                            NStr::ECase use_case) const
{
    return GetFieldRef(str, delim, use_case).GetPointer() ? true : false;
}


//
// retrieve a named field.  The named field can recurse, depending
// on a set of user-defined delimiters
//
CUser_field& CUser_object::SetField(const string& str,
                                    const string& delim,
                                    const string& obj_subtype,
                                    NStr::ECase use_case)
{
    return *SetFieldRef(str, delim, obj_subtype, use_case);
}


CRef<CUser_field> CUser_object::SetFieldRef(const string& str,
                                            const string& delim,
                                            const string& /* obj_subtype */,
                                            NStr::ECase use_case)
{
    list<string> toks;
    NStr::Split(str, delim, toks, NStr::fSplit_Tokenize);

    CRef<CUser_field>  field_ref;

    /// pass 1: see if we have a field that starts with this label already
    NON_CONST_ITERATE(TData, field_iter, SetData()) {
        CUser_field& field = **field_iter;
        if (field.GetLabel().IsStr()  &&
            NStr::Equal(field.GetLabel().GetStr(), toks.front(), use_case))
        {
            field_ref = *field_iter;
            break;
        }
    }

    if ( !field_ref ) {
        field_ref.Reset(new CUser_field());
        field_ref->SetLabel().SetStr(toks.front());
        SetData().push_back(field_ref);
    }

    toks.pop_front();
    if (toks.size()) {
        string s = NStr::Join(toks, delim);
        CRef<CUser_field> f = field_ref->SetFieldRef(s, delim, use_case);
        field_ref = f;
    }

    return field_ref;
}


// add a data field to the user object that holds a given value
CUser_object& CUser_object::AddField(const string& label,
                                     const string& value,
                                     EParseField parse)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value, CUser_field::EParseField(parse));

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const char* value,
                                     EParseField parse)
{
    return AddField(label, string(value), parse);
}


CUser_object& CUser_object::AddField(const string& label,
                                     Int8          value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     int           value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}


#ifdef NCBI_STRICT_GI
CUser_object& CUser_object::AddField(const string& label,
                                     TGi           value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}
#endif


CUser_object& CUser_object::AddField(const string& label,
                                     double        value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     bool          value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}



CUser_object& CUser_object::AddField(const string& label,
                                     CUser_object& object)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(object);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<string>& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<int>&    value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<double>& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);

    SetData().push_back(field);
    return *this;
}


CUser_object&
CUser_object::AddField(const string& label,
                       const vector< CRef<CUser_object> >& objects)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(objects);

    SetData().push_back(field);
    return *this;
}


CUser_object& CUser_object::AddField(const string& label,
                                     const vector<CRef<CUser_field> >& objects)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(objects);

    SetData().push_back(field);
    return *this;
}



// static consts here allow us to use reference counting
static const char* s_ncbi   = "NCBI";
static const char* s_expres = "experimental_results";
static const char* s_exp    = "experiment";
static const char* s_sage   = "SAGE";
static const char* s_tag    = "tag";
static const char* s_count  = "count";


// accessors: classify a given user object
CUser_object::ECategory CUser_object::GetCategory(void) const
{
    if (!IsSetClass()  ||
        GetClass() != s_ncbi) {
        // we fail to recognize non-NCBI classes of user-objects
        return eCategory_Unknown;
    }

    //
    // experimental results
    //
    if (GetType().IsStr()  &&
        NStr::CompareNocase(GetType().GetStr(), s_expres) == 0  &&
        GetData().size() == 1) {

        ITERATE (CUser_object::TData, iter, GetData()) {
            const CUser_field& field       = **iter;
            const CUser_field::TData& data = field.GetData();
            if (data.Which() != CUser_field::TData::e_Object  ||
                !field.IsSetLabel()  ||
                !field.GetLabel().IsStr()  ||
                NStr::CompareNocase(field.GetLabel().GetStr(),
                                    s_exp) != 0) {
                // poorly formed experiment spec
                return eCategory_Unknown;
            }
        }

        return eCategory_Experiment;
    }

    //
    // unrecognized - catch-all
    //
    return eCategory_Unknown;
}


// sub-category accessors:
CUser_object::EExperiment CUser_object::GetExperimentType(void) const
{
    // check to see if we have an experiment
    if ( GetCategory() != eCategory_Experiment) {
        return eExperiment_Unknown;
    }

    // we do - so we have one nested user object that contains the
    // specification of the experimental data
    const CUser_field& field = *GetData().front();
    const CUser_object& obj = field.GetData().GetObject();
    if (obj.GetType().IsStr()  &&
        NStr::CompareNocase(obj.GetType().GetStr(), s_sage) == 0) {
        return eExperiment_Sage;
    }

    //
    // catch-all
    //
    return eExperiment_Unknown;
}


// sub-category accessors:
const CUser_object& CUser_object::GetExperiment(void) const
{
    switch (GetExperimentType()) {
    case eExperiment_Sage:
        // we have one nested user object that contains the
        // specification of the experimental data
        return GetData().front()->GetData().GetObject();

    case eExperiment_Unknown:
    default:
        return *this;
    }
}


//
// format the type specification fo a given user object
//
static string s_GetUserObjectType(const CUser_object& obj)
{
    switch (obj.GetCategory()) {
    case CUser_object::eCategory_Experiment:
        switch (obj.GetExperimentType()) {
        case CUser_object::eExperiment_Sage:
            return "SAGE";

        case CUser_object::eExperiment_Unknown:
        default:
            return "Experiment";
        }
        break;

    case CUser_object::eCategory_Unknown:
    default:
        break;
    }

    return "User";
}


static string s_GetUserObjectContent(const CUser_object& obj)
{
    switch (obj.GetCategory()) {
    case CUser_object::eCategory_Experiment:
        switch (obj.GetExperimentType()) {
        case CUser_object::eExperiment_Sage:
            {{
                string label;
                const CUser_object& nested_obj =
                    obj.GetData().front()->GetData().GetObject();

                // grab the tag and count fields
                const CUser_field* tag = NULL;
                const CUser_field* count = NULL;
                ITERATE (CUser_object::TData, iter, nested_obj.GetData()) {
                    const CUser_field& field = **iter;
                    if (!field.GetLabel().IsStr()) {
                        continue;
                    }

                    const string& lbl = field.GetLabel().GetStr();
                    if (NStr::CompareNocase(lbl, s_tag) == 0) {
                        tag = &field;
                    } else if (NStr::CompareNocase(lbl, s_count) == 0) {
                        count = &field;
                    }
                }

                if (tag  &&  tag->GetData().IsStr()) {
                    if ( !label.empty() ) {
                        label += " ";
                    }
                    label += string(s_tag) + "=" + tag->GetData().GetStr();
                }

                if (count  &&  count->GetData().IsInt()) {
                    if ( !label.empty() ) {
                        label += " ";
                    }
                    label += string(s_count) + "=" +
                        NStr::NumericToString(count->GetData().GetInt());
                }

                return label;
            }}

        case CUser_object::eExperiment_Unknown:
        default:
            break;
        }
        return "[experiment]";

    case CUser_object::eCategory_Unknown:
    default:
        break;
    }
    return "[User]";
}


//
// append a formatted string to a label describing this object
//
void CUser_object::GetLabel(string* label, ELabelContent mode) const
{
    // Check the label is not null
    if (!label) {
        return;
    }

    switch (mode) {
    case eType:
        *label += s_GetUserObjectType(*this);
        break;
    case eContent:
        *label += s_GetUserObjectContent(*this);
        break;
    case eBoth:
        *label += s_GetUserObjectType(*this) + ": " +
            s_GetUserObjectContent(*this);
        break;
    }
}


CUser_object& CUser_object::SetCategory(ECategory category)
{
    Reset();
    SetClass(s_ncbi);
    switch (category) {
    case eCategory_Experiment:
        SetType().SetStr(s_expres);
        {{
            CRef<CUser_object> subobj(new CUser_object());
            AddField(s_exp, *subobj);
            SetClass(s_ncbi);
            return *subobj;
        }}

    case eCategory_Unknown:
    default:
        break;
    }

    return *this;
}


CUser_object& CUser_object::SetExperiment(EExperiment category)
{
    Reset();
    SetClass(s_ncbi);
    switch (category) {
    case eExperiment_Sage:
        SetType().SetStr(s_sage);
        break;

    case eExperiment_Unknown:
    default:
        break;
    }

    return *this;
}


static const char* kDBLink                = "DBLink";
static const char* kStructuredComment     = "StructuredComment";
static const char* kOriginalId            = "OriginalID";
static const char* kOrigIdAltSpell        = "OrginalID";
static const char* kUnverified            = "Unverified";
static const char* kValidationSuppression = "ValidationSuppression";
static const char* kNcbiCleanup           = "NcbiCleanup";
static const char* kAutoDefOptions        = "AutodefOptions";
static const char* kFileTrack             = "FileTrack";
static const char* kRefGeneTracking       = "RefGeneTracking";

typedef SStaticPair<const char*, CUser_object::EObjectType>  TObjectTypePair;
static const TObjectTypePair k_object_type_map[] = {
    { kAutoDefOptions, CUser_object::eObjectType_AutodefOptions },
    { kDBLink, CUser_object::eObjectType_DBLink },
    { kFileTrack, CUser_object::eObjectType_FileTrack },
    { kNcbiCleanup, CUser_object::eObjectType_Cleanup },
    { kOriginalId, CUser_object::eObjectType_OriginalId },
    { kRefGeneTracking, CUser_object::eObjectType_RefGeneTracking },
    { kStructuredComment, CUser_object::eObjectType_StructuredComment },
    { kUnverified, CUser_object::eObjectType_Unverified },
    { kValidationSuppression, CUser_object::eObjectType_ValidationSuppression }
};
typedef CStaticArrayMap<const char*, CUser_object::EObjectType, PNocase_CStr> TObjectTypeMap;
DEFINE_STATIC_ARRAY_MAP(TObjectTypeMap, sc_ObjectTypeMap, k_object_type_map);


CUser_object::EObjectType CUser_object::GetObjectType() const
{
    if (!IsSetType() || !GetType().IsStr()) {
        return eObjectType_Unknown;
    }
    EObjectType rval = eObjectType_Unknown;

    const string& label = GetType().GetStr();
    
    auto it = sc_ObjectTypeMap.find(label.c_str());
    if (it == sc_ObjectTypeMap.end()) {
        if (NStr::EqualNocase(label, kOrigIdAltSpell)) {
            rval = eObjectType_OriginalId;
        }
    } else {
        rval = it->second;
    }

    return rval;
}


void CUser_object::SetObjectType(EObjectType obj_type)
{
    for (auto it = sc_ObjectTypeMap.begin(); it != sc_ObjectTypeMap.end(); it++) {
        if (it->second == obj_type) {
            SetType().SetStr(it->first);
            return;
        }
    }
    ResetType();
}


bool CUser_object::x_IsUnverifiedType(const string& val, const CUser_field& field) const
{
    if (field.IsSetLabel() && field.GetLabel().IsStr()
        && NStr::Equal(field.GetLabel().GetStr(), "Type")
        && field.IsSetData()
        && field.GetData().IsStr()
        && NStr::Equal(field.GetData().GetStr(), val)) {
        return true;
    } else {
        return false;
    }
}


bool CUser_object::x_IsUnverifiedType(const string& val) const
{
    if (GetObjectType() != eObjectType_Unverified) {
        return false;
    }
    if (!IsSetData()) {
        return false;
    }
    bool found = false;

    ITERATE(CUser_object::TData, it, GetData()) {
        if (x_IsUnverifiedType(val, **it)) {
            found = true;
        }
    }
    return found;
}


void CUser_object::x_AddUnverifiedType(const string& val)
{
    SetObjectType(eObjectType_Unverified);
    if (x_IsUnverifiedType(val)) {
        // value already set, nothing to do
        return;
    }
    AddField("Type", val);
}


void CUser_object::x_RemoveUnverifiedType(const string& val)
{
    if (GetObjectType() != eObjectType_Unverified) {
        return;
    }
    if (!IsSetData()) {
        return;
    }
    CUser_object::TData::iterator it = SetData().begin();
while (it != SetData().end()) {
    if (x_IsUnverifiedType(val, **it)) {
        it = SetData().erase(it);
    } else {
        it++;
    }
}
if (GetData().empty()) {
    ResetData();
}
}


bool CUser_object::IsUnverifiedOrganism() const
{
    return x_IsUnverifiedType(kUnverifiedOrganism.Get());
}


void CUser_object::AddUnverifiedOrganism()
{
    x_AddUnverifiedType(kUnverifiedOrganism.Get());
}


void CUser_object::RemoveUnverifiedOrganism()
{
    x_RemoveUnverifiedType(kUnverifiedOrganism.Get());
}


bool CUser_object::IsUnverifiedFeature() const
{
    return x_IsUnverifiedType(kUnverifiedFeature.Get());
}


void CUser_object::AddUnverifiedFeature()
{
    x_AddUnverifiedType(kUnverifiedFeature.Get());
}


void CUser_object::RemoveUnverifiedFeature()
{
    x_RemoveUnverifiedType(kUnverifiedFeature.Get());
}


bool CUser_object::IsUnverifiedMisassembled() const
{
    return x_IsUnverifiedType(kUnverifiedMisassembled.Get());
}


void CUser_object::AddUnverifiedMisassembled()
{
    x_AddUnverifiedType(kUnverifiedMisassembled.Get());
}


void CUser_object::RemoveUnverifiedMisassembled()
{
    x_RemoveUnverifiedType(kUnverifiedMisassembled.Get());
}


bool CUser_object::IsUnverifiedContaminant() const
{
    return x_IsUnverifiedType(kUnverifiedContaminant.Get());
}


void CUser_object::AddUnverifiedContaminant()
{
    x_AddUnverifiedType(kUnverifiedContaminant.Get());
}


void CUser_object::RemoveUnverifiedContaminant()
{
    x_RemoveUnverifiedType(kUnverifiedContaminant.Get());
}


void CUser_object::UpdateNcbiCleanup(int version)
{
    SetObjectType(eObjectType_Cleanup);
    CRef<CUser_field> method = SetFieldRef("method");
    method->SetValue("ExtendedSeqEntryCleanup");
    CRef<CUser_field> version_field = SetFieldRef("version");
    version_field->SetValue(version);

    // get current time
    CTime curr_time(CTime::eCurrent);
    CRef<CUser_field> month = SetFieldRef("month");
    month->SetData().SetInt(curr_time.Month());
    CRef<CUser_field> day = SetFieldRef("day");
    day->SetData().SetInt(curr_time.Day());
    CRef<CUser_field> year = SetFieldRef("year");
    year->SetData().SetInt(curr_time.Year());
}


bool CUser_object::RemoveNamedField(const string& field_name, NStr::ECase ecase)
{
    if (!IsSetData()) {
        return false;
    }
    bool rval = false;
    auto it = SetData().begin();
    while (it != SetData().end()) {
        bool do_remove = false;
        if ((*it)->IsSetLabel()) {
            if ((*it)->GetLabel().IsStr()) {
                if (NStr::Equal((*it)->GetLabel().GetStr(), field_name, ecase)) {
                    do_remove = true;
                }
            } else if ((*it)->GetLabel().IsId()) {
                string label = NStr::NumericToString((*it)->GetLabel().GetId());
                if (NStr::Equal((*it)->GetLabel().GetStr(), field_name, ecase)) {
                    do_remove = true;
                }
            }
        }
        if (do_remove) {
            it = SetData().erase(it);
            rval = true;
        } else {
            it++;
        }
    }
    return rval;
}


void CUser_object::SetFileTrackURL(const string& url)
{
    SetObjectType(eObjectType_FileTrack);
    CRef<CUser_field> fturl = SetFieldRef("BaseModification-FileTrackURL");
    fturl->SetData().SetStr(url);
}


void CUser_object::SetFileTrackUploadId(const string& upload_id)
{
    string url = "https://submit.ncbi.nlm.nih.gov/ft/byid/" + upload_id;
    SetFileTrackURL(url);
}


// For RefGeneTracking

void CUser_object::x_SetRefGeneTrackingField(const string& field_name, const string& value)
{
    SetObjectType(eObjectType_RefGeneTracking);
    if (value.empty()) {
        RemoveNamedField(field_name);
    } else {
        CUser_field& field = SetField(field_name);
        field.SetString(value);
    }
}

const string& CUser_object::x_GetRefGeneTrackingField(const string& field_name) const
{
    if (GetObjectType() != eObjectType_RefGeneTracking) {
        return kEmptyStr;
    }
    CConstRef<CUser_field>  field = GetFieldRef(field_name);
    if (field && field->IsSetData() && field->GetData().IsStr()) {
        return field->GetData().GetStr();
    }
    return kEmptyStr;
}


typedef SStaticPair<const char*, CUser_object::ERefGeneTrackingStatus>  TRefGeneTrackingStatusPair;
static const TRefGeneTrackingStatusPair k_refgene_tracking_status_map[] = {
    { "INFERRED", CUser_object::eRefGeneTrackingStatus_INFERRED },
    { "PIPELINE", CUser_object::eRefGeneTrackingStatus_PIPELINE },
    { "PREDICTED", CUser_object::eRefGeneTrackingStatus_PREDICTED },
    { "PROVISIONAL", CUser_object::eRefGeneTrackingStatus_PROVISIONAL },
    { "REVIEWED", CUser_object::eRefGeneTrackingStatus_REVIEWED },
    { "VALIDATED", CUser_object::eRefGeneTrackingStatus_VALIDATED },
    { "WGS", CUser_object::eRefGeneTrackingStatus_WGS }
};
typedef CStaticArrayMap<const char*, CUser_object::ERefGeneTrackingStatus, PNocase_CStr> TRefGeneTrackingStatusMap;
DEFINE_STATIC_ARRAY_MAP(TRefGeneTrackingStatusMap, sc_RefGeneTrackingStatusMap, k_refgene_tracking_status_map);

const string kRefGeneTrackingStatus = "Status";

void CUser_object::SetRefGeneTrackingStatus(ERefGeneTrackingStatus status)
{
    for (auto it = sc_RefGeneTrackingStatusMap.begin(); it != sc_RefGeneTrackingStatusMap.end(); it++) {
        if (it->second == status) {
            x_SetRefGeneTrackingField(kRefGeneTrackingStatus, it->first);
            return;
        }
    }
    NCBI_THROW(CRefGeneTrackingException, eBadStatus, "Unrecognized RefGeneTracking Status");
}

CUser_object::ERefGeneTrackingStatus CUser_object::GetRefGeneTrackingStatus() const
{
    if (GetObjectType() != eObjectType_RefGeneTracking) {
        return eRefGeneTrackingStatus_Error;
    }
    CConstRef<CUser_field>  field = GetFieldRef(kRefGeneTrackingStatus);
    if (!field || !field->IsSetData()) {
        return eRefGeneTrackingStatus_NotSet;
    }
    if (!field->GetData().IsStr()) {
        return eRefGeneTrackingStatus_Error;
    }
    if (field->GetData().GetStr().empty()) {
        return eRefGeneTrackingStatus_NotSet;
    }
    auto it = sc_RefGeneTrackingStatusMap.find(field->GetData().GetStr().c_str());
    if (it != sc_RefGeneTrackingStatusMap.end()) {
        return it->second;
    }
    NCBI_THROW(CRefGeneTrackingException, eBadStatus, "Unrecognized RefGeneTracking Status " + field->GetData().GetStr());
}


void CUser_object::ResetRefGeneTrackingStatus()
{
    RemoveNamedField(kRefGeneTrackingStatus);
}


const string kRefGeneTrackingGenomicSource = "GenomicSource";

void CUser_object::SetRefGeneTrackingGenomicSource(const string& genomic_source)
{
    x_SetRefGeneTrackingField(kRefGeneTrackingGenomicSource, genomic_source);
}


const string& CUser_object::GetRefGeneTrackingGenomicSource() const
{
    return x_GetRefGeneTrackingField(kRefGeneTrackingGenomicSource);
}


void CUser_object::ResetRefGeneTrackingGenomicSource()
{
    RemoveNamedField(kRefGeneTrackingGenomicSource);
}


const string kRefGeneTrackingCollaborator = "Collaborator";
void CUser_object::SetRefGeneTrackingCollaborator(const string& val)
{
    x_SetRefGeneTrackingField(kRefGeneTrackingCollaborator, val);
}


const string& CUser_object::GetRefGeneTrackingCollaborator() const
{
    return x_GetRefGeneTrackingField(kRefGeneTrackingCollaborator);
}


void CUser_object::ResetRefGeneTrackingCollaborator()
{
    RemoveNamedField(kRefGeneTrackingCollaborator);
}


const string kRefGeneTrackingCollaboratorURL = "CollaboratorURL";
void CUser_object::SetRefGeneTrackingCollaboratorURL(const string& val)
{
    x_SetRefGeneTrackingField(kRefGeneTrackingCollaboratorURL, val);
}


const string& CUser_object::GetRefGeneTrackingCollaboratorURL() const
{
    return x_GetRefGeneTrackingField(kRefGeneTrackingCollaboratorURL);
}


void CUser_object::ResetRefGeneTrackingCollaboratorURL()
{
    RemoveNamedField(kRefGeneTrackingCollaboratorURL);
}


const string kRefGeneTrackingGenerated = "Generated";

void CUser_object::SetRefGeneTrackingGenerated(bool val)
{
    SetObjectType(eObjectType_RefGeneTracking);
    CUser_field& field = SetField(kRefGeneTrackingGenerated);
    field.SetBool(val);
}


bool CUser_object::GetRefGeneTrackingGenerated() const
{
    if (GetObjectType() != eObjectType_RefGeneTracking) {
        return false;
    }
    bool rval = false;
    CConstRef<CUser_field> field = GetFieldRef(kRefGeneTrackingGenerated);
    if (field && field->IsSetData() && field->GetData().IsBool() &&
        field->GetData().GetBool()) {
        rval = true;
    }
    return rval;
}


void CUser_object::ResetRefGeneTrackingGenerated()
{
    RemoveNamedField(kRefGeneTrackingGenerated);
}


const string kRGTAAccession = "accession";
const string kRGTAName = "name";
const string kRGTAGI = "gi";
const string kRGTAFrom = "from";
const string kRGTATo = "to";
const string kRGTAComment = "comment";

CRef<CUser_field> CUser_object::CRefGeneTrackingAccession::MakeAccessionField() const
{
    CRef<CUser_field> top(new CUser_field());
    //use IsSet construction here
    if (!NStr::IsBlank(m_Accession)) {
        CRef<CUser_field> uf(new CUser_field());
        uf->SetLabel().SetStr(kRGTAAccession);
        uf->SetData().SetStr(m_Accession);
        top->SetData().SetFields().push_back(uf);
    }
    if (!NStr::IsBlank(m_Name)) {
        CRef<CUser_field> uf(new CUser_field());
        uf->SetLabel().SetStr(kRGTAName);
        uf->SetData().SetStr(m_Name);
        top->SetData().SetFields().push_back(uf);
    }
    if (m_GI > ZERO_GI) {
        CRef<CUser_field> uf(new CUser_field());
        uf->SetLabel().SetStr(kRGTAGI);
        uf->SetData().SetInt(GI_TO(int, m_GI));
        top->SetData().SetFields().push_back(uf);
    }
    if (m_From != kInvalidSeqPos) {
        CRef<CUser_field> uf(new CUser_field());
        uf->SetLabel().SetStr(kRGTAFrom);
        uf->SetData().SetInt(m_From);
        top->SetData().SetFields().push_back(uf);
    }
    if (m_To != kInvalidSeqPos) {
        CRef<CUser_field> uf(new CUser_field());
        uf->SetLabel().SetStr(kRGTATo);
        uf->SetData().SetInt(m_To);
        top->SetData().SetFields().push_back(uf);
    }
    if (!NStr::IsBlank(m_Comment)) {
        CRef<CUser_field> uf(new CUser_field());
        uf->SetLabel().SetStr(kRGTAComment);
        uf->SetData().SetStr(m_Comment);
        top->SetData().SetFields().push_back(uf);
    }
    if (top->IsSetData()) {
        top->SetLabel().SetId(0);
    } else {
        top.Reset(NULL);
    }
    return top;
}


CRef<CUser_object::CRefGeneTrackingAccession> 
CUser_object::CRefGeneTrackingAccession::MakeAccessionFromUserField(const CUser_field& field)
{
    // TODO: Throw exception if no good fields or if any bad fields found
    if (!field.IsSetData() || !field.GetData().IsFields()) {
        return CRef<CRefGeneTrackingAccession>(NULL);
    }

    string accession, acc_name, comment;
    TSeqPos from = kInvalidSeqPos, to = kInvalidSeqPos;
    TGi gi = ZERO_GI;
    for (auto it : field.GetData().GetFields()) {
        if (it->IsSetLabel() && it->GetLabel().IsStr() && it->IsSetData()) {
            // finish taking out dereferences
            const string& label = it->GetLabel().GetStr();
            if (NStr::EqualNocase(label, kRGTAAccession)) {
                if (it->GetData().IsStr()) {
                    accession = it->GetData().GetStr();
                } else {
                    NCBI_THROW(CRefGeneTrackingException, eBadUserFieldData, kEmptyStr);
                }
            } else if (NStr::EqualNocase(label, kRGTAName)) {
                if (it->GetData().IsStr()) {
                    acc_name = it->GetData().GetStr();
                } else {
                    NCBI_THROW(CRefGeneTrackingException, eBadUserFieldData, kEmptyStr);
                }
            } else if (NStr::EqualNocase(label, kRGTAComment)) {
                if (it->GetData().IsStr()) {
                    comment = it->GetData().GetStr();
                } else {
                    NCBI_THROW(CRefGeneTrackingException, eBadUserFieldData, kEmptyStr);
                }
            } else if (NStr::EqualNocase(label, kRGTAGI)) {
                if (it->GetData().IsInt()) {
                    gi = GI_FROM(int, it->GetData().GetInt());
                } else {
                    NCBI_THROW(CRefGeneTrackingException, eBadUserFieldData, kEmptyStr);
                }
            } else if (NStr::EqualNocase(label, kRGTAFrom)) {
                if (it->GetData().IsInt()) {
                    from = it->GetData().GetInt();
                } else {
                    NCBI_THROW(CRefGeneTrackingException, eBadUserFieldData, kEmptyStr);
                }
            } else if (NStr::EqualNocase(label, kRGTATo)) {
                if (it->GetData().IsInt()) {
                    to = it->GetData().GetInt();
                } else {
                    NCBI_THROW(CRefGeneTrackingException, eBadUserFieldData, kEmptyStr);
                }
            } else {
                NCBI_THROW(CRefGeneTrackingException, eBadUserFieldName, "Unrecognized field name " + label);
            }
        } else {
            NCBI_THROW(CRefGeneTrackingException, eUserFieldWithoutLabel, kEmptyStr);
        }
    }
    CRef<CRefGeneTrackingAccession> rval(new CRefGeneTrackingAccession(accession, gi, from, to, comment, acc_name));
    if (rval->IsEmpty()) {
        return CRef<CRefGeneTrackingAccession>(NULL);
    } else {
        return rval;
    }
}


const string kRefGeneTrackingIdenticalTo = "IdenticalTo";

void CUser_object::SetRefGeneTrackingIdenticalTo(const CRefGeneTrackingAccession& accession)
{
    CUser_field& field = SetField(kRefGeneTrackingIdenticalTo);
    field.ResetData();
    CRef<CUser_field> ident = accession.MakeAccessionField();
    if (ident) {
        field.SetData().SetFields().push_back(ident);
    }
    SetObjectType(eObjectType_RefGeneTracking);
}


CConstRef<CUser_object::CRefGeneTrackingAccession> CUser_object::GetRefGeneTrackingIdenticalTo() const
{
    if (GetObjectType() != eObjectType_RefGeneTracking) {
        return CConstRef<CUser_object::CRefGeneTrackingAccession>(NULL);
    }
    CConstRef<CUser_field> field = GetFieldRef(kRefGeneTrackingIdenticalTo);
    if (field && field->IsSetData() && field->GetData().IsFields() && !field->GetData().GetFields().empty()) {
        return CRefGeneTrackingAccession::MakeAccessionFromUserField(*(field->GetData().GetFields().front()));
    }
    return CConstRef<CUser_object::CRefGeneTrackingAccession>(NULL);
}


void CUser_object::ResetRefGeneTrackingIdenticalTo()
{
    RemoveNamedField(kRefGeneTrackingIdenticalTo);
}

const string kRefGeneTrackingAssembly = "Assembly";

void CUser_object::SetRefGeneTrackingAssembly(const TRefGeneTrackingAccessions& acc_list)
{
    CUser_field& field = SetField(kRefGeneTrackingAssembly);
    field.ResetData();
    // use auto iterator syntax
    for (auto it : acc_list) {
        CRef<CUser_field> acc = it->MakeAccessionField();
        if (acc) {
            field.SetData().SetFields().push_back(acc);
        }
    }
    SetObjectType(eObjectType_RefGeneTracking);
}


CUser_object::TRefGeneTrackingAccessions CUser_object::GetRefGeneTrackingAssembly() const
{
    TRefGeneTrackingAccessions rval;

    if (GetObjectType() != eObjectType_RefGeneTracking) {
        return rval;
    }
    CConstRef<CUser_field> field = GetFieldRef(kRefGeneTrackingAssembly);
    if (field && field->IsSetData() && field->GetData().IsFields()) {
        rval.reserve(field->GetData().GetFields().size());
        for (auto it : field->GetData().GetFields()) {
            CConstRef<CRefGeneTrackingAccession> acc = CRefGeneTrackingAccession::MakeAccessionFromUserField(*it);
            if (acc) {
                rval.push_back(acc);
            }
        }
    }
    return rval;
}


void CUser_object::ResetRefGeneTrackingAssembly()
{
    RemoveNamedField(kRefGeneTrackingAssembly);
}




END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
