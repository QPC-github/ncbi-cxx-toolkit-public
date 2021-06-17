#ifndef TABLE2ASN_MULTIREADER_HPP
#define TABLE2ASN_MULTIREADER_HPP

#include <util/format_guess.hpp>
#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE

namespace objects
{
class CScope;
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class ILineErrorListener;
class CIdMapper;
class CBioseq;
class CSeq_annot;
}

class CTable2AsnContext;
class CSerialObject;
class CAnnotationLoader;
union CFileContentInfo;
struct CFileContentInfoGenbank;

USING_SCOPE(objects);

//  ============================================================================
class CMultiReader
//  ============================================================================
{
public:
    CMultiReader(CTable2AsnContext& context);
    ~CMultiReader();

    using TAnnots = list<CRef<CSeq_annot>>;

    CFormatGuess::EFormat OpenFile(const string& filename, CRef<CSerialObject>& input_sequence, TAnnots& annots);
    CRef<CSerialObject> ReadNextEntry();
    void WriteObject(const CSerialObject&, ostream&);
    void LoadTemplate(CTable2AsnContext& context, const string& ifname);
    void LoadDescriptors(const string& ifname, CRef<objects::CSeq_descr> & out_desc);
    void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeq_descr & source);
    void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeqdesc & source);
    void ApplyDescriptors(objects::CSeq_entry & obj, const objects::CSeq_descr & source);
    bool LoadAnnot(objects::CScope& scope, const string& filename);
    bool LoadAnnots(const string& filename, TAnnots& annots);
    void AddAnnots(TAnnots& annots, CScope& scope);
    void LoadGFF3Fasta(istream& in, TAnnots& annots);

    static
    void GetSeqEntry(CRef<objects::CSeq_entry>& entry, CRef<objects::CSeq_submit>& submit, CRef<CSerialObject> obj);

    CRef<objects::CSeq_entry> ReadAlignment(CNcbiIstream& instream, const CArgs& args);
    bool AtSeqenceData() const { return mAtSequenceData; };

protected:
private:
    CFormatGuess::EFormat xReadFile(CNcbiIstream& in, CRef<objects::CSeq_entry>& entry, CRef<objects::CSeq_submit>& submit);
    CRef<objects::CSeq_entry> xReadFasta(CNcbiIstream& instream);
    CRef<CSerialObject> xApplyTemplate(CRef<CSerialObject> obj, bool merge_template_descriptors);
    CRef<CSerialObject> xReadASN1Text(CObjectIStream& pObjIstrm);
    CRef<CSerialObject> xReadASN1Binary(CObjectIStream& pObjIstrm, const CFileContentInfoGenbank& content_info);
    TAnnots xReadGFF3(CNcbiIstream& instream, bool post_process=true);
    TAnnots xReadGTF(CNcbiIstream& instream);
    CRef<objects::CSeq_entry> xReadFlatfile(CFormatGuess::EFormat format, const string& filename);
    void x_PostProcessAnnots(TAnnots& annots, unsigned int sequenceSize=0);
    bool xGetAnnotLoader(CAnnotationLoader& loader, const string& filename);
    bool xFixupAnnot(objects::CScope&, CRef<objects::CSeq_annot>&);

    unique_ptr<CObjectIStream> xCreateASNStream(const string& filename);
    unique_ptr<CObjectIStream> xCreateASNStream(CFormatGuess::EFormat format, unique_ptr<istream>& instream);

    //CFormatGuess::EFormat xGetFormat(CNcbiIstream&) const;
    CFormatGuess::EFormat xInputGetFormat(CNcbiIstream&, CFileContentInfo* = nullptr) const;
    CFormatGuess::EFormat xAnnotGetFormat(CNcbiIstream&) const;

    int  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;
    CTable2AsnContext& m_context;
    unique_ptr<CObjectIStream> m_obj_stream;
    bool mAtSequenceData;
};

END_NCBI_SCOPE

#endif
