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
    if (mpDBusConnection == NULL) {
        mpDBusConnection = ClientBusInit();
        startDetect();
    }
}

TvClient::~TvClient() {
    if (mpDBusConnection != NULL) {
        dbus_connection_unref(mpDBusConnection);
        mpDBusConnection = NULL;
    }
}

DBusConnection *TvClient::ClientBusInit()
{
    DBusConnection *connection;
    DBusError err;
    int ret;

    dbus_error_init(&err);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        LOGE("TvClient: connection error: :%s -- %s\n", err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    ret = dbus_bus_request_name(connection, "aml.tv.client", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        LOGE("TvClient: Name error: %s -- %s\n", err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        return NULL;
    }

    dbus_bus_add_match(connection, "type='signal'", &err);

    dbus_connection_flush(connection);
    if (dbus_error_is_set(&err)) {
        LOGE("TvClient: add Match Error %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        return connection;
    }

    return connection;
}

int TvClient::SendMethodCall(char *CmdString)
{
    LOGD("%s.\n", __FUNCTION__);

    DBusMessage *msg;
    DBusMessageIter arg;
    DBusPendingCall *pending;
    int ReturnVal = 0;

    msg = dbus_message_new_method_call("aml.tv.service", "/aml/tv", "aml.tv", "cmd");
    if (msg == NULL) {
        LOGE("TvClient: no memory\n");
        return RET_FAILED;
    }

    if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &CmdString, DBUS_TYPE_INVALID)) {
        LOGE("TvClient: add args failed!\n");
        dbus_message_unref(msg);
        return RET_FAILED;
    }

    if (!dbus_connection_send_with_reply (mpDBusConnection, msg, &pending, -1)) {
        LOGE("TvClient: no memeory!");
        dbus_message_unref(msg);
        return RET_FAILED;
    }

    if (pending == NULL) {
        LOGE("TvClient: Pending is NULL, may be disconnect...\n");
        dbus_message_unref(msg);
        return RET_FAILED;
    }

    dbus_connection_flush(mpDBusConnection);
    dbus_message_unref(msg);

    dbus_pending_call_block (pending);
    msg = dbus_pending_call_steal_reply (pending);
    if (msg == NULL) {
        LOGE("TvClient: reply is null. error\n");
        return RET_FAILED;
    }

    dbus_pending_call_unref(pending);

    if (!dbus_message_iter_init(msg, &arg)) {
        LOGE("TvClient: no argument, error\n");
    }

    if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_INT32) {
        LOGE("TvClient: paramter type error\n");
    }

    dbus_message_iter_get_basic(&arg, &ReturnVal);
    LOGE("TvClient: ret = %d\n",ReturnVal);
    dbus_message_unref(msg);

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

int TvClient::HandSourceConnectEvent(DBusMessageIter messageIter)
{
    dbus_int32_t eventType, source, connectStatus;
    dbus_message_iter_get_basic(&messageIter, &eventType);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &source);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &connectStatus);
    LOGD("TvClient: source: %d, connect_status: %d\n", source, connectStatus);

    TvEvent::SourceConnectEvent event;
    event.mSourceInput = source;
    event.connectionState = connectStatus;
    mInstance->SendTvClientEvent(event);
    return 0;
}

int TvClient::HandSignalDetectEvent(DBusMessageIter messageIter)
{
    dbus_int32_t eventType, source, signalFormat, transFormat, signalStatus, dviFlag;

    dbus_message_iter_get_basic(&messageIter, &eventType);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &source);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &signalFormat);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &transFormat);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &signalStatus);
    dbus_message_iter_next(&messageIter);
    dbus_message_iter_get_basic(&messageIter, &dviFlag);
    LOGD("%s: signalFormat: %d, transFormat: %d, signalStatus: %d, dviFlag: %d\n",
         __FUNCTION__, source, signalFormat, transFormat, signalStatus, dviFlag);

    TvEvent::SignalDetectEvent event;
    event.mSourceInput = source;
    event.mFmt = signalFormat;
    event.mTrans_fmt = transFormat;
    event.mStatus = signalStatus;
    event.mDviFlag = dviFlag;
    mInstance->SendTvClientEvent(event);
    return 0;

}

void *TvClient::HandleTvServiceMessage(void *args)
{
    LOGD("%s\n", __FUNCTION__);
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusError err;
    DBusConnection *Connection = (DBusConnection *)args;

    dbus_error_init(&err);

    while (1) {
        dbus_connection_read_write(Connection, 0);
        msg = dbus_connection_pop_message(Connection);
        if (msg == NULL) {
            sleep(1);
            continue;
        }
        //LOGD("path: %s\n", dbus_message_get_path (msg));
        if (dbus_message_is_signal(msg, "aml.tv", "test")) {
            if (!dbus_message_iter_init(msg, &arg)) {
                LOGE("TvClient: no argument\n");
            } else {
                if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_INVALID) {
                    dbus_int32_t eventType;
                    dbus_message_iter_get_basic(&arg, &eventType);
                    switch (eventType) {
                    case EVENT_SIGLE_DETECT:
                        HandSignalDetectEvent(arg);
                        break;
                    case EVENT_SOURCE_CONNECT:
                        HandSourceConnectEvent(arg);
                        break;
                    default:
                        LOGE("TvClient: invalid event type!\n");
                        break;
                    }
                }
            }
        } else {
            //LOGD("TvClient: Not TV service signal call!\n");
        }
        dbus_message_unref(msg);
    }
    //dbus_bus_remove_match();
    return NULL;
}

