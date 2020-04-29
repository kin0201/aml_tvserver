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
    mpTvServiceConnection = TvServiceBusInit();
    if (mpTvServiceConnection != NULL) {
        mpTv = new CTv();
        mpTv->setTvObserver(this);
        mpPQcontrol = CPQControl::GetInstance();
    } else {
        LOGE("connect to d-bus failed!\n");
    }
}

TvService::~TvService() {

}

DBusConnection *TvService::TvServiceBusInit() {
    DBusConnection *connection;
    DBusError err;
    int ret = 0;

    dbus_error_init(&err);

    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        LOGE("%s: Connection Error: %s--%s\n", __FUNCTION__, err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    ret = dbus_bus_request_name(connection, "aml.tv.service", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        LOGE("%s: Name Error: %s--%s\n", __FUNCTION__, err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        return NULL;

    dbus_bus_add_match(connection, "type='signal'", &err);

    dbus_connection_flush(connection);
    if (dbus_error_is_set(&err)) {
        LOGE("%s: add Match Error %s--%s\n", __FUNCTION__, err.name, err.message);
        dbus_error_free(&err);
        return connection;
    }

    return connection;
}

int TvService::TvServiceHandleMessage()
{
    DBusMessage *msg;
    DBusError err;
    char *commandData;

    dbus_error_init(&err);

    while (1) {
        dbus_connection_read_write(mpTvServiceConnection, 0);

        msg = dbus_connection_pop_message(mpTvServiceConnection);
        if (msg == NULL) {
            sleep(1);
            continue;
        }

        //LOGD("%s: path: %s\n", __FUNCTION__, dbus_message_get_path (msg));
        if (dbus_message_is_method_call(msg, "aml.tv", "cmd")) {
            DBusMessage *rp;
            DBusMessageIter r_arg;
            int ReturnVal = 0;

            dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &commandData, DBUS_TYPE_INVALID);
            if (dbus_error_is_set(&err)) {
                LOGE("%s: recieve message failed!\n", __FUNCTION__);
                dbus_error_free(&err);
                ReturnVal = -1;
            } else {
                LOGD("%s: recieve message: %s\n", __FUNCTION__, commandData);
                ReturnVal = ParserTvCommand(commandData);
            }

            rp = dbus_message_new_method_return(msg);
            dbus_message_iter_init_append(rp, &r_arg);
            if (!dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_INT32, &ReturnVal)) {
                LOGE("%s: no memory!\n", __FUNCTION__);
                return -1;
            }

            if (!dbus_connection_send(mpTvServiceConnection, rp, NULL)) {
                LOGE("%s: no memory!!\n", __FUNCTION__);
                return -1;
            }
            dbus_connection_flush(mpTvServiceConnection);
            dbus_message_unref(rp);
        } /*else {
            LOGE("%s: Not TV client method call!\n", __FUNCTION__);
        }*/
        dbus_message_unref(msg);
    }
    //dbus_bus_remove_match();
    return 0;
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

    DBusMessage *msg;
    DBusMessageIter arg;

    if ((msg = dbus_message_new_signal("/aml/tv/service", "aml.tv", "test")) == NULL) {
        LOGE("%s: message is NULL\n", __FUNCTION__);
        return -1;
    }

    dbus_message_iter_init_append(msg, &arg);

    int eventType = CTvEvent::TV_EVENT_SOURCE_CONNECT;
    TvEvent::SourceConnectEvent *sourceConnectEvent = (TvEvent::SourceConnectEvent *)(&event);
    LOGD("%s: source: %d, connectstatus: %d\n", __FUNCTION__, sourceConnectEvent->mSourceInput, sourceConnectEvent->connectionState);

    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &eventType);
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &(sourceConnectEvent->mSourceInput));
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &(sourceConnectEvent->connectionState));

    dbus_connection_send(mpTvServiceConnection, msg, NULL);
    dbus_connection_flush(mpTvServiceConnection);
    dbus_message_unref(msg);

    return 0;
}

int TvService::SendSignalForSignalDetectEvent(CTvEvent &event)
{
    LOGD("%s\n", __FUNCTION__);

    DBusMessage *msg;
    DBusMessageIter arg;

    if ((msg = dbus_message_new_signal("/aml/tv/service", "aml.tv", "test")) == NULL) {
        LOGE("%s: message is NULL\n", __FUNCTION__);
        return -1;
    }

    dbus_message_iter_init_append(msg, &arg);
    int eventType = CTvEvent::TV_EVENT_SIGLE_DETECT;
    TvEvent::SignalDetectEvent *signalDetectEvent = (TvEvent::SignalDetectEvent *)(&event);
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &eventType);
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &signalDetectEvent->mSourceInput);
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &(signalDetectEvent->mFmt));
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &(signalDetectEvent->mTrans_fmt));
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &(signalDetectEvent->mStatus));
    dbus_message_iter_append_basic(&arg, DBUS_TYPE_INT32, &(signalDetectEvent->mDviFlag));

    dbus_connection_send(mpTvServiceConnection, msg, NULL);
    dbus_connection_flush(mpTvServiceConnection);
    dbus_message_unref(msg);

    return 0;
}

int TvService::ParserTvCommand(char *commandData)
{
    int ret = 0;
    const char *delimitation = ".";
    char *temp = strtok(commandData, delimitation);
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

#ifdef __cplusplus
}
#endif
