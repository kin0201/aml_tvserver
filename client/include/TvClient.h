/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef TVCLIENT_H
#define TVCLIENT_H

#include <dbus/dbus.h>
#include <map>
#include <memory>

#include "common.h"
#include "CTvEvent.h"

#ifdef __cplusplus
extern "C" {
#endif

class TvClient {
public:
    class TvClientIObserver {
    public:
        TvClientIObserver() {};
        virtual ~TvClientIObserver() {};
        virtual void onTvClientEvent(CTvEvent &event) = 0;
    };

    TvClient();
    ~TvClient();
    static TvClient *GetInstance();
    int setTvClientObserver(TvClientIObserver *observer);
    int StartTv(tv_source_input_t source);
    int StopTv(tv_source_input_t source);
    int SetEdidData(tv_source_input_t source, char *dataBuf);
    int GetEdidData(tv_source_input_t source,char *dataBuf);
    int SetPictureMode(int pictureMode);
    int GetPictureMode();
    int SetBacklight(int backlightValue);
    int GetBacklight();
    int SetBrightness(int brightnessValue);
    int GetBrightness();
    int SetContrast(int contrastValue);
    int GetContrast();
    int SetSharpness(int sharpnessValue);
    int GetSharpness();
    int SetSaturation(int saturationValue);
    int GetSaturation();
    int SetHue(int hueValue);
    int GetHue();
    int SetColorTemperature(int colorTemperatureValue);
    int GetColorTemperature();
    int SetAspectRatio(int aspectRatioValue);
    int GetAspectRatio();
    int FactorySetRGBPattern(int r, int g, int b);
    int FactoryGetRGBPattern();
    int FactorySetGrayPattern(int value);
    int FactoryGetGrayPattern();
    int FactorySetWhiteBalanceRedGain(int source, int colortemptureMode, int value);
    int FactoryGetWhiteBalanceRedGain(int source, int colortemptureMode);
    int FactorySetWhiteBalanceGreenGain(int source, int colortemptureMode, int value);
    int FactoryGetWhiteBalanceGreenGain(int source, int colortemptureMode);
    int FactorySetWhiteBalanceBlueGain(int source, int colortemptureMode, int value);
    int FactoryGetWhiteBalanceBlueGain(int source, int colortemptureMode);
    int FactorySetWhiteBalanceRedPostOffset(int source, int colortemptureMode, int value);
    int FactoryGetWhiteBalanceRedPostOffset(int source, int colortemptureMode);
    int FactorySetWhiteBalanceGreenPostOffset(int source, int colortemptureMode, int value);
    int FactoryGetWhiteBalanceGreenPostOffset(int source, int colortemptureMode);
    int FactorySetWhiteBalanceBluePostOffset(int source, int colortemptureMode, int value);
    int FactoryGetWhiteBalanceBluePostOffset(int source, int colortemptureMode);
private:
    DBusConnection *ClientBusInit();
    int SendMethodCall(char *CmdString);
    int startDetect();
    static void *HandleTvServiceMessage(void *args);
    static int HandSourceConnectEvent(DBusMessageIter messageIter);
    static int HandSignalDetectEvent(DBusMessageIter messageIter);
    int SendTvClientEvent(CTvEvent &event);

    DBusConnection *mpDBusConnection = NULL;
    std::map<int, TvClientIObserver *> mTvClientObserver;
};
#ifdef __cplusplus
}
#endif
#endif
