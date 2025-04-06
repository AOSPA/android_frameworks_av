/*
 * Copyright (C) 2024 The Android Open Source Project
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

#pragma once

#include <memory>
#include <string>

#include <android/binder_auto_utils.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>

namespace android {

/*
 * Helper macro to add instance name, function name in logs
 * classes should provide getInstanceName and getClassName API to use these macros.
 * print function names along with instance name.
 *
 * Usage:
 *  AUGMENT_LOG(I, "hello!");
 *  AUGMENT_LOG(W, "value: %d", value);
 *
 * AUGMENT_LOG_IF(D, value < 0, "negative");
 * AUGMENT_LOG_IF(E, value < 0, "bad value: %d", value);
 */

#define AUGMENT_LOG(level, ...)                                                              \
    ALOG##level("[%s] %s: " __android_second(0, __VA_ARGS__, ""), getInstanceName().c_str(), \
                __func__ __android_rest(__VA_ARGS__))

#define AUGMENT_LOG_IF(level, cond, ...)                                     \
    ALOG##level##_IF(cond, "[%s] %s: " __android_second(0, __VA_ARGS__, ""), \
                     getInstanceName().c_str(), __func__ __android_rest(__VA_ARGS__))
// Used to register an entry into the function
#define LOG_ENTRY() ALOGD("[%s] %s", getInstanceName().c_str(), __func__)

// entry logging as verbose level
#define LOG_ENTRY_V() ALOGV("[%s] %s", getInstanceName().c_str(), __func__)

class HalDeathHandler {
  public:
    static HalDeathHandler& getInstance();

    bool registerHandler(AIBinder* binder);
  private:
    static void OnBinderDied(void*);

    HalDeathHandler();

    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
};

template<class Intf>
std::shared_ptr<Intf> getServiceInstance(const std::string& instanceName) {
    const std::string serviceName =
            std::string(Intf::descriptor).append("/").append(instanceName);
    std::shared_ptr<Intf> service;
    while (!service) {
        AIBinder* serviceBinder = nullptr;
        while (!serviceBinder) {
            // 'waitForService' may return a nullptr, hopefully a transient error.
            serviceBinder = AServiceManager_waitForService(serviceName.c_str());
        }
        // `fromBinder` may fail and return a nullptr if the service has died in the meantime.
        service = Intf::fromBinder(ndk::SpAIBinder(serviceBinder));
        if (service != nullptr) {
            HalDeathHandler::getInstance().registerHandler(serviceBinder);
        }
    }
    return service;
}

}  // namespace android
