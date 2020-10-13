/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_MOUDLE_TAG "TV"
#define LOG_CLASS_TAG "CTv"

#include <stdint.h>
#include <string.h>

#include "CTv.h"
#include "tvutils.h"
#include "TvConfigManager.h"
#include "CTvLog.h"

static tv_hdmi_edid_version_t Hdmi1CurrentEdidVer = HDMI_EDID_VER_14;
static tv_hdmi_edid_version_t Hdmi2CurrentEdidVer = HDMI_EDID_VER_14;
static tv_hdmi_edid_version_t Hdmi3CurrentEdidVer = HDMI_EDID_VER_14;
static tv_hdmi_edid_version_t Hdmi4CurrentEdidVer = HDMI_EDID_VER_14;


CTv::CTv()
{
    mpObserver = NULL;

    const char* tvConfigFilePath = getenv(CFG_TV_CONFIG_FILE_PATH_STR);
    if (!tvConfigFilePath) {
        LOGD("%s: read tvconfig file path failed!\n", __FUNCTION__);
        tvConfigFilePath = CONFIG_FILE_PATH_DEF;
    } else {
        LOGD("%s: tvconfig file path is %s!\n", __FUNCTION__, tvConfigFilePath);
    }
    LoadConfigFile(tvConfigFilePath);

    mpTvin = CTvin::getInstance();
    mpTvin->Tvin_AddVideoPath(TV_PATH_VDIN_AMLVIDEO2_PPMGR_DEINTERLACE_AMVIDEO);
    mpTvin->Tvin_LoadSourceInputToPortMap();
    mpHDMIRxManager = new CHDMIRxManager();
    tvin_info_t signalInfo;
    signalInfo.trans_fmt = TVIN_TFMT_2D;
    signalInfo.fmt = TVIN_SIG_FMT_NULL;
    signalInfo.status = TVIN_SIG_STATUS_NULL;
    signalInfo.cfmt = TVIN_COLOR_FMT_MAX;
    signalInfo.hdr_info= 0;
    signalInfo.fps = 60;
    signalInfo.is_dvi = 0;
    signalInfo.aspect_ratio = TVIN_ASPECT_NULL;
    SetCurrenSourceInfo(signalInfo);
    int dolbyVisionEnableState = 0;
    char buf[32] = {0};
    tvReadSysfs(DOLBY_VISION_ENABLE_PATH, buf);
    if (strcmp("Y", buf) == 0) {
        dolbyVisionEnableState = true;
    } else {
        dolbyVisionEnableState = false;
    }
    LoadEdidData(0, dolbyVisionEnableState);
    mTvDevicesPollDetect.setObserver(this);
    mTvDevicesPollDetect.startDetect();
}

CTv::~CTv()
{
    if (mpTvin != NULL) {
        mpTvin->Tvin_RemoveVideoPath(TV_PATH_TYPE_TVIN);
        delete mpTvin;
        mpTvin = NULL;
    }

    if (mpHDMIRxManager != NULL) {
        delete mpHDMIRxManager;
        mpHDMIRxManager = NULL;
    }

    mpObserver = NULL;
    UnloadConfigFile();
}

int CTv::StartTv(tv_source_input_t source)
{
    LOGD("%s: source = %d!\n", __FUNCTION__, source);
    int ret = -1;
    setVideoLayerStatus(VIDEO_LAYER_STATUS_ENABLE_AND_CLEAN);
    tvin_port_t source_port = mpTvin->Tvin_GetSourcePortBySourceInput(source);
    ret = mpTvin->Tvin_OpenPort(source_port);
    mCurrentSource = source;
#ifdef HAVE_AUDIO
    CTvAudio::getInstance()->create_audio_patch(mapSourcetoAudiotupe(source));
#endif
    return ret;
}