int TvClient::startDetect()
{
    int ret;
    pthread_t thread_id;

    ret = pthread_create(&thread_id, NULL, HandleTvServiceMessage, (void *)mpDBusConnection);
    if (ret != 0) {
        LOGE("%s: HandleTvServiceMessage create thread fail\n", __FUNCTION__);
        ret = -1;
    } else {
        LOGD("%s: HandleTvServiceMessage thread id (%lu) done\n", __FUNCTION__, thread_id);
        ret = 0;
    }

    return ret;
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

int TvClient::SetPictureMode(int pictureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_PICTURE_MODE, pictureMode);
    return SendMethodCall(buf);
}

int TvClient::GetPictureMode()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_PICTURE_MODE);
    return SendMethodCall(buf);
}

int TvClient::SetBacklight(int backlightValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_BACKLIGHT, backlightValue);
    return SendMethodCall(buf);
}

int TvClient::GetBacklight()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_BACKLIGHT);
    return SendMethodCall(buf);
}

int TvClient::SetBrightness(int brightnessValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_BRIGHTNESS, brightnessValue);
    return SendMethodCall(buf);
}

int TvClient::GetBrightness()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_BRIGHTNESS);
    return SendMethodCall(buf);
}

int TvClient::SetContrast(int contrastValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_CONTRAST, contrastValue);
    return SendMethodCall(buf);
}

int TvClient::GetContrast()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_CONTRAST);
    return SendMethodCall(buf);
}

int TvClient::SetSharpness(int sharpnessValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_SHARPNESS, sharpnessValue);
    return SendMethodCall(buf);
}

int TvClient::GetSharpness()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_SHARPNESS);
    return SendMethodCall(buf);
}

int TvClient::SetSaturation(int saturationValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_SATUATION, saturationValue);
    return SendMethodCall(buf);
}

int TvClient::GetSaturation()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_SATUATION);
    return SendMethodCall(buf);
}

int TvClient::SetHue(int hueValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_HUE, hueValue);
    return SendMethodCall(buf);
}

int TvClient::GetHue()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_HUE);
    return SendMethodCall(buf);
}

int TvClient::SetColorTemperature(int colorTemperatureValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_COLOR_TEMPERATURE_MODE, colorTemperatureValue);
    return SendMethodCall(buf);
}

int TvClient::GetColorTemperature()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_COLOR_TEMPERATURE_MODE);
    return SendMethodCall(buf);
}

int TvClient::SetAspectRatio(int aspectRatioValue)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.set.%d.%d", PQ_SET_ASPECT_RATIO, aspectRatioValue);
    return SendMethodCall(buf);
}

int TvClient::GetAspectRatio()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pq.get.%d", PQ_GET_ASPECT_RATIO);
    return SendMethodCall(buf);
}

int TvClient::FactorySetRGBPattern(int r, int g, int b)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_RGB_PATTERN, r, g, b);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetRGBPattern()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d", PQ_FACTORY_GET_RGB_PATTERN);
    return SendMethodCall(buf);
}

int TvClient::FactorySetGrayPattern(int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d", PQ_FACTORY_SET_GRAY_PATTERN, value);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetGrayPattern()
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d", PQ_FACTORY_GET_GRAY_PATTERN);
    return SendMethodCall(buf);
}

int TvClient::FactorySetWhiteBalanceRedGain(int source, int colortemptureMode, int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_WB_RED_GAIN, source, colortemptureMode, value);
    return SendMethodCall(buf);
}

int TvClient::FactoryGetWhiteBalanceRedGain(int source, int colortemptureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d.%d.%d", PQ_FACTORY_GET_WB_RED_GAIN, source, colortemptureMode);
    return SendMethodCall(buf);

}

int TvClient::FactorySetWhiteBalanceGreenGain(int source, int colortemptureMode, int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_WB_GREE_GAIN, source, colortemptureMode, value);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetWhiteBalanceGreenGain(int source, int colortemptureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d.%d.%d", PQ_FACTORY_GET_WB_GREE_GAIN, source, colortemptureMode);
    return SendMethodCall(buf);

}

int TvClient::FactorySetWhiteBalanceBlueGain(int source, int colortemptureMode, int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_WB_BLUE_GAIN, source, colortemptureMode, value);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetWhiteBalanceBlueGain(int source, int colortemptureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d.%d.%d", PQ_FACTORY_GET_WB_BLUE_GAIN, source, colortemptureMode);
    return SendMethodCall(buf);

}

int TvClient::FactorySetWhiteBalanceRedPostOffset(int source, int colortemptureMode, int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_WB_RED_POSTOFFSET, source, colortemptureMode, value);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetWhiteBalanceRedPostOffset(int source, int colortemptureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d.%d.%d", PQ_FACTORY_GET_WB_RED_POSTOFFSET, source, colortemptureMode);
    return SendMethodCall(buf);

}

int TvClient::FactorySetWhiteBalanceGreenPostOffset(int source, int colortemptureMode, int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_WB_GREE_POSTOFFSET, source, colortemptureMode, value);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetWhiteBalanceGreenPostOffset(int source, int colortemptureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d.%d.%d", PQ_FACTORY_GET_WB_GREE_POSTOFFSET, source, colortemptureMode);
    return SendMethodCall(buf);

}

int TvClient::FactorySetWhiteBalanceBluePostOffset(int source, int colortemptureMode, int value)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.set.%d.%d.%d.%d", PQ_FACTORY_SET_WB_BLUE_POSTOFFSET, source, colortemptureMode, value);
    return SendMethodCall(buf);

}

int TvClient::FactoryGetWhiteBalanceBluePostOffset(int source, int colortemptureMode)
{
    LOGD("%s\n", __FUNCTION__);
    char buf[32] = {0};
    sprintf(buf, "pqFactory.get.%d.%d.%d", PQ_FACTORY_GET_WB_BLUE_POSTOFFSET, source, colortemptureMode);
    return SendMethodCall(buf);

}

