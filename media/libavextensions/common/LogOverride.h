/*Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 */
/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/properties.h>

enum LogMode {
    UNINIT,
    ENABLED,
    DISABLED
};

static LogMode log_mode = UNINIT;

#ifdef ENABLE_DYNAMIC_LOG
#undef ALOGV
#define ALOGV(...) \
do { \
    if (log_mode != DISABLED) { \
        if (log_mode == ENABLED) { \
            __ALOGV(__VA_ARGS__); \
        } else if (log_mode == UNINIT) { \
            if (property_get_bool("persist.log." LOG_TAG, false)) { \
                log_mode = ENABLED; \
            } else { \
                log_mode = DISABLED; \
            } \
        } \
    } \
} while(0)
#endif
