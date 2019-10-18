/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_MOUDLE_TAG "PQ"
#define LOG_CLASS_TAG "CSqlite"

#include "CSqlite.h"

pthread_mutex_t SqliteMutex = PTHREAD_MUTEX_INITIALIZER;

CSqlite::Cursor::Cursor()
{
    mData = NULL;
    mCurRowIndex = 0;
    mRowNums = 0;
    mColNums = 0;
    mIsClosed = false;
}

CSqlite::Cursor::~Cursor()
{
    close();
}

void CSqlite::Cursor::Init(char **data, int cow, int col)
{
    if (mData != NULL) {
        sqlite3_free_table(mData);
    }

    mData = data;
    mCurRowIndex = 0;
    mRowNums = cow;
    mColNums = col;
    mIsClosed = false;
}

int CSqlite::Cursor::getCount()
{
    return mRowNums;
}

int CSqlite::Cursor::getPosition()
{
    return 0;
}

bool CSqlite::Cursor::move(int offset)
{
    return false;
}

bool CSqlite::Cursor::moveToPosition(int position)
{
    return false;
}

bool CSqlite::Cursor::moveToFirst()
{
    if (mRowNums <= 0) {
        return false;
    } else {
        mCurRowIndex = 0;
        return true;
    }
}

bool CSqlite::Cursor::moveToLast()
{
    return false;
}

bool CSqlite::Cursor::moveToNext()
{
    if (mCurRowIndex >= mRowNums - 1) {
        return false;
    } else {
        mCurRowIndex++;
        return true;
    }
}

bool CSqlite::Cursor::moveToPrevious()
{
    return false;
}

int CSqlite::Cursor::getColumnIndex(const char *columnName)
{
    int index = 0;
    for (int i = 0; i < mColNums; i++) {
        if (strcmp(columnName, mData[i]) == 0)
            return index;
        index++;
    }

    return -1;
}

int CSqlite::Cursor::getColumnCount()
{
    return -1;
}

/*int CSqlite::Cursor::getString(char *str, int columnIndex)
{
    if (columnIndex >= mColNums || str == NULL) return -1;
    strcpy(str, mData[mColNums * (mCurRowIndex + 1) + columnIndex]);
    return 0;
}*/

std::string CSqlite::Cursor::getString(int columnIndex)
{
    if (columnIndex >= mColNums) {
        return std::string("");
    } else {
        return std::string(mData[mColNums * (mCurRowIndex + 1) + columnIndex]);
    }
}

int CSqlite::Cursor::getInt(int columnIndex)
{
    return atoi(mData[mColNums * (mCurRowIndex + 1) + columnIndex]);
}

unsigned long int CSqlite::Cursor::getUInt(int columnIndex)
{
    return strtoul(mData[mColNums * (mCurRowIndex + 1) + columnIndex], NULL, 10);
}

double CSqlite::Cursor::getF(int columnIndex)
{
    return atof(mData[mColNums * (mCurRowIndex + 1) + columnIndex]);
}

int CSqlite::Cursor::getType(int columnIndex)
{
    return -1;
}

void CSqlite::Cursor::close()
{
    if (mData != NULL) {
        sqlite3_free_table(mData);
    }

    mData = NULL;
    mCurRowIndex = 0;
    mRowNums = 0;
    mIsClosed = true;
}

bool CSqlite::Cursor::isClosed()
{
    return mIsClosed;
}

CSqlite::CSqlite()
{
    mHandle = NULL;
}

CSqlite::~CSqlite()
{
    pthread_mutex_lock(&SqliteMutex);
    if (mHandle != NULL) {
        sqlite3_close(mHandle);
        mHandle = NULL;
    }
    pthread_mutex_unlock(&SqliteMutex);
}

int CSqlite::sqlite3_exec_callback(void *data __unused, int nColumn, char **colValues __unused, char **colNames __unused)
{
    LOGD("%s: nums = %d", __FUNCTION__, nColumn);
    return 0;
}

int CSqlite::openDb(const char *path)
{
    pthread_mutex_lock(&SqliteMutex);
    if (sqlite3_open(path, &mHandle) != SQLITE_OK) {
        LOGD("open db(%s) error", path);
        mHandle = NULL;
        pthread_mutex_unlock(&SqliteMutex);
        return -1;
    }
    pthread_mutex_unlock(&SqliteMutex);
    return 0;
}

int CSqlite::closeDb()
{
    pthread_mutex_lock(&SqliteMutex);
    int rval = 0;
    if (mHandle != NULL) {
        rval = sqlite3_close(mHandle);
        mHandle = NULL;
    }
    pthread_mutex_unlock(&SqliteMutex);
    return rval;
}

void CSqlite::setHandle(sqlite3 *h)
{
    mHandle = h;
}

sqlite3 *CSqlite::getHandle()
{
    return mHandle;
}

int CSqlite::select(const char *sql, CSqlite::Cursor &c)
{
    pthread_mutex_lock(&SqliteMutex);
    int col, row;
    char **pResult = NULL;
    char *errmsg;
    //assert(mHandle && sql);

    if (strncmp(sql, "select", 6)) {
        pthread_mutex_unlock(&SqliteMutex);
        return -1;
    }
    if (sqlite3_get_table(mHandle, sql, &pResult, &row, &col, &errmsg) != SQLITE_OK) {
        LOGE("errmsg=%s.\n", errmsg);
        if (pResult != NULL)
            sqlite3_free_table(pResult);
        pthread_mutex_unlock(&SqliteMutex);
        return -1;
    }

    c.Init(pResult, row, col);
    pthread_mutex_unlock(&SqliteMutex);
    return 0;
}

void CSqlite::insert()
{
}

bool CSqlite::exeSql(const char *sql)
{
    pthread_mutex_lock(&SqliteMutex);
    char *errmsg;
    if (sql == NULL) {
        pthread_mutex_unlock(&SqliteMutex);
        return false;
    }

    if (sqlite3_exec(mHandle, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        LOGE("exeSql=: %s error=%s", sql, errmsg ? errmsg : "Unknown");
        if (errmsg)
            sqlite3_free(errmsg);
        pthread_mutex_unlock(&SqliteMutex);
        return false;
    }
    pthread_mutex_unlock(&SqliteMutex);
    return true;
}

bool CSqlite::beginTransaction()
{
    return exeSql("begin;");
}

bool CSqlite::commitTransaction()
{
    return exeSql("commit;");
}

bool CSqlite::rollbackTransaction()
{
    return exeSql("rollback;");
}

void CSqlite::del()
{
}

void CSqlite::update()
{
}

void CSqlite::xxtable()
{
}

