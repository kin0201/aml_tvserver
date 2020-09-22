/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef TVCLIENT_H
#define TVCLIENT_H

#include <map>
#include <memory>

#include <binder/Binder.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>
#include "common.h"
#include "CTvEvent.h"

using namespace android;

#ifdef __cplusplus
extern "C" {
#endif

class TvClient : public BBinder{
public:
    class TvClientIObserver {
    public:
        TvClientIObserver() {};
        virtual ~TvClientIObserver() {};
        virtual void onTvClientEvent(CTvEvent &event) = 0;
    };

    enum {
        CMD_START = IBinder::FIRST_CALL_TRANSACTION,
        CMD_TV_ACTION = IBinder::FIRST_CALL_TRANSACTION + 1,
        CMD_SET_TV_CB = IBinder::FIRST_CALL_TRANSACTION + 2,
        CMD_CLR_TV_CB = IBinder::FIRST_CALL_TRANSACTION + 3,
        EVT_SRC_CT_CB = IBinder::FIRST_CALL_TRANSACTION + 4,
        EVT_SIG_DT_CB = IBinder::FIRST_CALL_TRANSACTION + 5,
    };

    TvClient();
    ~TvClient();
    static TvClient *GetInstance();
    int setTvClientObserver(TvClientIObserver *observer);
    int StartTv(tv_source_input_t source);
    int StopTv(tv_source_input_t source);
    int SetEdidVersion(tv_source_input_t source, int edidVer);
    int GetEdidVersion(tv_source_input_t source);
    int SetEdidData(tv_source_input_t source, char *dataBuf);
    int GetEdidData(tv_source_input_t source,char *dataBuf);
private:
    int SendMethodCall(char *CmdString);
    static int HandSourceConnectEvent(const void *param);
    static int HandSignalDetectEvent(const void *param);
    int SendTvClientEvent(CTvEvent &event);

    std::map<int, TvClientIObserver *> mTvClientObserver;

    sp<IBinder> tvServicebinder;
    virtual status_t onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags = 0);
};
#ifdef __cplusplus
}
#endif
#endif