int CTv::StopTv(tv_source_input_t source)
{
    LOGD("%s: source = %d!\n", __FUNCTION__, source);

    mVdinWorkMode = VDIN_WORK_MODE_VFM;
    mCurrentSource = SOURCE_MPEG;
    tvin_info_t tempSignalInfo;
    tempSignalInfo.trans_fmt = TVIN_TFMT_2D;
    tempSignalInfo.fmt = TVIN_SIG_FMT_NULL;
    tempSignalInfo.status = TVIN_SIG_STATUS_NULL;
    tempSignalInfo.cfmt = TVIN_COLOR_FMT_MAX;
    tempSignalInfo.hdr_info= 0;
    tempSignalInfo.fps = 60;
    tempSignalInfo.is_dvi = 0;
    tempSignalInfo.aspect_ratio = TVIN_ASPECT_NULL;
    SetCurrenSourceInfo(tempSignalInfo);
#ifdef HAVE_AUDIO
    CTvAudio::getInstance()->release_audio_patch();
#endif
    mpTvin->Tvin_StopDecoder();
    tvin_port_t source_port = mpTvin->Tvin_GetSourcePortBySourceInput(source);
    return mpTvin->Tvin_ClosePort(source_port);
}

int CTv::SwitchSource(tv_source_input_t dest_source)
{
    if (dest_source != mCurrentSource) {
        StopTv(mCurrentSource);
        StartTv(dest_source);
    } else {
        LOGD("same source,no need switch!\n");
    }

    return 0;
}

void CTv::onSourceConnect(int source, int connect_status)
{
    LOGD("onSourceConnect: source = %d, connect_status= %d!\n", source, connect_status);
    //To do
    TvEvent::SourceConnectEvent event;
    event.mSourceInput = source;
    event.connectionState = connect_status;
    sendTvEvent(event);
}

void CTv::onVdinSignalChange()
{
    vdin_event_info_s SignalEventInfo;
    memset(&SignalEventInfo, 0, sizeof(vdin_event_info_s));
    int ret  = mpTvin->Tvin_GetSignalEventInfo(&SignalEventInfo);
    if (ret < 0) {
        LOGD("Get vidn event error!\n");
    } else {
        tv_source_input_type_t source_type = mpTvin->Tvin_SourceInputToSourceInputType(mCurrentSource);
        tvin_sig_change_flag_t vdinEventType = (tvin_sig_change_flag_t)SignalEventInfo.event_sts;
        switch (vdinEventType) {
        case TVIN_SIG_CHG_SDR2HDR:
        case TVIN_SIG_CHG_HDR2SDR:
        case TVIN_SIG_CHG_DV2NO:
        case TVIN_SIG_CHG_NO2DV: {
            LOGD("%s: hdr info change!\n", __FUNCTION__);
            tvin_info_t vdinSignalInfo;
            memset(&vdinSignalInfo, 0, sizeof(tvin_info_t));
            ret = mpTvin->Tvin_GetSignalInfo(&vdinSignalInfo);
            if (ret < 0) {
                LOGD("%s: Get vidn event error!\n", __FUNCTION__);
            } else {
                if ((mCurrentSignalInfo.status == TVIN_SIG_STATUS_STABLE) && (mCurrentSignalInfo.hdr_info != vdinSignalInfo.hdr_info)) {
                    if (source_type == SOURCE_TYPE_HDMI) {
                        //tvSetCurrentHdrInfo(vdinSignalInfo.hdr_info);
                    }
                    mCurrentSignalInfo.hdr_info = vdinSignalInfo.hdr_info;
                } else {
                    LOGD("%s: hdmi signal don't stable!\n", __FUNCTION__);
                }
            }
            break;
        }
        case TVIN_SIG_CHG_COLOR_FMT:
            LOGD("%s: no need do any thing for colorFmt change!\n", __FUNCTION__);
            break;
        case TVIN_SIG_CHG_RANGE:
            LOGD("%s: no need do any thing for colorRange change!\n", __FUNCTION__);
            break;
        case TVIN_SIG_CHG_BIT:
            LOGD("%s: no need do any thing for color bit deepth change!\n", __FUNCTION__);
            break;
        case TVIN_SIG_CHG_VS_FRQ:
            LOGD("%s: no need do any thing for VS_FRQ change!\n", __FUNCTION__);
            break;
        case TVIN_SIG_CHG_STS:
            LOGD("%s: vdin signal status change!\n", __FUNCTION__);
            onSigStatusChange();
            break;
        case TVIN_SIG_CHG_AFD: {
            LOGD("%s: AFD info change!\n", __FUNCTION__);
            if (source_type == SOURCE_TYPE_HDMI) {
                tvin_info_t newSignalInfo;
                memset(&newSignalInfo, 0, sizeof(tvin_info_t));
                int ret = mpTvin->Tvin_GetSignalInfo(&newSignalInfo);
                if (ret < 0) {
                    LOGD("%s: Get Signal Info error!\n", __FUNCTION__);
                } else {
                    if ((newSignalInfo.status == TVIN_SIG_STATUS_STABLE)
                        && (mCurrentSignalInfo.aspect_ratio != newSignalInfo.aspect_ratio)) {
                        mCurrentSignalInfo.aspect_ratio = newSignalInfo.aspect_ratio;
                        //tvSetCurrentAspectRatioInfo(newSignalInfo.aspect_ratio);
                    } else {
                        LOGD("%s: signal not stable or same AFD info!\n", __FUNCTION__);
                    }
                }
            }
            break;
        }
        case TVIN_SIG_CHG_DV_ALLM:
            LOGD("%s: allm info change!\n", __FUNCTION__);
            if (source_type == SOURCE_TYPE_HDMI) {
                //setPictureModeBySignal(PQ_MODE_SWITCH_TYPE_AUTO);
            } else {
                LOGD("%s: not hdmi source!\n", __FUNCTION__);
            }
            break;
        default:
            LOGD("%s: invalid vdin event!\n", __FUNCTION__);
            break;
        }
    }
}

