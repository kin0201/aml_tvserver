#define LOG_MOUDLE_TAG "TV"
#define LOG_CLASS_TAG "CTv"

#include <stdint.h>
#include <string.h>

#include "CTv.h"
#include "tvutils.h"
#include "TvConfigManager.h"
#include "CTvLog.h"
#include "CPQControl.h"

CTv::CTv()
{
    mpObserver = NULL;
    mpTvin = CTvin::getInstance();
    mpTvin->Tvin_AddVideoPath(TV_PATH_VDIN_AMLVIDEO2_PPMGR_DEINTERLACE_AMVIDEO);
    mpHDMIRxManager = new CHDMIRxManager();
    LoadConfigFile(CONFIG_FILE_PATH_DEF);
    tvin_info_t signalInfo;
    signalInfo.trans_fmt = TVIN_TFMT_2D;
    signalInfo.fmt = TVIN_SIG_FMT_NULL;
    signalInfo.status = TVIN_SIG_STATUS_NULL;
    signalInfo.cfmt = TVIN_COLOR_FMT_MAX;
    signalInfo.hdr_info= 0;
    signalInfo.fps = 60;
    signalInfo.is_dvi = 0;
    SetCurrenSourceInfo(signalInfo);
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
    tvin_port_t source_port = mpTvin->Tvin_GetSourcePortBySourceInput(source);
    ret = mpTvin->Tvin_OpenPort(source_port);
    mCurrentSource = source;
    return ret;
}

int CTv::StopTv(tv_source_input_t source)
{
    LOGD("%s: source = %d!\n", __FUNCTION__, source);

    mCurrentSource = SOURCE_MPEG;
    tvin_info_t tempSignalInfo;
    tempSignalInfo.trans_fmt = TVIN_TFMT_2D;
    tempSignalInfo.fmt = TVIN_SIG_FMT_NULL;
    tempSignalInfo.status = TVIN_SIG_STATUS_NULL;
    tempSignalInfo.cfmt = TVIN_COLOR_FMT_MAX;
    tempSignalInfo.hdr_info= 0;
    tempSignalInfo.fps = 60;
    tempSignalInfo.is_dvi = 0;
    SetCurrenSourceInfo(tempSignalInfo);

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

void CTv::onVdinSignalChange(void)
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

    if ((mCurrentSource == SOURCE_MPEG)
        || (mCurrentSource != SOURCE_MPEG && mCurrentSignalInfo.status == TVIN_SIG_STATUS_STABLE)) {
        source_input_param_t source_input_param;
        source_input_param.source_input = mCurrentSource;
        source_input_param.sig_fmt = mCurrentSignalInfo.fmt;
        source_input_param.trans_fmt = mCurrentSignalInfo.trans_fmt;
        CPQControl::GetInstance()->SetCurrentSourceInputInfo(source_input_param);
    }

    return 0;
}

tvin_info_t CTv::GetCurrentSourceInfo(void)
{
    if (mCurrentSource == SOURCE_DTV) {//DTV
        //todo
    } else {//Other source
        //todo
    }
    LOGD("mCurrentSource = %d, trans_fmt is %d,fmt is %d, status is %d",
            mCurrentSource, mCurrentSignalInfo.trans_fmt, mCurrentSignalInfo.fmt, mCurrentSignalInfo.status);
    return mCurrentSignalInfo;
}

int CTv::setTvObserver ( TvIObserver *ob )
{
    mpObserver = ob;
    return 0;
}

int CTv::UpdateEDID(tv_source_input_t source, char *data)
{
    char edidData[REAL_EDID_DATA_SIZE];
    memcpy(edidData, data, REAL_EDID_DATA_SIZE);
    return mpHDMIRxManager->HdmiRxEdidUpdate(edidData);
}

int CTv::getEDIDData(tv_source_input_t source, char *data)
{
    char edidData[REAL_EDID_DATA_SIZE];
    memset(edidData, 0, REAL_EDID_DATA_SIZE);
    tvReadSysfs(HDMI_EDID14_FILE_PATH, edidData);
    memcpy(data, edidData, REAL_EDID_DATA_SIZE);
    return 0;
}

void CTv::onSigToStable()
{
    LOGD("%s\n", __FUNCTION__);
    //start decoder
    mpTvin->Tvin_StartDecoder(mCurrentSignalInfo);

    //send signal to apk
    TvEvent::SignalDetectEvent event;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mFmt = mCurrentSignalInfo.fmt;
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
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mFmt = mCurrentSignalInfo.fmt;
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
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mFmt = mCurrentSignalInfo.fmt;
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
    }

    return 0;
}
