/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_MOUDLE_TAG "PQ"
#define LOG_CLASS_TAG "CPQControl"

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>

#include "CPQControl.h"

#define PI 3.14159265358979
CPQControl *CPQControl::mInstance = NULL;
pthread_mutex_t PqControlMutex = PTHREAD_MUTEX_INITIALIZER;

CPQControl *CPQControl::GetInstance()
{
    if (NULL == mInstance)
        mInstance = new CPQControl();
    return mInstance;
}

CPQControl::CPQControl()
{
    mInitialized = false;
    mAmvideoFd = -1;
    mDiFd = -1;
    mbDtvKitEnable = false;
    //Load config file
    mPQConfigFile = CConfigFile::GetInstance();
    mPQConfigFile->LoadFromFile(PQ_CONFIG_DEFAULT_PATH);
    SetFlagByCfg();
    //open DB
    mPQdb = new CPQdb();
    int ret = mPQdb->openPqDB(PQ_DB_DEFAULT_PATH);
    if (ret != 0) {
        LOGE("open pq DB failed!\n");
    } else {
        LOGD("open pq DB success!\n");
    }
    //open overscan DB
    if (mbCpqCfg_seperate_db_enable) {
        mpOverScandb = new COverScandb();
        ret = mpOverScandb->openOverScanDB(OVERSCAN_DB_DEFAULT_PATH);
        if (ret != 0) {
            LOGE("open overscan DB failed!\n");
        } else {
            LOGD("open overscan DB success!\n");
        }
    }
    //SSM file check
    mSSMAction = SSMAction::getInstance();
    mSSMAction->setObserver(this);
    mSSMAction->init();
    //open vpp module
    mAmvideoFd = VPPOpenModule();
    if (mAmvideoFd < 0) {
        LOGE("Open PQ module failed!\n");
    } else {
        LOGD("Open PQ module success!\n");
    }
    //open DI module
    mDiFd = DIOpenModule();
    if (mDiFd < 0) {
        LOGE("Open DI module failed!\n");
    } else {
        LOGD("Open DI module success!\n");
    }
    //init source
    mCurentSourceInputInfo.source_input = SOURCE_MPEG;
    mCurentSourceInputInfo.sig_fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
    mCurentSourceInputInfo.trans_fmt = TVIN_TFMT_2D;
    mSourceInputForSaveParam = SOURCE_MPEG;
    mCurrentHdrStatus = false;
    //Set DNLP
    if (mbCpqCfg_dnlp_enable) {
        Cpq_SetDNLPStatus(VE_DNLP_STATE_ON);
    } else {
        Cpq_SetDNLPStatus(VE_DNLP_STATE_OFF);
    }
    //set backlight
    BacklightInit();
    //Vframe size
    mCDevicePollCheckThread.setObserver(this);
    mCDevicePollCheckThread.StartCheckThread();
    mInitialized = true;
    //auto backlight
    if (isFileExist(LDIM_PATH)) {
        SetDynamicBacklight((Dynamic_backlight_status_t)GetDynamicBacklight(), 1);
    } else if (isFileExist(BACKLIGHT_PATH)) {//local diming or pwm
        mDynamicBackLight.setObserver(this);
        mDynamicBackLight.startDected();
    } else {
        LOGD("No auto backlight moudle!\n");
    }
}

CPQControl::~CPQControl()
{
    //close moduel
    VPPCloseModule();
    //close DI module
    DICloseModule();

    if (mSSMAction!= NULL) {
        delete mSSMAction;
        mSSMAction = NULL;
    }

    if (mPQdb != NULL) {
        //closed DB
        mPQdb->closeDb();

        delete mPQdb;
        mPQdb = NULL;
    }

    if (mpOverScandb != NULL) {
        mpOverScandb->closeDb();

        delete mpOverScandb;
        mpOverScandb = NULL;
    }

    if (mPQConfigFile != NULL) {
        delete mPQConfigFile;
        mPQConfigFile = NULL;
    }
}

int CPQControl::VPPOpenModule(void)
{
    mAmvideoFd = mCDevicePollCheckThread.HDR_fd;
    if (mAmvideoFd < 0) {
        mAmvideoFd = open(VPP_DEV_PATH, O_RDWR);
        if (mAmvideoFd < 0) {
            LOGE("Open PQ module, error(%s)!\n", strerror(errno));
            return -1;
        }
    } else {
        LOGD("vpp OpenModule has been opened before!\n");
    }

    return mAmvideoFd;
}

int CPQControl::VPPCloseModule(void)
{
    if (mAmvideoFd >= 0) {
        close ( mAmvideoFd);
        mAmvideoFd = -1;
    }
    return 0;
}

int CPQControl::VPPDeviceIOCtl(int request, ...)
{
    int ret = -1;
    va_list ap;
    void *arg;
    va_start(ap, request);
    arg = va_arg ( ap, void * );
    va_end(ap);
    ret = ioctl(mAmvideoFd, request, arg);
    return ret;
}

int CPQControl::DIOpenModule(void)
{
    if (mDiFd < 0) {
        mDiFd = open(DI_DEV_PATH, O_RDWR);

        LOGD("DI OpenModule path: %s", DI_DEV_PATH);

        if (mDiFd < 0) {
            LOGE("Open DI module, error(%s)!\n", strerror(errno));
            return -1;
        }
    }

    return mDiFd;
}

int CPQControl::DICloseModule(void)
{
    if (mDiFd>= 0) {
        close ( mDiFd);
        mDiFd = -1;
    }
    return 0;
}

int CPQControl::DIDeviceIOCtl(int request, ...)
{
    int tmp_ret = -1;
    va_list ap;
    void *arg;
    va_start(ap, request);
    arg = va_arg ( ap, void * );
    va_end(ap);
    tmp_ret = ioctl(mDiFd, request, arg);
    return tmp_ret;
}

int CPQControl::AFEDeviceIOCtl ( int request, ... )
{
    int tmp_ret = -1;
    int afe_dev_fd = -1;
    va_list ap;
    void *arg;

    afe_dev_fd = open ( AFE_DEV_PATH, O_RDWR );

    if ( afe_dev_fd >= 0 ) {
        va_start ( ap, request );
        arg = va_arg ( ap, void * );
        va_end ( ap );

        tmp_ret = ioctl ( afe_dev_fd, request, arg );

        close(afe_dev_fd);
        return tmp_ret;
    } else {
        LOGE ( "Open tvafe module error(%s).\n", strerror ( errno ));
        return -1;
    }
}

void CPQControl::onVframeSizeChange()
{
    source_input_param_t new_source_input_param;
    if (((mCurentSourceInputInfo.source_input == SOURCE_DTV) ||
        (mCurentSourceInputInfo.source_input == SOURCE_MPEG))) {
        new_source_input_param.sig_fmt = getVideoResolutionToFmt();
        LOGD("%s: sig_fmt = 0x%x(%d).\n", __FUNCTION__, new_source_input_param.sig_fmt, new_source_input_param.sig_fmt);
        new_source_input_param.source_input = mCurentSourceInputInfo.source_input;
        new_source_input_param.trans_fmt = mCurentSourceInputInfo.trans_fmt;
        SetCurrentSourceInputInfo(new_source_input_param);
    }
}

tvin_sig_fmt_t CPQControl::getVideoResolutionToFmt()
{
    int fd = -1;
    char buf[32] = {0};
    tvin_sig_fmt_t sig_fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;

    fd = open(SYS_VIDEO_FRAME_HEIGHT, O_RDONLY);
    if (fd < 0) {
        LOGE("[%s] open: %s error!\n", __FUNCTION__, SYS_VIDEO_FRAME_HEIGHT);
        return sig_fmt;
    }

    if (read(fd, buf, sizeof(buf)) >0) {
        int height = atoi(buf);
        if (height <= 576) {
            sig_fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
        } else if (height > 576 && height <= 720) {
            sig_fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
        } else if (height > 720 && height <= 1088) {
            sig_fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
        } else {
            sig_fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
        }
    } else {
        LOGE("[%s] read error!\n", __FUNCTION__);
    }
    close(fd);

    return sig_fmt;
}

void CPQControl::onHDRStatusChange()
{
    LOGD("%s!\n", __FUNCTION__);
    if ((mCurentSourceInputInfo.source_input >= SOURCE_HDMI1)
      &&(mCurentSourceInputInfo.source_input <= SOURCE_HDMI4)) {
        SetCurrentSourceInputInfo(mCurentSourceInputInfo);
    }
}

void CPQControl::onTXStatusChange()
{
    LOGD("%s!\n", __FUNCTION__);

    output_type_t mode = GetTxOutPutMode();
    mPQdb->mOutPutType = mode;
    if ((mode == OUTPUT_TYPE_MAX) || !(isCVBSParamValid())) {
        LOGD("%s: no need load TX pq param!\n", __FUNCTION__);
    } else {
        LoadPQSettings();
    }
}

int CPQControl::LoadPQSettings()
{
    int ret = 0;
    const char *config_value;
    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_ALL_PQ_MOUDLE_ENABLE, "enable");
    if (strcmp(config_value, "disable") == 0) {
        LOGD("All PQ moudle disabled!\n");
        ve_pq_moudle_state_t state = VE_PQ_MOUDLE_OFF;
        ret = VPPDeviceIOCtl(AMVECM_IOC_S_PQ_STATUE, &state);
    } else {
        LOGD("source_input: %d, sig_fmt: 0x%x(%d), trans_fmt: 0x%x\n", mCurentSourceInputInfo.source_input,
                 mCurentSourceInputInfo.sig_fmt, mCurentSourceInputInfo.sig_fmt, mCurentSourceInputInfo.trans_fmt);

        ret |= Cpq_SetXVYCCMode(VPP_XVYCC_MODE_STANDARD, mCurentSourceInputInfo);

        ret |= Cpq_SetDIModuleParam(mCurentSourceInputInfo);

        vpp_picture_mode_t pqmode = (vpp_picture_mode_t)GetPQMode();
        ret |= Cpq_SetPQMode(pqmode, mCurentSourceInputInfo);

        if (mInitialized) {//don't load gamma in device turn on
            vpp_gamma_curve_t GammaLevel = (vpp_gamma_curve_t)GetGammaValue();
            ret |= SetGammaValue(GammaLevel, 1);
        }

        vpp_color_basemode_t baseMode = GetColorBaseMode();
        ret |= SetColorBaseMode(baseMode, 1);

        vpp_color_temperature_mode_t temp_mode = (vpp_color_temperature_mode_t)GetColorTemperature();
        if (temp_mode != VPP_COLOR_TEMPERATURE_MODE_USER) {
            Cpq_CheckColorTemperatureParamAlldata(mCurentSourceInputInfo);
            ret |= SetColorTemperature((int)temp_mode, 1);
        } else {
            tcon_rgb_ogo_t param;
            memset(&param, 0, sizeof(tcon_rgb_ogo_t));
            if (Cpq_GetColorTemperatureUser(mCurentSourceInputInfo.source_input, &param) == 0) {
                ret |= Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, R_GAIN, 1, param.r_gain);
                ret |= Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, G_GAIN, 1, param.g_gain);
                ret |= Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, B_GAIN, 1, param.b_gain);
                ret |= Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, R_POST_OFFSET, 1, param.r_post_offset);
                ret |= Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, G_POST_OFFSET, 1, param.g_post_offset);
                ret |= Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, B_POST_OFFSET, 1, param.b_post_offset);
            }
        }

        int DnlpLevel = GetDnlpMode();
        ret |= SetDnlpMode(DnlpLevel);

        int LocalContrastMode = GetLocalContrastMode();
        ret |= SetLocalContrastMode((local_contrast_mode_t)LocalContrastMode, 1);

        vpp_display_mode_t display_mode = (vpp_display_mode_t)GetDisplayMode();
        ret |= SetDisplayMode(display_mode, 1);
    }
    return ret;
}

int CPQControl::Cpq_LoadRegs(am_regs_t regs)
{
    if (regs.length == 0) {
        LOGD("%s--Regs is NULL!\n", __FUNCTION__);
        return -1;
    }

    int count_retry = 20;
    int ret = 0;
    while (count_retry) {
        ret = VPPDeviceIOCtl(AMVECM_IOC_LOAD_REG, &regs);
        if (ret < 0) {
            LOGE("%s, error(%s), errno(%d)\n", __FUNCTION__, strerror(errno), errno);
            if (errno == EBUSY) {
                LOGE("%s, %s, retry...\n", __FUNCTION__, strerror(errno));
                count_retry--;
                continue;
            }
        }
        break;
    }

    return ret;
}

int CPQControl::Cpq_LoadDisplayModeRegs(ve_pq_load_t regs)
{
    if (regs.length == 0) {
        LOGD("%s--Regs is NULL!\n", __FUNCTION__);
        return -1;
    }

    int count_retry = 20;
    int ret = 0;
    while (count_retry) {
        ret = VPPDeviceIOCtl(AMVECM_IOC_SET_OVERSCAN, &regs);
        if (ret < 0) {
            LOGE("%s, error(%s), errno(%d)\n", __FUNCTION__, strerror(errno), errno);
            if (errno == EBUSY) {
                LOGE("%s, %s, retry...\n", __FUNCTION__, strerror(errno));
                count_retry--;
                continue;
            }
        }
        break;
    }

    return ret;
}

int CPQControl::DI_LoadRegs(am_pq_param_t di_regs)
{
    int count_retry = 20;
    int ret = 0;
    while (count_retry) {
        ret = DIDeviceIOCtl(AMDI_IOC_SET_PQ_PARM, &di_regs);
        if (ret < 0) {
            LOGE("%s, error(%s), errno(%d)\n", __FUNCTION__, strerror(errno), errno);
            if (errno == EBUSY) {
                LOGE("%s, %s, retry...\n", __FUNCTION__, strerror(errno));
                count_retry--;
                continue;
            }
        }
        break;
    }

    return ret;
}

int CPQControl::LoadCpqLdimRegs()
{
    bool ret = 0;
    int ldFd = -1;

    if (!isFileExist(LDIM_PATH)) {
        LOGD("Don't have ldim module!\n");
    } else {
        ldFd = open(LDIM_PATH, O_RDWR);

        if (ldFd < 0) {
            LOGE("Open ldim module, error(%s)!\n", strerror(errno));
            ret = -1;
        } else {
            vpu_ldim_param_s *ldim_param_temp = new vpu_ldim_param_s();

            if (ldim_param_temp) {
                if (!mPQdb->PQ_GetLDIM_Regs(ldim_param_temp) || ioctl(ldFd, LDIM_IOC_PARA, ldim_param_temp) < 0) {
                   LOGE("LoadCpqLdimRegs, error(%s)!\n", strerror(errno));
                   ret = -1;
                }

                delete ldim_param_temp;
            }
                close (ldFd);
        }
    }

    return ret;
}