void CTv::onSigStatusChange(void)
{
    LOGD("%s\n", __FUNCTION__);
    tvin_info_s tempSignalInfo;
    int ret = mpTvin->Tvin_GetSignalInfo(&tempSignalInfo);
    if (ret < 0) {
        LOGD("Get Signal Info error!\n");
        return;
    } else {
        SetCurrenSourceInfo(tempSignalInfo);
        LOGD("sig_fmt is %d, status is %d, isDVI is %d, hdr_info is 0x%x\n",
               mCurrentSignalInfo.fmt, mCurrentSignalInfo.status, mCurrentSignalInfo.is_dvi, mCurrentSignalInfo.hdr_info);
        if ( mCurrentSignalInfo.status == TVIN_SIG_STATUS_STABLE ) {
            onSigToStable();
        } else if (mCurrentSignalInfo.status == TVIN_SIG_STATUS_UNSTABLE ) {
            onSigToUnstable();
        } else if ( mCurrentSignalInfo.status == TVIN_SIG_STATUS_NOTSUP ) {
            onSigToUnSupport();
        } else if ( mCurrentSignalInfo.status == TVIN_SIG_STATUS_NOSIG ) {
            onSigToNoSig();
        } else {
            LOGD("%s: invalid signal status!\n");
        }

        return;
    }
}

int CTv::SetCurrenSourceInfo(tvin_info_t sig_info)
{
    mCurrentSignalInfo.trans_fmt = sig_info.trans_fmt;
    mCurrentSignalInfo.fmt = sig_info.fmt;
    mCurrentSignalInfo.status = sig_info.status;
    mCurrentSignalInfo.cfmt = sig_info.cfmt;
    mCurrentSignalInfo.hdr_info= sig_info.hdr_info;
    mCurrentSignalInfo.fps = sig_info.fps;
    mCurrentSignalInfo.is_dvi = sig_info.is_dvi;
    mCurrentSignalInfo.aspect_ratio = sig_info.aspect_ratio;

    return 0;
}

