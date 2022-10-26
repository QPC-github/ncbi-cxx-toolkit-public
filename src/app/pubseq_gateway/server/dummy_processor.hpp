#ifndef PSGS_DUMMYPROCESSOR__HPP
#define PSGS_DUMMYPROCESSOR__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: cassandra processor create dispatcher
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "ipsgs_processor.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


class CPSGS_DummyProcessor : public IPSGS_Processor
{
public:
    CPSGS_DummyProcessor();
    CPSGS_DummyProcessor(shared_ptr<CPSGS_Request> request,
                         shared_ptr<CPSGS_Reply> reply,
                         TProcessorPriority priority);
    virtual ~CPSGS_DummyProcessor();

    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const override;
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority priority) const;
    virtual void Process(void);
    virtual void Cancel(void);
    virtual EPSGS_Status GetStatus(void);
    virtual string GetName(void) const;
    virtual string GetGroupName(void) const;

public:
    IPSGS_Processor::EPSGS_Status   m_Status;
};

#endif  // PSGS_DUMMYPROCESSOR__HPP

