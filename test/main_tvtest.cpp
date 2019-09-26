#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <TvClient.h>

TvClient *mpTvClient;
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

static int AddVdinVideoPath()
{
    int fd;
    char cmd[64] = {"add tvpath vdin0 amlvideo2.0 deinterlace amvideo"};
    fd = open("/sys/class/vfm/map", O_CREAT|O_RDWR | O_TRUNC, 0777);

    if (fd >= 0) {
        write(fd, cmd, strlen(cmd));
        close(fd);
        return 0;
    } else {
        return -1;
    }
}

static int DisplayInit()
{
    AddVdinVideoPath();
    SetOsdBlankStatus("/sys/class/graphics/fb0/osd_display_debug", 1);
    SetOsdBlankStatus("/sys/class/graphics/fb0/blank", 1);
    return 0;

}

static void SendCmd(const char *data) {
    printf("SendCmd: cmd is %s.\n", data);
    if (strcmp(data, "start") == 0) {
        mpTvClient->StartTv();
    } else if (strcmp(data, "stop") == 0) {
        mpTvClient->StopTv();
    } else {
        printf("invalid cmd!\n");
    }
}

int main(int argc, char **argv) {
    unsigned char read_buf[256];
    memset(read_buf, 0, sizeof(read_buf));

    mpTvClient = new TvClient();
    char Command[1];
    int run = 1;
    DisplayInit();

    printf("#### please select cmd####\n");
    printf("#### select s to start####\n");
    printf("#### select q to stop####\n");
    printf("##########################\n");
    while (run) {
        scanf("%s", Command);
        switch (Command[0]) {
          case 'q': {
            SendCmd("stop");
            SetOsdBlankStatus("/sys/class/graphics/fb0/blank", 0);
            run = 0;
            break;
          }
          case 's': {
              SendCmd("start");
              break;
          }
          default: {
              SendCmd("start");
              break;
          }
        }
        fflush (stdout);
    }

    return 0;
}
