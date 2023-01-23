/*
 * Copyright (C) 2023 Paranoid Android
 * Copyright (C) 2024 Nameless-AOSP
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

#ifndef ANDROID_HARDWARE_OFFLINE_PARAMETERS_H
#define ANDROID_HARDWARE_OFFLINE_PARAMETERS_H

#include <CameraMetadata.h>
#include <binder/IBinder.h>
#include <binder/IMemory.h>
#include <binder/Parcelable.h>
#include <cutils/native_handle.h>

namespace android {
namespace hardware {

class OfflineParameters : public Parcelable {
  public:
    virtual status_t readFromParcel(const Parcel* parcel) override;
    virtual status_t writeToParcel(Parcel* parcel) const override;

    OfflineParameters() {}
    OfflineParameters(const CameraMetadata& metadata) : metadata(CameraMetadata(metadata)) {}
    OfflineParameters(const sp<IMemory>& memory) : pMem(memory) {}
    OfflineParameters(const android::Parcel& parcel) { readFromParcel(&parcel); }
    OfflineParameters(int f, int w, int h) : f(f), w(w), h(h) {}
    OfflineParameters(native_handle_t* in, native_handle_t* out) : in(in), out(out) {}
    OfflineParameters(native_handle_t* in, native_handle_t* out, native_handle_t* handle) :
        in(in), out(out), handle(handle) {}

  private:
    native_handle_t* in = nullptr;
    native_handle_t* out = nullptr;
    int f = 0, w = 0, h = 0;
    native_handle_t* handle = nullptr;
    CameraMetadata metadata = nullptr;
    sp<IMemory> pMem = nullptr;
};

}  // namespace hardware
}  // namespace android

#endif
