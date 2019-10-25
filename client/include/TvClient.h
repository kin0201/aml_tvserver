#include <dbus/dbus.h>
#include <map>
#include <memory>

#include "common.h"
#include "CTvEvent.h"

#ifdef __cplusplus
extern "C" {
#endif

class TvClient {
public:
    class TvClientIObserver {
    public:
        TvClientIObserver() {};
        virtual ~TvClientIObserver() {};
        virtual void onTvClientEvent(CTvEvent &event) = 0;
    };

    TvClient();
    ~TvClient();
    static TvClient *GetInstance();
    int setTvClientObserver(TvClientIObserver *observer);
    int StartTv(tv_source_input_t source);
    int StopTv(tv_source_input_t source);
    int SetEdidData(tv_source_input_t source, char *dataBuf);
    int GetEdidData(tv_source_input_t source,char *dataBuf);
private:
    DBusConnection *ClientBusInit();
    int SendMethodCall(char *CmdString);
    int startDetect();
    static void *HandleTvServiceMessage(void *args);
    static int HandSourceConnectEvent(DBusMessageIter messageIter);
    static int HandSignalDetectEvent(DBusMessageIter messageIter);
    int SendTvClientEvent(CTvEvent &event);

    DBusConnection *mpDBusConnection = NULL;
    std::map<int, TvClientIObserver *> mTvClientObserver;
};
#ifdef __cplusplus
}
#endif
