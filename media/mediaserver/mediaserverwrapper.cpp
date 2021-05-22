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

#define LOG_TAG "mediaserverwrapper"
//#define LOG_NDEBUG 0

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/Log.h>
#include "RegisterExtensions.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <cutils/properties.h>

// from LOCAL_C_INCLUDES
#include "MediaPlayerService.h"
#include "ResourceManagerService.h"
#include<set>

using namespace android;

int mediaserverwrapper(){
    auto pid = fork();
    int status = 0;
    bool MediaServer_PathSelect = property_get_bool("ro.mediaserver.64b.enable", false);
    if (pid < 0){
        ALOGE("fork() failed!");
    }
    if (pid == 0){
        //ChildProcess to run the media server
        const char *mediapath;
        if (MediaServer_PathSelect){
            ALOGI("Child Process running 64 bit");
            mediapath= "/system/bin/mediaserver64";
        }
        else
        {
            ALOGI("Child Process running 32 bit");
            mediapath= "/system/bin/mediaserver";
        }
        execl(mediapath,mediapath,NULL,NULL);
    }
    else{
        //Parent Process to wait until child process is complete and report the status
        waitpid(pid, &status,0); // wait for the child to exit
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == EXIT_SUCCESS) {
                ALOGI("Child Successfuly Run");
            } else {
                ALOGE("process exited with status : %d",WEXITSTATUS(status));
            }
        } else if (WIFSIGNALED(status)) {
            ALOGE("process killed by signal :%d ",WTERMSIG(status));
        }
    }
    return status;
}

int main(int argc __unused, char **argv __unused)
{
    mediaserverwrapper();
}