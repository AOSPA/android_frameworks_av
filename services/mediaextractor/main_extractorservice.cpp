/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "mediaextractor"
//#define LOG_NDEBUG 0

#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

// from LOCAL_C_INCLUDES
#include "IcuUtils.h"
#include "MediaExtractorService.h"
#include "MediaUtils.h"
#include "minijail/minijail.h"

using namespace android;

int main(int argc __unused, char** argv)
{
    limitProcessMemory(
        "ro.media.maxmem", /* property that defines limit */
        SIZE_MAX, /* upper limit in bytes */
#ifndef ENABLE_AV_ENHANCEMENTS
        15 /* upper limit as percentage of physical RAM */
#else
        25 /* higher upper limit as percentage of physical RAM */
#endif
    );

    signal(SIGPIPE, SIG_IGN);
    MiniJail();

    InitializeIcuOrDie();

    strcpy(argv[0], "media.extractor");
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    MediaExtractorService::instantiate();
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
}
