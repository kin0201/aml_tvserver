/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_MOUDLE_TAG "PQ"
#define LOG_CLASS_TAG "CPQFile"
//#define LOG_NDEBUG 0

#include "CPQFile.h"

CPQFile::CPQFile()
{
    mPath[0] = '\0';
    mFd = -1;
}

CPQFile::CPQFile(const char *path)
{
    strcpy(mPath, path);
    mFd = -1;
}

CPQFile::~CPQFile()
{
    PQ_CloseFile();
}

int CPQFile::PQ_OpenFile(const char *path)
{
    LOGD("openFile = %s.\n", path);
    if (mFd < 0) {
        const char *openPath = mPath;
        if (path != NULL)
            strcpy(mPath, path);

        if (strlen(openPath) <= 0) {
            LOGE("openFile openPath is NULL, path:%s.\n", path);
            return -1;
        }
        mFd = open(openPath, O_RDWR);
        if (mFd < 0) {
            LOGE("open file(%s) fail.\n", openPath);
            return -1;
        }
    }
    return mFd;
}

int CPQFile::PQ_CloseFile()
{
    if (mFd > 0) {
        close(mFd);
        mFd = -1;
    }
    return 0;
}

int CPQFile::PQ_WriteFile(const unsigned char *pData, int uLen)
{
    int ret = -1;
    if (mFd > 0)
        ret = write(mFd, pData, uLen);

    return ret;
}

int CPQFile::PQ_ReadFile(void *pBuf, int uLen)
{
    int ret = 0;
    if (mFd > 0) {
        ret = read(mFd, pBuf, uLen);
    }
    return ret;
}

int CPQFile::PQ_CopyFile(const char *dstPath)
{
    if (strlen(mPath) <= 0)
        return -1;
    int dstFd;
    if (mFd == -1) {
        if ((mFd = open(mPath, O_RDONLY)) == -1) {
            LOGE("Open %s Error:%s.\n", mPath, strerror(errno));
            return -1;
        }
    }

    if ((dstFd = open(dstPath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
        LOGE("Open %s Error:%s.\n", dstPath, strerror(errno));
    }

    int bytes_read, bytes_write;
    char buffer[BUFFER_SIZE];
    char *ptr;
    int ret = 0;
    while ((bytes_read = read(mFd, buffer, BUFFER_SIZE))) {
        /* read error */
        if ((bytes_read == -1) && (errno != EINTR)) {
            ret = -1;
            break;
        } else if (bytes_read > 0) {
            ptr = buffer;
            while ((bytes_write = write(dstFd, ptr, bytes_read))) {
                /* failed to write halfway */
                if ((bytes_write == -1) && (errno != EINTR)) {
                    ret = -1;
                    break;
                }
                /* write finish */
                else if (bytes_write == bytes_read) {
                    ret = 0;
                    break;
                }
                /* write continue */
                else if (bytes_write > 0) {
                    ptr += bytes_write;
                    bytes_read -= bytes_write;
                }
            }
            /* failed to write at first */
            if (bytes_write == -1) {
                ret = -1;
                break;
            }
        }
    }
    fsync(dstFd);
    close(dstFd);
    return ret;
}

bool CPQFile::PQ_IsFileExist(const char *file_name)
{
    struct stat tmp_st;
    int ret = -1;

    ret = stat(file_name, &tmp_st);
    if (ret != 0 ) {
       LOGE("%s don't exist!\n",file_name);
       return false;
    } else {
       return true;
    }
}

int CPQFile::PQ_DeleteFile(const char *path)
{
    if (strlen(path) <= 0) return -1;
    if (unlink(path) != 0) {
        LOGE("delete file(%s) err=%s.\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

int CPQFile::PQ_DeleteFile()
{
    if (strlen(mPath) <= 0) return -1;
    if (unlink(mPath) != 0) {
        LOGE("delete file(%s) err=%s.\n", mPath, strerror(errno));
        return -1;
    }
    return 0;
}

int CPQFile::PQ_GetFileAttrValue(const char *path)
{
    int value;

    int fd = open(path, O_RDONLY);
    if (fd <= 0) {
        LOGE("open  (%s)ERROR!!error = %s.\n", path, strerror ( errno ));
    }
    char s[8];
    read(fd, s, sizeof(s));
    close(fd);
    value = atoi(s);
    return value;
}

int CPQFile::PQ_SetFileAttrValue(const char *path, int value)
{
    FILE *fp = fopen ( path, "w" );

    if ( fp == NULL ) {
        LOGE ( "Open %s error(%s)!\n", path, strerror ( errno ) );
        return -1;
    }
    fprintf ( fp, "%d", value );
    fclose ( fp );
    return 0;
}

int CPQFile::PQ_GetFileAttrStr(__unused const char *path , __unused char *str)
{
    return 0;
}

int CPQFile::PQ_SetFileAttrStr(const char *path, const char *str)
{
    FILE *fp = fopen ( path, "w" );

    if ( fp == NULL ) {
        LOGE ( "Open %s error(%s)!\n", path, strerror ( errno ) );
        return -1;
    }
    fprintf ( fp, "%s", str );
    fclose ( fp );
    return 0;
}

int CPQFile::PQ_GetFileFd()
{
    return mFd;
};
