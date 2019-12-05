/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef AMLOGIC_TVCMD_H
#define AMLOGIC_TVCMD_H

enum tvcmd_e {
    //Tv CMD(0~99)
    TV_CONTROL_CMD_START          = 0,
    TV_CONTROL_OPEN_TV            = 1,
    TV_CONTROL_CLOSE_TV           = 2,
    TV_CONTROL_START_TV           = 3,
    TV_CONTROL_STOP_TV            = 4,
    TV_CONTROL_CMD_MAX            = 99,

    //hdmi CMD(101~199)
    HDMI_CONTROL_CMD_START        = 100,
    HDMI_EDID_VER_SET             = 101,
    HDMI_EDID_VER_GET             = 102,
    HDMI_HDCP_KEY_ENABLE          = 103,
    HDMI_COLOR_RANGE_MODE_SET     = 104,
    HDMI_COLOR_RANGE_MODE_GET     = 105,
    HDMI_CONTROL_CMD_MAX          = 199,

    //PQ CMD(201~299)
    PQ_MOUDLE_CMD_START           = 200,
    PQ_SET_PICTURE_MODE           = 201,
    PQ_GET_PICTURE_MODE           = 202,
    PQ_SET_BRIGHTNESS             = 203,
    PQ_GET_BRIGHTNESS             = 204,
    PQ_SET_CONTRAST               = 205,
    PQ_GET_CONTRAST               = 206,
    PQ_SET_SATUATION              = 207,
    PQ_GET_SATUATION              = 208,
    PQ_SET_HUE                    = 209,
    PQ_GET_HUE                    = 210,
    PQ_SET_SHARPNESS              = 211,
    PQ_GET_SHARPNESS              = 212,
    PQ_SET_COLOR_TEMPERATURE_MODE = 213,
    PQ_GET_COLOR_TEMPERATURE_MODE = 214,
    PQ_SET_NOISE_REDUCTION_MODE   = 215,
    PQ_GET_NOISE_REDUCTION_MODE   = 216,
    PQ_SET_GAMMA                  = 217,
    PQ_GET_GAMMA                  = 218,
    PQ_SET_BACKLIGHT              = 219,
    PQ_GET_BACKLIGHT              = 220,
    PQ_SET_ASPECT_RATIO           = 221,
    PQ_GET_ASPECT_RATIO           = 222,
    PQ_MOUDLE_CMD_MAX             = 299,
};

#endif  //AMLOGIC_TVCMD_H
