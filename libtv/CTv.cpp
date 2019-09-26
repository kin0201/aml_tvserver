#include <stdint.h>
#include <string.h>

#include "CTv.h"
#include "TvConfigManager.h"

CTv::CTv()
{
    printf("start CTV!\n");
    mpObserver = NULL;
    mpTvin = CTvin::getInstance();
    mpTvin->Tvin_AddVideoPath(TV_PATH_VDIN_AMLVIDEO2_PPMGR_DEINTERLACE_AMVIDEO);
    mpHDMIRxManager = new CHDMIRxManager();
    //LoadConfigFile(CONFIG_FILE_PATH_DEF);
    InitCurrenSourceInfo();
    printf("start setObserver!\n");
    mTvDevicesPollDetect.setObserver(this);
    printf("start startDetect!\n");
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
    //UnloadConfigFile();
}

int CTv::StartTv(tv_source_input_t source)
{
    int ret = -1;
    tvin_port_t source_port = mpTvin->Tvin_GetSourcePortBySourceInput(source);
    ret = mpTvin->Tvin_OpenPort(source_port);
    mCurrentSource = source;
    return ret;
}

int CTv::StopTv(tv_source_input_t source)
{
    InitCurrenSourceInfo();
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
        printf("same source,no need switch!\n");
    }

    return 0;
}

void CTv::onSourceConnect(int source, int connect_status)
{
    printf("onSourceConnect: source = %d, connect_status= %d!\n", source, connect_status);
    //To do
    TvEvent::SourceConnectEvent event;
    event.mSourceInput = source;
    event.connectionState = connect_status;
    sendTvEvent(event);
}

void CTv::onVdinSignalChange(void)
{
    printf("onVdinSignalChange!\n");
    int ret = mpTvin->Tvin_GetSignalInfo(&mCurrentSignalInfo);
    if (ret < 0) {
        printf("Get Signal Info error!\n");
        InitCurrenSourceInfo();
    }

    printf("sig_fmt is %d, status is %d, isDVI is %d, hdr_info is 0x%x\n",
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
        InitCurrenSourceInfo();
    }
}

void CTv::InitCurrenSourceInfo(void)
{
    mCurrentSignalInfo.trans_fmt = TVIN_TFMT_2D;
    mCurrentSignalInfo.fmt = TVIN_SIG_FMT_NULL;
    mCurrentSignalInfo.status = TVIN_SIG_STATUS_NULL;
    mCurrentSignalInfo.cfmt = TVIN_COLOR_FMT_MAX;
    mCurrentSignalInfo.hdr_info= 0;
    mCurrentSignalInfo.fps = 60;
    mCurrentSignalInfo.is_dvi = 0;
}

tvin_info_t CTv::GetCurrentSourceInfo(void)
{
    if (mCurrentSource == SOURCE_DTV) {//DTV
        //todo
    } else {//Other source
        //todo
    }
    printf("mCurrentSource = %d, trans_fmt is %d,fmt is %d, status is %d",
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
    return mpHDMIRxManager->HdmiRxEdidUpdate(data);
}

int CTv::getEDIDData(char *data)
{
    return 0;
}

void CTv::onSigToStable()
{
    printf("onSigToStable!\n");
    //load pq
    //auto set fps
    mpTvin->Tvin_StartDecoder(mCurrentSignalInfo);
    //send signal to apk

    TvEvent::SignalInfoEvent event;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mFmt = mCurrentSignalInfo.fmt;
    event.mStatus = mCurrentSignalInfo.status;
    event.mDviFlag = mCurrentSignalInfo.is_dvi;
    sendTvEvent(event);
}

void CTv::onSigToUnstable()
{
    mpTvin->Tvin_StopDecoder();

    printf("signal to Unstable!\n");
    //To do
}

void CTv::onSigToUnSupport()
{
    mpTvin->Tvin_StopDecoder();
    printf("signal to UnSupport!\n");
    TvEvent::SignalInfoEvent event;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mFmt = mCurrentSignalInfo.fmt;
    event.mStatus = mCurrentSignalInfo.status;
    event.mDviFlag = mCurrentSignalInfo.is_dvi;
    sendTvEvent(event);
    //To do
}

void CTv::onSigToNoSig()
{
    mpTvin->Tvin_StopDecoder();
    printf("signal to NoSig!\n");
    TvEvent::SignalInfoEvent event;
    event.mTrans_fmt = mCurrentSignalInfo.trans_fmt;
    event.mFmt = mCurrentSignalInfo.fmt;
    event.mStatus = mCurrentSignalInfo.status;
    event.mDviFlag = mCurrentSignalInfo.is_dvi;
    sendTvEvent (event);
    //To do
}

int CTv::sendTvEvent(CTvEvent event)
{
    if (mpObserver != NULL) {
        mpObserver->onTvEvent(event);
    }

    return 0;
}
