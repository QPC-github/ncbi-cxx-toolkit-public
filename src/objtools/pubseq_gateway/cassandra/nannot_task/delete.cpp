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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 * Cassandra detele named annotation task.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/delete.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassNAnnotTaskDelete::CCassNAnnotTaskDelete(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CNAnnotRecord * annot,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(move(conn), keyspace, annot->GetSatKey(), true, move(data_error_cb))
    , m_Annot(annot)
{}

void CCassNAnnotTaskDelete::Wait1()
{
    bool b_need_repeat{false};
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                m_QueryArr.resize(1);
                m_QueryArr[0] = { m_Conn->NewQuery(), 0};
                auto query = m_QueryArr[0].query;
                query->NewBatch();
                string annot_part = m_Annot->IsSatKeyPrimary()
                    ? (m_Annot->GetAnnotName() + ":" + to_string(m_Annot->GetSatKey()))
                    : m_Annot->GetAnnotName();
                CNAnnotChangelogWriter().WriteChangelogEvent(
                    query.get(),
                    GetKeySpace(),
                    CNAnnotChangelogRecord(
                        m_Annot->GetAccession(),
                        annot_part,
                        m_Annot->GetVersion(),
                        m_Annot->GetSeqIdType(),
                        m_Annot->GetModified(),
                        TChangelogOperation::eDeleted
                    )
                );
                SetupQueryCB3(query);
                query->RunBatch();
                m_State = eWaitingChangelogInserted;
                break;
            }

            case eWaitingChangelogInserted: {
                if (CheckReady(m_QueryArr[0])) {
                    auto query = m_QueryArr[0].query;
                    query->Close();
                    string writetime = (m_Writetime > 0) ? "USING TIMESTAMP ? ":"";
                    if (m_Annot->GetSatKey() == 0) {
                        string sql = "DELETE FROM " + GetKeySpace() + ".bioseq_na " + writetime +
                            "WHERE accession = ? AND version = ? AND seq_id_type = ? AND annot_name = ? "
                            "IF last_modified = ?";
                        query->SetSQL(sql, 5 + (m_Writetime > 0));
                    }
                    else if (m_Annot->IsSatKeyPrimary()) {
                        string sql = "DELETE FROM " + GetKeySpace() + ".bioseq_na " + writetime +
                            "WHERE accession = ? AND version = ? AND seq_id_type = ? AND annot_name = ? AND sat_key = ? "
                            "IF last_modified = ?";
                        query->SetSQL(sql, 6 + (m_Writetime > 0));
                    }
                    else {
                        string sql = "DELETE FROM " + GetKeySpace() + ".bioseq_na " + writetime +
                            "WHERE accession = ? AND version = ? AND seq_id_type = ? AND annot_name = ? "
                            "IF sat_key = ? AND last_modified = ?";
                        query->SetSQL(sql, 6 + (m_Writetime > 0));
                    }
                    query->SetSerialConsistency(CASS_CONSISTENCY_LOCAL_SERIAL);

                    int param{0};
                    if (m_Writetime > 0) {
                        query->BindInt64(param++, m_Writetime);
                    }

                    query->BindStr(param++, m_Annot->GetAccession());
                    query->BindInt16(param++, m_Annot->GetVersion());
                    query->BindInt16(param++, m_Annot->GetSeqIdType());
                    query->BindStr(param++, m_Annot->GetAnnotName());
                    if (m_Annot->GetSatKey() == 0) {
                        query->BindInt64(param++, m_Annot->GetModified());
                    } else {
                        query->BindInt32(param++, m_Annot->GetSatKey());
                        query->BindInt64(param++, m_Annot->GetModified());
                    }

                    SetupQueryCB3(query);
                    query->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async, false);
                    m_State = eWaitingBioseqNADeleted;
                }
                break;
            }

            case eWaitingBioseqNADeleted: {
                auto query = m_QueryArr[0].query;
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                query->NextRow();
                bool applied = query->FieldGetBoolValue(0);
                // m_State = applied ? eDeleteBlobRecord : eDone;
                // b_need_repeat = true;
                // We do not need to delete blob_prop and blob_chunk records information here
                // It will be deleted while matching blob_prop table with Sybase
                if (!applied) {
                    ERR_POST(Warning << "LWT was not able to delete 'bioseq_na' record key="
                             << GetKeySpace() << "." << m_Annot->GetSatKey()
                             << ", last_modified = " << m_Annot->GetModified()
                             << " seqid: " << m_Annot->GetAccession() << "."
                             << m_Annot->GetVersion() << "|" << m_Annot->GetSeqIdType()
                    );
                }
                m_State = eDone;
                break;
            }

            /*case eDeleteBlobRecord: {
                m_BlobDeleteTask = unique_ptr<CCassBlobTaskDelete>(
                    new CCassBlobTaskDelete(
                         m_OpTimeoutMs, m_Conn, m_Keyspace,
                         m_Annot->GetSatKey(), true,  m_MaxRetries,
                         [this]
                         (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
                         {this->m_ErrorCb(status, code, severity, message);}
                    )
                );
                m_State = eWaitingDeleteBlobRecord;
                break;
            }

            case eWaitingDeleteBlobRecord:
                if (m_BlobDeleteTask->Wait()) {
                    if (m_BlobDeleteTask->HasError()) {
                        m_State = eError;
                        m_LastError = m_BlobDeleteTask->LastError();
                    } else {
                        CloseAll();
                        m_State = eDone;
                    }
                    m_BlobDeleteTask.reset();
                }
                break;*/

            default: {
                char msg[1024];
                string keyspace = GetKeySpace();
                snprintf(msg, sizeof(msg), "Failed to delete named annot (key=%s.%d) unexpected state (%d)",
                        keyspace.c_str(), GetKey(), static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
