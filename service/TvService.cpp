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
#include "CPQControl.h"

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
    mpPQcontrol = CPQControl::GetInstance();
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

    int eventType = CTvEvent::TV_EVENT_SOURCE_CONNECT;
    TvEvent::SourceConnectEvent *sourceConnectEvent = (TvEvent::SourceConnectEvent *)(&event);
    if (evtCallBack != NULL) {
        send.writeInt32(eventType);
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

    int eventType = CTvEvent::TV_EVENT_SIGLE_DETECT;
    TvEvent::SignalDetectEvent *signalDetectEvent = (TvEvent::SignalDetectEvent *)(&event);
    if (evtCallBack != NULL) {
        send.writeInt32(eventType);
        send.writeInt32(signalDetectEvent->mSourceInput);
        send.writeInt32(signalDetectEvent->mFmt);
        send.writeInt32(signalDetectEvent->mTrans_fmt);
        send.writeInt32(signalDetectEvent->mStatus);
        send.writeInt32(signalDetectEvent->mDviFlag);
        evtCallBack->transact(EVT_SIG_DT_CB, send, &reply);
    } else {
        LOGW("Event callback is null.\n");
    }
    return 0;
}

int TvService::ParserTvCommand(const char *commandData)
{
    int ret = 0;
    char cmdbuff[1024];
    memcpy(cmdbuff, commandData, strlen(commandData));
    const char *delimitation = ".";
    char *temp = strtok(cmdbuff, delimitation);
    LOGD("%s: cmdType = %s\n", __FUNCTION__, temp);
    if (strcmp(temp, "source") == 0) {
        LOGD("%s: source cmd!\n", __FUNCTION__);
        temp = strtok(NULL, delimitation);
        if (strcmp(temp, "start") == 0) {
            temp = strtok(NULL, delimitation);
            tv_source_input_t startSource = (tv_source_input_t)atoi(temp);
            ret = mpTv->StartTv(startSource);
        } else if (strcmp(temp, "stop") == 0){
            temp = strtok(NULL, delimitation);
            tv_source_input_t stopSource = (tv_source_input_t)atoi(temp);
            ret = mpTv->StopTv(stopSource);
        } else {
            LOGD("%s: invalid sourec cmd!\n", __FUNCTION__);
        }
    } else if (strcmp(temp, "edid") == 0) {
        LOGD("%s: EDID cmd!\n", __FUNCTION__);
        temp = strtok(NULL, delimitation);
        if (strcmp(temp, "set") == 0) {
            temp = strtok(NULL, delimitation);
            tv_source_input_t setSource = (tv_source_input_t)atoi(temp);
            temp = strtok(NULL, delimitation);
            ret = mpTv->UpdateEDID(setSource, temp);
        } else if (strcmp(temp, "get") == 0) {
            temp = strtok(NULL, delimitation);
            tv_source_input_t getSource = (tv_source_input_t)atoi(temp);
            temp = strtok(NULL, delimitation);
            ret = mpTv->getEDIDData(getSource, temp);
        } else if (strcmp(temp, "preset") == 0) {
            temp = strtok(NULL, delimitation);
            tv_source_input_t setSource = (tv_source_input_t)atoi(temp);
            temp = strtok(NULL, delimitation);
            int edidVer = atoi(temp);
            ret = mpTv->PresetEdidVer(setSource, edidVer);
        } else {
            LOGD("%s: invalid cmd!\n", __FUNCTION__);
            ret = 0;
        }
    } else if (strcmp(temp, "pq") == 0){
        LOGD("%s: PQ cmd!\n", __FUNCTION__);
        temp = strtok(NULL, delimitation);
        if (strcmp(temp, "set") == 0) {
            temp = strtok(NULL, delimitation);
            int moudleID = atoi(temp);
            temp = strtok(NULL, delimitation);
            int value = atoi(temp);
            ret = mpPQcontrol->ParserSetCmd(moudleID, value);
        } else if (strcmp(temp, "get") == 0) {
            temp = strtok(NULL, delimitation);
            int moudleID = atoi(temp);
            ret = mpPQcontrol->ParserGetCmd(moudleID);
        } else {
            LOGD("%s: invalid cmd!\n", __FUNCTION__);
            ret = 0;
        }
    } else if (strcmp(temp, "pqFactory") == 0) {
        LOGD("%s: PQ factory cmd!\n", __FUNCTION__);
        pq_moudle_param_t pqParam;
        memset(&pqParam, 0, sizeof(pq_moudle_param_t));
        int paramCount = 0;
        int paramBuf[32] = {0};
        temp = strtok(NULL, delimitation);
        if (strcmp(temp, "set") == 0) {
            temp = strtok(NULL, delimitation);
            pqParam.moudleId = atoi(temp);
            while (temp = strtok(NULL, delimitation)) {
                paramBuf[paramCount] = atoi(temp);
                paramCount++;
            }
            pqParam.paramLength = paramCount;
            pqParam.paramBuf = paramBuf;
            ret = mpPQcontrol->parserFactorySetCmd(pqParam);
        } else if (strcmp(temp, "get") == 0) {
            temp = strtok(NULL, delimitation);
            pqParam.moudleId = atoi(temp);
            while (temp = strtok(NULL, delimitation)) {
                paramBuf[paramCount] = atoi(temp);
                paramCount++;
            }
            pqParam.paramLength = paramCount;
            pqParam.paramBuf = paramBuf;
            ret = mpPQcontrol->parserFactoryGetCmd(pqParam);
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
