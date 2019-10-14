/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef _TV_UTILS_H_
#define _TV_UTILS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <map>

#ifndef DVB_SUCCESS
#define DVB_SUCCESS     (0)
#endif

#ifndef DVB_FAILURE
#define DVB_FAILURE     (-1)
#endif

#define SYS_STR_LEN                         1024
#define PROPERTY_VALUE_MAX                  92
#define SYS_VIDEO_INUSE_PATH                "/sys/class/video/video_inuse"
#define AUDIO_STD_PATH                      "/sys/kernel/debug/aml_atvdemod/aud_std"
#define AUDIO_OUTMODE_PATH                  "/sys/kernel/debug/aml_atvdemod/aud_mode"
#define ATVDEMODE_DEBUG_PATH                "/sys/class/aml_atvdemod/atvdemod_debug"
#define AUDIO_STREAM_OUTMODE_PATH           "/sys/kernel/debug/aml_atvdemod/signal_audmode"

int tvReadSysfs(const char *path, char *value);
int tvWriteSysfs(const char *path, const char *value);
int tvWriteSysfs(const char *path, int value, int base=10);
extern bool isFileExist(const char *file_name);
extern int readSysfs(const char *path, char *value);
extern void writeSysfs(const char *path, const char *value);
extern int GetFileAttrIntValue(const char *fp, int flag = O_RDWR);

#if 0
template<typename T1, typename T2>
int ArrayCopy(T1 dst_buf[], T2 src_buf[], int copy_size)
{
    int i = 0;

    for (i = 0; i < copy_size; i++) {
        dst_buf[i] = src_buf[i];
    }

    return 0;
};

typedef std::map<std::string, std::string> STR_MAP;

extern void jsonToMap(STR_MAP &m, const std::string &j);
extern void mapToJson(std::string &j, const STR_MAP &m);
extern void mapToJson(std::string &j, const STR_MAP &m, const std::string &k);
extern void mapToJsonAppend(std::string &j, const STR_MAP &m, const std::string &k);

class Paras {
protected:
    STR_MAP mparas;

public:
    Paras() {}
    Paras(const Paras &paras):mparas(paras.mparas) {}
    Paras(const char *paras) { jsonToMap(mparas, std::string(paras)); }
    Paras(const STR_MAP &paras):mparas(paras) {}

    void clear() { mparas.clear(); }

    int getInt (const char *key, int def) const;
    void setInt(const char *key, int v);

    const std::string toc_str() { std::string s; mapToJson(s, mparas); return s; }

    Paras operator + (const Paras &p);
    Paras& operator = (const Paras &p) { mparas = p.mparas; return *this; };
};

float getUptimeSeconds();

extern int jsonGetInt(const char *json, const char *obj, const char *value, int def);
extern const std::string jsonGetString(const char *json, const char *obj, const char *value, const char *def);

extern int paramGetInt(const char *param, const char *section, const char *value, int def);
extern const std::string paramGetString(const char *param, const char *section, const char *value, const char *def);
#endif
#endif  //__TV_MISC_H__
