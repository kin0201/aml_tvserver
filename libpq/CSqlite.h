/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef _CSQLITE_H_
#define _CSQLITE_H_

#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <sqlite3.h>
//#include <assert.h>
#include <pthread.h>
#include "CPQLog.h"

#define DEBUG_FLAG 0
#define getSqlParams(func, buffer, args...) \
    do{\
        sprintf(buffer, ##args);\
        if (DEBUG_FLAG) {\
            LOGD("getSqlParams for %s\n", func);\
            LOGD("SQL cmd is: %s\n", buffer);\
        }\
    }while(0)

class CSqlite {
public:
    class Cursor {
    public:
        Cursor();
        ~Cursor();
        void Init(char **data, int cow, int col);
        int getCount();
        int getPosition();
        bool move(int offset);
        bool moveToPosition(int position);
        bool moveToFirst();
        bool moveToLast();
        bool moveToNext();
        bool moveToPrevious();
        int getColumnIndex(const char *columnName);
        int getColumnCount();
        //int getString(char *str, int columnIndex);
        std::string getString(int columnIndex);
        int getInt(int columnIndex);
        unsigned long int getUInt(int columnIndex);
        double getF(int columnIndex);
        int getType(int columnIndex);
        void close();
        bool isClosed();
    private:
        char **mData;
        int mCurRowIndex;
        int mRowNums;
        int mColNums;
        bool mIsClosed;
    };
public:
    CSqlite();
    virtual ~CSqlite();
    int openDb(const char *path);
    int closeDb();
    void setHandle(sqlite3 *h);
    sqlite3 *getHandle();
    int select(const char *sql, Cursor &);
    bool exeSql(const char *sql);
    void insert();
    void del();
    void update();
    void xxtable();
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

private:
    int  sqlite3_exec_callback(void *data, int nColumn, char **colValues, char **colNames);
    sqlite3 *mHandle;
};
#endif //CSQLITE
