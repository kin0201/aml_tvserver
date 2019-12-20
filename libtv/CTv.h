/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include "common.h"
#include "CTvin.h"
#include "CTvDevicesPollDetect.h"
#include "CTvEvent.h"
#include "CHDMIRxManager.h"
#ifdef HAVE_AUDIO
#include "CTvAudio.h"
#endif


#define CONFIG_FILE_PATH_DEF     "/vendor/etc/tvconfig/tvconfig.conf"

#define HDMI_EDID14_FILE_PATH    "/vendor/etc/tvconfig/hdmi/port_14.bin"
#define HDMI_EDID20_FILE_PATH    "/vendor/etc/tvconfig/hdmi/port_20.bin"

class CTv : public CTvDevicesPollDetect::ISourceConnectObserver {
public:
    class TvIObserver {
    public:
        TvIObserver() {};
        virtual ~TvIObserver() {};
        virtual void onTvEvent (CTvEvent &event) = 0;
    };
    CTv();
    ~CTv();
    int setTvObserver (TvIObserver *ob);
    int StartTv(tv_source_input_t source);
    int StopTv(tv_source_input_t source);
    int SwitchSource(tv_source_input_t dest_source);
    int SetCurrenSourceInfo(tvin_info_t sig_info);
    tvin_info_t GetCurrentSourceInfo(void);
    int UpdateEDID(tv_source_input_t source, char *data);
    int getEDIDData(tv_source_input_t source, char *data);

    virtual void onSourceConnect(int source, int connect_status);
    virtual void onVdinSignalChange();

private:
    void onSigToStable();
    void onSigToUnstable();
    void onSigToUnSupport();
    void onSigToNoSig();
    int  sendTvEvent(CTvEvent &event);
#ifdef HAVE_AUDIO
    int mapSourcetoAudiotupe(tv_source_input_t dest_source);
#endif

    CTvin *mpTvin;
    CHDMIRxManager *mpHDMIRxManager;
    tvin_info_t mCurrentSignalInfo;
    tv_source_input_t mCurrentSource;
    CTvDevicesPollDetect mTvDevicesPollDetect;
    TvIObserver *mpObserver;
};
