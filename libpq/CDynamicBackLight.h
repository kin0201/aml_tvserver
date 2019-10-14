/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */
#ifndef _DYNAMIC_BACKLIGHT_H
#define _DYNAMIC_BACKLIGHT_H

#include "PQTypeDefine.h"
#include "CPQLog.h"


#define ONE_FRAME_TIME             15*1000
#define COLOR_RANGE_MODE           0
#define SYSFS_HIST_SEL             "/sys/module/am_vecm/parameters/vpp_hist_sel"

typedef struct dynamic_backlight_Param_s {
    int CurBacklightValue;
    int UiBackLightValue;
    Dynamic_backlight_status_t CurDynamicBacklightMode;
    ve_hist_s hist;
    int VideoStatus;
} dynamic_backlight_Param_t;


class CDynamicBackLight {
public:
    CDynamicBackLight();
    ~CDynamicBackLight();
    int startDected(void);
    void gd_fw_alg_frm(int average, int *tf_bl_value, int *LUT);
    class IDynamicBackLightObserver {
        public:
            IDynamicBackLightObserver() {};
            virtual ~IDynamicBackLightObserver() {};
            virtual void Set_Backlight(int value) {};
            virtual void GetDynamicBacklighConfig(int *thtf, int *lut_mode, int *heigh_param, int *low_param) {};
            virtual void GetDynamicBacklighParam(dynamic_backlight_Param_t *DynamicBacklightParam) {};
    };

    void setObserver (IDynamicBackLightObserver *pOb)
    {
        mpObserver = pOb;
    };
private:
    int mPreBacklightValue;
    int mGD_mvreflsh;
    int mArithmeticPauseTime;
    int GD_LUT_MODE;
    int GD_ThTF;
    int backLightScale(int, int);
    static void *dynamicBacklightThread(void* data);
    void *threadLoop(void);
    IDynamicBackLightObserver *mpObserver;
};

#endif

