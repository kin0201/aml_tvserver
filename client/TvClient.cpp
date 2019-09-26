#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "TvClient.h"

#ifdef __cplusplus
extern "C" {
#endif

const int RET_SUCCESS = 0;
const int RET_FAILED  = -1;

TvClient::TvClient() {
    if (mpDBusConnection == NULL) {
        mpDBusConnection = ClientBusInit();
        startDetect();
    }
}

TvClient::~TvClient() {
    if (mpDBusConnection != NULL) {
        dbus_connection_unref(mpDBusConnection);
        mpDBusConnection = NULL;
    }
}

DBusConnection *TvClient::ClientBusInit()
{
    printf("init_bus\n");

    DBusConnection *connection;
    DBusError err;
    int ret;

    dbus_error_init(&err);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        printf("connection error: :%s -- %s\n", err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    ret = dbus_bus_request_name(connection, "aml.tv.client", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        printf("Name error: %s -- %s\n", err.name, err.message);
        dbus_error_free(&err);
        return NULL;
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        return NULL;
    }

    dbus_bus_add_match(connection, "type='signal'", &err);

    dbus_connection_flush(connection);
    if (dbus_error_is_set(&err)) {
        printf("add Match Error %s--%s\n", err.name, err.message);
        dbus_error_free(&err);
        return connection;
    }

    return connection;
}

int TvClient::SendMethodCall(const char *CmdString)
{
    printf("SendMethodCall\n");

    DBusMessage *msg;
    DBusMessageIter arg;
    DBusPendingCall *pending;
    int ReturnVal = 0;

    msg = dbus_message_new_method_call("aml.tv.service", "/aml/tv", "aml.tv", "cmd");
    if (msg == NULL) {
        printf("no memory\n");
        return RET_FAILED;
    }

    if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &CmdString, DBUS_TYPE_INVALID)) {
        printf("add args failed!\n");
        dbus_message_unref(msg);
        return RET_FAILED;
    }

    if (!dbus_connection_send_with_reply (mpDBusConnection, msg, &pending, -1)) {
        printf("no memeory!");
        dbus_message_unref(msg);
        return RET_FAILED;
    }

    if (pending == NULL) {
        printf("Pending is NULL, may be disconnect...\n");
        dbus_message_unref(msg);
        return RET_FAILED;
    }

    dbus_connection_flush(mpDBusConnection);
    dbus_message_unref(msg);

    dbus_pending_call_block (pending);
    msg = dbus_pending_call_steal_reply (pending);
    if (msg == NULL) {
        printf("reply is null. error\n");
        return RET_FAILED;
    }

    dbus_pending_call_unref(pending);

    if (!dbus_message_iter_init(msg, &arg)) {
        printf("no argument, error\n");
    }

    if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_INT32) {
        printf("paramter type error\n");
    }

    dbus_message_iter_get_basic(&arg, &ReturnVal);
    printf("ret = %d\n",ReturnVal);
    dbus_message_unref(msg);

    return RET_SUCCESS;
}

void *TvClient::HandleTvServiceMessage(void *args)
{
    printf("HandleTvServiceMessage\n");
    DBusMessage *msg;
    DBusMessageIter arg;
    DBusError err;
    char *str;
    DBusConnection *Connection = (DBusConnection *)args;

    dbus_error_init(&err);

    while (1) {
        dbus_connection_read_write(Connection, 0);
        msg = dbus_connection_pop_message(Connection);
        if (msg == NULL) {
            sleep(1);
            continue;
        }
        printf("path: %s\n", dbus_message_get_path (msg));
        if (dbus_message_is_signal(msg, "aml.tv", "test")) {
            if (!dbus_message_iter_init(msg, &arg)) {
                printf("no argument\n");
            }

            if (dbus_message_iter_get_arg_type(&arg) != DBUS_TYPE_INVALID) {
                /*dbus_int32_t source, connectStatus;
                dbus_message_iter_get_basic(&arg, &source);
                dbus_message_iter_get_basic(&arg, &connectStatus);
                printf("source: %d, connect_status: %d\n", source, connectStatus);*/
                dbus_message_iter_get_basic(&arg,&str);
                printf("recv param --: %s\n", str);
            }
        } else {
            printf("Not TV service signal call!\n");
        }
        dbus_message_unref(msg);
    }
    //dbus_bus_remove_match();
    return NULL;
}

int TvClient::startDetect()
{
    printf("startDetect!\n");

    int ret;
    pthread_t thread_id;

    ret = pthread_create(&thread_id, NULL, HandleTvServiceMessage, (void *)mpDBusConnection);
    if (ret != 0) {
        printf("HandleTvServiceMessage create thread fail\n");
        ret = -1;
    } else {
        printf("HandleTvServiceMessage thread id (%lu) done\n", thread_id);
        ret = 0;
    }

    return ret;
}

int TvClient::ConnectToTvClient() {
    new TvClient();
    return RET_SUCCESS;
}

int TvClient::DisConnectToTvClient() {
    if (mpDBusConnection != NULL) {
        dbus_connection_unref(mpDBusConnection);
        mpDBusConnection = NULL;
    }

    return RET_SUCCESS;
}

int TvClient::StartTv() {
    return SendMethodCall("start");
}

int TvClient::StopTv() {
    return SendMethodCall("stop");
}

#ifdef __cplusplus
}
#endif
