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
 * File Description: PSG main uv loop binder which lets to invoke a callback
 *                   from within a uv main loop
 *
 */

#include <ncbi_pch.hpp>

#include "psgs_uv_loop_binder.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"

USING_NCBI_SCOPE;

#if USE_PREPARE_CB
    void prepare_bind_cb(uv_prepare_t *     handle)
    {
        CPSGS_UvLoopBinder *    loop_binder_cb = (CPSGS_UvLoopBinder*)(handle->data);
        loop_binder_cb->x_UvOnCallback();
    }
#else
    void check_bind_cb(uv_check_t *  handle)
    {
        CPSGS_UvLoopBinder *    loop_binder_cb = (CPSGS_UvLoopBinder*)(handle->data);
        loop_binder_cb->x_UvOnCallback();
    }
#endif


CPSGS_UvLoopBinder::CPSGS_UvLoopBinder(uv_loop_t *  loop) :
    m_Loop(loop)
{
    // This call always succeeds
    #if USE_PREPARE_CB
        uv_prepare_init(loop, &m_Prepare);
        m_Prepare.data = this;
    #else
        uv_check_init(loop, &m_Check);
        m_Check.data = this;
    #endif

    // NULL callback because it is used for pushing libuv loop
    int     ret = uv_async_init(loop, &m_Async, NULL);
    if (ret < 0) {
        NCBI_THROW(CPubseqGatewayException, eInvalidAsyncInit, uv_strerror(ret));
    }

    // This call always succeeds
    #if USE_PREPARE_CB
        uv_prepare_start(&m_Prepare, prepare_bind_cb);
    #else
        uv_check_start(&m_Check, check_bind_cb);
    #endif
}


CPSGS_UvLoopBinder::~CPSGS_UvLoopBinder()
{
    // Note: Closing of the handles is done in the x_Unregister() method.
    // It must not be done in the destructor.
    // This is because the libuv appraoach. It has to be done as follows:
    // - to close the handles the libuv loop must still exist
    // - the loop is stopped
    // - the handles need to be closed
    // - the loop should have at least one more iteration within which the
    //   handles are really removed from the loop
    // So the x_Unregister() method is called by the workers at the appropriate
    // moment.
}


void CPSGS_UvLoopBinder::PostponeInvoke(TProcessorCB    cb,
                                        void *          user_data)
{
    m_QueueLock.lock();
    m_Callbacks.push_back(SUserCallback(user_data, cb));
    m_QueueLock.unlock();

    int     ret = uv_async_send(&m_Async);
    if (ret < 0)
        PSG_ERROR("Async send error: " + string(uv_strerror(ret)));
}


void CPSGS_UvLoopBinder::x_UvOnCallback(void)
{
    list<SUserCallback>     callbacks;

    m_QueueLock.lock();
    callbacks.swap(m_Callbacks);
    m_QueueLock.unlock();

    for (auto & cb : callbacks) {
        cb.m_ProcCallback(cb.m_UserData);
    }
}

void CPSGS_UvLoopBinder::x_Unregister(void)
{
    // See the comment in the destructor

    uv_close(reinterpret_cast<uv_handle_t*>(&m_Async), NULL);

    // This call always succeeds
    #if USE_PREPARE_CB
        uv_prepare_stop(&m_Prepare);
        uv_close(reinterpret_cast<uv_handle_t*>(&m_Prepare), NULL);
    #else
        uv_check_stop(&m_Check);
        uv_close(reinterpret_cast<uv_handle_t*>(&m_Check), NULL);
    #endif
}

