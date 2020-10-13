/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_MOUDLE_TAG "TV"
#define LOG_CLASS_TAG "TvService"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "TvService.h"
#include "common.h"
#include "CTvLog.h"
#include "tvcmd.h"

#ifdef __cplusplus
extern "C" {
#endif

TvService *mInstance = NULL;
TvService *TvService::GetInstance() {
    if (mInstance == NULL) {
        mInstance = new TvService();
    }

    return mInstance;
}

TvService::TvService() {
    mpTv = new CTv();
    mpTv->setTvObserver(this);
}

TvService::~TvService() {

}

void TvService::onTvEvent(CTvEvent &event) {
    int eventType = event.getEventType();
    LOGD("%s: eventType: %d\n", __FUNCTION__, eventType);
    switch (eventType) {
    case CTvEvent::TV_EVENT_SIGLE_DETECT:
        SendSignalForSignalDetectEvent(event);
        break;
    case CTvEvent::TV_EVENT_SOURCE_CONNECT:
        SendSignalForSourceConnectEvent(event);
        break;
    default :
        LOGE("TvService: invalie event type!\n");
        break;
    }
    return;
}

int TvService::SendSignalForSourceConnectEvent(CTvEvent &event)
{
    LOGD("%s\n", __FUNCTION__);
    Parcel send, reply;

    TvEvent::SourceConnectEvent *sourceConnectEvent = (TvEvent::SourceConnectEvent *)(&event);
    if (evtCallBack != NULL) {
        send.writeInt32(sourceConnectEvent->mSourceInput);
        send.writeInt32(sourceConnectEvent->connectionState);
        LOGD("send source evt(%d,%d) to client.\n",
             sourceConnectEvent->mSourceInput, sourceConnectEvent->connectionState);
        evtCallBack->transact(EVT_SRC_CT_CB, send, &reply);
    } else {
        LOGD("Event callback is null.\n");
    }
    return 0;
}

int TvService::SendSignalForSignalDetectEvent(CTvEvent &event)
{
    LOGD("%s\n", __FUNCTION__);
    Parcel send, reply;

    TvEvent::SignalDetectEvent *signalDetectEvent = (TvEvent::SignalDetectEvent *)(&event);
    if (evtCallBack != NULL) {
        send.writeInt32(signalDetectEvent->mSourceInput);
        send.writeInt32(signalDetectEvent->mFmt);
        send.writeInt32(signalDetectEvent->mTrans_fmt);
        send.writeInt32(signalDetectEvent->mStatus);
        send.writeInt32(signalDetectEvent->mDviFlag);
        evtCallBack->transact(EVT_SIG_DT_CB, send, &reply);
    } else {
        LOGD("Event callback is null.\n");
    }
    return 0;
}

int TvService::ParserTvCommand(const char *commandData)
{
    int ret = 0;
    char cmdbuff[1024];
    memset(cmdbuff, 0x0, sizeof(cmdbuff));
    memcpy(cmdbuff, commandData, strlen(commandData));
    const char *delimitation = ".";
    char *temp = strtok(cmdbuff, delimitation);
    LOGD("%s: cmdType = %s\n", __FUNCTION__, temp);
    if (strcmp(temp, "control") == 0) {
        LOGD("%s: control cmd!\n", __FUNCTION__);
        temp = strtok(NULL, delimitation);
        int moudleID = atoi(temp);
        if (moudleID == TV_CONTROL_START_TV) {
            temp = strtok(NULL, delimitation);
            tv_source_input_t startSource = (tv_source_input_t)atoi(temp);
            ret = mpTv->StartTv(startSource);
        } else if (moudleID == TV_CONTROL_STOP_TV) {
            temp = strtok(NULL, delimitation);
            tv_source_input_t stopSource = (tv_source_input_t)atoi(temp);
            ret = mpTv->StopTv(stopSource);
        } else if (moudleID == TV_CONTROL_VDIN_WORK_MODE_SET) {
            temp = strtok(NULL, delimitation);
            vdin_work_mode_t setVdinWorkMode = (vdin_work_mode_t)atoi(temp);
            ret = mpTv->SetVdinWorkMode(setVdinWorkMode);
        } else {
            LOGD("%s: invalid sourec cmd!\n", __FUNCTION__);
        }
    } else if (strcmp(temp, "edid") == 0) {
        LOGD("%s: EDID cmd!\n", __FUNCTION__);
        temp = strtok(NULL, delimitation);
        if (strcmp(temp, "set") == 0) {
            temp = strtok(NULL, delimitation);
            int moudleID = atoi(temp);
            temp = strtok(NULL, delimitation);
            tv_source_input_t setSource = (tv_source_input_t)atoi(temp);
            if (moudleID == HDMI_EDID_VER_SET) {
                temp = strtok(NULL, delimitation);
                tv_hdmi_edid_version_t setVersion = (tv_hdmi_edid_version_t)atoi(temp);
                ret = mpTv->SetEdidVersion(setSource, setVersion);
            } else if (moudleID == HDMI_EDID_DATA_SET) {
                temp = strtok(NULL, delimitation);
                ret = mpTv->SetEDIDData(setSource, temp);
            } else {
                LOGD("%s: invalid EDID set cmd!\n", __FUNCTION__);
                ret = 0;
            }
        } else if (strcmp(temp, "get") == 0) {
            temp = strtok(NULL, delimitation);
            int moudleID = atoi(temp);
            temp = strtok(NULL, delimitation);
            tv_source_input_t getSource = (tv_source_input_t)atoi(temp);
            if (moudleID == HDMI_EDID_VER_GET) {
                ret = mpTv->GetEdidVersion(getSource);
            } else if (moudleID == HDMI_EDID_DATA_GET) {
                ret = mpTv->GetEDIDData(getSource, temp);
            } else {
                LOGD("%s: invalid EDID get cmd!\n", __FUNCTION__);
                ret = 0;
            }
        } else {
            LOGD("%s: invalid cmd!\n", __FUNCTION__);
            ret = 0;
        }
    } else {
        LOGD("%s: invalie cmdType!\n", __FUNCTION__);
    }

    return ret;
}

status_t TvService::onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags) {
    LOGD("%s: cmd is %d.\n", __FUNCTION__, code);
    switch (code) {
        case CMD_TV_ACTION: {
            const char* command = data.readCString();
            int ret = ParserTvCommand(command);
            reply->writeInt32(ret);
            break;
        }
        case CMD_SET_TV_CB: {
            evtCallBack = data.readStrongBinder();
            break;
        }
        case CMD_CLR_TV_CB: {
            evtCallBack = NULL;
            break;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }

    return (0);
}

#ifdef __cplusplus
}
#endif