tvin_info_t CTv::GetCurrentSourceInfo(void)
{
    if (mCurrentSource == SOURCE_DTV) {//DTV
        //todo
    } else {//Other source
        //todo
    }
    LOGD("mCurrentSource = %d, trans_fmt is %d,fmt is %d, status is %d.\n",
            mCurrentSource, mCurrentSignalInfo.trans_fmt, mCurrentSignalInfo.fmt, mCurrentSignalInfo.status);
    return mCurrentSignalInfo;
}

int CTv::setTvObserver ( TvIObserver *ob )
{
    LOGD("%s\n", __FUNCTION__);
    if (ob != NULL) {
        mpObserver = ob;
    } else {
        LOGD("%s: Observer is NULL.\n", __FUNCTION__);
    }

    return 0;
}

int CTv::SetEDIDData(tv_source_input_t source, char *data)
{
    char edidData[REAL_EDID_DATA_SIZE];
    memcpy(edidData, data, REAL_EDID_DATA_SIZE);
    return mpHDMIRxManager->HdmiRxEdidDataSwitch(1, edidData);
}

int CTv::GetEDIDData(tv_source_input_t source, char *data)
{
    char edidData[REAL_EDID_DATA_SIZE];
    memset(edidData, 0, REAL_EDID_DATA_SIZE);
    //ReadDataFromFile(HDMI_EDID14_FILE_PATH, 0, REAL_EDID_DATA_SIZE, edidData);
    memcpy(data, edidData, REAL_EDID_DATA_SIZE);
    return 0;
}

int CTv::LoadEdidData(int isNeedBlackScreen, int isDolbyVisionEnable)
{
    if (isNeedBlackScreen  == 1) {
        mpTvin->Tvin_StopDecoder();
    }

    bool isDolbyVisionKoExist = isFileExist(DOLBY_VISION_TV_KO_PATH) || isFileExist(DOLBY_VISION_STB_KO_PATH);
    LOGD("%s: isDolbyVisionKoExist = %d, isDolbyVisionEnable = %d.\n", __FUNCTION__, isDolbyVisionKoExist, isDolbyVisionEnable);
    char edidLoadBuf[6 * REAL_EDID_DATA_SIZE] = {0};
    char edidReadBuf[REAL_EDID_DATA_SIZE] = {0};
    char edidFileName[100] = {0};
    int loadNum = 1;
    const char *edidFilePath = ConfigGetStr(CFG_SECTION_HDMI, CFG_HDMI_EDID_FILE_PATH, "/vendor/etc/tvconfig");
    if (isDolbyVisionKoExist && (isDolbyVisionEnable == 1)) {
        for (loadNum=1;loadNum<7;loadNum++) {
            LOGD("%s: load dolby vision EDID!\n", __FUNCTION__);
            if (loadNum%2 != 0) {
                sprintf(edidFileName, "%s/port%d_14_dv.bin", edidFilePath, loadNum/2 + 1);
            } else {
                sprintf(edidFileName, "%s/port%d_20_dv.bin", edidFilePath, loadNum/2);
            }
            memset(edidReadBuf, 0, REAL_EDID_DATA_SIZE);
            ReadDataFromFile(edidFileName, 0, REAL_EDID_DATA_SIZE, edidReadBuf);
            memcpy(edidLoadBuf + (loadNum-1)*REAL_EDID_DATA_SIZE, edidReadBuf, REAL_EDID_DATA_SIZE);
        }
    } else {
        for (loadNum=1;loadNum<7;loadNum++) {
            LOGD("%s: load ordinary EDID!\n", __FUNCTION__);
            if (loadNum%2 != 0) {
                sprintf(edidFileName, "%s/port%d_14.bin", edidFilePath, loadNum/2 + 1);
            } else {
                sprintf(edidFileName, "%s/port%d_20.bin", edidFilePath, loadNum/2);
            }
            memset(edidReadBuf, 0, REAL_EDID_DATA_SIZE);
            ReadDataFromFile(edidFileName, 0, REAL_EDID_DATA_SIZE, edidReadBuf);
            memcpy(edidLoadBuf + (loadNum-1)*REAL_EDID_DATA_SIZE, edidReadBuf, REAL_EDID_DATA_SIZE);
        }
    }

    int ret = mpHDMIRxManager->HdmiRxEdidDataSwitch(6, edidLoadBuf);
    if (ret == 0) {
        tvin_port_id_t portId = TVIN_PORT_ID_MAX;
        int edidSetValue = 0;
        int portEdidVersion = 0;
        for (int i=SOURCE_HDMI1;i<SOURCE_VGA;i++) {
            portId = mpTvin->Tvin_GetHdmiPortIdBySourceInput((tv_source_input_t)i);
            portEdidVersion = GetEdidVersion((tv_source_input_t)i);
            edidSetValue |=  (portEdidVersion << (4*portId - 4));
        }
        ret = mpHDMIRxManager->HdmiRxEdidVerSwitch(edidSetValue);
    } else {
        LOGE("%s failed!\n", __FUNCTION__);
    }

    return ret;

}

