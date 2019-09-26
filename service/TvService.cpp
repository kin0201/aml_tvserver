#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "TvService.h"

#ifdef __cplusplus
extern "C" {
#endif

TvService *mInstance = NULL;
TvService *TvService::GetInstance() {
    if (mInstance == NULL) {
        mInstance = new TvService();
    }

    return mInstance;
}

TvService::TvService() {
    mpTvServiceConnection = TvServiceBusInit();
    if (mpTvServiceConnection != NULL) {
        mpTv = new CTv();
        mpTv->setTvObserver(this);
    } else {
        printf("connect to d-bus failed!\n");
    }
}

TvService::~TvService() {

}

DBusConnection *TvService::TvServiceBusInit() {
    printf("TvServiceBusInit!\n");
    DBusConnection *connection;
    DBusError err;
    int ret = 0;

    dbus_error_init(&err);

    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        printf("Connection Error: %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    ret = dbus_bus_request_name(connection, "aml.tv.service", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        printf("Name Error: %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        return NULL;

    dbus_bus_add_match(connection, "type='signal'", &err);

    dbus_connection_flush(connection);
    if (dbus_error_is_set(&err)) {
        printf("add Match Error %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        return connection;
    }

    return connection;
}

int TvService::TvServiceSendSignal()
{
    printf("send_signal\n");

    DBusMessage *msg;
    DBusMessageIter arg;
    const char *str = "hello world!";

    if ((msg = dbus_message_new_signal("/aml/tv/service", "aml.tv", "test")) == NULL) {
        printf("message is NULL\n");
        return -1;
    }

    dbus_message_iter_init_append(msg, &arg);

    dbus_message_iter_append_basic(&arg, DBUS_TYPE_STRING, &str);

    dbus_connection_send(mpTvServiceConnection, msg, NULL);

    dbus_connection_flush(mpTvServiceConnection);

    dbus_message_unref(msg);

    return 0;
}

int TvService::TvServiceHandleMessage()
{
    printf("HandleTvClientMessage\n");
    DBusMessage *msg;
    DBusError err;
    char *str;

    dbus_error_init(&err);

    while (1) {
        dbus_connection_read_write(mpTvServiceConnection, 0);

        msg = dbus_connection_pop_message(mpTvServiceConnection);
        if (msg == NULL) {
            sleep(1);
            continue;
        }

        printf("path: %s\n", dbus_message_get_path (msg));
        if (dbus_message_is_method_call(msg, "aml.tv", "cmd")) {
            printf("handle client cmd\n");
            DBusMessage *rp;
            DBusMessageIter r_arg;
            int ReturnVal = 0;

            dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);
            if (dbus_error_is_set(&err)) {
                printf("receive message failed!\n");
                dbus_error_free(&err);
                ReturnVal = -1;
            } else {
                printf("receive message: %s\n", str);
                if (strcmp(str, "start") == 0) {
                    ReturnVal = mpTv->StartTv(SOURCE_HDMI1);
                } else {
                    ReturnVal = mpTv->StopTv(SOURCE_HDMI1);
                }
            }

            rp = dbus_message_new_method_return(msg);
            dbus_message_iter_init_append(rp, &r_arg);
            if (!dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_INT32, &ReturnVal)) {
                printf("no memory!!\n");
                return -1;
            }

            if (!dbus_connection_send(mpTvServiceConnection, rp, NULL)) {
                printf("no memory!!\n");
                return -1;
            }
            dbus_connection_flush(mpTvServiceConnection);
            dbus_message_unref(rp);
        } else {
            printf("Not TV client method call!\n");
        }
        dbus_message_unref(msg);
    }
    //dbus_bus_remove_match();
    return 0;
}

void TvService::onTvEvent(CTvEvent event) {
    printf("get tv event!\n");
    TvServiceSendSignal();
    return;
}

#ifdef __cplusplus
}
#endif
