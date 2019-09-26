#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "TvService.h"

int main(int argc, char **argv) {
    printf("new tvservice!\n");
    TvService *mpTvService = TvService::GetInstance();
    mpTvService->TvServiceHandleMessage();
}
