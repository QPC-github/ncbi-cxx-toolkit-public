#ifndef ALGO_BLAST_API__PSSM_ENGINE__HPP
#define ALGO_BLAST_API__PSSM_ENGINE__HPP

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
 * Author:  Christiam Camacho
 *
 */

/** @file pssm_engine.hpp
 * C++ API for the PSI-BLAST PSSM engine. 
 */
 
#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/pssm_input.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_results.hpp> // for CBlastAncillaryData
#include <algo/blast/api/cdd_pssm_input.hpp>

// Forward declarations
class CPssmCreateTestFixture;      // unit test class

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    /// forward declaration of ASN.1 object containing PSSM (scoremat.asn)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Exception class for the CPssmEngine class
class CPssmEngineException : public CBlastException
{
public:
    /// Types of exceptions generated by the CPssmEngine class
    enum EErrCode {
        eNullInputData,
        eInvalidInputData
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const override {
        switch (GetErrCode()) {
        case eNullInputData:    return "eNullInputData";
        case eInvalidInputData: return "eInvalidInputData";
        default:                return CException::GetErrCodeString();
        }
    }
#ifndef SKIP_DOXYGEN_PROCESSING
    NCBI_EXCEPTION_DEFAULT(CPssmEngineException, CBlastException);
#endif /* SKIP_DOXYGEN_PROCESSING */
};

/// Computes a PSSM as specified in PSI-BLAST.
///
/// This class must be configured with a concrete strategy for it to obtain 
/// its input data.
/// The following example uses the CPsiBlastInputData concrete strategy:
///
/// @code
/// ...
/// CPsiBlastInputData pssm_strategy(query, query_length, alignment, 
///                                  object_manager_scope, psi_blast_options);
/// CPssmEngine pssm_engine(&pssm_strategy);
/// CRef<CPssmWithParameters> scoremat = pssm_engine.Run();
/// ...
/// @endcode

class NCBI_XBLAST_EXPORT CPssmEngine : public CObject
{
public:
    /// Constructor to configure the PSSM engine with a PSSM input data
    /// strategy object
    /// Checks that no data returned by the IPssmInputData interface is NULL
    /// @throws CPssmEngineException if validation fails. Does not test the
    /// GetData() method as this is only populated after Process() is called.
    CPssmEngine(IPssmInputData* input);

    /// Constructor to perform the last 2 stages of the PSSM creation algorithm
    /// Checks that no data returned by the IPssmInputFreqRatios interface is
    /// NULL
    /// @throws CPssmEngineException if validation fails
    CPssmEngine(IPssmInputFreqRatios* input);

    /// Constructor to configure the PSSM engine with a PSSM input data
    /// strategy object for CDD-based PSSM computation
    CPssmEngine(IPssmInputCdd* input);

    /// Destructor
    ~CPssmEngine();

    /// Sets the Karlin & Altschul parameters in the BlastScoreBlk to be used
    /// in PSSM generation. This should be used when performing PSI-BLAST
    /// iterations, but it's not necessary for a single PSSM construction
    /// @param ancillary_data BLAST ancillary data from a previous iteration
    void SetUngappedStatisticalParams(CConstRef<CBlastAncillaryData>
                                      ancillary_data);

    /// Runs the PSSM engine to compute the PSSM
    CRef<objects::CPssmWithParameters> Run();

private:
    // Note: only one of the two pointers below should be non-NULL, as this
    // determines which API from the C PSSM engine core to call

    /// Handle to strategy to process raw PSSM input data
    IPssmInputData*         m_PssmInput;
    /// Pointer to input data to create PSSM from frequency ratios
    IPssmInputFreqRatios*   m_PssmInputFreqRatios;
    /// Blast score block structure
    CBlastScoreBlk          m_ScoreBlk;

    /// Pointer to strategy to process raw PSSM input data
    /// Note: Only one m_PssmInput* should be non-NULL
    IPssmInputCdd* m_PssmInputCdd;

    /// Copies query sequence and adds protein sentinel bytes at the beginning
    /// and at the end of the sequence.
    /// @param query sequence to copy [in]
    /// @param query_length length of the sequence above [in]
    /// @throws CBlastException if does not have enough memory
    /// @return copy of query guarded by protein sentinel bytes
    static unsigned char*
    x_GuardProteinQuery(const unsigned char* query,
                        unsigned int query_length);

    /// Initialiazes the core BlastQueryInfo structure for a single protein
    /// sequence.
    /// @todo this should be moved to the core of BLAST
    /// @param query_length length of the sequence above [in]
    BlastQueryInfo*
    x_InitializeQueryInfo(unsigned int query_length);

    /// Initializes the BlastScoreBlk data member required to run the PSSM 
    /// engine.
    /// @todo this should be moved to the core of BLAST
    /// @param query sequence [in]
    /// @param query_length length of the sequence above [in]
    /// @param matrix_name name of the underlying scoring matrix to use [in]
    /// @param gap_existence cost to open a gap
    /// @param gap_extension cost to open a gap
    /// @throws CBlastException if does not have enough memory or if there was
    /// an error when setting up the return value
    /// @todo add an overloaded version of this method which takes an already
    /// constructed BlastScoreBlk*
    void
    x_InitializeScoreBlock(const unsigned char* query,
                           unsigned int query_length,
                           const char* matrix_name,
                           int gap_existence,
                           int gap_extension);

    /// Private interface to retrieve query sequence from its data source
    /// interface
    unsigned char* x_GetQuery() const;

    /// Private interface to retrieve query length from its data source
    /// interface
    unsigned int x_GetQueryLength() const;

    /// Private interface to retrieve matrix name from its data source
    /// interface
    const char* x_GetMatrixName() const;

