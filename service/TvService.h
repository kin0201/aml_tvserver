/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef TV_SERVICE_H
#define TV_SERVICE_H

#include <dbus/dbus.h>
#include "CTv.h"
#include "tvcmd.h"

class TvService: public CTv::TvIObserver {
public:
    TvService();
    ~TvService();
    static TvService *GetInstance();
    int TvServiceHandleMessage();
private:
    DBusConnection* TvServiceBusInit();
    virtual void onTvEvent(CTvEvent &event);
    int SendSignalForSignalDetectEvent(CTvEvent &event);
    int SendSignalForSourceConnectEvent(CTvEvent &event);
    int ParserTvCommand(char *commandData);

    //static TvService *mInstance;
    CTv *mpTv;
    DBusConnection *mpTvServiceConnection;

};
#endif
