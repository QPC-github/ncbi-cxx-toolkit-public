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
* Author:  Aaron Ucko, Mati Shomrat, Colleen Bollin, NCBI
*
* File Description:
*   runs ExtendedCleanup on ASN.1 files
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <serial/objectio.hpp>


#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#ifdef HAVE_NCBI_VDB
#  include <sra/data_loaders/wgs/wgsloader.hpp>
#endif
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/autodef_with_tax.hpp>
#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/validator/dup_feats.hpp>

#include <objtools/readers/format_guess_ex.hpp>

#include "read_hooks.hpp"
#include "bigfile_processing.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const CDataLoadersUtil::TLoaders default_loaders =
  CDataLoadersUtil::fGenbank | CDataLoadersUtil::fGenbankOffByDefault | CDataLoadersUtil::fVDB;

enum EProcessingMode
{
    eModeRegular,
    eModeBatch,
    eModeBigfile
};

enum class EAsnType
{
    eAny,
    eSeqEntry,
    eBioseq,
    eBioseqSet,
    eSeqSubmit,
};

class CCleanupApp : public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler, public ISubmitBlockHandler, IProcessorCallback
{
public:
    CCleanupApp();
    void Init() override;
    int Run() override;

    bool HandleSubmitBlock(CSubmit_block& block) override;
    bool HandleSeqEntry(CRef<CSeq_entry>& se) override;
    bool HandleSeqEntry(CSeq_entry_Handle entry);
    bool HandleSeqID( const string& seqID );

    // IProcessorCallback interface functionality
    void Process(CRef<CSerialObject>& obj) override;

    bool ObtainSeqEntryFromSeqEntry(
        unique_ptr<CObjectIStream>& is,
        CRef<CSeq_entry>& se);
    bool ObtainSeqEntryFromBioseq(
        unique_ptr<CObjectIStream>& is,
        CRef<CSeq_entry>& se);
    bool ObtainSeqEntryFromBioseqSet(
        unique_ptr<CObjectIStream>& is,
        CRef<CSeq_entry>& se);

private:
    // types

    CObjectIStream* x_OpenIStream(const CArgs& args, const string& filename, ESerialDataFormat&, EAsnType&);
    void x_OpenOStream(const string& filename, const string& dir = kEmptyStr, bool remove_orig_dir = true);
    void x_CloseOStream();
    bool x_ProcessSeqSubmit(unique_ptr<CObjectIStream>& is);
    bool x_ProcessBigFile(unique_ptr<CObjectIStream>& is, EAsnType asn_type);
    void x_ProcessOneFile(unique_ptr<CObjectIStream>& is, EProcessingMode mode, EAsnType asn_type, bool first_only);
    void x_ProcessOneFile(const string& filename);
    void x_ProcessOneDirectory(const string& dirname, const string& suffix);

    void x_FeatureOptionsValid(const string& opt);
    void x_KOptionsValid(const string& opt);
    void x_XOptionsValid(const string& opt);
    bool x_ProcessFeatureOptions(const string& opt, CSeq_entry_Handle seh);
    bool x_RemoveDuplicateFeatures(CSeq_entry_Handle seh);
    bool x_ProcessXOptions(const string& opt, CSeq_entry_Handle seh, Uint4 options);
    bool x_GFF3Batch(CSeq_entry_Handle seh);
    enum EFixCDSOptions {
        eFixCDS_FrameFromLoc = 0x1,
        eFixCDS_Retranslate = 0x2,
        eFixCDS_ExtendToStop = 0x4
    };
    const Uint4 kGFF3CDSFixOptions = eFixCDS_FrameFromLoc | eFixCDS_Retranslate | eFixCDS_ExtendToStop;

    bool x_FixCDS(CSeq_entry_Handle seh, Uint4 options, const string& missing_prot_name);
    bool x_BatchExtendCDS(CSeq_feat&, CBioseq_Handle);
    bool x_BasicAndExtended(CSeq_entry_Handle entry, const string& label, bool do_basic = true, bool do_extended = false, Uint4 options = 0);

    template<typename T> void x_WriteToFile(const T& s);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
    unique_ptr<CObjectOStream>  m_Out;          // output
};


CCleanupApp::CCleanupApp()
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}

void CCleanupApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Perform ExtendedCleanup on an ASN.1 Seq-entry into a flat report");

    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile",
            "Input file name", CArgDescriptions::eInputFile);

        // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddOptionalKey("serial", "SerialFormat", "Obsolete; Input file format is now autodetected",
            CArgDescriptions::eString, CArgDescriptions::fHidden);

        // output file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddOptionalKey("outformat", "OutputSerialFormat", "Output file format",
            CArgDescriptions::eString);
        arg_desc->SetConstraint("outformat", &(*new CArgAllow_Strings,
            "text", "binary", "XML", "JSON"));

        // id
        arg_desc->AddOptionalKey("id", "ID",
            "Specific ID to display", CArgDescriptions::eString);

        // input type:
        arg_desc->AddOptionalKey("type", "AsnType", "Obsolete; ASN.1 object type is now autodetected",
            CArgDescriptions::eString, CArgDescriptions::fHidden);

        // path
        arg_desc->AddOptionalKey("indir", "path", "Path to files", CArgDescriptions::eDirectory);

        // suffix
        arg_desc->AddDefaultKey("x", "suffix", "File Selection Suffix", CArgDescriptions::eString, ".ent");

        // results
        arg_desc->AddOptionalKey("outdir", "results", "Path for Results", CArgDescriptions::eDirectory);
    }}

    // batch processing
    {{
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
        arg_desc->AddFlag("c", "Obsolete - do not use",
                          CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);

        // imitate limitation of C Toolkit version
        arg_desc->AddFlag("firstonly", "Process only first element");
    }}

    // big file processing
    {{
        arg_desc->AddFlag("bigfile", "Process big files containing many bioseqs");
    }}

    // output
    {{
        // name
        arg_desc->AddOptionalKey("o", "OutputFile",
            "Output file name", CArgDescriptions::eOutputFile);
    }}

    // report
    {{
        // html
        arg_desc->AddFlag("html", "Produce HTML output");
    }}

    // normal cleanup options (will replace -nocleanup and -basic)
    {{
        arg_desc->AddOptionalKey("K", "Cleanup", "Systemic Cleaning Options\n"
                                 "\tb Basic\n"
                                 "\ts Extended\n"
                                 "\tn Normalize Descriptor Order\n"
                                 "\tu Remove Cleanup User-object\n",
                                 CArgDescriptions::eString);
    }}

    // extra cleanup options
    {{
        arg_desc->AddOptionalKey("F", "Feature", "Feature Cleaning Options\n"
                                 "\tr Remove Redundant Gene xref\n"
                                 "\ta Adjust for Missing Stop Codon\n"
                                 "\tp Clear internal partials\n"
                                 "\tz Delete or Update EC Numbers\n"
                                 "\td Remove duplicate features\n",
                                  CArgDescriptions::eString);

        arg_desc->AddOptionalKey("X", "Miscellaneous", "Other Cleaning Options\n"
            "\td Automatic Definition Line\n"
            "\tw GFF/WGS Genome Cleanup\n"
            "\tr Regenerate Definition Lines\n"
            "\tb Batch Cleanup of Multireader Output\n"
            "\ta Remove Assembly Gaps\n"
            "\ti Make Influenza Small Genome Sets\n"
            "\tf Make IRD misc_feats\n",
            CArgDescriptions::eString);

        arg_desc->AddFlag("T", "TaxonomyLookup");
    }}

    // misc
    {{
        // no-cleanup
        arg_desc->AddFlag("nocleanup",
            "Do not perform extended data cleanup prior to formatting");
        arg_desc->AddFlag("basic",
            "Perform basic data cleanup prior to formatting");
        arg_desc->AddFlag("noobj",
            "Do not create Ncbi_cleanup object");

        // show progress
        arg_desc->AddFlag("showprogress",
            "List ID for which cleanup is occuring");
        arg_desc->AddFlag("debug", "Save before.sqn");
    }}

    // remote
    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, default_loaders);

    SetupArgDescriptions(arg_desc.release());
}


void CCleanupApp::x_FeatureOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized;
    for (char c : opt) {
        if (!isspace(c)) {
            if (c != 'r' && c != 'a' && c != 'p' && c != 'z' && c != 'd') {
                unrecognized += c;
            }
        }
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CFlatException, eInternal, "Invalid -F arguments:" + unrecognized);
    }
}


void CCleanupApp::x_KOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized;
    for (char c : opt) {
        if (!isspace(c)) {
            if (c != 'b' && c != 's' && c != 'u' && c != 'n') {
                unrecognized += c;
            }
        }
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CFlatException, eInternal, "Invalid -K arguments:" + unrecognized);
    }
}


void CCleanupApp::x_XOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized;
    for (char c : opt) {
        if (!isspace(c)) {
            if (c != 'w' && c != 'r' && c != 'b' && c != 'a' &&
                c != 'i' && c != 'f' && c != 'd') {
                unrecognized += c;
            }
        }
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CFlatException, eInternal, "Invalid -X arguments:" + unrecognized);
    }
}


static CObjectOStream* CreateTmpOStream(const std::string& outFileName, const std::string& tmpFileName)
{
    if (outFileName.empty()) // cout
        return CObjectOStream::Open(eSerial_AsnText, std::cout);

    return CObjectOStream::Open(eSerial_AsnText, tmpFileName, eSerial_StdWhenAny);
}


static unique_ptr<CObjectTypeInfo> GetEntryTypeInfo()
{
    // 'data' member of CSeq_submit ...
    CObjectTypeInfo submitTypeInfo = CType<CSeq_submit>();
    CObjectTypeInfoMI data = submitTypeInfo.FindMember("data");

    // points to a container (pointee may has different types) ...
    const CPointerTypeInfo* dataPointerType = CTypeConverter<CPointerTypeInfo>::SafeCast(data.GetMemberType().GetTypeInfo());
    const CChoiceTypeInfo* dataChoiceType = CTypeConverter<CChoiceTypeInfo>::SafeCast(dataPointerType->GetPointedType());

    // that is a list of pointers to 'CSeq_entry' (we process only that case)
    const CItemInfo* entries = dataChoiceType->GetItemInfo("entrys");

    return unique_ptr<CObjectTypeInfo>(new CObjectTypeInfo(entries->GetTypeInfo()));
}


