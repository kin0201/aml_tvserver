/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "TvService.h"

int main(int argc, char **argv) {
    TvService *mpTvService = TvService::GetInstance();
    mpTvService->TvServiceHandleMessage();
}
