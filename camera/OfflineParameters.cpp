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

#define LOG_TAG "OfflineParameters"

#include <CameraMetadata.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <camera/OfflineParameters.h>
#include <cutils/native_handle.h>
#include <utils/Errors.h>

namespace android {
namespace hardware {

status_t OfflineParameters::readFromParcel(const Parcel* parcel) {
    status_t res = OK;

    if (!parcel) {
        return BAD_VALUE;
    }

    in = parcel->readNativeHandle();
    out = parcel->readNativeHandle();

    int tmpF = 0;
    if ((res = parcel->readInt32(&tmpF)) != OK) {
        ALOGE("%s: Failed to read f from parcel", __FUNCTION__);
        return res;
    }
    int tmpW = 0;
    if ((res = parcel->readInt32(&tmpW)) != OK) {
        ALOGE("%s: Failed to read w from parcel", __FUNCTION__);
        return res;
    }
    int tmpH = 0;
    if ((res = parcel->readInt32(&tmpH)) != OK) {
        ALOGE("%s: Failed to read h from parcel", __FUNCTION__);
        return res;
    }

    handle = parcel->readNativeHandle();

    if ((res = parcel->readParcelable(&metadata)) != OK) {
        ALOGE("%s: Failed to read metadata from parcel", __FUNCTION__);
        return res;
    }

    f = tmpF; w = tmpW; h = tmpH;
    pMem = interface_cast<IMemory>(parcel->readStrongBinder());

    return res;
}

status_t OfflineParameters::writeToParcel(Parcel* parcel) const {
    if (!parcel) return BAD_VALUE;

    status_t res = OK;

    res = parcel->writeNativeHandle(in);
    if (res != OK) {
        return res;
    }

    res = parcel->writeNativeHandle(out);
    if (res != OK) {
        return res;
    }

    res = parcel->writeInt32(f);
    if (res != OK) {
        return res;
    }

    res = parcel->writeInt32(w);
    if (res != OK) {
        return res;
    }

    res = parcel->writeInt32(h);
    if (res != OK) {
        return res;
    }

    res = parcel->writeNativeHandle(handle);
    if (res != OK) {
        return res;
    }

    res = parcel->writeParcelable(metadata);
    if (res != OK) {
        return res;
    }

    res = parcel->writeStrongBinder(IInterface::asBinder(pMem));
    if (res != OK) {
        return res;
    }

    return res;
}

}  // namespace hardware
}  // namespace android