static CObjectTypeInfoMI GetSubmitBlockTypeInfo()
{
    CObjectTypeInfo submitTypeInfo = CType<CSeq_submit>();
    return submitTypeInfo.FindMember("sub");
}


static void CompleteOutputFile(CObjectOStream& out)
{
    out.EndContainer(); // ends the list of entries

    out.EndChoiceVariant(); // ends 'entrys'
    out.PopFrame();
    out.EndChoice(); // ends 'data'
    out.PopFrame();

    out.EndClass(); // ends 'CSeq_submit'

    Separator(out);

    fflush(NULL);
}


static std::string GetOutputFileName(const CArgs& args)
{
    std::string ret = args["o"] ? args["o"].AsString() : kEmptyStr;
    return ret;
}

// returns false if fails to read object of expected type, throws for all other errors
bool CCleanupApp::x_ProcessSeqSubmit(unique_ptr<CObjectIStream>& is)
{
    CRef<CSeq_submit> sub(new CSeq_submit);
    if (sub.Empty()) {
        NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Seq-submit object");
    }
    try {

        //CTmpFile tmpFile;
        //unique_ptr<CObjectOStream> out(CreateTmpOStream(outFileName, tmpFile.GetFileName()));

        CObjectTypeInfoMI submitBlockObj = GetSubmitBlockTypeInfo();
        submitBlockObj.SetLocalReadHook(*is, new CReadSubmitBlockHook(*this, *m_Out));

        unique_ptr<CObjectTypeInfo> entryObj = GetEntryTypeInfo();
        entryObj->SetLocalReadHook(*is, new CReadEntryHook(*this, *m_Out));

        *is >> *sub;

        entryObj->ResetLocalReadHook(*is);
        submitBlockObj.ResetLocalReadHook(*is);

        CompleteOutputFile(*m_Out);
    }
    catch (const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch (...) {
        return false;
    }

    if (!sub->IsSetSub() || !sub->IsSetData()) {
        NCBI_THROW(CFlatException, eInternal, "No data in Seq-submit");
    }
    else if (!sub->GetData().IsEntrys()) {
        NCBI_THROW(CFlatException, eInternal, "Wrong data in Seq-submit");
    }

    return true;
}

bool CCleanupApp::x_ProcessBigFile(unique_ptr<CObjectIStream>& is, EAsnType asn_type)
{
    _ASSERT(asn_type != EAsnType::eBioseq && asn_type != EAsnType::eAny);

    bool ret = false;
    EBigFileContentType content_type = eContentUndefined;
    if (asn_type == EAsnType::eSeqEntry) {
        content_type = eContentSeqEntry;
    }
    else if (asn_type == EAsnType::eBioseqSet) {
        content_type = eContentBioseqSet;
    }
    else if (asn_type == EAsnType::eSeqSubmit) {
        content_type = eContentSeqSubmit;
    }

    if (content_type != eContentUndefined) {
        ret = ProcessBigFile(*is, *m_Out, *this, content_type);
    }

    return ret;
}

void CCleanupApp::x_ProcessOneFile(unique_ptr<CObjectIStream>& is, EProcessingMode mode, EAsnType asn_type, bool first_only)
{
    if (mode == eModeBatch) {
        CGBReleaseFile in(*is.release());
        in.RegisterHandler(this);
        in.Read();  // HandleSeqEntry will be called from this function
    }
    else if (mode == eModeBigfile) {
        x_ProcessBigFile(is, asn_type);
    }
    else {
        bool proceed = true;
        size_t num_cleaned = 0;

        while (proceed) {

            // clean exit
            if (is->EndOfData()) {
                is->SetFailFlags(CObjectIStream::eEOF);
                break;
            }

            CRef<CSeq_entry> se(new CSeq_entry);

            if (asn_type == EAsnType::eSeqEntry) {
                //
                //  Straight through processing: Read a seq_entry, then process
                //  a seq_entry:
                //
                proceed = ObtainSeqEntryFromSeqEntry(is, se);
                if (proceed) {
                    HandleSeqEntry(se);
                    x_WriteToFile(*se);
                }
            } else if (asn_type == EAsnType::eBioseq) {
                //
                //  Read object as a bioseq, wrap it into a seq_entry, then process
                //  the wrapped bioseq as a seq_entry:
                //
                proceed = ObtainSeqEntryFromBioseq(is, se);
                if (proceed) {
                    HandleSeqEntry(se);
                    x_WriteToFile(se->GetSeq());
                }
            } else if (asn_type == EAsnType::eBioseqSet) {
                //
                //  Read object as a bioseq_set, wrap it into a seq_entry, then
                //  process the wrapped bioseq_set as a seq_entry:
                //
                proceed = ObtainSeqEntryFromBioseqSet(is, se);
                if (proceed) {
                    HandleSeqEntry(se);
                    x_WriteToFile(se->GetSet());
                }
            } else if (asn_type == EAsnType::eSeqSubmit) {
                proceed = x_ProcessSeqSubmit(is);
            } else {
                proceed = false;
            }

            if (proceed) {
                ++num_cleaned;
            }

            if (first_only) {
                break;
            }
        }

        if (num_cleaned == 0 || (!first_only && (is->GetFailFlags() & CObjectIStream::fEOF) != CObjectIStream::fEOF)) {
            NCBI_THROW(CFlatException, eInternal, "Unable to construct Seq-entry object");
        }
    }
}

void CCleanupApp::x_ProcessOneFile(const string& filename)
{
    const CArgs& args = GetArgs();
    _ASSERT(!NStr::IsBlank(filename));

    ESerialDataFormat serial = eSerial_None;
    EAsnType asn_type = EAsnType::eAny;

    if (args["type"]) {
        cerr << "Warning: -type argument should not be used; ASN.1 object type is now autodetected." << endl;
    }

    // open file
    unique_ptr<CObjectIStream> is(x_OpenIStream(args, filename, serial, asn_type));
    if (!is) {
        string msg = "Unable to open input file " + filename;
        NCBI_THROW(CFlatException, eInternal, msg);
    }

    // need to set output if -o not specified
    bool opened_output = false;

    if (!args["o"] && args["outdir"]) {
        x_OpenOStream(filename, args["outdir"].AsString());
        opened_output = true;
    }

    EProcessingMode mode = eModeRegular;
    if (args["batch"]) {
        mode = eModeBatch;
    }
    else if (args["bigfile"]) {
        if (asn_type != EAsnType::eAny && asn_type != EAsnType::eBioseq) {
            mode = eModeBigfile;
        }
    }

    x_ProcessOneFile(is, mode, asn_type, args["firstonly"]);

    is.reset();
    if (opened_output) {
        // close output file if we opened one
        x_CloseOStream();
    }
}


void CCleanupApp::x_ProcessOneDirectory(const string& dirname, const string& suffix)
{
    CDir dir(dirname);

    string mask = "*" + suffix;
    size_t num_files = 0;

    CDir::TEntries files(dir.GetEntries(mask, CDir::eFile));
    for (CDir::TEntry ii : files) {
        if (ii->IsFile()) {
            string fname = CDirEntry::MakePath(dirname, ii->GetName());
            x_ProcessOneFile(fname);
            num_files++;
        }
    }
    if (num_files == 0) {
        NCBI_THROW(CFlatException, eInternal, "No files found!");
    }
}


int CCleanupApp::Run()
{
    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();

    // flag validation
    if (args["F"]) {
        x_FeatureOptionsValid(args["F"].AsString());
    }
    if (args["K"]) {
        x_KOptionsValid(args["K"].AsString());
    }
    if (args["X"]) {
        x_XOptionsValid(args["X"].AsString());
    }
    if (args["batch"] && args["bigfile"]) {
        NCBI_THROW(CFlatException, eInternal, "\"batch\" and \"bigfile\" arguments are incompatible. Only one of them may be used.");
    }
    if (args["X"] && args["bigfile"]) {
        NCBI_THROW(CFlatException, eInternal, "\"X\" and \"bigfile\" arguments are incompatible. Only one of them may be used.");
    }

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }

    CDataLoadersUtil::SetupObjectManager(args, *m_Objmgr, default_loaders);
    m_Scope.Reset(new CScope(*m_Objmgr));
    m_Scope->AddDefaults();

    // need to set output (-o) if specified, if not -o and not -outdir need to use standard output
    bool opened_output = false;
    if (args["o"]) {
        string abs_output_path = CDirEntry::CreateAbsolutePath(args["o"].AsString());
        if (args["i"]) {
            string fname = args["i"].AsString();
            if (args["indir"]) {
                fname = CDirEntry::MakePath(args["indir"].AsString(), fname);
            }
            if (abs_output_path == CDirEntry::CreateAbsolutePath(fname)) {
                ERR_POST("Input and output files should be different");
                return 1;
            }
        }
        x_OpenOStream(args["o"].AsString(),
                      args["outdir"] ? args["outdir"].AsString() : kEmptyStr,
                      false);
        opened_output = true;
    } else if (!args["outdir"] || args["id"]) {
        x_OpenOStream(kEmptyStr);
        opened_output = true;
    }

    if (args["id"]) {
        string seqID = args["id"].AsString();
        HandleSeqID(seqID);
    } else if (args["i"]) {
        string fname = args["i"].AsString();
        if (args["indir"]) {
            fname = CDirEntry::MakePath(args["indir"].AsString(), fname);
        }
        x_ProcessOneFile(fname);
    } else if (args["outdir"]) {
        x_ProcessOneDirectory(args["indir"].AsString(), args["x"].AsString());
    } else {
        cerr << "Error: stdin is no longer supported; please use -i" << endl;
    }

    if (opened_output) {
        // close output file if we opened one
        x_CloseOStream();
    }
    return 0;
}

