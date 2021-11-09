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
 * Authors: Eugene Vasilchenko
 *
 * File Description: base class for processors which may generate os_gateway
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include "osg_caller.hpp"
#include "osg_fetch.hpp"
#include "osg_connection.hpp"
#include "osg_processor_base.hpp"
#include "pubseq_gateway_exception.hpp"

#include <corelib/request_ctx.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Error.hpp>
#include <objects/id2/ID2_Param.hpp>
#include <objects/id2/ID2_Params.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


COSGCaller::COSGCaller()
{
}


COSGCaller::~COSGCaller()
{
}


CRef<CID2_Request> COSGCaller::MakeInitRequest()
{
    CRef<CID2_Request> req(new CID2_Request());
    req->SetRequest().SetInit();
    if ( 1 ) {
        // set client name
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("log:client_name");
        param->SetValue().push_back("pubseq_gateway");
        req->SetParams().Set().push_back(param);
    }
    if ( 1 ) {
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("id2:allow");

        // allow new blob-state field in several ID2 replies
        param->SetValue().push_back("*.blob-state");
        // enable VDB-based WGS sequences
        param->SetValue().push_back("vdb-wgs");
        // enable VDB-based SNP sequences
        param->SetValue().push_back("vdb-snp");
        // enable VDB-based CDD sequences
        param->SetValue().push_back("vdb-cdd");
        req->SetParams().Set().push_back(param);
    }
    return req;
}


void COSGCaller::AddFetch(CID2_Request_Packet& packet, const CRef<COSGFetch>& fetch)
{
    CID2_Request& req = fetch->GetRequest().GetNCObject();
    req.SetSerial_number(m_Connection->AllocateRequestSerialNumber());
    packet.Set().push_back(fetch->GetRequest());
    fetch.GetNCObject().ResetReplies();
    m_Fetches.push_back(fetch);
}


static bool kAlwaysSendInit = false;


CRef<CID2_Request_Packet> COSGCaller::MakePacket(const TFetches& fetches)
{
    CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
    m_Fetches.clear();
    if ( !fetches.empty() ) {
        if ( kAlwaysSendInit || !m_Connection->InitRequestWasSent() ) {
            AddFetch(*packet, Ref(new COSGFetch(MakeInitRequest())));
        }
        for ( auto& f : fetches ) {
            AddFetch(*packet, f);
        }
    }
    return packet;
}


size_t COSGCaller::GetRequestIndex(const CID2_Reply& reply) const
{
    auto serial_number = reply.GetSerial_number();
    auto first_serial_number = m_RequestPacket->Get().front()->GetSerial_number();
    size_t index = serial_number - first_serial_number;
    if ( index >= m_Fetches.size() ) {
        NCBI_THROW_FMT(CPubseqGatewayException, eLogic,
                       "Invalid OSG reply SN: "<<serial_number<<
                       " first: "<<first_serial_number<<
                       " count: "<<m_Fetches.size());
    }
    if ( m_Fetches[index]->EndOfReplies() ) {
        NCBI_THROW_FMT(CPubseqGatewayException, eLogic,
                       "OSG reply after end-of-replies SN: "<<serial_number);
    }
    return index;
}


size_t COSGCaller::GetConnectionID() const
{
    return m_Connection? m_Connection->GetConnectionID(): 0;
}


void COSGCaller::AllocateConnection(const CRef<COSGConnectionPool>& connection_pool)
{
    _ASSERT(!m_ConnectionPool);
    m_ConnectionPool = connection_pool;
    _ASSERT(m_ConnectionPool);
    m_Connection = m_ConnectionPool->AllocateConnection();
}


void COSGCaller::SendRequest(COSGProcessorRef& processor)
{
    _ASSERT(m_Connection);
    _ASSERT(!m_RequestPacket);
    processor.NotifyOSGCallStart();
    m_RequestPacket = MakePacket(processor.GetFetches());
    _ASSERT(m_RequestPacket->Get().size() < 999999); // overflow guard
    if ( !m_RequestPacket->Get().empty() ) {
        if ( processor.NeedTrace() ) {
            ostringstream str;
            str << "OSG("<<m_Connection->GetConnectionID()<<") sending request: "
                << MSerial_AsnText << *m_RequestPacket;
            processor.SendTrace(str.str());
        }
        m_Connection->SendRequestPacket(*m_RequestPacket);
    }
    else {
        if ( processor.NeedTrace() ) {
            ostringstream str;
            str << "OSG("<<m_Connection->GetConnectionID()<<") empty request packet";
            processor.SendTrace(str.str());
        }
    }
}


void COSGCaller::WaitForReplies(COSGProcessorRef& processor)
{
    _ASSERT(m_Connection);
    _ASSERT(m_RequestPacket);
    CRef<CID2_Error> failed;
    size_t waiting_count = m_RequestPacket->Get().size();
    while ( waiting_count > 0 ) {
        if ( processor.NeedTrace() ) {
            ostringstream str;
            str << "OSG("<<m_Connection->GetConnectionID()<<") reading reply";
            processor.SendTrace(str.str());
        }
        CRef<CID2_Reply> reply = m_Connection->ReceiveReply();
        if ( processor.NeedTrace() ) {
            ostringstream str;
            str << "OSG("<<m_Connection->GetConnectionID()<<") received reply";
            processor.SendTrace(str.str());
        }
        if ( reply->IsSetError() ) {
            for ( auto& error : reply->GetError() ) {
                if ( error->GetSeverity() == CID2_Error::eSeverity_failed_command ) {
                    failed = error;
                    break;
                }
            }
        }
        size_t index = GetRequestIndex(*reply);
        m_Fetches[index]->AddReply(move(reply));
        if ( m_Fetches[index]->EndOfReplies() ) {
            --waiting_count;
        }
        if ( !failed ) {
            processor.NotifyOSGCallReply(*reply);
        }
    }
    if ( processor.NeedTrace() ) {
        ostringstream str;
        str << "OSG("<<m_Connection->GetConnectionID()<<") releasing connection";
        processor.SendTrace(str.str());
    }
    m_ConnectionPool->ReleaseConnection(m_Connection);
    _ASSERT(!m_Connection);
    if ( failed ) {
        NCBI_THROW_FMT(CPubseqGatewayException, eOutputNotInReadyState,
                       "OSG error reply: "<<MSerial_AsnText<<*failed);
    }
    processor.NotifyOSGCallEnd();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
