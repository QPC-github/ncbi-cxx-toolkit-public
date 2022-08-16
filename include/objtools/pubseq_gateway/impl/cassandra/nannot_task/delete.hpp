#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__NANNOT_TASK__DELETE_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__NANNOT_TASK__DELETE_HPP

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
 * Cassandra delete named annotation task.
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <memory>
#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/delete.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassNAnnotTaskDelete
    : public CCassBlobWaiter
{
    enum ENAnnotDeleterState {
        eInit = 0,
        eWaitingChangelogInserted,
        eWaitingBioseqNADeleted,
        eDeleteBlobRecord,
        eWaitingDeleteBlobRecord,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

 public:
    CCassNAnnotTaskDelete(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CNAnnotRecord * annot,
        TDataErrorCallback data_error_cb
    );

    // @todo Delete operation uses conditional updates for .bioseq_na table
    // Cassandra does not support conditional updates with USING TIMESTAMP clause
    // Later consider to use just one approach. It will be easy to get rid of conditional updates
    // as (gi, type) => sat_key one to many schema is adopted.
    void SetWritetime(CNAnnotRecord::TWritetime /*value*/)
    {
        /*if (value > 0) {
            m_Writetime = value;
        }*/
    }

 protected:
    void Wait1() override;

 private:
    CNAnnotRecord * m_Annot{nullptr};
    unique_ptr<CCassBlobTaskDelete> m_BlobDeleteTask;
    CNAnnotRecord::TWritetime m_Writetime{0};
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__NANNOT_TASK__DELETE_HPP
