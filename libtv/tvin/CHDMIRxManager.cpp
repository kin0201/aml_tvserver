/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_TAG "tvserver"
#define LOG_TV_TAG "CHDMIRxManager"

#include "CHDMIRxManager.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <CTvLog.h>
#include <tvutils.h>


CHDMIRxManager::CHDMIRxManager()
{

}

CHDMIRxManager::~CHDMIRxManager()
{
}

int CHDMIRxManager::HdmiRxEdidUpdate()
{
    int m_hdmi_fd = -1;
    m_hdmi_fd = open(CS_HDMIRX_DEV_PATH, O_RDWR);
    if (m_hdmi_fd < 0) {
        LOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, CS_HDMIRX_DEV_PATH, strerror ( errno ));
        return -1;
    }
    ioctl(m_hdmi_fd, HDMI_IOC_EDID_UPDATE, 1);
    close(m_hdmi_fd);
    m_hdmi_fd = -1;
    return 0;
}

int CHDMIRxManager::HdmiRxHdcpVerSwitch(tv_hdmi_hdcp_version_t version)
{
    int m_hdmi_fd = -1;
    m_hdmi_fd = open(CS_HDMIRX_DEV_PATH, O_RDWR);
    if (m_hdmi_fd < 0) {
        LOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, CS_HDMIRX_DEV_PATH, strerror ( errno ));
        return -1;
    }
    if (HDMI_HDCP_VER_14 == version) {
        ioctl(m_hdmi_fd, HDMI_IOC_HDCP22_FORCE14, NULL);
    } else if (HDMI_HDCP_VER_22 == version) {
        ioctl(m_hdmi_fd, HDMI_IOC_HDCP22_AUTO, NULL);
    } else {
        close(m_hdmi_fd);
        m_hdmi_fd = -1;
        return -1;
    }
    close(m_hdmi_fd);
    m_hdmi_fd = -1;
    return 0;
}

int CHDMIRxManager::HdmiRxHdcpOnOff(tv_hdmi_hdcpkey_enable_t flag)
{
    int m_hdmi_fd = -1;

    m_hdmi_fd = open(CS_HDMIRX_DEV_PATH, O_RDWR);
    if (m_hdmi_fd < 0) {
        LOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, CS_HDMIRX_DEV_PATH, strerror ( errno ));
        return -1;
    }

    if (hdcpkey_enable == flag) {
        ioctl(m_hdmi_fd, HDMI_IOC_HDCP_ON, NULL);
    }else if (hdcpkey_enable == flag) {
        ioctl(m_hdmi_fd, HDMI_IOC_HDCP_OFF, NULL);
    }else {
        return -1;
    }

    close(m_hdmi_fd);
    m_hdmi_fd = -1;

    return 0;
}

int CHDMIRxManager::GetHdmiHdcpKeyKsvInfo(struct _hdcp_ksv *msg)
{
    int m_hdmi_fd = -1;

    m_hdmi_fd = open(CS_HDMIRX_DEV_PATH, O_RDWR);
    if (m_hdmi_fd < 0) {
        LOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, CS_HDMIRX_DEV_PATH, strerror ( errno ));
        return -1;
    }

    ioctl(m_hdmi_fd, HDMI_IOC_HDCP_GET_KSV, msg);

    close(m_hdmi_fd);
    m_hdmi_fd = -1;

    return 0;
}

int CHDMIRxManager::SetHdmiColorRangeMode(tv_hdmi_color_range_t range_mode)
{
    char val[5] = {0};
    sprintf(val, "%d", range_mode);
    tvWriteSysfs(HDMI_FORCE_COLOR_RANGE, val);
    return 0;
}

tv_hdmi_color_range_t CHDMIRxManager::GetHdmiColorRangeMode()
{
    char value[5] = {0};
    tvReadSysfs(HDMI_FORCE_COLOR_RANGE, value);

    return (tv_hdmi_color_range_t)atoi(value);
}

int CHDMIRxManager::SetHdmiPortCecPhysicAddr()
{
    char val[10] = {0};
    tv_source_input_t tmpHdmiPortCecPhysicAddr[4] = {SOURCE_MAX};
    tvin_port_t tvInport[4] = {TVIN_PORT_HDMI0,TVIN_PORT_HDMI1,TVIN_PORT_HDMI2,TVIN_PORT_HDMI3};
    int HdmiPortCecPhysicAddr = 0x0;
    for (int i = 0; i < 4; i++) {
        tmpHdmiPortCecPhysicAddr[i] = CTvin::getInstance()->Tvin_PortToSourceInput(tvInport[i]);
    }
    HdmiPortCecPhysicAddr |= ((tmpHdmiPortCecPhysicAddr[0] == SOURCE_MAX? 0xf:(tmpHdmiPortCecPhysicAddr[0]-4))
                             |((tmpHdmiPortCecPhysicAddr[1] == SOURCE_MAX? 0xf:(tmpHdmiPortCecPhysicAddr[1]-4)) << 4)
                             |((tmpHdmiPortCecPhysicAddr[2] == SOURCE_MAX? 0xf:(tmpHdmiPortCecPhysicAddr[2]-4)) << 8)
                             |((tmpHdmiPortCecPhysicAddr[3] == SOURCE_MAX? 0xf:(tmpHdmiPortCecPhysicAddr[3]-4)) << 12));
    sprintf(val, "%x", HdmiPortCecPhysicAddr);
    tvWriteSysfs(HDMI_CEC_PORT_SEQUENCE, val);
    memset(val,0,10);
    sprintf(val, "%d", HdmiPortCecPhysicAddr);
    tvWriteSysfs(HDMI_CEC_PORT_MAP,val);
    return 0;
}

int CHDMIRxManager::SetHdmiPcMode_Monitor(int value)
{
    int m_hdmi_fd = -1;
    m_hdmi_fd = open(CS_HDMIRX_DEV_PATH, O_RDWR);
    if (m_hdmi_fd < 0) {
        LOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, CS_HDMIRX_DEV_PATH, strerror ( errno ));
        return -1;
    }
    if (value == 1) {
        ioctl(m_hdmi_fd, HDMI_IOC_PC_MODE_ON, NULL);
    }else if (value == 0) {
        ioctl(m_hdmi_fd, HDMI_IOC_PC_MODE_OFF, NULL);
    }
    close(m_hdmi_fd);
    return 0;
}

