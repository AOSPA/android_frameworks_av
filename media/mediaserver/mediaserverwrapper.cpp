/*copyright 2008, The Android Open Source Project
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
    int status = 0;
    bool MediaServer_PathSelect = property_get_bool("ro.mediaserver.64b.enable", false);
    const char *mediapath;
    const char *processName = "/system/bin/mediaserver";
    ALOGI("Mediaserver Wrapper Has been started");
    if (MediaServer_PathSelect){
        mediapath= "/system/bin/mediaserver64";
    }
    else
    {
        mediapath= "/system/bin/mediaserver";
    }
    status = execl(mediapath,processName,NULL,NULL);
    return status;
}

int main(int argc __unused, char **argv __unused)
{
    return mediaserverwrapper();
}
