/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#define LOG_MOUDLE_TAG "PQ"
#define LOG_CLASS_TAG "COverScandb"

#include "COverScandb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

COverScandb::COverScandb()
{
}

COverScandb::~COverScandb()
{
}

int COverScandb::openOverScanDB(const char *db_path)
{
    LOGD("%s: path = %s.\n", __FUNCTION__, db_path);
    int rval;

    if (access(db_path, 0) < 0) {
        LOGE("OVERSCAN_DB don't exist!\n");
        return -1;
    }

    closeDb();
    rval = openDb(db_path);
    if (rval == 0) {
        std::string OverScanDBToolVersion, OverScanDBVersion, OverScanDBGenerateTime, OverScanDBChipVersion, val;
        if (GetOverScanDbVersion(OverScanDBToolVersion, OverScanDBVersion, OverScanDBGenerateTime, OverScanDBChipVersion)) {
            val = OverScanDBToolVersion + " " + OverScanDBVersion + " " + OverScanDBGenerateTime+ " " + OverScanDBChipVersion;
        } else {
            val = "Get OverScan_DB Verion failure!!!";
        }
        LOGD("%s = %s\n", "OverScan.db.version", val.c_str());
    }

    return rval;
}

bool COverScandb::GetOverScanDbVersion(std::string& ToolVersion, std::string& ProjectVersion, std::string& GenerateTime, std::string& ChipVersion)
{
    bool ret = false;
    CSqlite::Cursor c;
    char sqlmaster[256];

    bool chipVersionExist = false;
    if (CheckIdExistInDb("ChipVersion", "PQ_VersionTable")) {
       chipVersionExist = true;
       getSqlParams(__FUNCTION__, sqlmaster,
                  "select ToolVersion,ProjectVersion,ChipVersion,GenerateTime from PQ_VersionTable;");
    } else {
       chipVersionExist = false;
       getSqlParams(__FUNCTION__, sqlmaster,
                  "select ToolVersion,ProjectVersion,GenerateTime from PQ_VersionTable;");
    }

    int rval = this->select(sqlmaster, c);

    if (!rval && c.getCount() > 0) {
        ToolVersion =  c.getString(0);
        ProjectVersion = c.getString(1);
        if (chipVersionExist) {
            ChipVersion = c.getString(2);
            GenerateTime = c.getString(3);
        } else {
            ChipVersion = std::string("");
            GenerateTime = c.getString(2);
        }
        ret = true;
    }

    return ret;
}

