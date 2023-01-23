#define LOG_TAG "OfflineParameters"

#include "android/hardware/OfflineParameters.h"

#include <CameraMetadata.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <cutils/native_handle.h>
#include <utils/Errors.h>

namespace android {
namespace hardware {

OfflineParameters::OfflineParameters() = default;

OfflineParameters::OfflineParameters(CameraMetadata& metadata) : metadata(metadata) {}

OfflineParameters::OfflineParameters(sp<IMemory>& memory) : memory(memory) {}

OfflineParameters::OfflineParameters(native_handle_t* in, native_handle_t* out)
    : in(in), out(out) {}

status_t OfflineParameters::readFromParcel(const Parcel* parcel) {
    status_t res = OK;

    if (!parcel) {
        return BAD_VALUE;
    }

    in = parcel->readNativeHandle();
    out = parcel->readNativeHandle();
    res = parcel->readParcelable(&metadata);
    memory = interface_cast<IMemory>(parcel->readStrongBinder());

    return res;
}

status_t OfflineParameters::writeToParcel(Parcel* parcel) const {
    status_t res = OK;

    res = parcel->writeNativeHandle(in);
    if (res != OK) {
        return res;
    }

    res = parcel->writeNativeHandle(out);
    if (res != OK) {
        return res;
    }

    res = parcel->writeParcelable(metadata);
    if (res != OK) {
        return res;
    }

    res = parcel->writeStrongBinder(IInterface::asBinder(memory));

    return res;
}

}  // namespace hardware
}  // namespace android
