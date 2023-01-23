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
    OfflineParameters();
    OfflineParameters(CameraMetadata& metadata);
    OfflineParameters(sp<IMemory>& memory);
    OfflineParameters(native_handle_t* in, native_handle_t* out);
    status_t readFromParcel(const Parcel* parcel) override;
    status_t writeToParcel(Parcel* parcel) const override;

  private:
    native_handle_t* in = nullptr;
    native_handle_t* out = nullptr;
    CameraMetadata metadata = CameraMetadata();
    sp<IMemory> memory = nullptr;
};

}  // namespace hardware
}  // namespace android

#endif
