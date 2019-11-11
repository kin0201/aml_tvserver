/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_MOUDLE_TAG "PQ"
#define LOG_CLASS_TAG "CDynamicBackLight"

#include "CDynamicBackLight.h"
#include "unistd.h"
#include <pthread.h>

CDynamicBackLight::CDynamicBackLight()
{
    mpObserver = NULL;
    mPreBacklightValue = 255;
    mGD_mvreflsh = 9;
    GD_LUT_MODE = 1;
    GD_ThTF = 0;
}

CDynamicBackLight::~CDynamicBackLight()
{

}

int CDynamicBackLight::startDected(void)
{
    int ret;
    pthread_t thread_id;

    ret = pthread_create(&thread_id, NULL, dynamicBacklightThread, (void *)this);
    if (ret != 0) {
        LOGD("create DynamicBackLight thread fail.\n");
        ret = -1;
    } else {
        LOGE("DynamicBackLight thread id (%lu) done.\n", thread_id);
        ret = 0;
    }

    return ret;
}

void CDynamicBackLight::gd_fw_alg_frm(int value, int *tf_bl_value, int *LUT)
{
    int nT0 = 0, nT1 = 0;
    int nL0 = 0, nR0 = 0;
    int nDt = 0;
    int bl_value = 0;
    int bld_lvl = 0, bl_diff = 0;//luma_dif = 0;
    int RBASE = 0;
    int apl_lut[10] = {0, 16, 35, 58, 69, 80, 91, 102, 235, 255};
    int step = 0;
    int i = 0;
    int average = 0;
    int GD_STEP_Th = 5;
    int GD_IIR_MODE = 0;//1-old iir;0-new iir,set constant step
    RBASE = (1 << mGD_mvreflsh);
    if (COLOR_RANGE_MODE == 1) {//color range limit
        if (value < 16) {
            value = 16;
        } else if (value > 236) {
            value = 236;
        }
        average = (value - 16)*256/(236-16);
    } else {
        if (value < 0) {//color renge full
            value = 0;
        } else if (value > 255) {
            *tf_bl_value = mPreBacklightValue;
            return;
        }
        average = value;
    }

    if (GD_LUT_MODE == 0) {//old or xiaomi project
        nT0 = average/16;
        nT1 = average%16;
        nL0 = LUT[nT0];
        nR0 = LUT[nT0+1];
        nDt = nL0*(16-nT1)+nR0*nT1+8;
        bl_value = nDt/16;
    } else {//new mode, only first ten elements used
        for (i=0;i<9;i++) {
            if (average <= apl_lut[i+1] && average >= apl_lut[i]) {
                nT0 = i;
                step= apl_lut[i+1] - apl_lut[i];
                break;
            }
        }

        nT1 = average - apl_lut[nT0];
        nL0 = LUT[nT0];
        nR0 = LUT[nT0+1];
        nDt = nL0*(step-nT1)+nR0*nT1+step/2;
        bl_value = nDt/step;//make sure that step != 0
    }

    if (GD_IIR_MODE) {
        bl_diff = (mPreBacklightValue > bl_value) ? (mPreBacklightValue - bl_value) : (bl_value - mPreBacklightValue);
        bld_lvl = (RBASE > (GD_ThTF + bl_diff)) ? (GD_ThTF + bl_diff) : RBASE;
        *tf_bl_value = ((RBASE - bld_lvl) * mPreBacklightValue + bld_lvl * bl_value + (RBASE >> 1)) >> mGD_mvreflsh;     //slowchange
    } else {
        step = bl_value - mPreBacklightValue;
        if (step > GD_STEP_Th  )// dark --> bright, limit increase step
            step = GD_STEP_Th;
        else if (step < (-GD_STEP_Th)) // bright --> dark, limit decrease step
            step = -GD_STEP_Th;

        *tf_bl_value = mPreBacklightValue + step;
    }
    mPreBacklightValue = *tf_bl_value;
    return;
}

int CDynamicBackLight::backLightScale(int backLight, int UIval)
{
    //LOGD ("%s: backLight =  %d, UIvalue = %d\n", __FUNCTION__, backLight, UIval);
    int ret = 255;
    if (backLight <= 0) {
        return 1;
    } else if (backLight >= 255) {
        backLight = 255;
    }

    ret = backLight * UIval / 100;
    if (ret <= 0) {
        ret = 1;
    } else if (ret >= 255) {
        ret = 255;
    }

    return ret;
}

void *CDynamicBackLight::dynamicBacklightThread(void* data)
{
    CDynamicBackLight *pThis = (CDynamicBackLight *)data;
    pThis->threadLoop();
    return NULL;
}

void *CDynamicBackLight::threadLoop(void)
{
    if (mpObserver == NULL) {
        LOGD ("%s: mpObserver is null.\n", __FUNCTION__);
        return NULL;
    } else {
        dynamic_backlight_Param_t DynamicBacklightParam;
        memset(&DynamicBacklightParam, 0, sizeof(dynamic_backlight_Param_t));
        int backLight = 0, NewBacklightValue = 0;
        int LUT_high[17], LUT_low[17];
        memset(LUT_high, 0, sizeof(LUT_high));
        memset(LUT_low, 0, sizeof(LUT_low));
        mpObserver->GetDynamicBacklighConfig(&GD_ThTF, &GD_LUT_MODE, LUT_high, LUT_low);

        if (GD_ThTF < 0 || GD_ThTF >(1<<mGD_mvreflsh)) {
            GD_ThTF = 0;
        }

        if (GD_LUT_MODE < 0 || GD_LUT_MODE >255) {
            GD_LUT_MODE = 0;
        }

        while (1) {
            mpObserver->GetDynamicBacklighParam(&DynamicBacklightParam);
            //LOGD("hist = %d, mode = %d\n", DynamicBacklightParam.hist.ave, DynamicBacklightParam.CurDynamicBacklightMode);

            if (DynamicBacklightParam.hist.ave == -1) {
                DynamicBacklightParam.CurDynamicBacklightMode = DYNAMIC_BACKLIGHT_OFF;
            }

            if (DYNAMIC_BACKLIGHT_HIGH == DynamicBacklightParam.CurDynamicBacklightMode) {
                gd_fw_alg_frm(DynamicBacklightParam.hist.ave, &backLight, LUT_high);
            } else if (DYNAMIC_BACKLIGHT_LOW == DynamicBacklightParam.CurDynamicBacklightMode) {
                gd_fw_alg_frm(DynamicBacklightParam.hist.ave, &backLight, LUT_low);
            } else {
                if ((255 - mPreBacklightValue) > 5) {
                    backLight = mPreBacklightValue + 5;
                } else {
                    backLight = 255;
                }
                mPreBacklightValue = backLight;
            }
            //LOGD("hist = %d， scale_value = %d\n", DynamicBacklightParam.hist.ave, backLight);
            NewBacklightValue = backLightScale(backLight, DynamicBacklightParam.UiBackLightValue);
            //LOGD("new set val = %d\n", NewBacklightValue);
            if (DynamicBacklightParam.CurBacklightValue != NewBacklightValue) {
                mpObserver->Set_Backlight(NewBacklightValue);
            }

            usleep( 30 * 1000);
        }

        return NULL;
    }
}
