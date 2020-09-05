#ifndef APP_TRACK_DATA_H
#define APP_TRACK_DATA_H

#include <utils/Errors.h>
#include <utils/String8.h>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>

#define APP_TRACK_DATA_MAX_PACKAGENAME_LEN 128

namespace android {
    class AppTrackData : public Parcelable {
    public:
        char packageName[APP_TRACK_DATA_MAX_PACKAGENAME_LEN];
        bool muted;
        float volume;
        bool active;

        bool operator <(const AppTrackData &obj) const {
            int t = strcmp(packageName, obj.packageName);
            return t < 0;
        }

        /* Parcel */
        status_t readFromParcel(const Parcel *parcel) override {
            String8 pn = parcel->readString8();
            strcpy(packageName, pn.c_str());
            muted = parcel->readInt32();
            volume = parcel->readFloat();
            active = parcel->readInt32();
            return NO_ERROR;
        }

        status_t writeToParcel(Parcel *parcel) const override {
            (void)parcel->writeString8(String8(packageName));
            (void)parcel->writeInt32(muted);
            (void)parcel->writeFloat(volume);
            (void)parcel->writeInt32(active);
            return NO_ERROR;
        }
    };
};

#endif // APP_TRACK_DATA_H
