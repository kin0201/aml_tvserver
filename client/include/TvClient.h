#include <dbus/dbus.h>

#include "tvcmd.h"

#ifdef __cplusplus
extern "C" {
#endif

class TvClient {
public:
    TvClient();
    ~TvClient();
    int ConnectToTvClient();
    int DisConnectToTvClient();
    int StartTv();
    int StopTv();
private:
    DBusConnection *ClientBusInit();
    int SendMethodCall(const char *CmdString);
    int startDetect();
    static void *HandleTvServiceMessage(void *args);

    DBusConnection *mpDBusConnection;
};
#ifdef __cplusplus
}
#endif
