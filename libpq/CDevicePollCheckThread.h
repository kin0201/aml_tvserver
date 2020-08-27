/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef C_DEVICE_POLL_CHECK_H
#define C_DEVICE_POLL_CHECK_H

#include "CEpoll.h"
#include "CPQFile.h"
#include "CPQLog.h"
#include <sys/prctl.h>
#include <fcntl.h>

#define VFRAME_MOUDLE_PATH    "/dev/amvideo_poll"
#define TX_MOUDLE_PATH        "/dev/display"

class CDevicePollCheckThread {
public:
    CDevicePollCheckThread();
    ~CDevicePollCheckThread();
    int StartCheckThread();

    class IDevicePollCheckObserver {
    public:
        IDevicePollCheckObserver() {};
        virtual ~IDevicePollCheckObserver() {};
        virtual void onVframeSizeChange() {};
        virtual void onTXStatusChange() {};
    };

    void setObserver ( IDevicePollCheckObserver *pOb ) {
        mpObserver = pOb;
    };

private:
    static void *PqPollThread(void* data);
    void *threadLoop(void);

    IDevicePollCheckObserver *mpObserver;
    CEpoll       mEpoll;
    epoll_event m_event;
    CPQFile mVFrameSizeFile;
    CPQFile mTXStatusFile;
};

#endif