int CPQControl::Cpq_LoadBasicRegs(source_input_param_t source_input_param)
{
    int ret = 0;
    if (mbCpqCfg_blackextension_enable) {
        ret |= SetBlackExtensionParam(source_input_param);
    } else {
        LOGD("%s: BlackExtension moudle disabled!\n", __FUNCTION__);
    }

    if (mbCpqCfg_sharpness0_enable) {
        ret |= Cpq_SetSharpness0FixedParam(source_input_param);
        ret |= Cpq_SetSharpness0VariableParam(source_input_param);
    } else {
        LOGD("%s: Sharpness0 moudle disabled!\n", __FUNCTION__);
    }

    if (mbCpqCfg_sharpness1_enable) {
        ret |= Cpq_SetSharpness1FixedParam(source_input_param);
        ret |= Cpq_SetSharpness1VariableParam(source_input_param);
    } else {
        LOGD("%s: Sharpness1 moudle disabled!\n", __FUNCTION__);
    }

    if (mbCpqCfg_brightness_contrast_enable) {
        ret |= Cpq_SetBrightnessBasicParam(source_input_param);
        ret |= Cpq_SetContrastBasicParam(source_input_param);
    } else {
        LOGD("%s: brightness and contrast moudle disabled!\n", __FUNCTION__);
    }

    if (mbCpqCfg_satuation_hue_enable) {
        ret |= Cpq_SetSaturationBasicParam(source_input_param);
        ret |= Cpq_SetHueBasicParam(source_input_param);
    } else {
        LOGD("%s: satuation and hue moudle disabled!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::BacklightInit(void)
{
    int ret = 0;
    int backlight = GetBacklight();
    ret = SetBacklight(backlight, 1);

    return ret;
}

int CPQControl::Cpq_SetDIModuleParam(source_input_param_t source_input_param)
{
    int ret = -1;
    am_regs_t regs;
    am_pq_param_t di_regs;
    memset(&regs, 0x0, sizeof(am_regs_t));
    memset(&di_regs, 0x0, sizeof(am_pq_param_t));
    if (mbCpqCfg_di_enable) {
        if (mPQdb->PQ_GetDIParams(source_input_param, &regs) == 0) {
            di_regs.table_name |= TABLE_NAME_DI;
        } else {
            LOGE("%s GetDIParams failed!\n",__FUNCTION__);
        }
    } else {
        LOGD("DI moudle disabled!\n");
    }

    if (mbCpqCfg_mcdi_enable) {
        if (mPQdb->PQ_GetMCDIParams(VPP_MCDI_MODE_STANDARD, source_input_param, &regs) == 0) {
            di_regs.table_name |= TABLE_NAME_MCDI;
        } else {
            LOGE("%s GetMCDIParams failed!\n",__FUNCTION__);
        }
    } else {
        LOGD("Mcdi moudle disabled!\n");
    }

    if (mbCpqCfg_deblock_enable) {
        if (mPQdb->PQ_GetDeblockParams(VPP_DEBLOCK_MODE_MIDDLE, source_input_param, &regs) == 0) {
            di_regs.table_name |= TABLE_NAME_DEBLOCK;
        } else {
            LOGE("%s GetDeblockParams failed!\n",__FUNCTION__);
        }
    } else {
        LOGD("Deblock moudle disabled!\n");
    }

    if (mbCpqCfg_nr_enable) {
        vpp_noise_reduction_mode_t nr_mode =(vpp_noise_reduction_mode_t)GetNoiseReductionMode();
        if (mPQdb->PQ_GetNR2Params(nr_mode, source_input_param,  &regs) == 0) {
            di_regs.table_name |= TABLE_NAME_NR;
        } else {
            LOGE("%s GetNR2Params failed!\n",__FUNCTION__);
        }
    } else {
        LOGD("NR moudle disabled!\n");
    }

    if (mbCpqCfg_demoSquito_enable) {
        if (mPQdb->PQ_GetDemoSquitoParams(source_input_param, &regs) == 0) {
            di_regs.table_name |= TABLE_NAME_DEMOSQUITO;
        } else {
            LOGE("%s GetDemoSquitoParams failed!\n",__FUNCTION__);
        }
    } else {
        LOGD("DemoSquito moudle disabled!\n");
    }

    if (regs.length != 0) {
        di_regs.table_len = regs.length;
        am_reg_t tmp_buf[regs.length];
        for (unsigned int i=0;i<regs.length;i++) {
              tmp_buf[i].addr = regs.am_reg[i].addr;
              tmp_buf[i].mask = regs.am_reg[i].mask;
              tmp_buf[i].type = regs.am_reg[i].type;
              tmp_buf[i].val  = regs.am_reg[i].val;
        }

        di_regs.table_ptr = (long long)tmp_buf;

        ret = DI_LoadRegs(di_regs);
    } else {
        LOGE("%s: get DI Module Param failed!\n",__FUNCTION__);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }
    return ret;
}

int CPQControl::SetPQMode(int pq_mode, int is_save , int is_autoswitch)
{
    LOGD("%s, source: %d, pq_mode: %d\n", __FUNCTION__, mCurentSourceInputInfo.source_input, pq_mode);
    int ret = -1;

    int cur_mode = GetPQMode();
    if (cur_mode == pq_mode) {
        LOGD("Same PQ mode,no need set again!\n");
        ret = 0;
    } else {
        if (is_autoswitch) {
            ret = Cpq_SetPQMode((vpp_picture_mode_t)pq_mode, mCurentSourceInputInfo);
        } else {
            ret = Cpq_SetPQMode((vpp_picture_mode_t)pq_mode, mCurentSourceInputInfo);
        }
    }

    if ((ret == 0) && (is_save == 1)) {
        SavePQMode(pq_mode);
        if ((mCurentSourceInputInfo.source_input >= SOURCE_HDMI1) &&
            (mCurentSourceInputInfo.source_input <= SOURCE_HDMI4)) {
            vpp_display_mode_t display_mode = (vpp_display_mode_t)GetDisplayMode();
            ret = SetDisplayMode(display_mode, 1);
        }
    }

    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::GetPQMode(void)
{
    int mode = VPP_PICTURE_MODE_STANDARD;
    mSSMAction->SSMReadPictureMode(mSourceInputForSaveParam, &mode);
    if (mode < VPP_PICTURE_MODE_STANDARD || mode >= VPP_PICTURE_MODE_MAX) {
        mode = VPP_PICTURE_MODE_STANDARD;
    }

    LOGD("%s, source: %d, mode: %d\n", __FUNCTION__, mSourceInputForSaveParam, mode);
    return mode;
}

int CPQControl::SavePQMode(int pq_mode)
{
    int ret = -1;
    LOGD("%s, source: %d, mode: %d\n", __FUNCTION__, mSourceInputForSaveParam, pq_mode);
    ret = mSSMAction->SSMSavePictureMode(mSourceInputForSaveParam, pq_mode);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }
    return ret;
}

int CPQControl::Cpq_SetPQMode(vpp_picture_mode_t pq_mode, source_input_param_t source_input_param)
{
    int ret = -1;
    vpp_pq_para_t pq_para;
    if (pq_mode == VPP_PICTURE_MODE_USER) {
        mSSMAction->SSMReadBrightness(mSourceInputForSaveParam, &pq_para.brightness);
        mSSMAction->SSMReadContrast(mSourceInputForSaveParam, &pq_para.contrast);
        mSSMAction->SSMReadSaturation(mSourceInputForSaveParam, &pq_para.saturation);
        mSSMAction->SSMReadHue(mSourceInputForSaveParam, &pq_para.hue);
        mSSMAction->SSMReadSharpness(mSourceInputForSaveParam, &pq_para.sharpness);

        Cpq_LoadBasicRegs(source_input_param);
        ret = SetPQParams(pq_para, source_input_param);
    } else {
        ret = GetPQParams(source_input_param, pq_mode, &pq_para);
        if (ret < 0) {
            LOGE("%s: GetPQParams failed!\n", __FUNCTION__);
        } else {
            Cpq_LoadBasicRegs(source_input_param);
            ret = SetPQParams(pq_para, source_input_param);
        }
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::SetPQParams(vpp_pq_para_t pq_para, source_input_param_t source_input_param)
{
    int ret = 0;
    int hue_level = 0, hue = 50, saturation = 50;
    if (((source_input_param.source_input == SOURCE_TV) ||
          (source_input_param.source_input == SOURCE_AV1) ||
          (source_input_param.source_input == SOURCE_AV2)) &&
        ((source_input_param.sig_fmt == TVIN_SIG_FMT_CVBS_NTSC_M) ||
         (source_input_param.sig_fmt == TVIN_SIG_FMT_CVBS_NTSC_443))) {
        hue_level = 100 - pq_para.hue;
    } else {
        hue_level = 50;
    }

    ret = mPQdb->PQ_GetHueParams(source_input_param, hue_level, &hue);
    if (ret == 0) {
        ret = mPQdb->PQ_GetSaturationParams(source_input_param, pq_para.saturation, &saturation);
        if (ret == 0) {
            ret = Cpq_SetVideoSaturationHue(saturation, hue);
        } else {
            LOGE("%s: PQ_GetSaturationParams failed!\n", __FUNCTION__);
        }
    } else {
        LOGE("%s: PQ_GetHueParams failed!\n", __FUNCTION__);
    }

    ret |= Cpq_SetSharpness(pq_para.sharpness, source_input_param);
    ret |= Cpq_SetBrightness(pq_para.brightness, source_input_param);
    ret |= Cpq_SetContrast(pq_para.contrast, source_input_param);

    return ret;
}

int CPQControl::GetPQParams(source_input_param_t source_input_param, vpp_picture_mode_t pq_mode, vpp_pq_para_t *pq_para)
{
    int ret = -1;
    if (pq_para == NULL) {
        return ret;
    }

    if (mbCpqCfg_seperate_db_enable) {
        ret = mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, pq_mode, pq_para);
    } else {
        ret = mPQdb->PQ_GetPQModeParams(source_input_param.source_input, pq_mode, pq_para);
    }

    if (ret != 0) {
        LOGE("GetPQParams, PQ_GetPQModeParams failed!\n");
        return -1;
    }

    return 0;
}

//color temperature
int CPQControl::SetColorTemperature(int temp_mode, int is_save, rgb_ogo_type_t rgb_ogo_type, int value)
{
    int ret = -1;
    LOGD("%s: source:%d, mode: %d\n", __FUNCTION__, mCurentSourceInputInfo.source_input, temp_mode);
    if (mbCpqCfg_whitebalance_enable) {
        if (temp_mode == VPP_COLOR_TEMPERATURE_MODE_USER) {
            ret = Cpq_SetColorTemperatureUser(mCurentSourceInputInfo.source_input, rgb_ogo_type, is_save, value);
        } else {
            ret = Cpq_SetColorTemperatureWithoutSave((vpp_color_temperature_mode_t)temp_mode, mCurentSourceInputInfo.source_input);
        }

        if ((ret == 0) && (is_save == 1)) {
            ret = SaveColorTemperature((vpp_color_temperature_mode_t)temp_mode);
        }
    } else {
        LOGD("whitebalance moudle disabled!\n");
        ret = 0;
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetColorTemperature(void)
{
    int mode = VPP_COLOR_TEMPERATURE_MODE_STANDARD;
    mSSMAction->SSMReadColorTemperature(mSourceInputForSaveParam, &mode);
    if (mode < VPP_COLOR_TEMPERATURE_MODE_STANDARD || mode > VPP_COLOR_TEMPERATURE_MODE_USER) {
        mode = VPP_COLOR_TEMPERATURE_MODE_STANDARD;
    }

    LOGD("%s: source: %d, mode: %d!\n",__FUNCTION__, mSourceInputForSaveParam, mode);
    return mode;
}

int CPQControl::SaveColorTemperature(int temp_mode)
{
    LOGD("%s, source: %d, mode = %d\n", __FUNCTION__, mSourceInputForSaveParam, temp_mode);
    int ret = mSSMAction->SSMSaveColorTemperature(mSourceInputForSaveParam, temp_mode);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

tcon_rgb_ogo_t CPQControl::GetColorTemperatureUserParam(void) {
    tcon_rgb_ogo_t param;
    memset(&param, 0, sizeof(tcon_rgb_ogo_t));
    Cpq_GetColorTemperatureUser(mCurentSourceInputInfo.source_input, &param);
    return param;
}

int CPQControl::Cpq_SetColorTemperatureWithoutSave(vpp_color_temperature_mode_t Tempmode, tv_source_input_t tv_source_input __unused)
{
    tcon_rgb_ogo_t rgbogo;

    GetColorTemperatureParams(Tempmode, &rgbogo);

    if (GetEyeProtectionMode(mCurentSourceInputInfo.source_input))//if eye protection mode is enable, b_gain / 2.
        rgbogo.b_gain /= 2;

    return Cpq_SetRGBOGO(&rgbogo);
}

int CPQControl::Cpq_CheckColorTemperatureParamAlldata(source_input_param_t source_input_param)
{
    int ret= -1;
    unsigned short ret1 = 0, ret2 = 0;

    ret = Cpq_CheckTemperatureDataLable();
    ret1 = Cpq_CalColorTemperatureParamsChecksum();
    ret2 = Cpq_GetColorTemperatureParamsChecksum();

    if (ret && (ret1 == ret2)) {
        LOGD("%s, color temperature param lable & checksum ok.\n",__FUNCTION__);
        if (Cpq_CheckColorTemperatureParams() == 0) {
            LOGD("%s, color temperature params check failed.\n", __FUNCTION__);
            Cpq_RestoreColorTemperatureParamsFromDB(source_input_param);
         }
    } else {
        LOGD("%s, color temperature param data error.\n", __FUNCTION__);
        Cpq_SetTemperatureDataLable();
        Cpq_RestoreColorTemperatureParamsFromDB(source_input_param);
    }

    return 0;
}

unsigned short CPQControl::Cpq_CalColorTemperatureParamsChecksum(void)
{
    unsigned char data_buf[SSM_CR_RGBOGO_LEN];
    unsigned short sum = 0;
    int cnt;

    mSSMAction->SSMReadRGBOGOValue(0, SSM_CR_RGBOGO_LEN, data_buf);

    for (cnt = 0; cnt < SSM_CR_RGBOGO_LEN; cnt++) {
        sum += data_buf[cnt];
    }

    LOGD("%s, sum = 0x%X.\n", __FUNCTION__, sum);

    return sum;
}

int CPQControl::Cpq_SetColorTemperatureParamsChecksum(void)
{
    int ret = 0;
    USUC usuc;

    usuc.s = Cpq_CalColorTemperatureParamsChecksum();

    LOGD("%s, sum = 0x%X.\n", __FUNCTION__, usuc.s);

    ret |= mSSMAction->SSMSaveRGBOGOValue(SSM_CR_RGBOGO_LEN, SSM_CR_RGBOGO_CHKSUM_LEN, usuc.c);

    return ret;
}

unsigned short CPQControl::Cpq_GetColorTemperatureParamsChecksum(void)
{
    USUC usuc;

    mSSMAction->SSMReadRGBOGOValue(SSM_CR_RGBOGO_LEN, SSM_CR_RGBOGO_CHKSUM_LEN, usuc.c);

    LOGD("%s, sum = 0x%X.\n", __FUNCTION__, usuc.s);

    return usuc.s;
}

int CPQControl::Cpq_SetColorTemperatureUser(tv_source_input_t source_input, rgb_ogo_type_t rgb_ogo_type, int is_save, int value)
{
    LOGD("%s: type: %d, value: %u!\n", __FUNCTION__, rgb_ogo_type, value);
    int ret = -1;
    tcon_rgb_ogo_t rgbogo;
    memset(&rgbogo, 0, sizeof(tcon_rgb_ogo_t));
    ret = Cpq_GetColorTemperatureUser(source_input, &rgbogo);

    switch (rgb_ogo_type)
    {
        case R_GAIN:
            rgbogo.r_gain = (unsigned)value;
        break;
        case G_GAIN:
            rgbogo.g_gain = (unsigned)value;
        break;
        case B_GAIN:
            rgbogo.b_gain = (unsigned)value;
        break;
        case R_POST_OFFSET:
            rgbogo.r_post_offset = value;
        break;
        case G_POST_OFFSET:
            rgbogo.g_post_offset = value;
        break;
        case B_POST_OFFSET:
            rgbogo.b_post_offset = value;
        break;
        default:
            ret = -1;
        break;
    }

    if (GetEyeProtectionMode(source_input) == 1) {
        LOGD("eye protection mode is enable!\n");
        rgbogo.b_gain /= 2;
    }

    if (ret != -1) {
       ret = Cpq_SetRGBOGO(&rgbogo);
    }

    if ((ret != -1) && (is_save == 1)) {
        ret = Cpq_SaveColorTemperatureUser(source_input, rgb_ogo_type, value);
    }

    if (ret < 0) {
        LOGD("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }
    return ret;
}

int CPQControl::Cpq_GetColorTemperatureUser(tv_source_input_t source_input __unused, tcon_rgb_ogo_t* p_tcon_rgb_ogo)
{
    int ret = 0;
    if (p_tcon_rgb_ogo != NULL) {
        p_tcon_rgb_ogo->en = 1;
        p_tcon_rgb_ogo->r_pre_offset = 0;
        p_tcon_rgb_ogo->g_pre_offset = 0;
        p_tcon_rgb_ogo->b_pre_offset = 0;
        ret |= mSSMAction->SSMReadRGBGainRStart(0, &p_tcon_rgb_ogo->r_gain);
        ret |= mSSMAction->SSMReadRGBGainGStart(0, &p_tcon_rgb_ogo->g_gain);
        ret |= mSSMAction->SSMReadRGBGainBStart(0, &p_tcon_rgb_ogo->b_gain);
        ret |= mSSMAction->SSMReadRGBPostOffsetRStart(0, &p_tcon_rgb_ogo->r_post_offset);
        ret |= mSSMAction->SSMReadRGBPostOffsetGStart(0, &p_tcon_rgb_ogo->g_post_offset);
        ret |= mSSMAction->SSMReadRGBPostOffsetBStart(0, &p_tcon_rgb_ogo->b_post_offset);
    } else {
        LOGD("%s: buf is null!\n", __FUNCTION__);
        ret = -1;
    }

    if (ret < 0) {
        LOGD("%s failed!\n", __FUNCTION__);
        ret = -1;
    }

    return ret;
}

int CPQControl::Cpq_SaveColorTemperatureUser(tv_source_input_t source_input __unused, rgb_ogo_type_t rgb_ogo_type, int value)
{
    LOGD("%s: rgb_ogo_type[%d]:[%d]", __FUNCTION__, rgb_ogo_type, value);

    int ret = 0;
    switch (rgb_ogo_type)
    {
        case R_GAIN:
            ret |= mSSMAction->SSMSaveRGBGainRStart(0, (unsigned)value);
        break;
        case G_GAIN:
            ret |= mSSMAction->SSMSaveRGBGainGStart(0, (unsigned)value);
        break;
        case B_GAIN:
            ret |= mSSMAction->SSMSaveRGBGainBStart(0, (unsigned)value);
        break;
        case R_POST_OFFSET:
            ret |= mSSMAction->SSMSaveRGBPostOffsetRStart(0, value);
        break;
        case G_POST_OFFSET:
            ret |= mSSMAction->SSMSaveRGBPostOffsetGStart(0, value);
        break;
        case B_POST_OFFSET:
            ret |= mSSMAction->SSMSaveRGBPostOffsetBStart(0, value);
        break;
        default:
            ret = -1;
        break;
    }

    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_RestoreColorTemperatureParamsFromDB(source_input_param_t source_input_param)
{
    int i = 0;
    tcon_rgb_ogo_t rgbogo;

    for (i = 0; i < 3; i++) {
        mPQdb->PQ_GetColorTemperatureParams((vpp_color_temperature_mode_t) i, source_input_param, &rgbogo);
        SaveColorTemperatureParams((vpp_color_temperature_mode_t) i, rgbogo);
    }

    Cpq_SetColorTemperatureParamsChecksum();

    return 0;
}

int CPQControl::Cpq_CheckTemperatureDataLable(void)
{
    USUC usuc;
    USUC ret;

    mSSMAction->SSMReadRGBOGOValue(SSM_CR_RGBOGO_LEN - 2, 2, ret.c);

    usuc.c[0] = 0x55;
    usuc.c[1] = 0xAA;

    if ((usuc.c[0] == ret.c[0]) && (usuc.c[1] == ret.c[1])) {
        LOGD("%s, lable ok.\n", __FUNCTION__);
        return 1;
    } else {
        LOGE("%s, lable error.\n", __FUNCTION__);
        return 0;
    }
}

int CPQControl::Cpq_SetTemperatureDataLable(void)
{
    USUC usuc;
    int ret = 0;

    usuc.c[0] = 0x55;
    usuc.c[1] = 0xAA;

    ret = mSSMAction->SSMSaveRGBOGOValue(SSM_CR_RGBOGO_LEN - 2, 2, usuc.c);

    return ret;
}

int CPQControl::SetColorTemperatureParams(vpp_color_temperature_mode_t Tempmode, tcon_rgb_ogo_t params)
{
    SaveColorTemperatureParams(Tempmode, params);
    Cpq_SetColorTemperatureParamsChecksum();

    return 0;
}

int CPQControl::GetColorTemperatureParams(vpp_color_temperature_mode_t Tempmode, tcon_rgb_ogo_t *params)
{
    SUC suc;
    USUC usuc;
    int ret = 0;
    if (VPP_COLOR_TEMPERATURE_MODE_STANDARD == Tempmode) { //standard
        ret |= mSSMAction->SSMReadRGBOGOValue(0, 2, usuc.c);
        params->en = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(2, 2, suc.c);
        params->r_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(4, 2, suc.c);
        params->g_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(6, 2, suc.c);
        params->b_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(8, 2, usuc.c);
        params->r_gain = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(10, 2, usuc.c);
        params->g_gain = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(12, 2, usuc.c);
        params->b_gain = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(14, 2, suc.c);
        params->r_post_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(16, 2, suc.c);
        params->g_post_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(18, 2, suc.c);
        params->b_post_offset = suc.s;
    } else if (VPP_COLOR_TEMPERATURE_MODE_WARM == Tempmode) { //warm
        ret |= mSSMAction->SSMReadRGBOGOValue(20, 2, usuc.c);
        params->en = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(22, 2, suc.c);
        params->r_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(24, 2, suc.c);
        params->g_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(26, 2, suc.c);
        params->b_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(28, 2, usuc.c);
        params->r_gain = usuc.s;
        ret |= mSSMAction->SSMReadRGBOGOValue(30, 2, usuc.c);
        params->g_gain = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(32, 2, usuc.c);
        params->b_gain = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(34, 2, suc.c);
        params->r_post_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(36, 2, suc.c);
        params->g_post_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(38, 2, suc.c);
        params->b_post_offset = suc.s;
    } else if (VPP_COLOR_TEMPERATURE_MODE_COLD == Tempmode) { //cool
        ret |= mSSMAction->SSMReadRGBOGOValue(40, 2, usuc.c);
        params->en = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(42, 2, suc.c);
        params->r_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(44, 2, suc.c);
        params->g_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(46, 2, suc.c);
        params->b_pre_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(48, 2, usuc.c);
        params->r_gain = usuc.s;
        ret |= mSSMAction->SSMReadRGBOGOValue(50, 2, usuc.c);
        params->g_gain = usuc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(52, 2, usuc.c);
        params->b_gain = usuc.s;
        ret |= mSSMAction->SSMReadRGBOGOValue(54, 2, suc.c);
        params->r_post_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(56, 2, suc.c);
        params->g_post_offset = suc.s;

        ret |= mSSMAction->SSMReadRGBOGOValue(58, 2, suc.c);
        params->b_post_offset = suc.s;
    }

    LOGD("%s, rgain[%d], ggain[%d],bgain[%d],roffset[%d],goffset[%d],boffset[%d]\n", __FUNCTION__,
         params->r_gain, params->g_gain, params->b_gain, params->r_post_offset,
         params->g_post_offset, params->b_post_offset);

    return ret;
}

int CPQControl::SaveColorTemperatureParams(vpp_color_temperature_mode_t Tempmode, tcon_rgb_ogo_t params)
{
    SUC suc;
    USUC usuc;
    int ret = 0;

    if (VPP_COLOR_TEMPERATURE_MODE_STANDARD == Tempmode) { //standard
        usuc.s = params.en;
        ret |= mSSMAction->SSMSaveRGBOGOValue(0, 2, usuc.c);

        suc.s = params.r_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(2, 2, suc.c);

        suc.s = params.g_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(4, 2, suc.c);

        suc.s = params.b_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(6, 2, suc.c);

        usuc.s = params.r_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(8, 2, usuc.c);

        usuc.s = params.g_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(10, 2, usuc.c);

        usuc.s = params.b_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(12, 2, usuc.c);

        suc.s = params.r_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(14, 2, suc.c);

        suc.s = params.g_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(16, 2, suc.c);

        suc.s = params.b_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(18, 2, suc.c);
    } else if (VPP_COLOR_TEMPERATURE_MODE_WARM == Tempmode) { //warm
        usuc.s = params.en;
        ret |= mSSMAction->SSMSaveRGBOGOValue(20, 2, usuc.c);

        suc.s = params.r_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(22, 2, suc.c);

        suc.s = params.g_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(24, 2, suc.c);
        suc.s = params.b_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(26, 2, suc.c);

        usuc.s = params.r_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(28, 2, usuc.c);

        usuc.s = params.g_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(30, 2, usuc.c);

        usuc.s = params.b_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(32, 2, usuc.c);

        suc.s = params.r_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(34, 2, suc.c);

        suc.s = params.g_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(36, 2, suc.c);

        suc.s = params.b_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(38, 2, suc.c);
    } else if (VPP_COLOR_TEMPERATURE_MODE_COLD == Tempmode) { //cool
        usuc.s = params.en;
        ret |= mSSMAction->SSMSaveRGBOGOValue(40, 2, usuc.c);

        suc.s = params.r_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(42, 2, suc.c);

        suc.s = params.g_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(44, 2, suc.c);

        suc.s = params.b_pre_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(46, 2, suc.c);

        usuc.s = params.r_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(48, 2, usuc.c);

        usuc.s = params.g_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(50, 2, usuc.c);

        usuc.s = params.b_gain;
        ret |= mSSMAction->SSMSaveRGBOGOValue(52, 2, usuc.c);

        suc.s = params.r_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(54, 2, suc.c);

        suc.s = params.g_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(56, 2, suc.c);

        suc.s = params.b_post_offset;
        ret |= mSSMAction->SSMSaveRGBOGOValue(58, 2, suc.c);
    }

    LOGD("%s, rgain[%d], ggain[%d],bgain[%d],roffset[%d],goffset[%d],boffset[%d]\n", __FUNCTION__,
         params.r_gain, params.g_gain, params.b_gain, params.r_post_offset,
         params.g_post_offset, params.b_post_offset);
    return ret;
}

int CPQControl::Cpq_CheckColorTemperatureParams(void)
{
    int i = 0;
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    for (i = 0; i < 3; i++) {
        GetColorTemperatureParams((vpp_color_temperature_mode_t) i, &rgbogo);

        if (rgbogo.r_gain > 2047 || rgbogo.b_gain > 2047 || rgbogo.g_gain > 2047) {
            if (rgbogo.r_post_offset > 1023 || rgbogo.g_post_offset > 1023 || rgbogo.b_post_offset > 1023 ||
                rgbogo.r_post_offset < -1024 || rgbogo.g_post_offset < -1024 || rgbogo.b_post_offset < -1024) {
                return 0;
            }
        }
    }

    return 1;
}

//Brightness
int CPQControl::SetBrightness(int value, int is_save)
{
    int ret =0;
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    ret = Cpq_SetBrightness(value, mCurentSourceInputInfo);

    if ((ret == 0) && (is_save == 1)) {
        ret = SaveBrightness(value);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }
    return 0;
}

int CPQControl::GetBrightness(void)
{
    int data = 50;
    vpp_picture_mode_t pq_mode = (vpp_picture_mode_t)GetPQMode();
    if (pq_mode == VPP_PICTURE_MODE_USER) {
        mSSMAction->SSMReadBrightness(mSourceInputForSaveParam, &data);
    } else {
        vpp_pq_para_t pq_para;
        if (GetPQParams(mCurentSourceInputInfo, pq_mode, &pq_para) == 0) {
            data = pq_para.brightness;
        }
    }

    if (data < 0 || data > 100) {
        data = 50;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, data);
    return data;
}

int CPQControl::SaveBrightness(int value)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = mSSMAction->SSMSaveBrightness(mSourceInputForSaveParam, value);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetBrightnessBasicParam(source_input_param_t source_input_param)
{
    int ret = -1;
    ret = mPQdb->LoadVppBasicParam(TVPQ_DATA_BRIGHTNESS, source_input_param);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetBrightness(int value, source_input_param_t source_input_param)
{
    int ret = -1;
    int params;
    int level;
    if (mbCpqCfg_brightness_contrast_enable) {
        if (value >= 0 && value <= 100) {
            level = value;
            if (mPQdb->PQ_GetBrightnessParams(source_input_param, level, &params) == 0) {
                if (Cpq_SetVideoBrightness(params) == 0) {
                    return 0;
                }
            } else {
                LOGE("Vpp_SetBrightness, PQ_GetBrightnessParams failed!\n");
            }
        }
    } else {
        LOGD("%s: brightness and contrast moudle disabled!\n", __FUNCTION__);
        ret = 0;
    }
    return ret;
}

int CPQControl::Cpq_SetVideoBrightness(int value)
{
    LOGD("Cpq_SetVideoBrightness brightness : %d", value);
    am_pic_mode_t params;
    memset(&params, 0, sizeof(params));
    if (mbCpqCfg_brightness_withOSD) {
        params.flag |= (0x1<<1);
        params.brightness2 = value;
    } else {
        params.flag |= 0x1;
        params.brightness = value;
    }

    int ret = VPPDeviceIOCtl(AMVECM_IOC_S_PIC_MODE, &params);
    if (ret < 0) {
        LOGE("%s error: %s!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

//Contrast
int CPQControl::SetContrast(int value, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = Cpq_SetContrast(value, mCurentSourceInputInfo);
    if ((ret == 0) && (is_save == 1)) {
        ret = mSSMAction->SSMSaveContrast(mSourceInputForSaveParam, value);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetContrast(void)
{
    int data = 50;
    vpp_picture_mode_t pq_mode = (vpp_picture_mode_t)GetPQMode();
    if (pq_mode == VPP_PICTURE_MODE_USER) {
        mSSMAction->SSMReadContrast(mSourceInputForSaveParam, &data);
    } else {
        vpp_pq_para_t pq_para;
        if (GetPQParams(mCurentSourceInputInfo, pq_mode, &pq_para) == 0) {
            data = pq_para.contrast;
        }
    }

    if (data < 0 || data > 100) {
        data = 50;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, data);
    return data;
}

int CPQControl::SaveContrast(int value)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = mSSMAction->SSMSaveContrast(mSourceInputForSaveParam, value);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetContrastBasicParam(source_input_param_t source_input_param)
{
    int ret = -1;
    ret = mPQdb->LoadVppBasicParam(TVPQ_DATA_CONTRAST, source_input_param);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetContrast(int value, source_input_param_t source_input_param)
{
    int ret = -1;
    int params;
    int level;
    if (mbCpqCfg_brightness_contrast_enable) {
        if (value >= 0 && value <= 100) {
            level = value;
            if (mPQdb->PQ_GetContrastParams(source_input_param, level, &params) == 0) {
                if (Cpq_SetVideoContrast(params) == 0) {
                    return 0;
                }
            } else {
                LOGE("%s: PQ_GetContrastParams failed!\n", __FUNCTION__);
            }
        }
    } else {
        LOGD("%s: brightness and contrast moudle disabled!\n", __FUNCTION__);
        ret = 0;
    }

    return ret;
}

int CPQControl::Cpq_SetVideoContrast(int value)
{
    LOGD("Cpq_SetVideoContrast: %d", value);
    am_pic_mode_t params;
    memset(&params, 0, sizeof(params));

    if (mbCpqCfg_contrast_withOSD) {
        params.flag |= (0x1<<5);
        params.contrast2 = value;
    } else {
        params.flag |= (0x1<<4);
        params.contrast = value;
    }

    int ret = VPPDeviceIOCtl(AMVECM_IOC_S_PIC_MODE, &params);
    if (ret < 0) {
        LOGE("%s error: %s!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

//Saturation
int CPQControl::SetSaturation(int value, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = Cpq_SetSaturation(value, mCurentSourceInputInfo);
    if ((ret == 0) && (is_save == 1)) {
        ret = mSSMAction->SSMSaveSaturation(mSourceInputForSaveParam, value);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetSaturation(void)
{
    int data = 50;
    vpp_picture_mode_t pq_mode = (vpp_picture_mode_t)GetPQMode();

    if (pq_mode == VPP_PICTURE_MODE_USER) {
        mSSMAction->SSMReadSaturation(mSourceInputForSaveParam, &data);
    } else {
        vpp_pq_para_t pq_para;
        if (GetPQParams(mCurentSourceInputInfo, pq_mode, &pq_para) == 0) {
            data = pq_para.saturation;
        }
    }

    if (data < 0 || data > 100) {
        data = 50;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, data);
    return data;
}

int CPQControl::SaveSaturation(int value)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = mSSMAction->SSMSaveSaturation(mSourceInputForSaveParam, value);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetSaturationBasicParam(source_input_param_t source_input_param)
{
    int ret = -1;
    ret = mPQdb->LoadVppBasicParam(TVPQ_DATA_SATURATION, source_input_param);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetSaturation(int value, source_input_param_t source_input_param)
{
    int ret = -1;
    int saturation = 0, hue = 0;
    int satuation_level = 0, hue_level = 0;
    if (mbCpqCfg_satuation_hue_enable) {
        if (value >= 0 && value <= 100) {
            satuation_level = value;
            if (((source_input_param.source_input == SOURCE_TV) ||
                  (source_input_param.source_input == SOURCE_AV1) ||
                  (source_input_param.source_input == SOURCE_AV2)) &&
                ((source_input_param.sig_fmt == TVIN_SIG_FMT_CVBS_NTSC_M) ||
                 (source_input_param.sig_fmt == TVIN_SIG_FMT_CVBS_NTSC_443))) {
                hue_level = 100 - GetHue();
            } else {
                hue_level = 50;
            }
            ret = mPQdb->PQ_GetHueParams(source_input_param, hue_level, &hue);
            if (ret == 0) {
                ret = mPQdb->PQ_GetSaturationParams(source_input_param, satuation_level, &saturation);
                if (ret == 0) {
                    ret = Cpq_SetVideoSaturationHue(saturation, hue);
                } else {
                    LOGE("%s: PQ_GetSaturationParams failed!\n", __FUNCTION__);
                }
            } else {
                LOGE("%s: PQ_GetHueParams failed!\n", __FUNCTION__);
            }
        }
    }else {
        LOGD("%s: satuation and hue moudle disabled!\n", __FUNCTION__);
        ret = 0;
    }

    return ret;
}

//Hue
int CPQControl::SetHue(int value, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = Cpq_SetHue(value, mCurentSourceInputInfo);
    if ((ret == 0) && (is_save == 1)) {
        ret = mSSMAction->SSMSaveHue(mSourceInputForSaveParam, value);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetHue(void)
{
    int data = 50;
    vpp_picture_mode_t pq_mode = (vpp_picture_mode_t)GetPQMode();

    if (pq_mode == VPP_PICTURE_MODE_USER) {
        mSSMAction->SSMReadHue(mSourceInputForSaveParam, &data);
    } else {
        vpp_pq_para_t pq_para;
        if (GetPQParams(mCurentSourceInputInfo, pq_mode, &pq_para) == 0) {
            data = pq_para.hue;
        }
    }

    if (data < 0 || data > 100) {
        data = 50;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, data);
    return data;
}

int CPQControl::SaveHue(int value)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = mSSMAction->SSMSaveHue(mSourceInputForSaveParam, value);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetHueBasicParam(source_input_param_t source_input_param)
{
    int ret = -1;
    ret = mPQdb->LoadVppBasicParam(TVPQ_DATA_HUE, source_input_param);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetHue(int value, source_input_param_t source_input_param)
{
    int ret = -1;
    int hue_params = 0, saturation_params = 0;
    int hue_level = 0, saturation_level = 0;
    if (mbCpqCfg_satuation_hue_enable) {
        if (value >= 0 && value <= 100) {
            hue_level = 100 - value;
            ret = mPQdb->PQ_GetHueParams(source_input_param, hue_level, &hue_params);
            if (ret == 0) {
                saturation_level = GetSaturation();
                ret = mPQdb->PQ_GetSaturationParams(source_input_param, saturation_level, &saturation_params);
                if (ret == 0) {
                    ret = Cpq_SetVideoSaturationHue(saturation_params, hue_params);
                } else {
                    LOGE("PQ_GetSaturationParams failed!\n");
                }
            } else {
                LOGE("PQ_GetHueParams failed!\n");
            }
        }
    } else {
        LOGD("%s: satuation and hue moudle disabled!\n", __FUNCTION__);
        ret = 0;
    }

    return ret;
}

int CPQControl::Cpq_SetVideoSaturationHue(int satVal, int hueVal)
{
    signed long temp;
    LOGD("Cpq_SetVideoSaturationHue: %d %d", satVal, hueVal);
    am_pic_mode_t params;
    memset(&params, 0, sizeof(params));
    video_set_saturation_hue(satVal, hueVal, &temp);

    if (mbCpqCfg_hue_withOSD) {
        params.flag |= (0x1<<3);
        params.saturation_hue_post = temp;
    } else {
        params.flag |= (0x1<<2);
        params.saturation_hue = temp;
    }

    int ret = VPPDeviceIOCtl(AMVECM_IOC_S_PIC_MODE, &params);
    if (ret < 0) {
        LOGE("%s error: %s!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

void CPQControl::video_set_saturation_hue(signed char saturation, signed char hue, signed long *mab)
{
    signed short ma = (signed short) (cos((float) hue * PI / 128.0) * ((float) saturation / 128.0
                                      + 1.0) * 256.0);
    signed short mb = (signed short) (sin((float) hue * PI / 128.0) * ((float) saturation / 128.0
                                      + 1.0) * 256.0);

    if (ma > 511) {
        ma = 511;
    }

    if (ma < -512) {
        ma = -512;
    }

    if (mb > 511) {
        mb = 511;
    }

    if (mb < -512) {
        mb = -512;
    }

    *mab = ((ma & 0x3ff) << 16) | (mb & 0x3ff);
}

void CPQControl::video_get_saturation_hue(signed char *sat, signed char *hue, signed long *mab)
{
    signed long temp = *mab;
    signed int ma = (signed int) ((temp << 6) >> 22);
    signed int mb = (signed int) ((temp << 22) >> 22);
    signed int sat16 = (signed int) ((sqrt(
                                          ((float) ma * (float) ma + (float) mb * (float) mb) / 65536.0) - 1.0) * 128.0);
    signed int hue16 = (signed int) (atan((float) mb / (float) ma) * 128.0 / PI);

    if (sat16 > 127) {
        sat16 = 127;
    }

    if (sat16 < -128) {
        sat16 = -128;
    }

    if (hue16 > 127) {
        hue16 = 127;
    }

    if (hue16 < -128) {
        hue16 = -128;
    }

    *sat = (signed char) sat16;
    *hue = (signed char) hue16;
}

//sharpness
int CPQControl::SetSharpness(int value, int is_enable __unused, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = Cpq_SetSharpness(value, mCurentSourceInputInfo);
    if ((ret== 0) && (is_save == 1)) {
        ret = mSSMAction->SSMSaveSharpness(mSourceInputForSaveParam, value);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetSharpness(void)
{
    int data = 50;
    vpp_picture_mode_t pq_mode = (vpp_picture_mode_t)GetPQMode();

    if (pq_mode == VPP_PICTURE_MODE_USER) {
        mSSMAction->SSMReadSharpness(mSourceInputForSaveParam, &data);
    } else {
        vpp_pq_para_t pq_para;
        if (GetPQParams(mCurentSourceInputInfo, pq_mode, &pq_para) == 0) {
            data = pq_para.sharpness;
        }
    }

    if (data < 0 || data > 100) {
        data = 50;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, data);
    return data;
}

int CPQControl::SaveSharpness(int value)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, value);
    int ret = mSSMAction->SSMSaveSharpness(mSourceInputForSaveParam, value);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }
    return ret;
}

int CPQControl::Cpq_SetSharpness(int value, source_input_param_t source_input_param)
{
    int ret = -1;
    am_regs_t regs;
    memset(&regs, 0, sizeof(am_regs_t));
    int level;

    if (value >= 0 && value <= 100) {
        level = value;
        if (mbCpqCfg_sharpness0_enable) {
            ret = mPQdb->PQ_GetSharpness0Params(source_input_param, level, &regs);
            if (ret == 0) {
                ret = Cpq_LoadRegs(regs);
            } else {
                LOGE("%s: PQ_GetSharpness0Params failed!\n", __FUNCTION__);
            }
        } else {
            LOGD("%s: sharpness0 moudle disabled!\n", __FUNCTION__);
            ret = 0;
        }

        if (mbCpqCfg_sharpness1_enable) {
            ret = mPQdb->PQ_GetSharpness1Params(source_input_param, level, &regs);
            if (ret == 0) {
                ret = Cpq_LoadRegs(regs);
            } else {
                LOGE("%s: PQ_GetSharpness1Params failed!\n", __FUNCTION__);
            }
        } else {
            LOGD("%s: sharpness1 moudle disabled!\n", __FUNCTION__);
            ret = 0;
        }
    }else {
        LOGE("%s: invalid value!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetSharpness0FixedParam(source_input_param_t source_input_param)
{
    am_regs_t regs;
    memset(&regs, 0, sizeof(am_regs_t));
    int ret = -1;

    ret = mPQdb->PQ_GetSharpness0FixedParams(source_input_param, &regs);
    if (ret < 0) {
        LOGE("%s: PQ_GetSharpness0FixedParams failed!\n", __FUNCTION__);
    } else {
        ret = Cpq_LoadRegs(regs);
    }

    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetSharpness0VariableParam(source_input_param_t source_input_param)
{
    int ret = mPQdb->PQ_SetSharpness0VariableParams(source_input_param);
    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetSharpness1FixedParam(source_input_param_t source_input_param)
{
    am_regs_t regs;
    memset(&regs, 0, sizeof(am_regs_t));
    int ret = -1;

    ret = mPQdb->PQ_GetSharpness1FixedParams(source_input_param, &regs);
    if (ret < 0) {
        LOGE("%s: PQ_GetSharpness1FixedParams failed!\n", __FUNCTION__);
    } else {
        ret = Cpq_LoadRegs(regs);
    }

    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetSharpness1VariableParam(source_input_param_t source_input_param)
{
    int ret = mPQdb->PQ_SetSharpness1VariableParams(source_input_param);
    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

//NoiseReductionMode
int CPQControl::SetNoiseReductionMode(int nr_mode, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, nr_mode);
    int ret = Cpq_SetNoiseReductionMode((vpp_noise_reduction_mode_t)nr_mode, mCurentSourceInputInfo);
    if ((ret ==0) && (is_save == 1)) {
        ret = SaveNoiseReductionMode((vpp_noise_reduction_mode_t)nr_mode);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetNoiseReductionMode(void)
{
    int mode = VPP_NOISE_REDUCTION_MODE_MID;
    mSSMAction->SSMReadNoiseReduction(mSourceInputForSaveParam, &mode);
    if (mode < VPP_NOISE_REDUCTION_MODE_OFF || mode > VPP_NOISE_REDUCTION_MODE_AUTO) {
        mode = VPP_NOISE_REDUCTION_MODE_MID;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, mode);
    return mode;
}

int CPQControl::SaveNoiseReductionMode(int nr_mode)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, nr_mode);
    int ret = mSSMAction->SSMSaveNoiseReduction(mSourceInputForSaveParam, nr_mode);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetNoiseReductionMode(vpp_noise_reduction_mode_t nr_mode, source_input_param_t source_input_param)
{
    int ret = -1;
    am_regs_t regs;
    am_pq_param_t di_regs;
    memset(&regs, 0x0, sizeof(am_regs_t));
    memset(&di_regs, 0x0,sizeof(am_pq_param_t));

    if (mbCpqCfg_nr_enable) {
        if (mPQdb->PQ_GetNR2Params((vpp_noise_reduction_mode_t)nr_mode, source_input_param, &regs) == 0) {
            di_regs.table_name = TABLE_NAME_NR;
            di_regs.table_len = regs.length;
            am_reg_t tmp_buf[regs.length];
            for (unsigned int i=0;i<regs.length;i++) {
                  tmp_buf[i].addr = regs.am_reg[i].addr;
                  tmp_buf[i].mask = regs.am_reg[i].mask;
                  tmp_buf[i].type = regs.am_reg[i].type;
                  tmp_buf[i].val  = regs.am_reg[i].val;
            }
            di_regs.table_ptr = (long long)tmp_buf;

            ret = DI_LoadRegs(di_regs);
        } else {
            LOGE("PQ_GetNR2Params failed!\n");
        }
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

//Gamma
int CPQControl::SetGammaValue(vpp_gamma_curve_t gamma_curve, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, gamma_curve);
    int ret = -1;
    if (mbCpqCfg_gamma_enable) {
        ret = Cpq_LoadGamma(gamma_curve);
        if ((ret == 0) && (is_save == 1)) {
            ret = mSSMAction->SSMSaveGammaValue(mSourceInputForSaveParam, gamma_curve);
        }
    } else {
        LOGD("Gamma moudle disabled!\n");
        ret = 0;
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }
    return ret;
}

int CPQControl::GetGammaValue()
{
    int gammaValue = 0;
    if (mSSMAction->SSMReadGammaValue(mSourceInputForSaveParam, &gammaValue) < 0) {
        LOGE("%s, SSMReadGammaValue ERROR!!!\n", __FUNCTION__);
        return -1;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, gammaValue);
    return gammaValue;
}

int CPQControl::Cpq_LoadGamma(vpp_gamma_curve_t gamma_curve)
{
    int ret = 0;
    tcon_gamma_table_t gamma_r, gamma_g, gamma_b;

    ret |= mPQdb->PQ_GetGammaSpecialTable(gamma_curve, "Red", &gamma_r);
    ret |= mPQdb->PQ_GetGammaSpecialTable(gamma_curve, "Green", &gamma_g);
    ret |= mPQdb->PQ_GetGammaSpecialTable(gamma_curve, "Blue", &gamma_b);

    if (ret < 0) {
        LOGE("%s, PQ_GetGammaSpecialTable failed!", __FUNCTION__);
    } else {
        Cpq_SetGammaTbl_R((unsigned short *) gamma_r.data);
        Cpq_SetGammaTbl_G((unsigned short *) gamma_g.data);
        Cpq_SetGammaTbl_B((unsigned short *) gamma_b.data);
    }

    return ret;
}

int CPQControl::Cpq_SetGammaTbl_R(unsigned short red[256])
{
    struct tcon_gamma_table_s Redtbl;
    int ret = -1, i = 0;

    for (i = 0; i < 256; i++) {
        Redtbl.data[i] = red[i];
    }

    ret = VPPDeviceIOCtl(AMVECM_IOC_GAMMA_TABLE_R, &Redtbl);
    if (ret < 0) {
        LOGE("%s error(%s)!\n", __FUNCTION__, strerror(errno));
    }
    return ret;
}

int CPQControl::Cpq_SetGammaTbl_G(unsigned short green[256])
{
    struct tcon_gamma_table_s Greentbl;
    int ret = -1, i = 0;

    for (i = 0; i < 256; i++) {
        Greentbl.data[i] = green[i];
    }

    ret = VPPDeviceIOCtl(AMVECM_IOC_GAMMA_TABLE_G, &Greentbl);
    if (ret < 0) {
        LOGE("%s error(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

int CPQControl::Cpq_SetGammaTbl_B(unsigned short blue[256])
{
    struct tcon_gamma_table_s Bluetbl;
    int ret = -1, i = 0;

    for (i = 0; i < 256; i++) {
        Bluetbl.data[i] = blue[i];
    }

    ret = VPPDeviceIOCtl(AMVECM_IOC_GAMMA_TABLE_B, &Bluetbl);
    if (ret < 0) {
        LOGE("%s error(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

//Displaymode
int CPQControl::SetDisplayMode(vpp_display_mode_t display_mode, int is_save)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, display_mode);
    int ret = -1;
    if (mbCpqCfg_display_overscan_enable) {
        if ((mCurentSourceInputInfo.source_input == SOURCE_DTV) || (mCurentSourceInputInfo.source_input == SOURCE_TV)) {
            ret = Cpq_SetDisplayModeAllTiming(mCurentSourceInputInfo.source_input, display_mode);
        } else {
            ret = Cpq_SetDisplayModeAllTiming(mCurentSourceInputInfo.source_input, display_mode);
            ret = Cpq_SetDisplayModeOneTiming(mCurentSourceInputInfo.source_input, display_mode);
        }

        if ((ret == 0) && (is_save == 1))
            ret = SaveDisplayMode(display_mode);
    } else {
        LOGD("%s:Display overscan disabled!\n", __FUNCTION__);
        ret= 0;
    }

    return ret;
}

int CPQControl::GetDisplayMode()
{
    int mode = VPP_DISPLAY_MODE_169;
    mSSMAction->SSMReadDisplayMode(mSourceInputForSaveParam, &mode);
    if (mode < VPP_DISPLAY_MODE_169 || mode >= VPP_DISPLAY_MODE_MAX) {
        mode = VPP_DISPLAY_MODE_169;
    }

    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, mode);
    return mode;
}

int CPQControl::SaveDisplayMode(vpp_display_mode_t display_mode)
{
    LOGD("%s, source: %d, value = %d\n", __FUNCTION__, mSourceInputForSaveParam, display_mode);
    int ret = mSSMAction->SSMSaveDisplayMode(mSourceInputForSaveParam, (int)display_mode);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetDisplayModeOneTiming(tv_source_input_t source_input, vpp_display_mode_t display_mode)
{
    int ret = -1;
    tvin_cutwin_t cutwin;
    if (mbCpqCfg_seperate_db_enable) {
        ret = mpOverScandb->PQ_GetOverscanParams(mCurentSourceInputInfo, display_mode, &cutwin);
    } else {
        ret = mPQdb->PQ_GetOverscanParams(mCurentSourceInputInfo, display_mode, &cutwin);
    }

    if (ret == 0) {
        int ScreenModeValue = Cpq_GetScreenModeValue(display_mode);
        if (source_input == SOURCE_MPEG) {//MPEG
            //DtvKit AFD or Local video
            if ((mbDtvKitEnable && (ScreenModeValue == SCREEN_MODE_NORMAL)) ||
                (!mbDtvKitEnable)) {
                ScreenModeValue = SCREEN_MODE_FULL_STRETCH;
                cutwin.vs = 0;
                cutwin.hs = 0;
                cutwin.ve = 0;
                cutwin.he = 0;
            }
        } else if ((source_input >= SOURCE_HDMI1) && (source_input <= SOURCE_HDMI4) &&
                   (GetPQMode() == VPP_PICTURE_MODE_MONITOR)) {//hdmi monitor mode
                cutwin.vs = 0;
                cutwin.hs = 0;
                cutwin.ve = 0;
                cutwin.he = 0;
        }

        LOGD("%s: screenmode:%d hs:%d he:%d vs:%d ve:%d\n", __FUNCTION__, ScreenModeValue, cutwin.hs, cutwin.he, cutwin.vs, cutwin.ve);
        Cpq_SetVideoCrop(cutwin.vs, cutwin.hs, cutwin.ve, cutwin.he);
        Cpq_SetVideoScreenMode(ScreenModeValue);
    } else {
        LOGD("PQ_GetOverscanParams failed!\n");
    }

    return ret;
}

int CPQControl::Cpq_SetDisplayModeAllTiming(tv_source_input_t source_input, vpp_display_mode_t display_mode)
{
    int i = 0, ScreenModeValue = 0;
    int ret = -1;
    ve_pq_load_t ve_pq_load_reg;
    memset(&ve_pq_load_reg, 0, sizeof(ve_pq_load_t));

    ve_pq_load_reg.param_id = TABLE_NAME_OVERSCAN;
    ve_pq_load_reg.length = SIG_TIMING_TYPE_MAX;

    ve_pq_table_t ve_pq_table[SIG_TIMING_TYPE_MAX];
    tvin_cutwin_t cutwin[SIG_TIMING_TYPE_MAX];
    memset(ve_pq_table, 0, sizeof(ve_pq_table));
    memset(cutwin, 0, sizeof(cutwin));

    tvin_sig_fmt_t sig_fmt[SIG_TIMING_TYPE_MAX];
    ve_pq_timing_type_t flag[SIG_TIMING_TYPE_MAX];
    sig_fmt[0] = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
    sig_fmt[1] = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
    sig_fmt[2] = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
    sig_fmt[3] = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
    sig_fmt[4] = TVIN_SIG_FMT_CVBS_NTSC_M;
    sig_fmt[5] = TVIN_SIG_FMT_CVBS_NTSC_443;
    sig_fmt[6] = TVIN_SIG_FMT_CVBS_PAL_I;
    sig_fmt[7] = TVIN_SIG_FMT_CVBS_PAL_M;
    sig_fmt[8] = TVIN_SIG_FMT_CVBS_PAL_60;
    sig_fmt[9] = TVIN_SIG_FMT_CVBS_PAL_CN;
    sig_fmt[10] = TVIN_SIG_FMT_CVBS_SECAM;
    sig_fmt[11] = TVIN_SIG_FMT_CVBS_NTSC_50;
    flag[0] = SIG_TIMING_TYPE_SD;
    flag[1] = SIG_TIMING_TYPE_HD;
    flag[2] = SIG_TIMING_TYPE_FHD;
    flag[3] = SIG_TIMING_TYPE_UHD;
    flag[4] = SIG_TIMING_TYPE_NTSC_M;
    flag[5] = SIG_TIMING_TYPE_NTSC_443;
    flag[6] = SIG_TIMING_TYPE_PAL_I;
    flag[7] = SIG_TIMING_TYPE_PAL_M;
    flag[8] = SIG_TIMING_TYPE_PAL_60;
    flag[9] = SIG_TIMING_TYPE_PAL_CN;
    flag[10] = SIG_TIMING_TYPE_SECAM;
    flag[11] = SIG_TIMING_TYPE_NTSC_50;

    source_input_param_t source_input_param;
    source_input_param.source_input = source_input;
    source_input_param.trans_fmt = mCurentSourceInputInfo.trans_fmt;
    ScreenModeValue = Cpq_GetScreenModeValue(display_mode);
    if (source_input == SOURCE_DTV) {//DTV
        for (i=0;i<SIG_TIMING_TYPE_NTSC_M;i++) {
            ve_pq_table[i].src_timing = (0x1<<31) | ((ScreenModeValue & 0x7f) << 24) | ((source_input & 0x7f) << 16 ) | (flag[i]);
            source_input_param.sig_fmt = sig_fmt[i];
            if (mbCpqCfg_seperate_db_enable) {
                ret = mpOverScandb->PQ_GetOverscanParams(source_input_param, display_mode, cutwin+i);
            } else {
                ret = mPQdb->PQ_GetOverscanParams(source_input_param, display_mode, cutwin+i);
            }

            if (ret == 0) {
                LOGD("signal_fmt:0x%x, screen mode:%d hs:%d he:%d vs:%d ve:%d!\n", sig_fmt[i], ScreenModeValue, cutwin[i].he, cutwin[i].hs, cutwin[i].ve, cutwin[i].vs);
                ve_pq_table[i].value1 = ((cutwin[i].he & 0xffff)<<16) | (cutwin[i].hs & 0xffff);
                ve_pq_table[i].value2 = ((cutwin[i].ve & 0xffff)<<16) | (cutwin[i].vs & 0xffff);
            } else {
                LOGD("PQ_GetOverscanParams failed!\n");
            }
        }
        ve_pq_load_reg.param_ptr = (long long)ve_pq_table;
    } else if (source_input == SOURCE_TV) {//ATV
        for (i=SIG_TIMING_TYPE_NTSC_M;i<SIG_TIMING_TYPE_MAX;i++) {
            ve_pq_table[i].src_timing = (0x1<<31) | ((ScreenModeValue & 0x7f) << 24) | ((source_input & 0x7f) << 16 ) | (flag[i]);
            source_input_param.sig_fmt = sig_fmt[i];
            if (mbCpqCfg_seperate_db_enable) {
                ret = mpOverScandb->PQ_GetOverscanParams(source_input_param, display_mode, cutwin+i);
            } else {
                ret = mPQdb->PQ_GetOverscanParams(source_input_param, display_mode, cutwin+i);
            }

            if (ret == 0) {
                LOGD("signal_fmt:0x%x, screen mode:%d hs:%d he:%d vs:%d ve:%d!\n", sig_fmt[i], ScreenModeValue, cutwin[i].he, cutwin[i].hs, cutwin[i].ve, cutwin[i].vs);
                ve_pq_table[i].value1 = ((cutwin[i].he & 0xffff)<<16) | (cutwin[i].hs & 0xffff);
                ve_pq_table[i].value2 = ((cutwin[i].ve & 0xffff)<<16) | (cutwin[i].vs & 0xffff);
            } else {
                LOGD("PQ_GetOverscanParams failed!\n");
            }
        }
        ve_pq_load_reg.param_ptr = (long long)ve_pq_table;
    } else {//HDMI && MPEG
        ve_pq_table[0].src_timing = (0x0<<31) | ((ScreenModeValue & 0x7f) << 24) | ((source_input & 0x7f) << 16 ) | (0x0);
        ve_pq_table[0].value1 = 0;
        ve_pq_table[0].value2 = 0;
        ve_pq_load_reg.param_ptr = (long long)ve_pq_table;

        ret = 0;
    }

    if (ret == 0) {
        ret = Cpq_LoadDisplayModeRegs(ve_pq_load_reg);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
        return -1;
    } else {
        LOGD("%s success!\n",__FUNCTION__);
        return 0;
    }

}

int CPQControl::Cpq_GetScreenModeValue(vpp_display_mode_t display_mode)
{
    int value = SCREEN_MODE_16_9;

    switch ( display_mode ) {
    case VPP_DISPLAY_MODE_169:
        value = SCREEN_MODE_16_9;
        break;
    case VPP_DISPLAY_MODE_MODE43:
        value = SCREEN_MODE_4_3;
        break;
    case VPP_DISPLAY_MODE_NORMAL:
        value = SCREEN_MODE_NORMAL;
        break;
    case VPP_DISPLAY_MODE_FULL:
        value = SCREEN_MODE_NONLINEAR;
        Cpq_SetNonLinearFactor(20);
        break;
    case VPP_DISPLAY_MODE_NOSCALEUP:
        value = SCREEN_MODE_NORMAL_NOSCALEUP;
        break;
    case VPP_DISPLAY_MODE_MOVIE:
    case VPP_DISPLAY_MODE_PERSON:
    case VPP_DISPLAY_MODE_CAPTION:
    case VPP_DISPLAY_MODE_CROP:
    case VPP_DISPLAY_MODE_CROP_FULL:
    case VPP_DISPLAY_MODE_ZOOM:
    default:
        value = SCREEN_MODE_FULL_STRETCH;
        break;
    }

    return value;
}

int CPQControl::Cpq_SetVideoScreenMode(int value)
{
    LOGD("Cpq_SetVideoScreenMode, value = %d\n" , value);
    char val[64] = {0};
    sprintf(val, "%d", value);
    pqWriteSys(SCREEN_MODE_PATH, val);
    return 0;
}

int CPQControl::Cpq_SetVideoCrop(int Voffset0, int Hoffset0, int Voffset1, int Hoffset1)
{
    char set_str[32];

    LOGD("Cpq_SetVideoCrop value: %d %d %d %d\n", Voffset0, Hoffset0, Voffset1, Hoffset1);
    int fd = open(CROP_PATH, O_RDWR);
    if (fd < 0) {
        LOGE("Open %s error(%s)!\n", CROP_PATH, strerror(errno));
        return -1;
    }

    memset(set_str, 0, 32);
    sprintf(set_str, "%d %d %d %d", Voffset0, Hoffset0, Voffset1, Hoffset1);
    write(fd, set_str, strlen(set_str));
    close(fd);

    return 0;
}

int CPQControl::Cpq_SetNonLinearFactor(int value)
{
    LOGD("Cpq_SetNonLinearFactor : %d\n", value);
    FILE *fp = fopen(NOLINER_FACTORY, "w");
    if (fp == NULL) {
        LOGE("Open %s error(%s)!\n", NOLINER_FACTORY, strerror(errno));
        return -1;
    }

    fprintf(fp, "%d", value);
    fclose(fp);
    fp = NULL;
    return 0;
}

//Backlight
int CPQControl::SetBacklight(int value, int is_save)
{
    int ret = -1;
    LOGD("%s: value = %d\n", __FUNCTION__, value);
    if (value < 0 || value > 100) {
        value = DEFAULT_BACKLIGHT_BRIGHTNESS;
    }

    if (isFileExist(LDIM_PATH)) {//local diming
        int temp = (value * 255 / 100);
        Cpq_SetBackLight(temp);
    }

    if (is_save == 1) {
        ret = SaveBacklight(value);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
        return -1;
    } else {
        LOGD("%s success!\n",__FUNCTION__);
        return 0;
    }

}

int CPQControl::GetBacklight(void)
{
    int data = 0;
    mSSMAction->SSMReadBackLightVal(&data);

    if (data < 0 || data > 100) {
        data = DEFAULT_BACKLIGHT_BRIGHTNESS;
    }

    return data;
}

int CPQControl::SaveBacklight(int value)
{
    int ret = -1;
    LOGD("%s: value = %d\n", __FUNCTION__, value);

    ret = mSSMAction->SSMSaveBackLightVal(value);

    return ret;
}

int CPQControl::Cpq_SetBackLight(int value)
{
    //LOGD("%s, value = %d\n",__FUNCTION__, value);
    char val[64] = {0};
    sprintf(val, "%d", value);
    return pqWriteSys(BACKLIGHT_PATH, val);
}

void CPQControl::Cpq_GetBacklight(int *value)
{
    int ret = 0;
    char buf[64] = {0};

    ret = pqReadSys(BACKLIGHT_PATH, buf, sizeof(buf));
    if (ret > 0) {
        ret = strtol(buf, NULL, 10);
    } else {
        ret = 0;
    }

    *value = ret;
}

void CPQControl::Set_Backlight(int value)
{
    Cpq_SetBackLight(value);
}

//dynamic backlight
int CPQControl::SetDynamicBacklight(Dynamic_backlight_status_t mode, int is_save)
{
    LOGD("%s, mode = %d\n",__FUNCTION__, mode);
    int ret = -1;
    if (isFileExist(LDIM_PATH)) {//local diming
        char val[64] = {0};
        sprintf(val, "%d", mode);
        pqWriteSys(LDIM_CONTROL_PATH, val);
    }

    if (is_save == 1) {
        ret = mSSMAction->SSMSaveDynamicBacklightMode(mode);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetDynamicBacklight()
{
    int ret = -1;
    int mode = -1;
    ret = mSSMAction->SSMReadDynamicBacklightMode(&mode);
    if (0 == ret) {
        return mode;
    } else {
        return ret;
    }
    LOGD("%s: value is %d\n", __FUNCTION__, mode);
}

int CPQControl::GetHistParam(ve_hist_t *hist)
{
    memset(hist, 0, sizeof(ve_hist_s));
    int ret = VPPDeviceIOCtl(AMVECM_IOC_G_HIST_AVG, hist);
    if (ret < 0) {
        //LOGE("GetAVGHistParam, error(%s)!\n", strerror(errno));
        hist->ave = -1;
    }
    return ret;
}

bool CPQControl::isFileExist(const char *file_name)
{
    struct stat tmp_st;
    int ret = -1;

    ret = stat(file_name, &tmp_st);
    if (ret != 0 ) {
       LOGE("%s, %s don't exist!\n", __FUNCTION__, file_name);
       return false;
    } else {
       return true;
    }
}

void CPQControl::GetDynamicBacklighConfig(int *thtf, int *lut_mode, int *heigh_param, int *low_param)
{
    *thtf = mPQConfigFile->GetInt(CFG_SECTION_BACKLIGHT, CFG_AUTOBACKLIGHT_THTF, 0);
    *lut_mode = mPQConfigFile->GetInt(CFG_SECTION_BACKLIGHT, CFG_AUTOBACKLIGHT_LUTMODE, 1);

    const char *buf = NULL;
    buf = mPQConfigFile->GetString(CFG_SECTION_BACKLIGHT, CFG_AUTOBACKLIGHT_LUTHIGH, NULL);
    pqTransformStringToInt(buf, heigh_param);

    buf = mPQConfigFile->GetString(CFG_SECTION_BACKLIGHT, CFG_AUTOBACKLIGHT_LUTLOW, NULL);
    pqTransformStringToInt(buf, low_param);
}

void CPQControl::GetDynamicBacklighParam(dynamic_backlight_Param_t *DynamicBacklightParam)
{
    int value = 0;
    ve_hist_t hist;
    memset(&hist, 0, sizeof(ve_hist_t));
    GetHistParam(&hist);
    DynamicBacklightParam->hist.ave = hist.ave;
    DynamicBacklightParam->hist.sum = hist.sum;
    DynamicBacklightParam->hist.width = hist.width;
    DynamicBacklightParam->hist.height = hist.height;

    Cpq_GetBacklight(&value);
    DynamicBacklightParam->CurBacklightValue = value;
    DynamicBacklightParam->UiBackLightValue = GetBacklight();
    DynamicBacklightParam->CurDynamicBacklightMode = (Dynamic_backlight_status_t)GetDynamicBacklight();
    DynamicBacklightParam->VideoStatus = GetVideoPlayStatus();
}

int CPQControl::GetVideoPlayStatus(void)
{
    int curVideoState = 0;
    int offset = 0;
    char vframeMap[1024] = {0};
    char tmp[1024] = {0};
    char *findRet = NULL;
    char findStr1[20] = "provider";
    char findStr2[20] = "ionvideo";
    char findStr3[20] = "deinterlace(1)";
    int readRet =  pqReadSys(SYSFS_VFM_MAP_PATH, tmp, sizeof(tmp));
    strcpy(vframeMap, tmp);
    if (readRet > 0) {
        findRet = strstr(vframeMap, findStr1);
        if (findRet) {
            offset = findRet - vframeMap;
            memset(tmp, 0, sizeof(tmp));
            strncpy(tmp, vframeMap, offset);
            if (strstr(tmp, findStr2) || strstr(tmp, findStr3)) {
                curVideoState = 1;
            } else {
                curVideoState = 0;
            }
        }
    }

    //LOGD("%s: curVideoState = %d!\n",__FUNCTION__, curVideoState);
    return curVideoState;
}

int CPQControl::SetLocalContrastMode(local_contrast_mode_t mode, int is_save)
{
    LOGD("%s: mode is %d!\n",__FUNCTION__, mode);
    int ret = -1;
    if (mbCpqCfg_local_contrast_enable) {
        ret = Cpq_SetLocalContrastMode(mode);
        if ((ret == 0) && (is_save == 1)) {
            ret = SaveLocalContrastMode(mode);
        }
    } else {
        LOGD("%s: local contrast moudle disabled!\n",__FUNCTION__);
        ret = 0;
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }
    return ret;
}

int CPQControl::GetLocalContrastMode(void)
{
    int mode = LOCAL_CONTRAST_MODE_MID;
    int ret = mSSMAction->SSMReadLocalContrastMode(mCurentSourceInputInfo.source_input, &mode);
    if (0 == ret) {
        LOGD("%s: mode is %d\n", __FUNCTION__, mode);
    } else {
        LOGE("%s failed!\n", __FUNCTION__);
    }

    return mode;
}

int CPQControl::SaveLocalContrastMode(local_contrast_mode_t mode)
{
    int ret = -1;
    LOGD("%s: mode = %d\n", __FUNCTION__, mode);

    ret = mSSMAction->SSMSaveLocalContrastMode(mCurentSourceInputInfo.source_input, mode);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SetLocalContrastMode(local_contrast_mode_t mode)
{
    int ret = -1;
    ve_lc_curve_parm_t lc_param;
    am_regs_t regs;
    memset(&lc_param, 0x0, sizeof(ve_lc_curve_parm_t));
    memset(&regs, 0x0, sizeof(am_regs_t));

    ret = mPQdb->PQ_GetLocalContrastNodeParams(mCurentSourceInputInfo, mode, &lc_param);
    if (ret == 0 ) {
        ret = VPPDeviceIOCtl(AMVECM_IOC_S_LC_CURVE, &lc_param);
        if (ret == 0) {
            ret = mPQdb->PQ_GetLocalContrastRegParams(mCurentSourceInputInfo, mode, &regs);
            if (ret == 0) {
                ret = Cpq_LoadRegs(regs);
            } else {
                LOGE("%s: PQ_GetLocalContrastRegParams failed!\n", __FUNCTION__ );
            }
        }
    } else {
        LOGE("%s: PQ_GetLocalContrastNodeParams failed!\n", __FUNCTION__ );
    }

    return ret;
}

int CPQControl::SetBlackExtensionParam(source_input_param_t source_input_param)
{
    am_regs_t regs;
    memset(&regs, 0, sizeof(am_regs_t));
    int ret = -1;

    ret = mPQdb->PQ_GetBlackExtensionParams(source_input_param, &regs);
    if (ret < 0) {
        LOGE("%s: PQ_GetBlackExtensionParams failed!\n", __FUNCTION__);
    } else {
        ret = Cpq_LoadRegs(regs);
    }

    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

//PQ Factory
int CPQControl::FactoryResetPQMode(void)
{
    if (mbCpqCfg_seperate_db_enable) {
        mpOverScandb->PQ_ResetAllPQModeParams();
    } else {
        mPQdb->PQ_ResetAllPQModeParams();
    }
    return 0;
}

int CPQControl::FactoryResetColorTemp(void)
{
    mPQdb->PQ_ResetAllColorTemperatureParams();
    return 0;
}

int CPQControl::FactorySetPQMode_Brightness(source_input_param_t source_input_param, int pq_mode, int brightness)
{
    int ret = -1;
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.brightness = brightness;
            if (mpOverScandb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.brightness = brightness;
            if (mPQdb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    }

    return ret;
}

int CPQControl::FactoryGetPQMode_Brightness(source_input_param_t source_input_param, int pq_mode)
{
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.brightness = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.brightness = -1;
        }
    }
    return pq_para.brightness;
}

int CPQControl::FactorySetPQMode_Contrast(source_input_param_t source_input_param, int pq_mode, int contrast)
{
    int ret = -1;
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.contrast = contrast;
            if (mpOverScandb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.contrast = contrast;
            if (mPQdb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    }

    return ret;
}

int CPQControl::FactoryGetPQMode_Contrast(source_input_param_t source_input_param, int pq_mode)
{
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.contrast = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.contrast = -1;
        }
    }

    return pq_para.contrast;
}

int CPQControl::FactorySetPQMode_Saturation(source_input_param_t source_input_param, int pq_mode, int saturation)
{
    int ret = -1;
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.saturation = saturation;
            if (mpOverScandb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.saturation = saturation;
            if (mPQdb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    }

    return ret;
}

int CPQControl::FactoryGetPQMode_Saturation(source_input_param_t source_input_param, int pq_mode)
{
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.saturation = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.saturation = -1;
        }
    }

    return pq_para.saturation;
}

int CPQControl::FactorySetPQMode_Hue(source_input_param_t source_input_param, int pq_mode, int hue)
{
    int ret = -1;
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.hue = hue;
            if (mpOverScandb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.hue = hue;
            if (mPQdb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    }

    return ret;
}

int CPQControl::FactoryGetPQMode_Hue(source_input_param_t source_input_param, int pq_mode)
{
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.hue = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.hue = -1;
        }
    }

    return pq_para.hue;
}

int CPQControl::FactorySetPQMode_Sharpness(source_input_param_t source_input_param, int pq_mode, int sharpness)
{
    int ret = -1;
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.sharpness = sharpness;
            if (mpOverScandb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
            pq_para.sharpness = sharpness;
            if (mPQdb->PQ_SetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) == 0) {
                ret = 0;
            } else {
                ret = 1;
            }
        } else {
            ret = -1;
        }
    }
    return ret;
}

int CPQControl::FactoryGetPQMode_Sharpness(source_input_param_t source_input_param, int pq_mode)
{
    vpp_pq_para_t pq_para;
    if (mbCpqCfg_seperate_db_enable) {
        if (mpOverScandb->PQ_GetPQModeParams(source_input_param.source_input,
                                         (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
            pq_para.sharpness = -1;
        }
    } else {
        if (mPQdb->PQ_GetPQModeParams(source_input_param.source_input, (vpp_picture_mode_t) pq_mode, &pq_para) != 0) {
             pq_para.sharpness = -1;
        }
    }
    return pq_para.sharpness;
}

int CPQControl::FactorySetColorTemp_Rgain(int source_input,int colortemp_mode, int rgain)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo);
    rgbogo.r_gain = rgain;
    LOGD("%s, source[%d], colortemp_mode[%d], rgain[%d].", __FUNCTION__, source_input,
         colortemp_mode, rgain);
    rgbogo.en = 1;

    if (Cpq_SetRGBOGO(&rgbogo) == 0) {
        return 0;
    }

    return -1;
}

int CPQControl::FactorySaveColorTemp_Rgain(int source_input __unused, int colortemp_mode, int rgain)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        rgbogo.r_gain = rgain;
        return SetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, rgbogo);
    } else {
        LOGE("FactorySaveColorTemp_Rgain error!\n");
        return -1;
    }
}

int CPQControl::FactoryGetColorTemp_Rgain(int source_input __unused, int colortemp_mode)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        return rgbogo.r_gain;
    }

    LOGE("FactoryGetColorTemp_Rgain error!\n");
    return -1;
}

int CPQControl::FactorySetColorTemp_Ggain(int source_input, int colortemp_mode, int ggain)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo);
    rgbogo.g_gain = ggain;
    LOGD("%s, source[%d], colortemp_mode[%d], ggain[%d].", __FUNCTION__, source_input,
         colortemp_mode, ggain);
    rgbogo.en = 1;

    if (Cpq_SetRGBOGO(&rgbogo) == 0) {
        return 0;
    }

    return -1;
}

int CPQControl::FactorySaveColorTemp_Ggain(int source_input __unused, int colortemp_mode, int ggain)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        rgbogo.g_gain = ggain;
        return SetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, rgbogo);
    } else {
        LOGE("FactorySaveColorTemp_Ggain error!\n");
        return -1;
    }
}

int CPQControl::FactoryGetColorTemp_Ggain(int source_input __unused, int colortemp_mode)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        return rgbogo.g_gain;
    }

    LOGE("FactoryGetColorTemp_Ggain error!\n");
    return -1;
}

int CPQControl::FactorySetColorTemp_Bgain(int source_input, int colortemp_mode, int bgain)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo);
    rgbogo.b_gain = bgain;
    LOGD("%s, source[%d], colortemp_mode[%d], bgain[%d].", __FUNCTION__, source_input,
         colortemp_mode, bgain);
    rgbogo.en = 1;

    if (Cpq_SetRGBOGO(&rgbogo) == 0) {
        return 0;
    }

    return -1;
}

int CPQControl::FactorySaveColorTemp_Bgain(int source_input __unused, int colortemp_mode, int bgain)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        rgbogo.b_gain = bgain;
        return SetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, rgbogo);
    } else {
        LOGE("FactorySaveColorTemp_Bgain error!\n");
        return -1;
    }
}

int CPQControl::FactoryGetColorTemp_Bgain(int source_input __unused, int colortemp_mode)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        return rgbogo.b_gain;
    }

    LOGE("FactoryGetColorTemp_Bgain error!\n");
    return -1;
}

int CPQControl::FactorySetColorTemp_Roffset(int source_input, int colortemp_mode, int roffset)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo);
    rgbogo.r_post_offset = roffset;
    LOGD("%s, source[%d], colortemp_mode[%d], r_post_offset[%d].", __FUNCTION__, source_input,
         colortemp_mode, roffset);
    rgbogo.en = 1;

    if (Cpq_SetRGBOGO(&rgbogo) == 0) {
        return 0;
    }

    return -1;
}

int CPQControl::FactorySaveColorTemp_Roffset(int source_input __unused, int colortemp_mode, int roffset)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        rgbogo.r_post_offset = roffset;
        return SetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, rgbogo);
    } else {
        LOGE("FactorySaveColorTemp_Roffset error!\n");
        return -1;
    }
}

int CPQControl::FactoryGetColorTemp_Roffset(int source_input __unused, int colortemp_mode)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        return rgbogo.r_post_offset;
    }

    LOGE("FactoryGetColorTemp_Roffset error!\n");
    return -1;
}

int CPQControl::FactorySetColorTemp_Goffset(int source_input, int colortemp_mode, int goffset)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo);
    rgbogo.g_post_offset = goffset;
    LOGD("%s, source[%d], colortemp_mode[%d], g_post_offset[%d].", __FUNCTION__, source_input,
         colortemp_mode, goffset);
    rgbogo.en = 1;

    if (Cpq_SetRGBOGO(&rgbogo) == 0) {
        return 0;
    }

    return -1;
}

int CPQControl::FactorySaveColorTemp_Goffset(int source_input __unused, int colortemp_mode, int goffset)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        rgbogo.g_post_offset = goffset;
        return SetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, rgbogo);
    } else {
        LOGE("FactorySaveColorTemp_Goffset error!\n");
        return -1;
    }
}

int CPQControl::FactoryGetColorTemp_Goffset(int source_input __unused, int colortemp_mode)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        return rgbogo.g_post_offset;
    }

    LOGE("FactoryGetColorTemp_Goffset error!\n");
    return -1;
}

int CPQControl::FactorySetColorTemp_Boffset(int source_input, int colortemp_mode, int boffset)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo);
    rgbogo.b_post_offset = boffset;
    LOGD("%s, source_input[%d], colortemp_mode[%d], b_post_offset[%d].", __FUNCTION__, source_input,
         colortemp_mode, boffset);
    rgbogo.en = 1;

    if (Cpq_SetRGBOGO(&rgbogo) == 0) {
        return 0;
    }

    return -1;
}

int CPQControl::FactorySaveColorTemp_Boffset(int source_input __unused, int colortemp_mode, int boffset)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        rgbogo.b_post_offset = boffset;
        return SetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, rgbogo);
    } else {
        LOGE("FactorySaveColorTemp_Boffset error!\n");
        return -1;
    }
}

