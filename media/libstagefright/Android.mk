# This file was modified by Dolby Laboratories, Inc. The portions of the
# code that are surrounded by "DOLBY..." are copyrighted and
# licensed separately, as follows:
#
# (C)  2016 Dolby Laboratories, Inc.
# All rights reserved.
#
# This program is protected under international and U.S. Copyright laws as
# an unpublished work. This program is confidential and proprietary to the
# copyright owners. Reproduction or disclosure, in whole or in part, or the
# production of derivative works therefrom without the express permission of
# the copyright owners is prohibited.
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)


LOCAL_SRC_FILES:=                         \
        ACodec.cpp                        \
        AACExtractor.cpp                  \
        AACWriter.cpp                     \
        AMRExtractor.cpp                  \
        AMRWriter.cpp                     \
        AudioPlayer.cpp                   \
        AudioSource.cpp                   \
        CallbackDataSource.cpp            \
        CameraSource.cpp                  \
        CameraSourceTimeLapse.cpp         \
        CodecBase.cpp                     \
        DataConverter.cpp                 \
        DataSource.cpp                    \
        DataURISource.cpp                 \
        DRMExtractor.cpp                  \
        ESDS.cpp                          \
        FileSource.cpp                    \
        FLACExtractor.cpp                 \
        FrameRenderTracker.cpp            \
        HTTPBase.cpp                      \
        HevcUtils.cpp                     \
        JPEGSource.cpp                    \
        MP3Extractor.cpp                  \
        MPEG2TSWriter.cpp                 \
        MPEG4Extractor.cpp                \
        MPEG4Writer.cpp                   \
        MediaAdapter.cpp                  \
        MediaClock.cpp                    \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaCodecListOverrides.cpp       \
        MediaCodecSource.cpp              \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
        MediaSync.cpp                     \
        MidiExtractor.cpp                 \
        http/MediaHTTP.cpp                \
        MediaMuxer.cpp                    \
        MediaSource.cpp                   \
        NuCachedSource2.cpp               \
        NuMediaExtractor.cpp              \
        OMXClient.cpp                     \
        OggExtractor.cpp                  \
        ProcessInfo.cpp                   \
        SampleIterator.cpp                \
        SampleTable.cpp                   \
        SimpleDecodingSource.cpp          \
        SkipCutBuffer.cpp                 \
        StagefrightMediaScanner.cpp       \
        StagefrightMetadataRetriever.cpp  \
        SurfaceMediaSource.cpp            \
        SurfaceUtils.cpp                  \
        ThrottledSource.cpp               \
        Utils.cpp                         \
        VBRISeeker.cpp                    \
        VideoFrameScheduler.cpp           \
        WAVExtractor.cpp                  \
        WVMExtractor.cpp                  \
        XINGSeeker.cpp                    \
        avc_utils.cpp                     \

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/ \
        $(TOP)/frameworks/av/media/libavextensions \
        $(TOP)/frameworks/av/media/libstagefright/mpeg2ts \
        $(TOP)/frameworks/av/include/media/stagefright/timedtext \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
        $(TOP)/external/libvpx/libwebm \
        $(TOP)/system/netd/include \
        $(call include-path-for, audio-utils)

LOCAL_SHARED_LIBRARIES := \
        libaudioutils \
        libbinder \
        libcamera_client \
        libcutils \
        libdl \
        libdrmframework \
        libexpat \
        libgui \
        libicui18n \
        libicuuc \
        liblog \
        libmedia \
        libmediautils \
        libnetd_client \
        libopus \
        libsonivox \
        libssl \
        libstagefright_omx \
        libstagefright_yuv \
        libsync \
        libui \
        libutils \
        libvorbisidec \
        libz \
        libpowermanager \

LOCAL_STATIC_LIBRARIES := \
        libstagefright_color_conversion \
        libyuv_static \
        libstagefright_aacenc \
        libstagefright_matroska \
        libstagefright_mediafilter \
        libstagefright_webm \
        libstagefright_timedtext \
        libvpx \
        libwebm \
        libstagefright_mpeg2ts \
        libstagefright_id3 \
        libFLAC \
        libmedia_helper \

LOCAL_WHOLE_STATIC_LIBRARIES := libavextensions

LOCAL_SHARED_LIBRARIES += \
        libstagefright_enc_common \
        libstagefright_avc_common \
        libstagefright_foundation \
        libdl \
        libRScpp \

ifeq ($(BOARD_USE_SAMSUNG_CAMERAFORMAT_NV21), true)
# This needs flag requires the following string constant in
# CameraParametersExtra.h:
#
# const char CameraParameters::PIXEL_FORMAT_YUV420SP_NV21[] = "nv21";
LOCAL_CFLAGS += -DUSE_SAMSUNG_CAMERAFORMAT_NV21
endif

ifneq ($(TARGET_USES_MEDIA_EXTENSIONS),true)
ifeq ($(TARGET_HAS_LEGACY_CAMERA_HAL1),true)
LOCAL_CFLAGS += -DCAMCORDER_GRALLOC_SOURCE
endif
endif

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

# enable experiments only if requested
ifeq ($(TARGET_ENABLE_AV_EXPERIMENTS),true)
LOCAL_CFLAGS += -DENABLE_STAGEFRIGHT_EXPERIMENTS
endif
# DOLBY_START
ifeq ($(strip $(DOLBY_ENABLE)),true)
    LOCAL_CFLAGS += $(dolby_cflags)
endif
# DOLBY_END

ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_EXTN_FLAC_DECODER)),true)
LOCAL_CFLAGS += -DQTI_FLAC_DECODER
endif
endif


ifeq ($(strip $(AUDIO_FEATURE_ENABLED_FLAC_OFFLOAD)),true)
LOCAL_CFLAGS += -DFLAC_OFFLOAD_ENABLED
endif
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_ALAC_OFFLOAD)),true)
LOCAL_CFLAGS += -DALAC_OFFLOAD_ENABLED
endif
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_WMA_OFFLOAD)), true)
LOCAL_CFLAGS += -DWMA_OFFLOAD_ENABLED
endif
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_APE_OFFLOAD)),true)
LOCAL_CFLAGS += -DAPE_OFFLOAD_ENABLED
endif
ifeq ($(strip $(AUDIO_FEATURE_ENABLED_VORBIS_OFFLOAD)),true)
LOCAL_CFLAGS += -DVORBIS_OFFLOAD_ENABLED
endif

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_PCM_OFFLOAD)),true)
   LOCAL_CFLAGS += -DPCM_OFFLOAD_ENABLED_16
endif

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_PCM_OFFLOAD_24)),true)
   LOCAL_CFLAGS += -DPCM_OFFLOAD_ENABLED_24
endif

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_AAC_ADTS_OFFLOAD)),true)
   LOCAL_CFLAGS += -DAAC_ADTS_OFFLOAD_ENABLED
endif

LOCAL_CLANG := true
LOCAL_SANITIZE := unsigned-integer-overflow signed-integer-overflow

LOCAL_MODULE:= libstagefright

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