    /// Private interface to retrieve gap existence cost from data source
    int x_GetGapExistence() const;

    /// Private interface to retrieve gap extension cost from data source
    int x_GetGapExtension() const;

    /// Using IPssmInputData as a delegate to provide input data in the form of
    /// a multiple sequence alignment, creates a PSSM using the CORE C PSSM 
    /// engine API
    CRef<objects::CPssmWithParameters>
    x_CreatePssmFromMsa();

    /// Using IPssmInputFreqRatios as a delegate to provide the input PSSM's
    /// frequency ratios, creates a PSSM using the CORE C PSSM engine API
    CRef<objects::CPssmWithParameters>
    x_CreatePssmFromFreqRatios();

    /// Using IPssmInputCdd as a delegate to provide data in the form of
    /// multiple alignment of CDs, creates PSSM using the CORE C PSSM
    /// engine API
    CRef<objects::CPssmWithParameters>
    x_CreatePssmFromCDD();

    /// Converts the PSIMatrix structure into a ASN.1 CPssmWithParameters object
    /// @param pssm input PSIMatrix structure [in]
    /// @param opts options to be used in the PSSM engine [in]
    /// @param matrix_name name of the underlying scoring matrix used
    /// @param diagnostics contains diagnostics data from PSSM creation process
    /// to save into the return value [in]
    /// @return CPssmWithParameters object with equivalent contents to
    /// those in pssm argument
    static CRef<objects::CPssmWithParameters>
    x_PSIMatrix2Asn1(const PSIMatrix* pssm,
                     const char* matrix_name,
                     const PSIBlastOptions* opts = NULL,
                     const PSIDiagnosticsResponse* diagnostics = NULL);

    /// Convert a PSSM return status into a string
    /// @param error_code return value of a PSSM engine function as defined in
    /// blast_psi_priv.h [in]
    /// @return string containing a description of the error
    static std::string
    x_ErrorCodeToString(int error_code);

    /// Default constructor available for derived test classes
    CPssmEngine() {}
    /// Prohibit copy constructor
    CPssmEngine(const CPssmEngine& rhs);
    /// Prohibit assignment operator
    CPssmEngine& operator=(const CPssmEngine& rhs);

    /// unit test class
    friend class ::CPssmCreateTestFixture; 
};

/// Auxiliary class to convert data encoded in the PSSM to CNcbiMatrix
class NCBI_XBLAST_EXPORT CScorematPssmConverter 
{
public:
    /// Returns matrix of BLASTAA_SIZE by query size (dimensions are opposite of
    /// what is stored in the BlastScoreBlk) containing scores
    /// @param pssm PSSM to extract data from [in]
    /// @throws std::runtime_error if scores are not available
    static CNcbiMatrix<int>*
    GetScores(const objects::CPssmWithParameters& pssm);

    /// Returns matrix of BLASTAA_SIZE by query size (dimensions are opposite of
    /// what is stored in the BlastScoreBlk) containing frequency ratios
    /// @param pssm PSSM to extract data from [in]
    /// @throws std::runtime_error if frequency ratios are not available
    static CNcbiMatrix<double>* 
    GetFreqRatios(const objects::CPssmWithParameters& pssm);

    /// Returns the information content per position of the PSSM
    /// @param pssm PSSM to extract data from [in]
    /// @param retval vector containing the information content or an empty
    /// vector if this data is not available [in|out]
    static void 
    GetInformationContent(const objects::CPssmWithParameters& pssm, 
                          vector<double>& retval);

    /// Returns the relative gapless PSSM column weights to pseudocounts
    /// for the provided PSSM
    /// @param pssm PSSM to extract data from [in]
    /// @param retval vector containing the gapless column weights or an empty
    /// vector if this data is not available [in|out]
    static void 
    GetGaplessColumnWeights(const objects::CPssmWithParameters& pssm, 
                            vector<double>& retval);

    /// Returns matrix of BLASTAA_SIZE by query size (dimensions are opposite of
    /// what is stored in the BlastScoreBlk) containing the residue frequencies
    /// per position of the PSSM
    /// @param pssm PSSM to extract data from [in]
    /// @return NULL if residue frequencies are not available
    static CNcbiMatrix<int>*
    GetResidueFrequencies(const objects::CPssmWithParameters& pssm);

    /// Returns matrix of BLASTAA_SIZE by query size (dimensions are opposite of
    /// what is stored in the BlastScoreBlk) containing the weighted residue 
    /// frequencies per position of the PSSM
    /// @param pssm PSSM to extract data from [in]
    /// @return NULL if weighted residue frequencies are not available
    static CNcbiMatrix<double>*
    GetWeightedResidueFrequencies(const objects::CPssmWithParameters& pssm);

    /// Data used in sequence weights computation
    /// @param pssm PSSM to extract data from [in]
    /// @param retval vector containing the sigma values or an empty
    /// vector if this data is not available [in|out]
    static void
    GetSigma(const objects::CPssmWithParameters& pssm, vector<double>& retval);

    /// Length of the aligned regions per position of the query sequence
    /// @param pssm PSSM to extract data from [in]
    /// @param retval vector containing the interval sizes or an empty
    /// vector if this data is not available [in|out]
    static void
    GetIntervalSizes(const objects::CPssmWithParameters& pssm, 
                     vector<int>& retval);

    /// Gets the number of matching sequences per position of the PSSM
    /// @param pssm PSSM to extract data from [in]
    /// @param retval vector containing the number of matching sequences or an
    /// empty vector if this data is not available [in|out]
    static void
    GetNumMatchingSeqs(const objects::CPssmWithParameters& pssm, 
                       vector<int>& retval);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__PSSM_ENGINE__HPP */
