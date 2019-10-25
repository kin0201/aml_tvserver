#define LOG_MOUDLE_TAG "TV"
#define LOG_CLASS_TAG "TvTest"

#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <TvClient.h>
#include "CTvClientLog.h"

class TvTest: public TvClient::TvClientIObserver {
public:
    TvTest()
    {
        mpTvClient = TvClient::GetInstance();
        mpTvClient->setTvClientObserver(this);
    }

    ~TvTest()
    {

    }

    void onTvClientEvent(CTvEvent &event)
    {
        int eventType = event.getEventType();
        LOGD("%s: eventType: %d.\n", __FUNCTION__, eventType);
        switch (eventType) {
        case CTvEvent::TV_EVENT_SIGLE_DETECT: {
            TvEvent::SignalDetectEvent *signalDetectEvent = (TvEvent::SignalDetectEvent *)(&event);
            LOGD("%s: source: %d, signalFmt: %d, transFmt: %d, status: %d, isDVI: %d.\n", __FUNCTION__,
                                                       signalDetectEvent->mSourceInput,
                                                       signalDetectEvent->mFmt,
                                                       signalDetectEvent->mTrans_fmt,
                                                       signalDetectEvent->mStatus,
                                                       signalDetectEvent->mDviFlag);
            break;
            }
        case CTvEvent::TV_EVENT_SOURCE_CONNECT: {
            TvEvent::SourceConnectEvent *sourceConnectEvent = (TvEvent::SourceConnectEvent *)(&event);
            LOGD("%s: source: %d, connectStatus: %d\n", __FUNCTION__,
                      sourceConnectEvent->mSourceInput, sourceConnectEvent->connectionState);
            break;
            }
        default:
            LOGD("invalid event!\n", __FUNCTION__);
            break;
        }
        return;
    }

    int SendCmd(const char *data) {
        LOGD("%s: cmd is %s.\n", __FUNCTION__, data);
        if (strcmp(data, "start") == 0) {
            mpTvClient->StartTv(SOURCE_HDMI1);
        } else if (strcmp(data, "stop") == 0) {
            mpTvClient->StopTv(SOURCE_HDMI1);
        } else {
            LOGE("invalid cmd!\n");
        }

        return 0;
    }

    TvClient *mpTvClient;
};

static int SetOsdBlankStatus(const char *path, int cmd)
{
    int fd;
    char  bcmd[16];
    fd = open(path, O_CREAT|O_RDWR | O_TRUNC, 0777);

    if (fd >= 0) {
        sprintf(bcmd,"%d",cmd);
        write(fd,bcmd,strlen(bcmd));
        close(fd);
        return 0;
    }

    return -1;
}

static int DisplayInit()
{
    SetOsdBlankStatus("/sys/class/graphics/fb0/osd_display_debug", 1);
    SetOsdBlankStatus("/sys/class/graphics/fb0/blank", 1);
    return 0;

}

int main(int argc, char **argv) {
    unsigned char read_buf[256];
    memset(read_buf, 0, sizeof(read_buf));

    TvTest *test = new TvTest();
    char Command[1];
    int run = 1;
    DisplayInit();

    LOGD("#### please select cmd####\n");
    LOGD("#### select s to start####\n");
    LOGD("#### select q to stop####\n");
    LOGD("##########################\n");
    while (run) {
        scanf("%s", Command);
        switch (Command[0]) {
          case 'q': {
            test->SendCmd("stop");
            SetOsdBlankStatus("/sys/class/graphics/fb0/blank", 0);
            run = 0;
            break;
          }
          case 's': {
              test->SendCmd("start");
              break;
          }
          default: {
              test->SendCmd("start");
              break;
          }
        }
        fflush (stdout);
    }

    return 0;
}
