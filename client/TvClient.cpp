/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_MOUDLE_TAG "TV"
#define LOG_CLASS_TAG "TvClient"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "TvClient.h"
#include "CTvClientLog.h"
#include "tvcmd.h"

const int RET_SUCCESS = 0;
const int RET_FAILED  = -1;

const int EVENT_SIGLE_DETECT = 4;
const int EVENT_SOURCE_CONNECT = 10;

TvClient *mInstance = NULL;
TvClient *TvClient::GetInstance() {
    if (mInstance == NULL) {
        mInstance = new TvClient();
    }

    return mInstance;
}

TvClient::TvClient() {
    Parcel send, reply;
    sp<IServiceManager> sm = defaultServiceManager();
    do {
        tvServicebinder = sm->getService(String16("tvservice"));
        if (tvServicebinder != 0) break;
        LOGD("TvClient: Waiting tvservice published.\n");
        usleep(500000);
    } while(true);
    LOGD("Connected to tvservice.\n");
    send.writeStrongBinder(sp<IBinder>(this));
    tvServicebinder->transact(CMD_SET_TV_CB, send, &reply);
}

TvClient::~TvClient() {
    Parcel send, reply;
    tvServicebinder->transact(CMD_CLR_TV_CB, send, &reply);
    tvServicebinder = NULL;
}

int TvClient::SendMethodCall(char *CmdString)
{
    LOGD("%s.\n", __FUNCTION__);
    int ReturnVal = 0;
    Parcel send, reply;

    if (tvServicebinder != NULL) {
        send.writeCString(CmdString);
        if (tvServicebinder->transact(CMD_TV_ACTION, send, &reply) != 0) {
            LOGE("TvClient: call %s failed.\n", CmdString);
            ReturnVal = -1;
        } else {
            ReturnVal = reply.readInt32();
        }
    }
    return ReturnVal;
}

int TvClient::SendTvClientEvent(CTvEvent &event)
{
    LOGD("%s\n", __FUNCTION__);

    int clientSize = mTvClientObserver.size();
    int i = 0;
    for (i = 0; i < clientSize; i++) {
        if (mTvClientObserver[i] != NULL) {
            LOGI("%s, client cookie:%d notifyCallback", __FUNCTION__, i);
            mTvClientObserver[i]->onTvClientEvent(event);
        }
    }

    LOGD("send event for %d TvClientObserver!\n", i);

    return 0;
}

int TvClient::HandSourceConnectEvent(const void* param)
{
    Parcel *parcel = (Parcel *) param;
    TvEvent::SourceConnectEvent event;
    event.mSourceInput = parcel->readInt32();
    event.connectionState = parcel->readInt32();
    mInstance->SendTvClientEvent(event);

    return 0;
}

int TvClient::HandSignalDetectEvent(const void* param)
{
    Parcel *parcel = (Parcel*) param;
    TvEvent::SignalDetectEvent event;
    event.mSourceInput = parcel->readInt32();
    event.mFmt = parcel->readInt32();
    event.mTrans_fmt = parcel->readInt32();
    event.mStatus = parcel->readInt32();
    event.mDviFlag = parcel->readInt32();
    mInstance->SendTvClientEvent(event);

    return 0;

}

int TvClient::setTvClientObserver(TvClientIObserver *observer)
{
    LOGD("%s\n", __FUNCTION__);
    if (observer != nullptr) {
        int cookie = -1;
        int clientSize = mTvClientObserver.size();
        for (int i = 0; i < clientSize; i++) {
            if (mTvClientObserver[i] == NULL) {
                cookie = i;
                mTvClientObserver[i] = observer;
                break;
            }
        }

        if (cookie < 0) {
            cookie = clientSize;
            mTvClientObserver[clientSize] = observer;
        }
    }

    return 0;
}

int TvClient::StartTv(tv_source_input_t source) {
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "source.start.%d", source);
    return SendMethodCall(buf);
}

int TvClient::StopTv(tv_source_input_t source) {
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "source.stop.%d", source);
    return SendMethodCall(buf);
}

int TvClient::PresetEdidVer(tv_source_input_t source, int edidVer)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[512] = {0};
    sprintf(buf, "edid.set.%d.%d", source, edidVer);
    return SendMethodCall(buf);
}

int TvClient::SetEdidData(tv_source_input_t source, char *dataBuf)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[512] = {0};
    sprintf(buf, "edid.set.%d.%s", source, dataBuf);
    return SendMethodCall(buf);
}

int TvClient::GetEdidData(tv_source_input_t source, char *dataBuf)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[512] = {0};
    sprintf(buf, "edid.get.%d.%s", source, dataBuf);
    return SendMethodCall(buf);
}

status_t TvClient::onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags) {
    LOGD("TvClient get tanscode: %u\n", code);
    switch (code) {
        case EVT_SRC_CT_CB: {
            HandSourceConnectEvent(&data);
            break;
        }
        case EVT_SIG_DT_CB: {
            HandSignalDetectEvent(&data);
            break;
        }
        case CMD_START:
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }

    return (0);
}
