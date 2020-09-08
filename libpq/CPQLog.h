/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifndef _C_PQ_LOG_H_
#define _C_PQ_LOG_H_

#ifdef DEFAULT_LOG_BUFFER_LEN
#undef DEFAULT_LOG_BUFFER_LEN
#endif

#define DEFAULT_LOG_BUFFER_LEN    1152        //1024 + 128

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

extern int __pq_log_print(const char *moudle_tag, const char *level_tag ,const char *class_tag, const char *fmt, ...);

#ifdef LOG_MOUDLE_TAG

#undef LOGD
#define LOGD(...) \
    __pq_log_print(LOG_MOUDLE_TAG, "D", LOG_CLASS_TAG, __VA_ARGS__)

#undef LOGE
#define LOGE(...) \
    __pq_log_print(LOG_MOUDLE_TAG, "E", LOG_CLASS_TAG, __VA_ARGS__)

#undef LOGV
#define LOGV(...) \
    __pq_log_print(LOG_MOUDLE_TAG, "W", LOG_CLASS_TAG, __VA_ARGS__)

#undef LOGI
#define LOGI(...) \
    __pq_log_print(LOG_MOUDLE_TAG, "I", LOG_CLASS_TAG, __VA_ARGS__)

#else
#undef LOGD
#define LOGD(...) \
    __pq_log_print("PQ", "D", " ", __VA_ARGS__)
#undef LOGE
#define LOGE(...) \
    __pq_log_print("PQ", "E", " ", __VA_ARGS__)
#undef LOGV
#define LOGV(...) \
    __pq_log_print("PQ", "W", " ", __VA_ARGS__)
#undef LOGI
#define LOGI(...) \
    __pq_log_print("PQ", "I", " ", __VA_ARGS__)
#endif

#endif//end #ifndef _C_PQ_LOG_H_