int CPQControl::FactoryGetColorTemp_Boffset(int source_input __unused, int colortemp_mode)
{
    tcon_rgb_ogo_t rgbogo;
    memset (&rgbogo, 0, sizeof (rgbogo));
    if (0 == GetColorTemperatureParams((vpp_color_temperature_mode_t) colortemp_mode, &rgbogo)) {
        return rgbogo.b_post_offset;
    }

    LOGE("FactoryGetColorTemp_Boffset error!\n");
    return -1;
}

int CPQControl::FactoryResetNonlinear(void)
{
    return mPQdb->PQ_ResetAllNoLineParams();
}

int CPQControl::FactorySetParamsDefault(void)
{
    FactoryResetPQMode();
    FactoryResetNonlinear();
    FactoryResetColorTemp();
    if (mbCpqCfg_seperate_db_enable) {
        mpOverScandb->PQ_ResetAllOverscanParams();
    } else {
        mPQdb->PQ_ResetAllOverscanParams();
    }
    return 0;
}

int CPQControl::FactorySetNolineParams(source_input_param_t source_input_param, int type, noline_params_t noline_params)
{
    int ret = -1;

    switch (type) {
    case NOLINE_PARAMS_TYPE_BRIGHTNESS:
        ret = mPQdb->PQ_SetNoLineAllBrightnessParams(source_input_param.source_input,
                noline_params.osd0, noline_params.osd25, noline_params.osd50, noline_params.osd75,
                noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_CONTRAST:
        ret = mPQdb->PQ_SetNoLineAllContrastParams(source_input_param.source_input,
                noline_params.osd0, noline_params.osd25, noline_params.osd50, noline_params.osd75,
                noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_SATURATION:
        ret = mPQdb->PQ_SetNoLineAllSaturationParams(source_input_param.source_input,
                noline_params.osd0, noline_params.osd25, noline_params.osd50, noline_params.osd75,
                noline_params.osd100);

    case NOLINE_PARAMS_TYPE_HUE:
        ret = mPQdb->PQ_SetNoLineAllHueParams(source_input_param.source_input,
                noline_params.osd0, noline_params.osd25, noline_params.osd50, noline_params.osd75,
                noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_SHARPNESS:
        ret = mPQdb->PQ_SetNoLineAllSharpnessParams(source_input_param.source_input,
                noline_params.osd0, noline_params.osd25, noline_params.osd50, noline_params.osd75,
                noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_VOLUME:
        ret = mPQdb->PQ_SetNoLineAllVolumeParams(source_input_param.source_input,
                noline_params.osd0, noline_params.osd25, noline_params.osd50, noline_params.osd75,
                noline_params.osd100);
        break;

    default:
        break;
    }

    return ret;
}

noline_params_t CPQControl::FactoryGetNolineParams(source_input_param_t source_input_param, int type)
{
    int ret = -1;
    noline_params_t noline_params;
    memset(&noline_params, 0, sizeof(noline_params_t));

    switch (type) {
    case NOLINE_PARAMS_TYPE_BRIGHTNESS:
        ret = mPQdb->PQ_GetNoLineAllBrightnessParams(source_input_param.source_input,
                &noline_params.osd0, &noline_params.osd25, &noline_params.osd50,
                &noline_params.osd75, &noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_CONTRAST:
        ret = mPQdb->PQ_GetNoLineAllContrastParams(source_input_param.source_input,
                &noline_params.osd0, &noline_params.osd25, &noline_params.osd50,
                &noline_params.osd75, &noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_SATURATION:
        ret = mPQdb->PQ_GetNoLineAllSaturationParams(source_input_param.source_input,
                &noline_params.osd0, &noline_params.osd25, &noline_params.osd50,
                &noline_params.osd75, &noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_HUE:
        ret = mPQdb->PQ_GetNoLineAllHueParams(source_input_param.source_input,
                &noline_params.osd0, &noline_params.osd25, &noline_params.osd50,
                &noline_params.osd75, &noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_SHARPNESS:
        ret = mPQdb->PQ_GetNoLineAllSharpnessParams(source_input_param.source_input,
                &noline_params.osd0, &noline_params.osd25, &noline_params.osd50,
                &noline_params.osd75, &noline_params.osd100);
        break;

    case NOLINE_PARAMS_TYPE_VOLUME:
        ret = mPQdb->PQ_GetNoLineAllVolumeParams(source_input_param.source_input,
                &noline_params.osd0, &noline_params.osd25, &noline_params.osd50,
                &noline_params.osd75, &noline_params.osd100);
        break;

    default:
        break;
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return noline_params;
}

int CPQControl::FactorySetHdrMode(int mode)
{
    return SetHDRMode(mode);
}

int CPQControl::FactoryGetHdrMode(void)
{
    return GetHDRMode();
}

int CPQControl::FactorySetOverscanParam(source_input_param_t source_input_param, tvin_cutwin_t cutwin_t)
{
    int ret = -1;
    if (mbCpqCfg_seperate_db_enable) {
        ret = mpOverScandb->PQ_SetOverscanParams(source_input_param, cutwin_t);
    } else {
        ret = mPQdb->PQ_SetOverscanParams(source_input_param, cutwin_t);
    }
    if (ret != 0) {
        LOGE("%s failed.\n", __FUNCTION__);
    } else {
        LOGD("%s success.\n", __FUNCTION__);
    }

    return ret;
}

tvin_cutwin_t CPQControl::FactoryGetOverscanParam(source_input_param_t source_input_param)
{
    int ret = -1;
    tvin_cutwin_t cutwin_t;
    memset(&cutwin_t, 0, sizeof(cutwin_t));

    if (source_input_param.trans_fmt < TVIN_TFMT_2D || source_input_param.trans_fmt > TVIN_TFMT_3D_LDGD) {
        return cutwin_t;
    }
    if (mbCpqCfg_seperate_db_enable) {
        ret = mpOverScandb->PQ_GetOverscanParams(source_input_param, VPP_DISPLAY_MODE_169, &cutwin_t);
    } else {
        ret = mPQdb->PQ_GetOverscanParams(source_input_param, VPP_DISPLAY_MODE_169, &cutwin_t);
    }

    if (ret != 0) {
        LOGE("%s failed.\n", __FUNCTION__);
    } else {
        LOGD("%s success.\n", __FUNCTION__);
    }

    return cutwin_t;
}

int CPQControl::FactorySetGamma(int gamma_r_value, int gamma_g_value, int gamma_b_value)
{
    int ret = 0;
    tcon_gamma_table_t gamma_r, gamma_g, gamma_b;

    memset(gamma_r.data, (unsigned short)gamma_r_value, sizeof(tcon_gamma_table_t));
    memset(gamma_g.data, (unsigned short)gamma_g_value, sizeof(tcon_gamma_table_t));
    memset(gamma_b.data, (unsigned short)gamma_b_value, sizeof(tcon_gamma_table_t));

    ret |= Cpq_SetGammaTbl_R((unsigned short *) gamma_r.data);
    ret |= Cpq_SetGammaTbl_G((unsigned short *) gamma_g.data);
    ret |= Cpq_SetGammaTbl_B((unsigned short *) gamma_b.data);

    return ret;
}

int CPQControl::FcatorySSMRestore(void)
{
    resetAllUserSettingParam();
    return 0;
}

int CPQControl::Cpq_SetXVYCCMode(vpp_xvycc_mode_t xvycc_mode, source_input_param_t source_input_param)
{
    int ret = -1;
    am_regs_t regs, regs_1;
    memset(&regs, 0, sizeof(am_regs_t));
    memset(&regs_1, 0, sizeof(am_regs_t));

    if (mbCpqCfg_xvycc_enable) {
        if (mPQdb->PQ_GetXVYCCParams((vpp_xvycc_mode_t) xvycc_mode, source_input_param, &regs, &regs_1) == 0) {
            ret = Cpq_LoadRegs(regs);
            ret |= Cpq_LoadRegs(regs_1);
        } else {
            LOGE("PQ_GetXVYCCParams failed!\n");
        }
    } else {
        LOGD("XVYCC Moudle disabled!\n");
        ret = 0;
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::SetColorDemoMode(vpp_color_demomode_t demomode)
{
    LOGD("%s: mode is %d\n", __FUNCTION__, demomode);
    int ret = -1;
    cm_regmap_t regmap;
    unsigned long *temp_regmap;
    int i = 0;
    vpp_display_mode_t displaymode = VPP_DISPLAY_MODE_MODE43;

    switch (demomode) {
    case VPP_COLOR_DEMO_MODE_YOFF:
        temp_regmap = DemoColorYOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_COFF:
        temp_regmap = DemoColorCOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_GOFF:
        temp_regmap = DemoColorGOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_MOFF:
        temp_regmap = DemoColorMOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_ROFF:
        temp_regmap = DemoColorROffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_BOFF:
        temp_regmap = DemoColorBOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_RGBOFF:
        temp_regmap = DemoColorRGBOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_YMCOFF:
        temp_regmap = DemoColorYMCOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_ALLOFF:
        temp_regmap = DemoColorALLOffRegMap;
        break;

    case VPP_COLOR_DEMO_MODE_ALLON:
    default:
        if (displaymode == VPP_DISPLAY_MODE_MODE43) {
            temp_regmap = DemoColorSplit4_3RegMap;
        } else {
            temp_regmap = DemoColorSplitRegMap;
        }

        break;
    }

    for (i = 0; i < CM_REG_NUM; i++) {
        regmap.reg[i] = temp_regmap[i];
    }

    ret = VPPDeviceIOCtl(AMSTREAM_IOC_CM_REGMAP, regmap);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::SetColorBaseMode(vpp_color_basemode_t basemode, int isSave)
{
    LOGD("%s: mode is %d\n", __FUNCTION__, basemode);
    int ret = Cpq_SetColorBaseMode(basemode, mCurentSourceInputInfo);
    if (ret < 0) {
        LOGE("Cpq_SetColorBaseMode Failed!!!");
    } else {
        if (isSave == 1) {
            ret = SaveColorBaseMode(basemode);
        } else {
            LOGD("%s: No need save!\n", __FUNCTION__);
        }
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

vpp_color_basemode_t CPQControl::GetColorBaseMode(void)
{
    vpp_color_basemode_t data = VPP_COLOR_BASE_MODE_OFF;
    unsigned char tmp_base_mode = 0;
    mSSMAction->SSMReadColorBaseMode(&tmp_base_mode);
    data = (vpp_color_basemode_t) tmp_base_mode;
    if (data < VPP_COLOR_BASE_MODE_OFF || data >= VPP_COLOR_BASE_MODE_MAX) {
        data = VPP_COLOR_BASE_MODE_OPTIMIZE;
    }
    LOGD("%s: mode is %d\n", __FUNCTION__, data);
    return data;
}

int CPQControl::SaveColorBaseMode(vpp_color_basemode_t basemode)
{
    LOGD("%s: mode is %d\n", __FUNCTION__, basemode);
    int ret = -1;
    if (basemode == VPP_COLOR_BASE_MODE_DEMO) {
        ret = 0;
    } else {
        ret = mSSMAction->SSMSaveColorBaseMode(basemode);
    }

    return ret;
}

int CPQControl::Cpq_SetColorBaseMode(vpp_color_basemode_t basemode, source_input_param_t source_input_param)
{
    int ret = -1;
    am_regs_t regs;
    memset(&regs, 0, sizeof(am_regs_t));

    if (mbCpqCfg_cm2_enable) {
        if (mPQdb->PQ_GetCM2Params((vpp_color_management2_t)basemode, source_input_param, &regs) == 0) {
            ret = Cpq_LoadRegs(regs);
        } else {
            LOGE("PQ_GetCM2Params failed!\n");
        }
    } else {
        LOGD("CM moudle disabled!\n");
        ret = 0;
    }

    return ret;
}


int CPQControl::Cpq_SetRGBOGO(const struct tcon_rgb_ogo_s *rgbogo)
{
    int ret = VPPDeviceIOCtl(AMVECM_IOC_S_RGB_OGO, rgbogo);
    if (ret < 0) {
        LOGE("%s failed(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

int CPQControl::Cpq_GetRGBOGO(const struct tcon_rgb_ogo_s *rgbogo)
{
    int ret = VPPDeviceIOCtl(AMVECM_IOC_G_RGB_OGO, rgbogo);
    if (ret < 0) {
        LOGE("%s failed(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

int CPQControl::Cpq_SetGammaOnOff(int onoff)
{
    int ret = -1;

    if (onoff == 1) {
        LOGD("%s: enable gamma!\n", __FUNCTION__);
        ret = VPPDeviceIOCtl(AMVECM_IOC_GAMMA_TABLE_EN);
    } else {
        LOGD("%s: disable gamma!\n", __FUNCTION__);
        ret = VPPDeviceIOCtl(AMVECM_IOC_GAMMA_TABLE_DIS);
    }

    if (ret < 0) {
        LOGE("%s error(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

int CPQControl::SetDnlpMode(int level)
{
    int ret = -1;
    ve_dnlp_curve_param_t newdnlp;
    if (mbCpqCfg_dnlp_enable) {
        if (mPQdb->PQ_GetDNLPParams(mCurentSourceInputInfo, (Dynamic_contrst_status_t)level, &newdnlp) == 0) {
            ret = Cpq_SetVENewDNLP(&newdnlp);
            if (ret == 0) {
                mSSMAction->SSMSaveDnlpMode(mCurentSourceInputInfo.source_input, level);
            }
        } else {
            LOGE("mPQdb->PQ_GetDNLPParams failed!\n");
        }
    } else {
        LOGD("DNLP moudle disabled!\n");
        ret = 0;
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetDnlpMode()
{
    int ret = -1, level = 0;
    ret = mSSMAction->SSMReadDnlpMode(mCurentSourceInputInfo.source_input, &level);
    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        LOGD("%s success!\n",__FUNCTION__);
    }

    LOGD("%s, source_input = %d, mode is %d\n",__FUNCTION__, mCurentSourceInputInfo.source_input, level);
    return level;
}

int CPQControl::Cpq_SetVENewDNLP(const ve_dnlp_curve_param_t *pDNLP)
{
    int ret = VPPDeviceIOCtl(AMVECM_IOC_VE_NEW_DNLP, pDNLP);
    if (ret < 0) {
        LOGE("%s error(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

int CPQControl::Cpq_SetDNLPStatus(ve_dnlp_state_t status)
{
    int ret = VPPDeviceIOCtl(AMVECM_IOC_S_DNLP_STATE, &status);
    if (ret < 0) {
        LOGE("%s error(%s)!\n", __FUNCTION__, strerror(errno));
    }

    return ret;
}

int CPQControl::FactorySetDNLPCurveParams(source_input_param_t source_input_param, int level, int final_gain)
{
    int ret = -1;
    int cur_final_gain = -1;
    char tmp_buf[128];

    cur_final_gain = mPQdb->PQ_GetDNLPGains(source_input_param, (Dynamic_contrst_status_t)level);
    if (cur_final_gain == final_gain) {
        LOGD("FactorySetDNLPCurveParams, same value, no need to update!");
        return ret;
    } else {
        LOGD("%s final_gain = %d \n", __FUNCTION__, final_gain);
        sprintf(tmp_buf, "%s %s %d", "w", "final_gain", final_gain);
        pqWriteSys("/sys/class/amvecm/dnlp_debug", tmp_buf);
        ret |= mPQdb->PQ_SetDNLPGains(source_input_param, (Dynamic_contrst_status_t)level, final_gain);

    }
    return ret;
}

int CPQControl::FactoryGetDNLPCurveParams(source_input_param_t source_input_param, int level)
{
    return mPQdb->PQ_GetDNLPGains(source_input_param, (Dynamic_contrst_status_t)level);
}

int CPQControl::FactorySetNoiseReductionParams(source_input_param_t source_input_param, vpp_noise_reduction_mode_t nr_mode, int addr, int val)
{
    return mPQdb->PQ_SetNoiseReductionParams(nr_mode, source_input_param, addr, val);
}

int CPQControl::FactoryGetNoiseReductionParams(source_input_param_t source_input_param, vpp_noise_reduction_mode_t nr_mode, int addr)
{
    return mPQdb->PQ_GetNoiseReductionParams(nr_mode, source_input_param, addr);
}

int CPQControl::SetEyeProtectionMode(tv_source_input_t source_input __unused, int enable, int is_save __unused)
{
    LOGD("%s: mode:%d!\n", __FUNCTION__, enable);
    int ret = -1;
    vpp_color_temperature_mode_t TempMode = (vpp_color_temperature_mode_t)GetColorTemperature();
    tcon_rgb_ogo_t param;
    memset(&param, 0, sizeof(tcon_rgb_ogo_t));
    if (TempMode == VPP_COLOR_TEMPERATURE_MODE_USER) {
        ret = Cpq_GetColorTemperatureUser(mCurentSourceInputInfo.source_input, &param);
    } else {
        ret = GetColorTemperatureParams(TempMode, &param);
    }

    if (ret < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
    } else {
        if (enable) {
            param.b_gain /= 2;
        }
        ret = Cpq_SetRGBOGO(&param);
        mSSMAction->SSMSaveEyeProtectionMode(enable);
        LOGD("%s success!\n",__FUNCTION__);
    }

    return ret;
}

int CPQControl::GetEyeProtectionMode(tv_source_input_t source_input __unused)
{
    int mode = -1;

    if (mSSMAction->SSMReadEyeProtectionMode(&mode) < 0) {
        LOGE("%s failed!\n",__FUNCTION__);
        return -1;
    } else {
        LOGD("%s: mode is %d!\n",__FUNCTION__, mode);
        return mode;
    }
}

int CPQControl::SetFlagByCfg(void)
{
    const char *config_value;
    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_BIG_SMALL_DB_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_seperate_db_enable = true;
    } else {
        mbCpqCfg_seperate_db_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_BRIGHTNESS_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_brightness_contrast_enable = true;
    } else {
        mbCpqCfg_brightness_contrast_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_SATUATION_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_satuation_hue_enable = true;
    } else {
        mbCpqCfg_satuation_hue_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_BLACKEXTENSION_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_blackextension_enable = true;
    } else {
        mbCpqCfg_blackextension_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_SHARPNESS0_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_sharpness0_enable = true;
    } else {
        mbCpqCfg_sharpness0_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_SHARPNESS1_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_sharpness1_enable = true;
    } else {
        mbCpqCfg_sharpness1_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_DI_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_di_enable = true;
    } else {
        mbCpqCfg_di_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_MCDI_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_mcdi_enable = true;
    } else {
        mbCpqCfg_mcdi_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_DEBLOCK_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_deblock_enable = true;
    } else {
        mbCpqCfg_deblock_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_NOISEREDUCTION_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_nr_enable = true;
    } else {
        mbCpqCfg_nr_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_DEMOSQUITO_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_demoSquito_enable = true;
    } else {
        mbCpqCfg_demoSquito_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_GAMMA_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_gamma_enable = true;
    } else {
        mbCpqCfg_gamma_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_CM2_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_cm2_enable = true;
    } else {
        mbCpqCfg_cm2_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_WHITEBALANCE_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_whitebalance_enable = true;
    } else {
        mbCpqCfg_whitebalance_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_DNLP_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_dnlp_enable = true;
    } else {
        mbCpqCfg_dnlp_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_XVYCC_ENABLE, "disable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_xvycc_enable = true;
    } else {
        mbCpqCfg_xvycc_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_CONTRAST_WITHOSD_ENABLE, "disable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_contrast_withOSD = true;
    } else {
        mbCpqCfg_contrast_withOSD = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_HUE_WITHOSD_ENABLE, "disable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_hue_withOSD = true;
    } else {
        mbCpqCfg_hue_withOSD = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_BRIGHTNESS_WITHOSD_ENABLE, "disable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_brightness_withOSD = true;
    } else {
        mbCpqCfg_brightness_withOSD = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_DISPLAY_OVERSCAN_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_display_overscan_enable = true;
    } else {
        mbCpqCfg_display_overscan_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_LOCAL_CONTRAST_ENABLE, "disable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_local_contrast_enable = true;
    } else {
        mbCpqCfg_local_contrast_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_HDMI, CFG_HDMI_OUT_WITH_FBC_ENABLE, "disable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_hdmi_out_with_fbc_enable = true;
    } else {
        mbCpqCfg_hdmi_out_with_fbc_enable = false;
    }

    config_value = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_PQ_PARAM_CHECK_SOURCE_ENABLE, "enable");
    if (strcmp(config_value, "enable") == 0) {
        mbCpqCfg_pq_param_check_source_enable = true;
    } else {
        mbCpqCfg_pq_param_check_source_enable = false;
    }

    return 0;
}

int CPQControl::SetPLLValues(source_input_param_t source_input_param)
{
    am_regs_t regs;
    int ret = 0;
    if (mPQdb->PQ_GetPLLParams (source_input_param, &regs ) == 0 ) {
        ret = AFEDeviceIOCtl(TVIN_IOC_LOAD_REG, &regs);
        if ( ret < 0 ) {
            LOGE ( "%s error(%s)!\n", __FUNCTION__, strerror(errno));
            return -1;
        }
    } else {
        LOGE ( "%s, PQ_GetPLLParams failed!\n", __FUNCTION__ );
        return -1;
    }

    return 0;
}

int CPQControl::SetCVD2Values(void)
{
    am_regs_t regs;
    int ret = mPQdb->PQ_GetCVD2Params ( mCurentSourceInputInfo, &regs);
    if (ret < 0) {
        LOGE ( "%s, PQ_GetCVD2Params failed!\n", __FUNCTION__);
    } else {
        ret = AFEDeviceIOCtl(TVIN_IOC_LOAD_REG, &regs);
        if ( ret < 0 ) {
            LOGE ( "%s: ioctl failed!\n", __FUNCTION__);
        }
    }

    if (ret < 0) {
        LOGE("%s failed!\n", __FUNCTION__);
    } else {
        LOGD("%s success!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::Cpq_SSMReadNTypes(int id, int data_len, int offset)
{
    int value = 0;
    int ret = 0;

    ret = mSSMAction->SSMReadNTypes(id, data_len, &value, offset);

    if (ret < 0) {
        LOGE("Cpq_SSMReadNTypes, error(%s).\n", strerror ( errno ) );
        return -1;
    } else {
        return value;
    }
}

int CPQControl::Cpq_SSMWriteNTypes(int id, int data_len, int data_buf, int offset)
{
    int ret = 0;
    ret = mSSMAction->SSMWriteNTypes(id, data_len, &data_buf, offset);

    if (ret < 0) {
        LOGE("Cpq_SSMWriteNTypes, error(%s).\n", strerror ( errno ) );
    }

    return ret;
}

int CPQControl::Cpq_GetSSMActualAddr(int id)
{
    return mSSMAction->GetSSMActualAddr(id);
}

int CPQControl::Cpq_GetSSMActualSize(int id)
{
    return mSSMAction->GetSSMActualSize(id);
}

int CPQControl::Cpq_SSMRecovery(void)
{
    return mSSMAction->SSMRecovery();
}

int CPQControl::Cpq_GetSSMStatus()
{
    return mSSMAction->GetSSMStatus();
}

int CPQControl::SetCurrentSourceInputInfo(source_input_param_t source_input_param)
{
    LOGD("%s: param_check_source_enable = %d!\n", __FUNCTION__, mbCpqCfg_pq_param_check_source_enable);
    pthread_mutex_lock(&PqControlMutex);
    if (source_input_param.source_input == SOURCE_MPEG) {
        int HdrMode = GetHDRMode();
        if (HdrMode >= VPP_MATRIX_BT2020YUV_BT2020RGB && HdrMode != VPP_MATRIX_DEFAULT_CSCTYPE) {
            mPQdb->mHdrStatus = true;
        } else {
            mPQdb->mHdrStatus = false;
        }
    } else if ((source_input_param.source_input >= SOURCE_HDMI1) &&
               (source_input_param.source_input <= SOURCE_HDMI4)) {
        int isHdr = (mHdmiHdrInfo >> 29) & 0x1;
        if (isHdr == 1) {
            mPQdb->mHdrStatus = true;
        } else {
            mPQdb->mHdrStatus = false;
        }
    } else {
        LOGD("%s: This source no need check hdr!\n", __FUNCTION__);
    }
    LOGD("%s: hdr status is %d!\n", __FUNCTION__, mPQdb->mHdrStatus);

    if ((mCurentSourceInputInfo.source_input != source_input_param.source_input) ||
         (mCurentSourceInputInfo.sig_fmt != source_input_param.sig_fmt) ||
         (mCurentSourceInputInfo.trans_fmt != source_input_param.trans_fmt) ||
         (mCurrentHdrStatus != mPQdb->mHdrStatus)) {
        mCurentSourceInputInfo.source_input = source_input_param.source_input;
        mCurentSourceInputInfo.sig_fmt = source_input_param.sig_fmt;
        mCurentSourceInputInfo.trans_fmt = source_input_param.trans_fmt;
        mCurrentHdrStatus = mPQdb->mHdrStatus;

        if (mbCpqCfg_pq_param_check_source_enable) {
            mSourceInputForSaveParam = mCurentSourceInputInfo.source_input;
        } else {
            mSourceInputForSaveParam = SOURCE_MPEG;
        }

        if (mCurentSourceInputInfo.sig_fmt != TVIN_SIG_FMT_NULL) {
            LoadPQSettings();
        } else {
            vpp_display_mode_t display_mode = (vpp_display_mode_t)GetDisplayMode();
            SetDisplayMode(display_mode, 1);
        }
    } else {
        LOGD("%s: same signal, no need set!\n", __FUNCTION__);
    }
    pthread_mutex_unlock(&PqControlMutex);
    return 0;
}

source_input_param_t CPQControl::GetCurrentSourceInputInfo()
{
    pthread_mutex_lock(&PqControlMutex);
    source_input_param_t info = mCurentSourceInputInfo;
    pthread_mutex_unlock(&PqControlMutex);
    return info;
}

int CPQControl::GetRGBPattern() {
    char value[32] = {0};
    pqReadSys(VIDEO_RGB_SCREEN, value, sizeof(value));
    return strtol(value, NULL, 10);
}

int CPQControl::SetRGBPattern(int r, int g, int b) {
    int value = ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
    char str[32] = {0};
    sprintf(str, "%d", value);
    int ret = pqWriteSys(VIDEO_RGB_SCREEN, str);
    return ret;
}

int CPQControl::FactorySetDDRSSC(int step) {
    if (step < 0 || step > 5) {
        LOGE ("%s, step = %d is too long", __FUNCTION__, step);
        return -1;
    }

    return mSSMAction->SSMSaveDDRSSC(step);
}

int CPQControl::FactoryGetDDRSSC() {
    unsigned char data = 0;
    mSSMAction->SSMReadDDRSSC(&data);
    return data;
}

int CPQControl::SetLVDSSSC(int step) {
    if (step > 4)
        step = 4;

    FILE *fp = fopen(SSC_PATH, "w");
    if (fp != NULL) {
        fprintf(fp, "%d", step);
        fclose(fp);
    } else {
        LOGE("open /sys/class/lcd/ss ERROR(%s)!!\n", strerror(errno));
        return -1;
    }
    return 0;
}
int CPQControl::FactorySetLVDSSSC(int step) {
    int data[2] = {0, 0};
    int value = 0, panel_idx = 0, tmp = 0;
    const char *PanelIdx;
    if (step > 4)
        step = 4;
    PanelIdx = "0";//config_get_str ( CFG_SECTION_TV, "get.panel.index", "0" ); can't parse ini file in systemcontrol
    panel_idx = strtoul(PanelIdx, NULL, 10);
    LOGD ("%s, panel_idx = %x", __FUNCTION__, panel_idx);
    mSSMAction->SSMReadLVDSSSC(data);

    //every 2 bits represent one panel, use 2 byte store 8 panels
    value = (data[1] << 8) | data[0];
    step = step & 0x03;
    panel_idx = panel_idx * 2;
    tmp = 3 << panel_idx;
    value = (value & (~tmp)) | (step << panel_idx);
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    LOGD ("%s, tmp = %x, save value = %x", __FUNCTION__, tmp, value);

    SetLVDSSSC(step);
    return mSSMAction->SSMSaveLVDSSSC(data);
}

int CPQControl::FactoryGetLVDSSSC() {
    int data[2] = {0, 0};
    int value = 0, panel_idx = 0;
    const char *PanelIdx = "0";//config_get_str ( CFG_SECTION_TV, "get.panel.index", "0" );can't parse ini file in systemcontrol

    panel_idx = strtoul(PanelIdx, NULL, 10);
    mSSMAction->SSMReadLVDSSSC(data);
    value = (data[1] << 8) | data[0];
    value = (value >> (2 * panel_idx)) & 0x03;
    LOGD ("%s, panel_idx = %x, value= %d", __FUNCTION__, panel_idx, value);
    return value;
}

int CPQControl::SetGrayPattern(int value) {
    if (value < 0) {
        value = 0;
    } else if (value > 255) {
        value = 255;
    }
    value = value << 16 | 0x8080;

    LOGD("SetGrayPattern /sys/class/video/test_screen : %x", value);
    FILE *fp = fopen(TEST_SCREEN, "w");
    if (fp == NULL) {
        LOGE("Open /sys/classs/video/test_screen error(%s)!\n", strerror(errno));
        return -1;
    }

    fprintf(fp, "0x%x", value);
    fclose(fp);
    fp = NULL;

    return 0;
}

int CPQControl::GetGrayPattern() {
    int value = 0;

    FILE *fp = fopen(TEST_SCREEN, "r+");
    if (fp == NULL) {
        LOGE("Open /sys/class/video/test_screen error(%s)!\n", strerror(errno));
        return -1;
    }

    fscanf(fp, "%x", &value);

    LOGD("GetGrayPattern /sys/class/video/test_screen %x", value);
    fclose(fp);
    fp = NULL;
    if (value < 0) {
        return 0;
    } else {
        value = value >> 16;
        if (value > 255) {
            value = 255;
        }
        return value;
    }
}

int CPQControl::SetHDRMode(int mode)
{
    int ret = -1;
    if ((mCurentSourceInputInfo.source_input == SOURCE_MPEG) ||
       ((mCurentSourceInputInfo.source_input >= SOURCE_HDMI1) && mCurentSourceInputInfo.source_input <= SOURCE_HDMI4)) {
        ret = VPPDeviceIOCtl(AMVECM_IOC_S_CSCTYPE, &mode);
        if (ret < 0) {
            LOGE("%s error: %s!\n", __FUNCTION__, strerror(errno));
        }
    } else {
        LOGE("%s: Curent source no hdr status!\n", __FUNCTION__);
    }

    return ret;
}

int CPQControl::GetHDRMode()
{
    ve_csc_type_t mode = VPP_MATRIX_NULL;
    if ((mCurentSourceInputInfo.source_input == SOURCE_MPEG) ||
       ((mCurentSourceInputInfo.source_input >= SOURCE_HDMI1) && mCurentSourceInputInfo.source_input <= SOURCE_HDMI4)) {
        int ret = VPPDeviceIOCtl(AMVECM_IOC_G_CSCTYPE, &mode);
        if (ret < 0) {
            LOGE("%s error: %s!\n", __FUNCTION__, strerror(errno));
            mode = VPP_MATRIX_NULL;
        } else {
            LOGD("%s: mode is %d\n", __FUNCTION__, mode);
        }
    } else {
        LOGD("%s: Curent source no hdr status!\n", __FUNCTION__);
    }

    return mode;
}

tvpq_databaseinfo_t CPQControl::GetDBVersionInfo(db_name_t name) {
    bool val = false;
    std::string tmpToolVersion, tmpProjectVersion, tmpGenerateTime;
    tvpq_databaseinfo_t pqdatabaseinfo_t;
    memset(&pqdatabaseinfo_t, 0, sizeof(pqdatabaseinfo_t));
    switch (name) {
        case DB_NAME_PQ:
            val = mPQdb->PQ_GetPqVersion(tmpToolVersion, tmpProjectVersion, tmpGenerateTime);
            break;
        case DB_NAME_OVERSCAN:
            val = mpOverScandb->GetOverScanDbVersion(tmpToolVersion, tmpProjectVersion, tmpGenerateTime);
            break;
        default:
            val = mPQdb->PQ_GetPqVersion(tmpToolVersion, tmpProjectVersion, tmpGenerateTime);
            break;
    }

    if (val) {
        strcpy(pqdatabaseinfo_t.ToolVersion, tmpToolVersion.c_str());
        strcpy(pqdatabaseinfo_t.ProjectVersion, tmpProjectVersion.c_str());
        strcpy(pqdatabaseinfo_t.GenerateTime, tmpGenerateTime.c_str());
    }

    return pqdatabaseinfo_t;
}

int CPQControl::SetCurrentHdrInfo (int hdrInfo)
{
    mHdmiHdrInfo = hdrInfo;
    return 0;
}

int CPQControl::SetDtvKitSourceEnable(bool isEnable)
{
    mbDtvKitEnable = isEnable;
    return 0;
}

void CPQControl::resetAllUserSettingParam()
{
    int i = 0, config_val = 0;
    vpp_pq_para_t pq_para;
    const char *buf = NULL;
    for (i=SOURCE_TV;i<SOURCE_MAX;i++) {
        if (mbCpqCfg_seperate_db_enable) {
            mpOverScandb->PQ_GetPQModeParams((tv_source_input_t)i, VPP_PICTURE_MODE_USER, &pq_para);
        } else {
            mPQdb->PQ_GetPQModeParams((tv_source_input_t)i, VPP_PICTURE_MODE_USER, &pq_para);
        }
        /*LOGD("%s: brightness=%d, contrast=%d, saturation=%d, hue=%d, sharpness=%d, backlight=%d, nr=%d\n",
                 __FUNCTION__, pq_para.brightness, pq_para.contrast, pq_para.saturation, pq_para.hue,
                 pq_para.sharpness, pq_para.backlight, pq_para.nr);*/
        mSSMAction->SSMSaveBrightness((tv_source_input_t)i, pq_para.brightness);
        mSSMAction->SSMSaveContrast((tv_source_input_t)i, pq_para.contrast);
        mSSMAction->SSMSaveSaturation((tv_source_input_t)i, pq_para.saturation);
        mSSMAction->SSMSaveHue((tv_source_input_t)i, pq_para.hue);
        mSSMAction->SSMSaveSharpness((tv_source_input_t)i, pq_para.sharpness);
        mSSMAction->SSMSaveBackLightVal(pq_para.backlight);
        mSSMAction->SSMSaveNoiseReduction((tv_source_input_t)i, pq_para.nr);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_PICTUREMODE_DEF, VPP_PICTURE_MODE_STANDARD);
        mSSMAction->SSMSavePictureMode(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_COLORTEMPTUREMODE_DEF, VPP_COLOR_TEMPERATURE_MODE_STANDARD);
        mSSMAction->SSMSaveColorTemperature(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_DISPLAYMODE_DEF, VPP_DISPLAY_MODE_NORMAL);
        mSSMAction->SSMSaveDisplayMode(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_GAMMALEVEL_DEF, VPP_GAMMA_CURVE_DEFAULT);
        mSSMAction->SSMSaveGammaValue(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_AUTOASPECT_DEF, 1);
        mSSMAction->SSMSaveAutoAspect(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_43STRETCH_DEF, 0);
        mSSMAction->SSMSave43Stretch(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_DNLPLEVEL_DEF, DYNAMIC_CONTRAST_MID);
        mSSMAction->SSMSaveDnlpMode(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_DNLPGAIN_DEF, 0);
        mSSMAction->SSMSaveDnlpGainValue(i, config_val);

        config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_LOCALCONTRASTMODE_DEF, 2);
        mSSMAction->SSMSaveLocalContrastMode(i, config_val);
    }

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_COLORDEMOMODE_DEF, VPP_COLOR_DEMO_MODE_ALLON);
    mSSMAction->SSMSaveColorDemoMode(config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_COLORBASEMODE_DEF, VPP_COLOR_DEMO_MODE_ALLON);
    mSSMAction->SSMSaveColorBaseMode ( VPP_COLOR_BASE_MODE_OPTIMIZE);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_RGBGAIN_R_DEF, 0);
    mSSMAction->SSMSaveRGBGainRStart(0, config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_RGBGAIN_G_DEF, 0);
    mSSMAction->SSMSaveRGBGainGStart(0, config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_RGBGAIN_B_DEF, 0);
    mSSMAction->SSMSaveRGBGainBStart(0, config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_RGBPOSTOFFSET_R_DEF_DEF, 1024);
    mSSMAction->SSMSaveRGBPostOffsetRStart(0, config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_RGBPOSTOFFSET_G_DEF_DEF, 1024);
    mSSMAction->SSMSaveRGBPostOffsetGStart(0, config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_RGBPOSTOFFSET_B_DEF_DEF, 1024);
    mSSMAction->SSMSaveRGBPostOffsetBStart(0, config_val);

    int8_t std_buf[6] = { 0, 0, 0, 0, 0, 0 };
    int8_t warm_buf[6] = { 0, 0, -8, 0, 0, 0 };
    int8_t cold_buf[6] = { -8, 0, 0, 0, 0, 0 };
    for (i = 0; i < 6; i++) {
        mSSMAction->SSMSaveRGBValueStart(i + VPP_COLOR_TEMPERATURE_MODE_STANDARD * 6, std_buf[i]); //0~5
        mSSMAction->SSMSaveRGBValueStart(i + VPP_COLOR_TEMPERATURE_MODE_WARM * 6, warm_buf[i]); //6~11
        mSSMAction->SSMSaveRGBValueStart(i + VPP_COLOR_TEMPERATURE_MODE_COLD * 6, cold_buf[i]); //12~17
    }

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_COLORSPACE_DEF, VPP_COLOR_SPACE_AUTO);
    mSSMAction->SSMSaveColorSpaceStart(config_val);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_DDRSSC_DEF, 0);
    mSSMAction->SSMSaveDDRSSC(config_val);

    buf = mPQConfigFile->GetString(CFG_SECTION_PQ, CFG_LVDSSSC_DEF, NULL);
    int tmp[2] = {0, 0};
    pqTransformStringToInt(buf, tmp);
    mSSMAction->SSMSaveLVDSSSC(tmp);

    config_val = mPQConfigFile->GetInt(CFG_SECTION_PQ, CFG_EYEPROJECTMODE_DEF, 0);
    mSSMAction->SSMSaveEyeProtectionMode(config_val);

    buf = mPQConfigFile->GetString(CFG_SECTION_HDMI, CFG_EDID_VERSION_DEF, NULL);
    if (strcmp(buf, "edid_20") == 0) {
        mSSMAction->SSMEdidRestoreDefault(1);
    } else {
        mSSMAction->SSMEdidRestoreDefault(0);
    }

    config_val = mPQConfigFile->GetInt(CFG_SECTION_HDMI, CFG_HDCP_SWITCHER_DEF, 0);
    mSSMAction->SSMHdcpSwitcherRestoreDefault(0);

    buf = mPQConfigFile->GetString(CFG_SECTION_HDMI, CFG_COLOR_RANGE_MODE_DEF, NULL);
    if (strcmp(buf, "full") == 0) {
        mSSMAction->SSMSColorRangeModeRestoreDefault(1);
    } else if (strcmp(buf, "limit") == 0) {
        mSSMAction->SSMSColorRangeModeRestoreDefault(2);
    } else {
        mSSMAction->SSMSColorRangeModeRestoreDefault(0);
    }

    return;
}

int CPQControl::pqWriteSys(const char *path, const char *val)
{
    int fd;
    if ((fd = open(path, O_RDWR)) < 0) {
        LOGE("writeSys, open %s error(%s)", path, strerror (errno));
        return -1;
    }

    int len = write(fd, val, strlen(val));
    close(fd);
    return len;
}

int CPQControl::pqReadSys(const char *path, char *buf, int count)
{
    int fd, len;

    if ( NULL == buf ) {
        LOGE("buf is NULL");
        return -1;
    }

    if ((fd = open(path, O_RDONLY)) < 0) {
        LOGE("pqReadSys, open %s error(%s)", path, strerror (errno));
        return -1;
    }

    len = read(fd, buf, count);
    if (len < 0) {
        LOGE("pqReadSys %s error, %s\n", path, strerror(errno));
        close(fd);
        return len;
    }

    int i , j;
    for (i = 0, j = 0; i <= len -1; i++) {
        //change '\0' to 0x20(spacing), otherwise the string buffer will be cut off ,if the last char is '\0' should not replace it
        if (0x0 == buf[i] && i < len - 1) {
            buf[i] = 0x20;
        }
        /* delete all the character of '\n' */
        if (0x0a != buf[i]) {
            buf[j++] = buf[i];
        }
    }
    buf[j] = 0x0;

    close(fd);
    return len;
}

void CPQControl::pqTransformStringToInt(const char *buf, int *val)
{
    if (buf != NULL) {
        //LOGD("%s: %s\n", __FUNCTION__, buf);
        char temp_buf[256];
        char *p = NULL;
        int i = 0;
        strcpy(temp_buf, buf);
        p = strtok(temp_buf, ",");
        while (NULL != p) {
           val[i++] = atoi(p);
           p = strtok(NULL,  ",");
        }
    } else {
        LOGE("%s:Invalid param!\n", __FUNCTION__);
    }
    return;
}

output_type_t CPQControl::GetTxOutPutMode(void)
{
    char buf[32] = {0};
    output_type_t OutPutType = OUTPUT_TYPE_MAX;
    if ((pqReadSys(SYS_DISPLAY_MODE_PATH, buf, sizeof(buf)) < 0) || (strlen(buf) == 0)) {
        LOGD("Read /sys/class/display/mode failed!\n");
        OutPutType = OUTPUT_TYPE_MAX;
    } else {
        LOGD( "%s: current output mode is %s!\n", __FUNCTION__, buf);
        if (strstr(buf, "null")) {
            OutPutType = OUTPUT_TYPE_MAX;
        } else if (strstr(buf, "480cvbs")) {
            OutPutType = OUTPUT_TYPE_NTSC;
        } else if(strstr(buf, "576cvbs")) {
            OutPutType = OUTPUT_TYPE_PAL;
        } else {
            OutPutType = OUTPUT_TYPE_HDMI;
        }

        LOGD("%s: output mode is %d!\n", __FUNCTION__, OutPutType);
    }

    return OutPutType;
}

bool CPQControl::isCVBSParamValid(void)
{
    bool ret = mPQdb->CheckCVBSParamValidStatus();
    if (ret) {
        LOGD("cvbs param exist!\n");
    } else {
        LOGD("cvbs param don't exist!\n");
    }
    return ret;
}
