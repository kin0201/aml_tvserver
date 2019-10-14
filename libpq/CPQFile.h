/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef C_PQFILE_H
#define C_PQFILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "CPQLog.h"

#define CC_MAX_FILE_PATH_LEN       (256)

#define BUFFER_SIZE 1024

class CPQFile {
public:
    CPQFile(const char *path);
    CPQFile();
    ~CPQFile();
    int PQ_OpenFile(const char *path);
    int PQ_CloseFile();
    int PQ_WriteFile(const unsigned char *pData, int uLen);
    int PQ_ReadFile(void *pBuf, int uLen);
    int PQ_CopyFile(const char *dstPath);
    bool PQ_IsFileExist(const char *file_name);
    int PQ_DeleteFile(const char *path);
    int PQ_DeleteFile();
    int PQ_GetFileAttrValue(const char *path);
    int PQ_SetFileAttrValue(const char *path, int value);
    int PQ_GetFileAttrStr(const char *path, char *str);
    int PQ_SetFileAttrStr(const char *path, const char *str);
    int PQ_GetFileFd();
protected:
    char mPath[CC_MAX_FILE_PATH_LEN];
    int  mFd;
};
#endif
