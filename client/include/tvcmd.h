#ifndef AMLOGIC_TVCMD_H
#define AMLOGIC_TVCMD_H

enum tvcmd_e {
    //Tv CMD START
    OPEN_TV = 1,
    CLOSE_TV = 2,
    START_TV = 3,
    STOP_TV = 4,
    //Tv CMD END

    //hdmi CMD START
    HDMI_EDID_VER_SET = 100,
    HDMI_EDID_VER_GET = 101,
    HDMI_HDCP_KEY_ENABLE = 102,
    HDMI_COLOR_RANGE_MODE_SET = 103,
    HDMI_COLOR_RANGE_MODE_GET = 104,
    //hdmi CMD END

    //PQ CMD START
    PQ_LOAD_PARAM = 200,
    //PQ CMD END
};

#endif  //AMLOGIC_TVCMD_H