bool CCleanupApp::ObtainSeqEntryFromSeqEntry(
    unique_ptr<CObjectIStream>& is,
    CRef<CSeq_entry>& se )
{
    try {
        *is >> *se;
        if (se->Which() == CSeq_entry::e_not_set) {
            return false;
        }
        return true;
    }
    catch(const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::ObtainSeqEntryFromBioseq(
    unique_ptr<CObjectIStream>& is,
    CRef<CSeq_entry>& se )
{
    try {
        CRef<CBioseq> bs( new CBioseq );
        if ( ! bs ) {
            NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Bioseq object");
        }
        *is >> *bs;

        se->SetSeq( bs.GetObject() );
        return true;
    }
    catch (const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::ObtainSeqEntryFromBioseqSet(
    unique_ptr<CObjectIStream>& is,
    CRef<CSeq_entry>& se )
{
    try {
        CRef<CBioseq_set> bss( new CBioseq_set );
        if ( ! bss ) {
            NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Bioseq object");
        }
        *is >> *bss;

        se->SetSet( bss.GetObject() );
        return true;
    }
    catch (const CEofException&) {
        is->SetFailFlags(CObjectIStream::eEOF);
        return false;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::HandleSeqID( const string& seq_id )
{
    CRef<CScope> scope(new CScope(*m_Objmgr));
    scope->AddDefaults();

    CBioseq_Handle bsh;
    try {
        CSeq_id SeqId(seq_id);
        bsh = scope->GetBioseqHandle(SeqId);
    }
    catch ( CException& ) {
        ERR_FATAL("The ID " << seq_id << " is not a valid seq ID." );
    }

    if (!bsh) {
        ERR_FATAL("Sequence for " << seq_id << " cannot be retrieved.");
        return false;
    }

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->Assign(*(bsh.GetSeq_entry_Handle().GetCompleteSeq_entry()));
    HandleSeqEntry(entry);
    x_WriteToFile(*entry);

    return true;
}

bool CCleanupApp::x_ProcessFeatureOptions(const string& opt, CSeq_entry_Handle seh)
{
    if (NStr::IsBlank(opt)) {
        return false;
    }
    bool any_changes = false;
    if (NStr::Find(opt, "r") != string::npos) {
        any_changes |= CCleanup::RemoveUnnecessaryGeneXrefs(seh);
    }
    if (NStr::Find(opt, "a") != string::npos) {
        any_changes |= x_FixCDS(seh, eFixCDS_ExtendToStop, kEmptyStr);
    }
    if (NStr::Find(opt, "p") != string::npos) {
        any_changes |= CCleanup::ClearInternalPartials(seh);
    }
    if (NStr::Find(opt, "z") != string::npos) {
        any_changes |= CCleanup::FixECNumbers(seh);
    }
    if (NStr::Find(opt, "d") != string::npos) {
        any_changes |= x_RemoveDuplicateFeatures(seh);
    }
    return any_changes;
}

bool CCleanupApp::x_RemoveDuplicateFeatures(CSeq_entry_Handle seh)
{
    bool any_change = false;
    set< CSeq_feat_Handle > deleted_feats = validator::GetDuplicateFeaturesForRemoval(seh);
    if (deleted_feats.empty()) {
        return false;
    }
    set< CBioseq_Handle > orphans = validator::ListOrphanProteins(seh);
    for (auto df : deleted_feats) {
        CSeq_feat_EditHandle eh(df);
        eh.Remove();
        any_change = true;
    }
    for (auto orph : orphans) {
        CBioseq_EditHandle eh(orph);
        eh.Remove();
        any_change = true;
    }
    any_change |= CCleanup::RenormalizeNucProtSets(seh);
    return any_change;

}

bool CCleanupApp::x_ProcessXOptions(const string& opt, CSeq_entry_Handle seh, Uint4 options)
{
    bool any_changes = false;
    if (NStr::Find(opt, "w") != string::npos) {
        any_changes = CCleanup::WGSCleanup(seh, true, options);
    }
    if (NStr::Find(opt, "r") != string::npos) {
        bool change_defline = CAutoDefWithTaxonomy::RegenerateDefLines(seh);
        if (change_defline) {
            any_changes = true;
            CCleanup::NormalizeDescriptorOrder(seh);
        }
    }
    if (NStr::Find(opt, "b") != string::npos) {
        any_changes |= x_GFF3Batch(seh);
    }
    if (NStr::Find(opt, "a") != string::npos) {
        any_changes |= CCleanup::ConvertDeltaSeqToRaw(seh);
    }
    if (NStr::Find(opt, "i") != string::npos) {
        if (CCleanup::MakeSmallGenomeSet(seh) > 0) {
            any_changes = true;
        }
    }
    if (NStr::Find(opt, "f") != string::npos) {
        if (CCleanup::MakeIRDFeatsFromSourceXrefs(seh)) {
            any_changes = true;
        }
    }
    if (NStr::Find(opt, "d") != string::npos) {
        CCleanup::AutodefId(seh);
        any_changes = true;
    }
    return any_changes;
}


bool CCleanupApp::x_BatchExtendCDS(CSeq_feat& sf, CBioseq_Handle b)
{
    if (!sf.GetData().IsCdregion()) {
        // not coding region
        return false;
    }
    if (sequence::IsPseudo(sf, b.GetScope())) {
        return false;
    }

    // check for existing stop codon
    string translation;
    try {
        CSeqTranslator::Translate(sf, b.GetScope(), translation, true);
    } catch (CSeqMapException& e) {
        cout << e.what() << endl;
        return false;
    } catch (CSeqVectorException& e) {
        cout << e.what() << endl;
        return false;
    }
    if (NStr::EndsWith(translation, "*")) {
        //already has stop codon
        return false;
    }

    if (CCleanup::ExtendToStopCodon(sf, b, 50)) {
        feature::RetranslateCDS(sf, b.GetScope());
        return true;
    } else {
        return false;
    }
}


bool CCleanupApp::x_FixCDS(CSeq_entry_Handle seh, Uint4 options, const string& missing_prot_name)
{
    bool any_changes = false;
    for (CBioseq_CI bi(seh, CSeq_inst::eMol_na); bi; ++bi) {
        any_changes |= CCleanup::SetGeneticCodes(*bi);
        for (CFeat_CI fi(*bi, CSeqFeatData::eSubtype_cdregion); fi; ++fi) {
            CConstRef<CSeq_feat> orig = fi->GetSeq_feat();
            CRef<CSeq_feat> sf(new CSeq_feat());
            sf->Assign(*orig);
            bool feat_change = false;
            if ((options & eFixCDS_FrameFromLoc) &&
                CCleanup::SetFrameFromLoc(sf->SetData().SetCdregion(), sf->GetLocation(), bi.GetScope())) {
                feat_change = true;
            }
            if ((options & eFixCDS_Retranslate)) {
                feat_change |= feature::RetranslateCDS(*sf, bi.GetScope());
            }
            if ((options & eFixCDS_ExtendToStop) &&
                x_BatchExtendCDS(*sf, *bi)) {
                CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(*orig, seh.GetScope());
                if (mrna && CCleanup::LocationMayBeExtendedToMatch(mrna->GetLocation(), sf->GetLocation())) {
                    CRef<CSeq_feat> new_mrna(new CSeq_feat());
                    new_mrna->Assign(*mrna);
                    if (CCleanup::ExtendStopPosition(*new_mrna, sf)) {
                        CSeq_feat_EditHandle efh(seh.GetScope().GetSeq_featHandle(*mrna));
                        efh.Replace(*new_mrna);
                    }
                }
                CConstRef<CSeq_feat> gene = sequence::GetGeneForFeature(*orig, seh.GetScope());
                if (gene && CCleanup::LocationMayBeExtendedToMatch(gene->GetLocation(), sf->GetLocation())) {
                    CRef<CSeq_feat> new_gene(new CSeq_feat());
                    new_gene->Assign(*gene);
                    if (CCleanup::ExtendStopPosition(*new_gene, sf)) {
                        CSeq_feat_EditHandle efh(seh.GetScope().GetSeq_featHandle(*gene));
                        efh.Replace(*new_gene);
                    }
                }

                feat_change = true;
            }
            if (feat_change) {
                CSeq_feat_EditHandle ofh = CSeq_feat_EditHandle(seh.GetScope().GetSeq_featHandle(*orig));
                ofh.Replace(*sf);
                any_changes = true;
            }
            //also set protein name if missing, change takes place on protein bioseq
            if (!NStr::IsBlank(missing_prot_name)) {
                string current_name = CCleanup::GetProteinName(*sf, seh);
                if (NStr::IsBlank(current_name)) {
                    CCleanup::SetProteinName(*sf, missing_prot_name, false, seh.GetScope());
                    any_changes = true;
                }
            }
        }
    }
    return any_changes;
}


bool CCleanupApp::x_GFF3Batch(CSeq_entry_Handle seh)
{
    bool any_changes = x_FixCDS(seh, kGFF3CDSFixOptions, kEmptyStr);
    CCleanup cleanup;
    cleanup.SetScope(&(seh.GetScope()));
    Uint4 options = CCleanup::eClean_NoNcbiUserObjects;
    CConstRef<CCleanupChange> changes = cleanup.BasicCleanup(seh, options);
    any_changes |= (!changes.Empty());
    changes = cleanup.ExtendedCleanup(seh, options);
    any_changes |= (!changes.Empty());
    any_changes |= x_FixCDS(seh, 0, "unnamed protein product");

    return any_changes;
}


bool CCleanupApp::x_BasicAndExtended(CSeq_entry_Handle entry, const string& label,
                                     bool do_basic, bool do_extended, Uint4 options)
{
    if (!do_basic && !do_extended) {
        return false;
    }

    bool any_changes = false;
    CCleanup cleanup;
    cleanup.SetScope(&(entry.GetScope()));

    if (do_basic) {
        // perform BasicCleanup
        try {
            CConstRef<CCleanupChange> changes = cleanup.BasicCleanup(entry, options);
            vector<string> changes_str = changes->GetAllDescriptions();
            if (changes_str.empty()) {
                LOG_POST(Error << "No changes from BasicCleanup\n");
            }
            else {
                LOG_POST(Error << "Changes from BasicCleanup:\n");
                for (const string& it : changes_str) {
                    LOG_POST(Error << it);
                }
                any_changes = true;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error in basic cleanup: " << e.GetMsg() << label);
        }
    }

    if (do_extended) {
        // perform ExtendedCleanup
        try {
            CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup(entry, options);
            vector<string> changes_str = changes->GetAllDescriptions();
            if (changes_str.empty()) {
                LOG_POST(Error << "No changes from ExtendedCleanup\n");
            }
            else {
                LOG_POST(Error << "Changes from ExtendedCleanup:\n");
                for (const string& it : changes_str) {
                    LOG_POST(Error << it);
                }
                any_changes = true;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error in extended cleanup: " << e.GetMsg() << label);
        }
    }
    return any_changes;
}


bool CCleanupApp::HandleSubmitBlock(CSubmit_block& block)
{
    CCleanup cleanup;
    bool any_changes = false;
    try {
        CConstRef<CCleanupChange> changes = cleanup.BasicCleanup(block);
        vector<string> changes_str = changes->GetAllDescriptions();
        if (changes_str.empty()) {
            LOG_POST(Error << "No changes from BasicCleanup of SubmitBlock\n");
        } else {
            LOG_POST(Error << "Changes from BasicCleanup of SubmitBlock:\n");
            for (const string& it : changes_str) {
                LOG_POST(Error << it);
            }
            any_changes = true;
        }
    } catch (CException& e) {
        LOG_POST(Error << "error in cleanup of SubmitBlock: " << e.GetMsg());
    }
    return any_changes;
}


bool CCleanupApp::HandleSeqEntry(CSeq_entry_Handle entry)
{
    string label;
    entry.GetCompleteSeq_entry()->GetLabel(&label, CSeq_entry::eBoth);

    const CArgs& args = GetArgs();

    if (args["showprogress"]) {
        LOG_POST(Error << label + "\n");
    }

    ESerialDataFormat outFormat = eSerial_AsnText;

    if (args["debug"]) {
        unique_ptr<CObjectOStream> debug_out(CObjectOStream::Open(outFormat, "before.sqn",
            eSerial_StdWhenAny));

        *debug_out << *(entry.GetCompleteSeq_entry());
    }

    bool any_changes = false;

    if (args["T"]) {
        validator::CTaxValidationAndCleanup tval;
        any_changes |= tval.DoTaxonomyUpdate(entry, true);
    }

    if (args["K"] && NStr::Find(args["K"].AsString(), "u") != string::npos) {
        CRef<CSeq_entry> se(const_cast<CSeq_entry *>(entry.GetCompleteSeq_entry().GetPointer()));
        any_changes |= CCleanup::RemoveNcbiCleanupObject(*se);
    }

    bool do_basic = false;
    bool do_extended = false;
    if (args["K"]) {
        if (NStr::Find(args["K"].AsString(), "b") != string::npos) {
            do_basic = true;
        }
        if (NStr::Find(args["K"].AsString(), "s") != string::npos) {
            do_basic = true;
            do_extended = true;
        }
    }
    else if (args["X"]) {
        do_basic = true;
        if (NStr::Find(args["X"].AsString(), "w") != string::npos) {
            //Extended Cleanup is part of -X w
            do_extended = false;
        }
    } else if (args["F"]) {
        do_basic = true;
    } else {
        if (args["basic"]) {
            do_basic = true;
        }
        if (!args["nocleanup"]) {
            do_extended = true;
        }
    }

    Uint4 options = 0;
    if (args["noobj"]) {
        options = CCleanup::eClean_NoNcbiUserObjects;
    }

    any_changes |= x_BasicAndExtended(entry, label, do_basic, do_extended, options);

    if (args["F"]) {
        any_changes |= x_ProcessFeatureOptions(args["F"].AsString(), entry);
    }
    if (args["X"]) {
        any_changes |= x_ProcessXOptions(args["X"].AsString(), entry, options);
    }
    if (args["K"] && NStr::Find(args["K"].AsString(), "n") != string::npos && !do_extended) {
        any_changes |= CCleanup::NormalizeDescriptorOrder(entry);
    }

    return true;
}

bool CCleanupApp::HandleSeqEntry(CRef<CSeq_entry>& se)
{
    if (!se) {
        return false;
    }

    CSeq_entry_Handle entry = m_Scope->AddTopLevelSeqEntry(*se);
    if ( !entry ) {
        NCBI_THROW(CFlatException, eInternal, "Failed to insert entry to scope.");
    }

    if (HandleSeqEntry(entry)) {
        if (entry.GetCompleteSeq_entry().GetPointer() != se.GetPointer()) {
            se->Assign(*entry.GetCompleteSeq_entry());
        }
        m_Scope->RemoveTopLevelSeqEntry(entry);
        return true;
    } else {
        m_Scope->RemoveTopLevelSeqEntry(entry);
        return false;
    }
}


template<typename T>void CCleanupApp::x_WriteToFile(const T& obj)
{
    *m_Out << obj;

    fflush(NULL);
}


CObjectIStream* CCleanupApp::x_OpenIStream(const CArgs& args, const string& filename, ESerialDataFormat& serial, EAsnType& asn_type)
{
    // determine the file serialization format.
    serial = eSerial_None;

    // default for batch files is binary, otherwise text.
    if (args["batch"]) {
        serial = eSerial_AsnBinary;
    }

    if (args["serial"]) {
        cerr << "Warning: -serial argument should not be used; Input file format is now autodetected." << endl;
    }

    // make sure of the underlying input stream. -i must be given on the command line
    // then the input comes from a file.
    unique_ptr<CNcbiIstream> pInputStream(make_unique<CNcbiIfstream>(filename, ios::binary));

    // autodetect
    if (pInputStream) {
        CFormatGuessEx fg(*pInputStream);
        auto& fh(fg.GetFormatHints());
        fh.AddPreferredFormat(CFormatGuess::eTextASN);
        fh.AddPreferredFormat(CFormatGuess::eBinaryASN);
        fh.AddPreferredFormat(CFormatGuess::eXml);
        fh.AddPreferredFormat(CFormatGuess::eZip);
        fh.AddPreferredFormat(CFormatGuess::eGZip);
        fh.DisableAllNonpreferred();

        CFormatGuess::EFormat inFormat;
        CFileContentInfo contentInfo;
        inFormat = fg.GuessFormatAndContent(contentInfo);

        if (serial == eSerial_None) {
            switch (inFormat) {
            default:
            case CFormatGuess::eTextASN:
                serial = eSerial_AsnText;
                break;
            case CFormatGuess::eBinaryASN:
                serial = eSerial_AsnBinary;
                break;
            case CFormatGuess::eXml:
                serial = eSerial_Xml;
                break;
            case CFormatGuess::eZip:
            case CFormatGuess::eGZip:
                cerr << "Error: Compressed files no longer supported." << endl;
                return nullptr;
            }
        }

        const string& genbankType = contentInfo.mInfoGenbank.mObjectType;
        if (genbankType == "Seq-entry") {
            asn_type = EAsnType::eSeqEntry;
        } else if (genbankType == "Bioseq") {
            asn_type = EAsnType::eBioseq;
        } else if (genbankType == "Bioseq-set") {
            asn_type = EAsnType::eBioseqSet;
        } else if (genbankType == "Seq-submit") {
            asn_type = EAsnType::eSeqSubmit;
        } else {
            asn_type = EAsnType::eSeqSubmit; // ?
        }
    }

    // if -c was specified then wrap the input stream into a gzip decompressor before
    // turning it into an object stream:
    CObjectIStream* pI = nullptr;
    if ( args["c"] ) {
        cerr << "Error: Compressed files no longer supported" << endl;
    }
    else {
        pI = CObjectIStream::Open( serial, *pInputStream.release(), eTakeOwnership );
    }

    if ( pI ) {
        pI->UseMemoryPool();
        pI->SetDelayBufferParsingPolicy(CObjectIStream::eDelayBufferPolicyAlwaysParse);
    }
    return pI;
}


void CCleanupApp::x_OpenOStream(const string& filename, const string& dir, bool remove_orig_dir)
{
    ESerialDataFormat outFormat = eSerial_AsnText;

    const CArgs& args = GetArgs();
    if (args["outformat"]) {
        if (args["outformat"].AsString() == "binary") {
            outFormat = eSerial_AsnBinary;
        }
        else if (args["outformat"].AsString() == "XML") {
            outFormat = eSerial_Xml;
        }
        else if (args["outformat"].AsString() == "JSON") {
            outFormat = eSerial_Json;
        }
    }

    if (NStr::IsBlank(filename)) {
        m_Out.reset(CObjectOStream::Open(outFormat, cout));
    } else if (!NStr::IsBlank(dir)) {
        string base = filename;
        if (remove_orig_dir) {
            const char buf[2] = { CDirEntry::GetPathSeparator(), 0 };
            size_t pos = NStr::Find(base, buf, NStr::eCase, NStr::eReverseSearch);
            if (pos != string::npos) {
                base = base.substr(pos + 1);
            }
        }
        string fname = CDirEntry::MakePath(dir, base);
        m_Out.reset(CObjectOStream::Open(outFormat, fname, eSerial_StdWhenAny));
    } else {
        m_Out.reset(CObjectOStream::Open(outFormat, filename, eSerial_StdWhenAny));
    }
}


void CCleanupApp::x_CloseOStream()
{
    m_Out->Close();
    m_Out.reset();
}

// IProcessorCallback interface functionality
void CCleanupApp::Process(CRef<CSerialObject>& obj)
{
    //static long long cnt;
    //cerr << ++cnt << ' ' << obj->GetThisTypeInfo()->GetName() << '\n';
    if (obj->GetThisTypeInfo() == CSeq_entry::GetTypeInfo()) {
        CRef<CSeq_entry> entry(dynamic_cast<CSeq_entry*>(obj.GetPointer()));
        HandleSeqEntry(entry);
    }
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    // scan and replace deprecated arguments; RW-1324
    for (int i = 1; i < argc; ++i)
    {
        string a = argv[i];
        if (a == "-r")
        {
            if ((i+1) < argc)
            {
                string param = argv[i+1];
                if (!param.empty() && param[0] != '-')
                {
                    argv[i] = "-outdir";
                    ++i; // skip parameter
                    cerr << "Warning: deprecated use of -r argument. Please use -outdir instead." << endl;
                }
            }
        }
        else if (a == "-p")
        {
            argv[i] = "-indir";
            cerr << "Warning: argument -p is deprecated. Please use -indir instead." << endl;
        }
        else if (a == "-R")
        {
            argv[i] = "-r";
            cerr << "Warning: argument -R is deprecated. Please use -r instead." << endl;
        }
        else if (a == "-gbload")
        {
            argv[i] = "-genbank";
            cerr << "Warning: argument -gbload is deprecated. Please use -genbank instead." << endl;
        }
    }

    return CCleanupApp().AppMain(argc, argv);
}