int COverScandb::PQ_GetOverscanParams(source_input_param_t source_input_param, vpp_display_mode_t dmode, tvin_cutwin_t *cutwin_t)
{
    CSqlite::Cursor c;
    char sqlmaster[256];
    int rval = -1;
    char table_name[30];
    tv_source_input_t source_input = source_input_param.source_input;
    tvin_sig_fmt_t fmt = source_input_param.sig_fmt;
    tvin_trans_fmt_t trans_fmt = source_input_param.trans_fmt;

    memset(table_name, 0, sizeof(table_name));
    switch (dmode) {
        case VPP_DISPLAY_MODE_169 :
            strcpy(table_name, "OVERSCAN_16_9");
            break;
        case VPP_DISPLAY_MODE_PERSON :
            strcpy(table_name, "OVERSCAN_PERSON");
            break;
        case VPP_DISPLAY_MODE_MOVIE :
            strcpy(table_name, "OVERSCAN_MOVIE");
            break;
        case VPP_DISPLAY_MODE_CAPTION :
            strcpy(table_name, "OVERSCAN_CAPTION");
            break;
        case VPP_DISPLAY_MODE_MODE43 :
            strcpy(table_name, "OVERSCAN_4_3");
            break;
        case VPP_DISPLAY_MODE_FULL :
            strcpy(table_name, "OVERSCAN_FULL");
            break;
        case VPP_DISPLAY_MODE_NORMAL :
            strcpy(table_name, "OVERSCAN_NORMAL");
            break;
        case VPP_DISPLAY_MODE_NOSCALEUP :
            strcpy(table_name, "OVERSCAN_NOSCALEUP");
            break;
        case VPP_DISPLAY_MODE_CROP_FULL :
            strcpy(table_name, "OVERSCAN_CROP_FULL");
            break;
        case VPP_DISPLAY_MODE_CROP :
            strcpy(table_name, "OVERSCAN_CROP");
            break;
        case VPP_DISPLAY_MODE_ZOOM :
            strcpy(table_name, "OVERSCAN_ZOOM");
            break;
        default :
            strcpy(table_name, "OVERSCAN_NORMAL");
            break;
    }

    memset(cutwin_t, 0, sizeof(tvin_cutwin_t));
    getSqlParams(__FUNCTION__, sqlmaster, "select hs, he, vs, ve from %s where "
                 "TVIN_PORT = %d and "
                 "TVIN_SIG_FMT = %d and "
                 "TVIN_TRANS_FMT = %d ;", table_name, source_input, fmt, trans_fmt);

    rval = this->select(sqlmaster, c);

    if (c.getCount() <= 0) {
        fmt = TVIN_SIG_FMT_NULL;
        c.close();
        LOGD ("%s - Load default.\n", __FUNCTION__);

        getSqlParams(__FUNCTION__, sqlmaster, "select hs, he, vs, ve from %s where "
                                              "TVIN_PORT = %d and "
                                              "TVIN_SIG_FMT = %d and "
                                              "TVIN_TRANS_FMT = %d ;", table_name, source_input, fmt, trans_fmt);
        this->select(sqlmaster, c);
    }

    if (c.moveToFirst()) {
        cutwin_t->hs = c.getInt(0);
        cutwin_t->he = c.getInt(1);
        cutwin_t->vs = c.getInt(2);
        cutwin_t->ve = c.getInt(3);
    }
    return rval;
}
int COverScandb::PQ_SetOverscanParams(source_input_param_t source_input_param , tvin_cutwin_t cutwin_t)
{
    CSqlite::Cursor c;
    char sqlmaster[256];
    int rval = -1;
    tv_source_input_t source_input = source_input_param.source_input;
    tvin_sig_fmt_t fmt = source_input_param.sig_fmt;
    tvin_trans_fmt_t trans_fmt = source_input_param.trans_fmt;

    getSqlParams(__FUNCTION__, sqlmaster,
        "select * from OVERSCAN where TVIN_PORT = %d and TVIN_SIG_FMT = %d and TVIN_TRANS_FMT = %d;",
        source_input, fmt, trans_fmt);

    rval = this->select(sqlmaster, c);

    if (c.getCount() <= 0) {
        fmt = TVIN_SIG_FMT_NULL;
        c.close();
        LOGE ("%s - Load default.\n", __FUNCTION__);

        getSqlParams(__FUNCTION__, sqlmaster,
                    "select * from OVERSCAN where TVIN_PORT = %d and TVIN_SIG_FMT = %d and TVIN_TRANS_FMT = %d;",
                    source_input, fmt, trans_fmt);
        this->select(sqlmaster, c);
    }

    if (c.moveToFirst()) {
        getSqlParams(__FUNCTION__, sqlmaster,
            "update OVERSCAN set hs = %d, he = %d, vs = %d, ve = %d where TVIN_PORT = %d and TVIN_SIG_FMT = %d and TVIN_TRANS_FMT = %d;",
            cutwin_t.hs, cutwin_t.he, cutwin_t.vs, cutwin_t.ve, source_input, fmt, trans_fmt);
    } else {
        getSqlParams(__FUNCTION__, sqlmaster,
            "Insert into OVERSCAN(TVIN_PORT, TVIN_SIG_FMT, TVIN_TRANS_FMT, hs, he, vs, ve) values(%d, %d, %d ,%d ,%d, %d, %d);",
            source_input, fmt, trans_fmt, cutwin_t.hs, cutwin_t.he, cutwin_t.vs, cutwin_t.ve);
    }

    if (this->exeSql(sqlmaster)) {
        rval = 0;
    } else {
        LOGE("%s--SQL error!\n",__FUNCTION__);
        rval = -1;
    }

    return rval;
}
int COverScandb::PQ_ResetAllOverscanParams(void)
{
    int rval;
    char sqlmaster[256];

    getSqlParams(
        __FUNCTION__,
        sqlmaster,
        "delete from OVERSCAN; insert into OVERSCAN(TVIN_PORT, TVIN_SIG_FMT, TVIN_TRANS_FMT, hs, he, vs, ve) select TVIN_PORT, TVIN_SIG_FMT, TVIN_TRANS_FMT, hs, he, vs, ve from OVERSCAN_default;");
    if (this->exeSql(sqlmaster)) {
        rval = 0;
    } else {
        LOGE("%s--SQL error!\n",__FUNCTION__);
        rval = -1;
    }

    return rval;
}