int CTv::SetEdidVersion(tv_source_input_t source, tv_hdmi_edid_version_t edidVer)
{
    LOGD("%s: setSource: %d, setVersion: %d\n", __FUNCTION__, source, edidVer);
    int ret = -1;
    if (mCurrentSource == source) {
        tv_hdmi_edid_version_t currentVersion = (tv_hdmi_edid_version_t)GetEdidVersion(source);
        if (currentVersion != edidVer) {
            mpTvin->Tvin_StopDecoder();
            tvin_port_id_t portId = TVIN_PORT_ID_MAX;
            int edidSetValue = 0;
            int portEdidVersion = 0;
            for (int i=SOURCE_HDMI1;i<SOURCE_VGA;i++) {
                portId = mpTvin->Tvin_GetHdmiPortIdBySourceInput((tv_source_input_t)i);
                if (i == source) {
                    portEdidVersion = edidVer;
                } else {
                    portEdidVersion = GetEdidVersion((tv_source_input_t)i);
                }
                edidSetValue |=  (portEdidVersion << (4*portId - 4));
            }
            ret = mpHDMIRxManager->HdmiRxEdidVerSwitch(edidSetValue);
            if (ret < 0) {
                LOGE("%s failed.\n", __FUNCTION__);
                ret = -1;
            } else {
                //TODO:add user setting read/write flow
                switch (source) {
                case SOURCE_HDMI1:
                    Hdmi1CurrentEdidVer = edidVer;
                    break;
                case SOURCE_HDMI2:
                    Hdmi2CurrentEdidVer = edidVer;
                    break;
                case SOURCE_HDMI3:
                    Hdmi3CurrentEdidVer = edidVer;
                    break;
                case SOURCE_HDMI4:
                    Hdmi4CurrentEdidVer = edidVer;
                    break;
                default:
                    LOGD("%s: not hdmi source.\n", __FUNCTION__);
                    break;
                }
            }
        } else {
            LOGD("%s: same EDID version, no need set.\n", __FUNCTION__);
            ret = 0;
        }
    } else {
        LOGD("%s: not current sorce, don't set.\n", __FUNCTION__);
        ret = 0;
    }

    return ret;

}

int CTv::GetEdidVersion(tv_source_input_t source)
{
    //TODO:add user setting read/write flow
    int retValue = HDMI_EDID_VER_14;
    switch (source) {
    case SOURCE_HDMI1:
        retValue = Hdmi1CurrentEdidVer;
        break;
    case SOURCE_HDMI2:
        retValue = Hdmi2CurrentEdidVer;
        break;
    case SOURCE_HDMI3:
        retValue = Hdmi3CurrentEdidVer;
        break;
    case SOURCE_HDMI4:
        retValue = Hdmi4CurrentEdidVer;
        break;
    default:
        LOGD("%s: not hdmi source.\n", __FUNCTION__);
        break;
    }
    return retValue;
}

