/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_MOUDLE_TAG "PQ"
#define LOG_CLASS_TAG "CDevicePollCheckThread"

#include "CDevicePollCheckThread.h"
#include <pthread.h>

CDevicePollCheckThread::CDevicePollCheckThread()
{
    mpObserver = NULL;
    if (mEpoll.create() < 0) {
        LOGE("create epoll fail\n");
        return;
    }

    //VFrameSize change
    if (mVFrameSizeFile.PQ_OpenFile(VFRAME_MOUDLE_PATH) > 0) {
        m_event.data.fd = mVFrameSizeFile.PQ_GetFileFd();
        m_event.events = EPOLLIN | EPOLLET;
        mEpoll.add(mVFrameSizeFile.PQ_GetFileFd(), &m_event);
    }
    //HDR
    if (mHDRStatusFile.PQ_OpenFile(HDR_MOUDLE_PATH) > 0) {
        m_event.data.fd = mHDRStatusFile.PQ_GetFileFd();
        m_event.events = EPOLLIN | EPOLLET;
        mEpoll.add(mHDRStatusFile.PQ_GetFileFd(), &m_event);
        HDR_fd = mHDRStatusFile.PQ_GetFileFd();
    }

    //TX
    if (mTXStatusFile.PQ_OpenFile(TX_MOUDLE_PATH) > 0) {
        m_event.data.fd = mTXStatusFile.PQ_GetFileFd();
        m_event.events = EPOLLIN | EPOLLET;
        mEpoll.add(mTXStatusFile.PQ_GetFileFd(), &m_event);
    }
}

CDevicePollCheckThread::~CDevicePollCheckThread()
{

}

int CDevicePollCheckThread::StartCheckThread()
{
    int ret;
    pthread_t thread_id;

    ret = pthread_create(&thread_id, NULL, PqPollThread, (void *)this);
    if (ret != 0) {
        LOGD("create PQPollDetect thread fail\n");
        ret = -1;
    } else {
        LOGE("PQPollDetect thread id (%lu) done\n", thread_id);
        ret = 0;
    }

    return ret;
}

void* CDevicePollCheckThread::PqPollThread(void *param)
{
    CDevicePollCheckThread *pThis = (CDevicePollCheckThread *)param;
    pThis->threadLoop();
    return NULL;
}

void *CDevicePollCheckThread::threadLoop(void)
{
    if ( mpObserver == NULL ) {
        LOGD("%s: mpObserver is null!\n", __FUNCTION__);
        return NULL;
    } else {
        while (1) {
            int num = mEpoll.wait();
            for (int i = 0; i < num; ++i) {
                int fd = (mEpoll)[i].data.fd;
                /**
                 * EPOLLIN event
                 */
                if ((mEpoll)[i].events & EPOLLIN) {
                    if (fd == mVFrameSizeFile.PQ_GetFileFd()) {//vframesize change
                        mpObserver->onVframeSizeChange();
                    } else if (fd == mHDRStatusFile.PQ_GetFileFd()) {//HDR
                        mpObserver->onHDRStatusChange();
                    } else if (fd == mTXStatusFile.PQ_GetFileFd()) {//TX
                        mpObserver->onTXStatusChange();
                    }

                    if ((mEpoll)[i].events & EPOLLOUT) {

                    }
                }
            }
        }//exit

        LOGD("exit CDevicePollCheckThread!\n");
        return NULL;
    }
}