int COverScandb::PQ_GetPQModeParams(tv_source_input_t source_input, vpp_picture_mode_t pq_mode,
                                vpp_pq_para_t *params)
{
    CSqlite::Cursor c;
    char sqlmaster[256];

    int rval = -1;

    getSqlParams(__FUNCTION__, sqlmaster,
                 "select Brightness, Contrast, Saturation, Hue, Sharpness, Backlight, NR from Picture_Mode where "
                 "TVIN_PORT = %d and "
                 "Mode = %d ;", source_input, pq_mode);

    rval = this->select(sqlmaster, c);

    if (c.moveToFirst()) {
        params->brightness = c.getInt(0);
        params->contrast = c.getInt(1);
        params->saturation = c.getInt(2);
        params->hue = c.getInt(3);
        params->sharpness = c.getInt(4);
        params->backlight = c.getInt(5);
        params->nr = c.getInt(6);
    } else {
        LOGE("%s error!\n",__FUNCTION__);
        rval = -1;
    }
    return rval;
}

int COverScandb::PQ_SetPQModeParams(tv_source_input_t source_input, vpp_picture_mode_t pq_mode, vpp_pq_para_t *params)
{
    int rval;
    char sql[256];

    getSqlParams(__FUNCTION__, sql,
        "update Picture_Mode set Brightness = %d, Contrast = %d, Saturation = %d, Hue = %d, Sharpness = %d, Backlight = %d, NR= %d "
        " where TVIN_PORT = %d and Mode = %d;", params->brightness, params->contrast,
        params->saturation, params->hue, params->sharpness, params->backlight, params->nr,
        source_input, pq_mode);
    if (this->exeSql(sql)) {
        rval = 0;
    } else {
        LOGE("%s--SQL error!\n",__FUNCTION__);
        rval = -1;
    }
    return rval;
}

int COverScandb::PQ_SetPQModeParamsByName(const char *name, tv_source_input_t source_input,
                                      vpp_picture_mode_t pq_mode, vpp_pq_para_t *params)
{
    int rval;
    char sql[256];

    getSqlParams(__FUNCTION__, sql,
                 "insert into %s(TVIN_PORT, Mode, Brightness, Contrast, Saturation, Hue, Sharpness, Backlight, NR)"
                 " values(%d,%d,%d,%d,%d,%d,%d,%d,%d);", name, source_input, pq_mode,
                 params->brightness, params->contrast, params->saturation, params->hue,
                 params->sharpness, params->backlight, params->nr);

    if (this->exeSql(sql)) {
        rval = 0;
    } else {
        LOGE("%s--SQL error!\n",__FUNCTION__);
        rval = -1;
    }

    return rval;
}

int COverScandb::PQ_ResetAllPQModeParams(void)
{
    int rval;
    char sqlmaster[256];

    getSqlParams(
        __FUNCTION__,
        sqlmaster,
        "delete from Picture_Mode; insert into Picture_Mode(TVIN_PORT, Mode, Brightness, Contrast, Saturation, Hue, Sharpness, Backlight, NR) select TVIN_PORT, Mode, Brightness, Contrast, Saturation, Hue, Sharpness, Backlight, NR from picture_mode_default;");

    if (this->exeSql(sqlmaster)) {
        rval = 0;
    } else {
        LOGE("%s--SQL error!\n",__FUNCTION__);
        rval = -1;
    }

    return rval;
}

bool COverScandb::CheckIdExistInDb(const char *Id, const char *TableName)
{
    bool ret = false;
    char sqlmaster[256];
    CSqlite::Cursor tempCursor;

    getSqlParams(__FUNCTION__, sqlmaster,
                 "select sql from sqlite_master where type = 'table' and tbl_name = '%s';", TableName);

    int retVal = this->select(sqlmaster, tempCursor);
    if ((retVal == 0) && (tempCursor.moveToFirst())) {
        if (strstr(tempCursor.getString(0).c_str(), Id) != NULL) {
            ret = true;
        } else {
            ret = false;
        }
    } else {
        LOGE("%s: error!\n", __FUNCTION__);
        ret = false;
    }

    /*if (ret) {
        SYS_LOGE("%s: %s exist in %s!\n", __FUNCTION__, Id, TableName);
    } else {
        SYS_LOGE("%s: %s don't exist in %s!\n", __FUNCTION__, Id, TableName);
    }*/

    return ret;
}