int CTv::SetVdinWorkMode(vdin_work_mode_t vdinWorkMode)
{
    mVdinWorkMode = vdinWorkMode;
    return 0;
}

void CTv::onSigToStable()
{
    LOGD("%s: mVdinWorkMode is %d\n", __FUNCTION__, mVdinWorkMode);
    //start decoder
    if (mVdinWorkMode == VDIN_WORK_MODE_VFM) {
        mpTvin->Tvin_StartDecoder(mCurrentSignalInfo);
    } else {
        LOGD("%s: not VFM mode.\n", __FUNCTION__);
    }

    //send signal to apk
    TvEvent::SignalDetectEvent event;
    event.mSourceInput = mCurrentSource;
    event.mFmt = mCurrentSignalInfo.fmt;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mStatus = mCurrentSignalInfo.status;
    event.mDviFlag = mCurrentSignalInfo.is_dvi;
    sendTvEvent(event);
}

void CTv::onSigToUnstable()
{
    mpTvin->Tvin_StopDecoder();

    LOGD("signal to Unstable!\n");
    //To do
}

void CTv::onSigToUnSupport()
{
    LOGD("%s\n", __FUNCTION__);

    mpTvin->Tvin_StopDecoder();
    TvEvent::SignalDetectEvent event;
    event.mSourceInput = mCurrentSource;
    event.mFmt = mCurrentSignalInfo.fmt;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mStatus = mCurrentSignalInfo.status;
    event.mDviFlag = mCurrentSignalInfo.is_dvi;
    sendTvEvent(event);
    //To do
}

void CTv::onSigToNoSig()
{
    LOGD("%s\n", __FUNCTION__);

    mpTvin->Tvin_StopDecoder();
    TvEvent::SignalDetectEvent event;
    event.mSourceInput = mCurrentSource;
    event.mFmt = mCurrentSignalInfo.fmt;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mStatus = mCurrentSignalInfo.status;
    event.mDviFlag = mCurrentSignalInfo.is_dvi;
    sendTvEvent (event);
    //To do
}

int CTv::sendTvEvent(CTvEvent &event)
{
    LOGD("%s\n", __FUNCTION__);

    if (mpObserver != NULL) {
        mpObserver->onTvEvent(event);
    } else {
        LOGD("%s: Observer is NULL.\n", __FUNCTION__);
    }

    return 0;
}

int CTv::setVideoLayerStatus(videolayer_status_t status)
{
    LOGD("%s: status = %d\n", __FUNCTION__, status);

    char temp[8] = {0};
    sprintf(temp, "%d", status);
    return tvWriteSysfs(VIDEO_DISABLE_VIDEO, temp);
}

#ifdef HAVE_AUDIO
int CTv::mapSourcetoAudiotupe(tv_source_input_t dest_source)
{
    int ret = -1;
    switch (dest_source) {
        case SOURCE_TV:
        case SOURCE_DTV:
            ret = AUDIO_DEVICE_IN_TV_TUNER;
            break;
        case SOURCE_AV1:
        case SOURCE_AV2:
            ret = AUDIO_DEVICE_IN_LINE;
            break;
        case SOURCE_HDMI1:
        case SOURCE_HDMI2:
        case SOURCE_HDMI3:
        case SOURCE_HDMI4:
            ret = AUDIO_DEVICE_IN_HDMI;
            break;
        case SOURCE_SPDIF:
            ret = AUDIO_DEVICE_IN_SPDIF;
            break;
        default:
            ret = AUDIO_DEVICE_IN_LINE;
            break;
    }
    return ret;
}
#endif
