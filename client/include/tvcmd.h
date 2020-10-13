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
    TV_CONTROL_VDIN_WORK_MODE_SET = 98,
    TV_CONTROL_CMD_MAX            = 99,

    //hdmi CMD(101~199)
    HDMI_CONTROL_CMD_START        = 100,
    HDMI_EDID_VER_SET             = 101,
    HDMI_EDID_VER_GET             = 102,
    HDMI_EDID_DATA_SET            = 103,
    HDMI_EDID_DATA_GET            = 104,
    HDMI_CONTROL_CMD_MAX          = 199,
};

#endif  //AMLOGIC_TVCMD_H
