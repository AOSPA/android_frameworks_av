/*
 * Copyright (C) 2015 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "HevcUtils"

#include <cstring>
#include <utility>

#include "include/HevcUtils.h"

#include <media/stagefright/foundation/ABitReader.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/avc_utils.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/Utils.h>

#define UNUSED_PARAM __attribute__((unused))

namespace android {

static const uint8_t kHevcNalUnitTypes[8] = {
    kHevcNalUnitTypeCodedSliceIdr,
    kHevcNalUnitTypeCodedSliceIdrNoLP,
    kHevcNalUnitTypeCodedSliceCra,
    kHevcNalUnitTypeVps,
    kHevcNalUnitTypeSps,
    kHevcNalUnitTypePps,
    kHevcNalUnitTypePrefixSei,
    kHevcNalUnitTypeSuffixSei,
};

HevcParameterSets::HevcParameterSets()
    : mInfo(kInfoNone)
    , mIsLhevc(false) {
}

HevcParameterSets::HevcParameterSets(bool isMvHevc)
    : mInfo(kInfoNone) {
    mIsLhevc = isMvHevc;
}

status_t HevcParameterSets::addNalUnit(const uint8_t* data, size_t size) {
    if (size < 1) {
        ALOGE("empty NAL b/35467107");
        return ERROR_MALFORMED;
    }
    uint8_t nalUnitType = (data[0] >> 1) & 0x3f;
    uint8_t nuhLayerId = ((data[0] & 0x01) << 5) | ((data[1] >> 3) & 0x1f);
    status_t err = OK;
    switch (nalUnitType) {
        case 32:  // VPS
            if (size < 2) {
                ALOGE("invalid NAL/VPS size b/35467107");
                return ERROR_MALFORMED;
            }
            err = parseVps(data + 2, size - 2);
            break;
        case 33:  // SPS
            if (size < 2) {
                ALOGE("invalid NAL/SPS size b/35467107");
                return ERROR_MALFORMED;
            }
            err = parseSps(data + 2, size - 2, nuhLayerId);
            break;
        case 34:  // PPS
            if (size < 2) {
                ALOGE("invalid NAL/PPS size b/35467107");
                return ERROR_MALFORMED;
            }
            err = parsePps(data + 2, size - 2, nuhLayerId);
            break;
        case 35:  // AUD
        case 39:  // Prefix SEI
        case 40:  // Suffix SEI
            if (size < 2) {
                ALOGE("invalid NAL/PPS size b/35467107");
                return ERROR_MALFORMED;
            }
            err = parseSeiMessage(data + 2, size - 2);
            break;
        default:
            ALOGE("Unrecognized NAL unit type.");
            return ERROR_MALFORMED;
    }

    if (err != OK) {
        ALOGE("error parsing VPS or SPS or PPS or SEI");
        return err;
    }

    sp<ABuffer> buffer = ABuffer::CreateAsCopy(data, size);
    buffer->setInt32Data(nalUnitType);
    mNalUnits.push(buffer);
    // nal unit layer id
    mNalLayerIds.push(nuhLayerId);
    if (mNalUnits.size() != mNalLayerIds.size()) {
        ALOGE("mNalUnits.size():%d, mNalLayerIds.size():%d",
            mNalUnits.size(), mNalLayerIds.size());
        return ERROR_MALFORMED;
    }
    return OK;
}

template <typename T>
static bool findParam(uint32_t key, T *param,
        KeyedVector<uint32_t, uint64_t> &params) {
    CHECK(param);
    ssize_t index = params.indexOfKey(key);
    if (index < 0) {
        return false;
    }
    *param = (T) params[index];
    return true;
}

bool HevcParameterSets::findParam8(uint32_t key, uint8_t *param) {
    return findParam(key, param, mParams);
}

bool HevcParameterSets::findParam16(uint32_t key, uint16_t *param) {
    return findParam(key, param, mParams);
}

bool HevcParameterSets::findParam32(uint32_t key, uint32_t *param) {
    return findParam(key, param, mParams);
}

bool HevcParameterSets::findParam64(uint32_t key, uint64_t *param) {
    return findParam(key, param, mParams);
}

bool HevcParameterSets::getThreeDimParamParsed() {
    uint8_t leftViewId;
    if (!findParam8(kSeiLeftViewId, &leftViewId)) {
        return false;
    }
    return true;
}

size_t HevcParameterSets::getNumNalUnitsOfType(uint8_t type, uint8_t layerId) {
    size_t num = 0;
    if (mNalUnits.size() != mNalLayerIds.size()) {
        ALOGE("the number of Nal units and their corresponding layer Ids do not match");
        return num;
    }

    for (size_t i = 0; i < mNalUnits.size(); ++i) {
        if (getType(i) == type && getLayerId(i) == layerId) {
            ++num;
        }
    }
    return num;
}
size_t HevcParameterSets::getNumNalUnitsOfType(uint8_t type) {
    size_t num = 0;
    for (size_t i = 0; i < mNalUnits.size(); ++i) {
        if (getType(i) == type) {
            ++num;
        }
    }
    return num;
}

uint8_t HevcParameterSets::getType(size_t index) {
    CHECK_LT(index, mNalUnits.size());
    return mNalUnits[index]->int32Data();
}

size_t HevcParameterSets::getSize(size_t index) {
    CHECK_LT(index, mNalUnits.size());
    return mNalUnits[index]->size();
}
size_t HevcParameterSets::getLayerId(size_t index) {
    CHECK_LT(index, mNalLayerIds.size());
    return mNalLayerIds[index];
}

bool HevcParameterSets::write(size_t index, uint8_t* dest, size_t size) {
    CHECK_LT(index, mNalUnits.size());
    const sp<ABuffer>& nalUnit = mNalUnits[index];
    if (size < nalUnit->size()) {
        ALOGE("dest buffer size too small: %zu vs. %zu to be written",
                size, nalUnit->size());
        return false;
    }
    memcpy(dest, nalUnit->data(), nalUnit->size());
    return true;
}

status_t HevcParameterSets::parseVps(const uint8_t* data, size_t size) {
    size_t bitCounter = 0;
    // See Rec. ITU-T H.265 v3 (04/2015) Chapter 7.3.2.1 for reference
    NALBitReader reader(data, size);
    // Skip vps_video_parameter_set_id
    reader.skipBits(4);
    // vps_base_layer_internal_flag
    bool baseLayerInternalFlag = reader.getBits(1);
    // Skip vps_base_layer_available_flag
    reader.skipBits(1);
    // vps_max_layers_minus_1
    // if the bitstream is for multiview profile, this value is set greater than 0.
    uint8_t maxLayersMinusOne = reader.getBits(6);
    mParams.add(kMaxLayersMinusOne, maxLayersMinusOne);
    // vps_max_sub_layers_minus1
    uint8_t maxSubLayersMinusOne;
    maxSubLayersMinusOne = reader.getBits(3);
    // Skip vps_temporal_id_nesting_flags
    reader.skipBits(1);
    // Skip reserved
    reader.skipBits(16);

    status_t err = OK;
    if (reader.atLeastNumBitsLeft(96)) {
        err = parseProfileTierLevel(1, maxSubLayersMinusOne, reader, 1);
        ALOGV("kMaxLayersMinus1 : %d", maxLayersMinusOne);
    } else {
        reader.skipBits(96);
        return reader.overRead() ? ERROR_MALFORMED : OK;
    }

    if (maxLayersMinusOne == 0) { // main(10) profile
        mParams.add(kNumViews, 1);
        if (err != OK) {
            ALOGE("error parsing PTL in VPS for HEVC main profile");
            return err;
        }
        ALOGV("PTL parsing correctly for HEVC main profile.");
        return err;
    } else if (maxLayersMinusOne != 0 && maxLayersMinusOne != 1) { // Currently we consider layer 0 and 1 only
        ALOGE("VPS maxLayersMinusOne is greater than 1.");
        return ERROR_MALFORMED;
    }
    // Additional parsing bits for more information
    bool subLayerOrderingInfoPresentFlag;
    subLayerOrderingInfoPresentFlag = reader.getBits(1);

    for (size_t i = (subLayerOrderingInfoPresentFlag? 0 : maxSubLayersMinusOne);
            i <= maxSubLayersMinusOne; i++) {
        //skip vps_max_dec_pic_buffering_minus1[i]
        parseUEWithFallback(&reader, 0U);
        //skip vps_max_num_reorder_pics[i]
        parseUEWithFallback(&reader, 0U);
        //skip vps_max_latency_increase_plus1[i]
        parseUEWithFallback(&reader, 0U);
    }

    uint8_t maxLayerId, numLayerSetsMinusOne;
    // vps_max_layer_id
    maxLayerId = reader.getBits(6);
    // vps_num_layer_sets_minus1
    numLayerSetsMinusOne = parseUEWithFallback(&reader, 0U);
    for (size_t i = 1; i <= numLayerSetsMinusOne; i++) {
        for (size_t j = 0; j <= maxLayerId; j++) {
            // Skip layer_id_included_flag[i][j]
            reader.skipBits(1);
        }
    }

    // vps_timing_info_present_flag
    bool timingInfoPresentFlag = reader.getBits(1);
    if (timingInfoPresentFlag) {
        // Skip vps_num_units_in_tick
        reader.skipBits(32);
        // skip vps_time_scale
        reader.skipBits(32);
        // vps_poc_proportional_to_timing_flag
        bool pocProportionalToTimingFlag = reader.getBits(1);
        if (pocProportionalToTimingFlag) {
            // skip vps_num_ticks_poc_diff_one_minus1;
            parseUEWithFallback(&reader, 0U);
        }
        // skip vps_num_hrd_parameters
        uint8_t numHrdParameters = parseUEWithFallback(&reader, 0U);
        for (size_t i = 0; i < numHrdParameters; i++) {
            // skip hrd_layer_set_idx[i]
            parseUEWithFallback(&reader, 0U);
            bool cprmsPresentFlag = false;
            if (i > 0) {
                // cprms_present_flag[i]
                cprmsPresentFlag = reader.getBits(1);
            }
            err = parseHrdParameters(cprmsPresentFlag,
                    maxSubLayersMinusOne, &reader);
            if (err != OK) {
                ALOGE("error parsing HRD Parameters");
                return err;
            }
        }
    }

    bool extensionFlag = reader.getBits(1);
    if (extensionFlag) {
        while(!byteAligned(reader.numBitsLeft())) {
            // skip vps_extension_alignment_bit_equal_to_one
            reader.skipBits(1);
        }
        status_t err = parseVpsExtension(kMaxLayersMinusOne,
                            baseLayerInternalFlag, reader);
        if (err != OK) {
            ALOGE("error parsing VPS Extension.");
            return err;
        }
    }

    return reader.overRead() ? ERROR_MALFORMED : OK;
}
status_t HevcParameterSets::parseSps(const uint8_t *data, size_t size,
        const uint8_t nuhLayerId) {
    ALOGV("parseSPS, nuh_layer_id : %d", nuhLayerId);
    // See Rec. ITU-T H.265 v3 (04/2015) Chapter 7.3.2.2 for reference
    NALBitReader reader(data, size);
    // Skip sps_video_parameter_set_id
    reader.skipBits(4);
    uint8_t maxSubLayersMinus1 = reader.getBitsWithFallback(3, 0);
    if (nuhLayerId == 0) {
        mParams.add(kSpsMaxSubLayersMinusOne, maxSubLayersMinus1);
    }
    // extOrMaxSubLayersMinus1 is defined in F.7.3.2.2.1
    uint8_t extOrMaxSubLayersMinus1 = maxSubLayersMinus1;
    const bool MultiLayerExtSpsFlag = nuhLayerId > 0 && extOrMaxSubLayersMinus1 == 7;
    // additonal condition is defined in F.7.3.2.2.1
    if (!MultiLayerExtSpsFlag) {
        // sps_temporal_id_nesting_flag
        reader.skipBits(1);
        // Skip general profile
        parseProfileTierLevel(1, mParams.valueFor(kSpsMaxSubLayersMinusOne), reader, false);
    }

    // Skip sps_seq_parameter_set_id
    skipUE(&reader);
    if (MultiLayerExtSpsFlag) {
        if (reader.getBitsWithFallback(1, 0) /*update_rep_format_flag*/) {
            // sps_rep_format_idx
            reader.skipBits(8);
        }
    } else {
        uint8_t chromaFormatIdc = parseUEWithFallback(&reader, 0);
        // FIXME : chroma_format_idc for each layer may be necessary
        if (nuhLayerId == 0) {
            mParams.add(kChromaFormatIdc, chromaFormatIdc);
        }
        if (chromaFormatIdc == 3) {
            // Skip separate_colour_plane_flag
            reader.skipBits(1);
        }
        // Skip pic_width_in_luma_samples
        skipUE(&reader);
        // Skip pic_height_in_luma_samples
        skipUE(&reader);
        if (reader.getBitsWithFallback(1, 0) /* i.e. conformance_window_flag */) {
            // Skip conf_win_left_offset
            skipUE(&reader);
            // Skip conf_win_right_offset
            skipUE(&reader);
            // Skip conf_win_top_offset
            skipUE(&reader);
            // Skip conf_win_bottom_offset
            skipUE(&reader);
        }
        if (nuhLayerId == 0) {
            mParams.add(kBitDepthLumaMinus8, parseUEWithFallback(&reader, 0));
            mParams.add(kBitDepthChromaMinus8, parseUEWithFallback(&reader, 0));
        } else {
            // FIXME : bit_depth_luma_minus8 and bit_depth_chroma_minus8
            // for each layer may be necessary in the future
            skipUE(&reader);
            skipUE(&reader);
        }
    }

    // log2_max_pic_order_cnt_lsb_minus4
    size_t log2MaxPicOrderCntLsb = parseUEWithFallback(&reader, 0) + (size_t)4;
    if (!MultiLayerExtSpsFlag) {
        bool spsSubLayerOrderingInfoPresentFlag = reader.getBitsWithFallback(1, 0);
        for (uint32_t i = spsSubLayerOrderingInfoPresentFlag ? 0 : maxSubLayersMinus1;
                i <= maxSubLayersMinus1; ++i) {
            skipUE(&reader); // sps_max_dec_pic_buffering_minus1[i]
            skipUE(&reader); // sps_max_num_reorder_pics[i]
            skipUE(&reader); // sps_max_latency_increase_plus1[i]
        }
    }
    skipUE(&reader); // log2_min_luma_coding_block_size_minus3
    skipUE(&reader); // log2_diff_max_min_luma_coding_block_size
    skipUE(&reader); // log2_min_luma_transform_block_size_minus2
    skipUE(&reader); // log2_diff_max_min_luma_transform_block_size
    skipUE(&reader); // max_transform_hierarchy_depth_inter
    skipUE(&reader); // max_transform_hierarchy_depth_intra
    if (reader.getBitsWithFallback(1, 0)) { // scaling_list_enabled_flag u(1)
        bool inferScalingListFlag = false;
        if (MultiLayerExtSpsFlag) {
            // sps_infer_scaling_list_flag
            inferScalingListFlag = reader.getBits(1);
        }
        if (inferScalingListFlag) {
            // sps_scaling_list_ref_layer_id
            reader.skipBits(6);
        } else {
            // scaling_list_data
            if (reader.getBitsWithFallback(1, 0)) { // sps_scaling_list_data_present_flag
                for (uint32_t sizeId = 0; sizeId < 4; ++sizeId) {
                    for (uint32_t matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1) {
                        if (!reader.getBitsWithFallback(1, 1)) {
                            // scaling_list_pred_mode_flag[sizeId][matrixId]
                            skipUE(&reader); // scaling_list_pred_matrix_id_delta[sizeId][matrixId]
                        } else {
                            uint32_t coefNum = std::min(64, (1 << (4 + (sizeId << 1))));
                            if (sizeId > 1) {
                                skipSE(&reader); // scaling_list_dc_coef_minus8[sizeId − 2][matrixId]
                            }
                            for (uint32_t i = 0; i < coefNum; ++i) {
                                skipSE(&reader); // scaling_list_delta_coef
                            }
                        }
                    }
                }
            }
        }
    }
    reader.skipBits(1); // amp_enabled_flag
    reader.skipBits(1); // sample_adaptive_offset_enabled_flag u(1)
    if (reader.getBitsWithFallback(1, 0)) { // pcm_enabled_flag
        reader.skipBits(4); // pcm_sample_bit_depth_luma_minus1
        reader.skipBits(4); // pcm_sample_bit_depth_chroma_minus1 u(4)
        skipUE(&reader); // log2_min_pcm_luma_coding_block_size_minus3
        skipUE(&reader); // log2_diff_max_min_pcm_luma_coding_block_size
        reader.skipBits(1); // pcm_loop_filter_disabled_flag
    }
    uint32_t numShortTermRefPicSets = parseUEWithFallback(&reader, 0);
    uint32_t numPics = 0;
    for (uint32_t i = 0; i < numShortTermRefPicSets; ++i) {
        // st_ref_pic_set(i)
        if (i != 0 && reader.getBitsWithFallback(1, 0)) { // inter_ref_pic_set_prediction_flag
            reader.skipBits(1); // delta_rps_sign
            skipUE(&reader); // abs_delta_rps_minus1
            uint32_t nextNumPics = 0;
            for (uint32_t j = 0; j <= numPics; ++j) {
                if (reader.getBitsWithFallback(1, 0) // used_by_curr_pic_flag[j]
                        || reader.getBitsWithFallback(1, 0)) { // use_delta_flag[j]
                    ++nextNumPics;
                }
            }
            numPics = nextNumPics;
        } else {
            uint32_t numNegativePics = parseUEWithFallback(&reader, 0);
            uint32_t numPositivePics = parseUEWithFallback(&reader, 0);
            if (numNegativePics > UINT32_MAX - numPositivePics) {
                return ERROR_MALFORMED;
            }
            numPics = numNegativePics + numPositivePics;
            for (uint32_t j = 0; j < numPics; ++j) {
                skipUE(&reader); // delta_poc_s0|1_minus1[i]
                reader.skipBits(1); // used_by_curr_pic_s0|1_flag[i]
                if (reader.overRead()) {
                    return ERROR_MALFORMED;
                }
            }
        }
        if (reader.overRead()) {
            return ERROR_MALFORMED;
        }
    }
    if (reader.getBitsWithFallback(1, 0)) { // long_term_ref_pics_present_flag
        uint32_t numLongTermRefPicSps = parseUEWithFallback(&reader, 0);
        for (uint32_t i = 0; i < numLongTermRefPicSps; ++i) {
            reader.skipBits(log2MaxPicOrderCntLsb); // lt_ref_pic_poc_lsb_sps[i]
            reader.skipBits(1); // used_by_curr_pic_lt_sps_flag[i]
            if (reader.overRead()) {
                return ERROR_MALFORMED;
            }
        }
    }
    reader.skipBits(1); // sps_temporal_mvp_enabled_flag
    reader.skipBits(1); // strong_intra_smoothing_enabled_flag
    if (reader.getBitsWithFallback(1, 0)) { // vui_parameters_present_flag
        if (reader.getBitsWithFallback(1, 0)) { // aspect_ratio_info_present_flag
            uint32_t aspectRatioIdc = reader.getBitsWithFallback(8, 0);
            if (aspectRatioIdc == 0xFF /* EXTENDED_SAR */) {
                reader.skipBits(16); // sar_width
                reader.skipBits(16); // sar_height
            }
        }
        if (reader.getBitsWithFallback(1, 0)) { // overscan_info_present_flag
            reader.skipBits(1); // overscan_appropriate_flag
        }
        if (reader.getBitsWithFallback(1, 0)) { // video_signal_type_present_flag
            reader.skipBits(3); // video_format
            if (nuhLayerId == 0) {
                uint32_t videoFullRangeFlag;
                if (reader.getBitsGraceful(1, &videoFullRangeFlag)) {
                    mParams.add(kVideoFullRangeFlag, videoFullRangeFlag);
                }
                if (reader.getBitsWithFallback(1, 0)) { // colour_description_present_flag
                    mInfo = (Info)(mInfo | kInfoHasColorDescription);
                    uint32_t colourPrimaries, transferCharacteristics, matrixCoeffs;
                    if (reader.getBitsGraceful(8, &colourPrimaries)) {
                        mParams.add(kColourPrimaries, colourPrimaries);
                    }
                    if (reader.getBitsGraceful(8, &transferCharacteristics)) {
                        mParams.add(kTransferCharacteristics, transferCharacteristics);
                        if (transferCharacteristics == 16 /* ST 2084 */
                                || transferCharacteristics == 18 /* ARIB STD-B67 HLG */) {
                            mInfo = (Info)(mInfo | kInfoIsHdr);
                        }
                    }
                    if (reader.getBitsGraceful(8, &matrixCoeffs)) {
                        mParams.add(kMatrixCoeffs, matrixCoeffs);
                    }
                }
                // skip rest of VUI
            }
        }
    }

    return reader.overRead() ? ERROR_MALFORMED : OK;
}

void HevcParameterSets::FindHEVCDimensions(const sp<ABuffer> &SpsBuffer, int32_t *width, int32_t *height)
{
    ALOGD("FindHEVCDimensions");
    // See Rec. ITU-T H.265 v3 (04/2015) Chapter 7.3.2.2 for reference
    ABitReader reader(SpsBuffer->data() + 1, SpsBuffer->size() - 1);
    // Skip sps_video_parameter_set_id
    reader.skipBits(4);
    uint8_t maxSubLayersMinus1 = reader.getBitsWithFallback(3, 0);
    // Skip sps_temporal_id_nesting_flag;
    reader.skipBits(1);
    // Skip general profile
    reader.skipBits(96);
    if (maxSubLayersMinus1 > 0) {
        bool subLayerProfilePresentFlag[8];
        bool subLayerLevelPresentFlag[8];
        for (int i = 0; i < maxSubLayersMinus1; ++i) {
            subLayerProfilePresentFlag[i] = reader.getBitsWithFallback(1, 0);
            subLayerLevelPresentFlag[i] = reader.getBitsWithFallback(1, 0);
        }
        // Skip reserved
        reader.skipBits(2 * (8 - maxSubLayersMinus1));
        for (int i = 0; i < maxSubLayersMinus1; ++i) {
            if (subLayerProfilePresentFlag[i]) {
                // Skip profile
                reader.skipBits(88);
            }
            if (subLayerLevelPresentFlag[i]) {
                // Skip sub_layer_level_idc[i]
                reader.skipBits(8);
            }
        }
    }
    // Skip sps_seq_parameter_set_id
    skipUE(&reader);
    uint8_t chromaFormatIdc = parseUEWithFallback(&reader, 0);
    if (chromaFormatIdc == 3) {
        // Skip separate_colour_plane_flag
        reader.skipBits(1);
    }
    skipUE(&reader);
    skipUE(&reader);

    // pic_width_in_luma_samples
    *width = parseUEWithFallback(&reader, 0);
    // pic_height_in_luma_samples
    *height = parseUEWithFallback(&reader, 0);
}

status_t HevcParameterSets::parsePps(
        const uint8_t* data UNUSED_PARAM, size_t size UNUSED_PARAM,
        const uint8_t nuhLayerId) {
    ALOGV("parse PPS, nuh_layer_id : %d", nuhLayerId);
    return OK;
}

status_t HevcParameterSets::parseProfileTierLevel(const bool profilePresentFlag, uint8_t maxNumSubLayersMinus1, NALBitReader& reader,
                                                const bool isInVps) {
    ALOGV("parseProfileTierLevel()");
    if (profilePresentFlag) {
        if (isInVps && reader.atLeastNumBitsLeft(88)) {
            mParams.add(kGeneralProfileSpace, reader.getBits(2));
            mParams.add(kGeneralTierFlag, reader.getBits(1));
            mParams.add(kGeneralProfileIdc, reader.getBits(5));
            mParams.add(kGeneralProfileCompatibilityFlags, reader.getBits(32));
            mParams.add(
                    kGeneralConstraintIndicatorFlags,
                    ((uint64_t)reader.getBits(16) << 32) | reader.getBits(32));
        }
    }

    if (isInVps) {
        // general_level_idc
        mParams.add(kGeneralLevelIdc, reader.getBits(8));
        // If the bitstream is the main(main10) profile, the syntax elements below are not necessary for hvcc box.
        // Otherwise, if the bitstream is multiview profile, the max_sub_layers_minus1 would be set to 0,
        // which means that there is no further syntax element to be parsed from this point.
        return reader.overRead() ? ERROR_MALFORMED : OK;
    } else {
        // general profile / tier information
        if (profilePresentFlag) {
            reader.skipBits(88);
        }
        // general_level_idc
        reader.skipBits(8);
    }

    bool subLayerProfilePresentFlag[8];
    bool subLayerLevelPresentFlag[8];
    for (int i = 0; i < maxNumSubLayersMinus1; ++i) {
        subLayerProfilePresentFlag[i] = reader.getBits(1);
        subLayerLevelPresentFlag[i] = reader.getBits(1);
    }
    // Skip
    if (maxNumSubLayersMinus1 > 0) {
        reader.skipBits(2 * (8 - maxNumSubLayersMinus1));
    }
    for (int i = 0; i < maxNumSubLayersMinus1; ++i) {
        if (subLayerProfilePresentFlag[i]) {
            // Skip profile
            reader.skipBits(88);
        }
        if (subLayerLevelPresentFlag[i]) {
            // Skip sub_layer_level_idc[i]
            reader.skipBits(8);
        }
    }

    return reader.overRead() ? ERROR_MALFORMED : OK;
}

status_t HevcParameterSets::parseHrdParameters(const bool cprmsPresentFlag, uint8_t maxNumSubLayersMinus1, NALBitReader* reader) {
    bool nalHrdParamPresentFlag=0, vclHrdParamPresentFlag=0, subPicHrdParamsPresentFlag = 0;
    if (cprmsPresentFlag) {
        // nal_hrd_parameters_present_flag
        nalHrdParamPresentFlag = reader->getBits(1);
        // vcl_hrd_parameters_present_flag
        vclHrdParamPresentFlag = reader->getBits(1);

        if (nalHrdParamPresentFlag || vclHrdParamPresentFlag) {
            // sub_pic_hrd_params_present_flag
            subPicHrdParamsPresentFlag = reader->getBits(1);
            if (subPicHrdParamsPresentFlag) {
                // Skip tick_divisor_minus2
                reader->skipBits(8);
                // Skip du_cpb_removal_delay_increment_length_minus1
                reader->skipBits(5);
                // Skip sub_pic_cpb_params_in_pic_timing_sei_flag
                reader->skipBits(1);
                // Skip dpb_output_delay_du_length_minus1
                reader->skipBits(5);
            }
            // Skip bit_rate_scale
            reader->skipBits(4);
            // Skip cpb_size_scale
            reader->skipBits(4);
            if (subPicHrdParamsPresentFlag) {
                // Skip cbp_size_du_scale
                reader->skipBits(4);
            }
            // Skip initial_cpb_removal_delay_length_minus1
            reader->skipBits(5);
            // Skip au_cpb_removal_delay_length_minus1
            reader->skipBits(5);
            // Skip dpb_output_delay_length_minus1
            reader->skipBits(5);
        }
    }

    for (size_t i = 0; i <= maxNumSubLayersMinus1; i++) {
        uint8_t cpbCntMinus1=0;
        bool fixedPicRateGeneralFlag = 0, fixedPicRateWithinCvsFlag = 0, lowDelayHrdFlag = 0;
        // fixed_pic_rate_general_flag[i]
        fixedPicRateGeneralFlag = reader->getBits(1);
        if (!fixedPicRateGeneralFlag) {
            // fixed_pic_rate_within_cvs_flag[i]
            fixedPicRateWithinCvsFlag = reader->getBits(1);
        }
        if ( fixedPicRateWithinCvsFlag) {
            // Skip elemental_duration_in_tc_minus1[i]
            parseUEWithFallback(reader, 0U);
        } else {
            // low_delay_hrd_flag[i]
            lowDelayHrdFlag = reader->getBits(1);
        }
        if (!lowDelayHrdFlag) {
            // Skip cpb_cnt_minus1[i]
            cpbCntMinus1 = parseUEWithFallback(reader, 0U);
        }
        if (nalHrdParamPresentFlag) {
            status_t err = parseSubLayerHrdParameters(subPicHrdParamsPresentFlag, cpbCntMinus1, reader);
            if (err != OK) {
                ALOGE("error parsing Sub layer HRD Parameters (NAL)");
                return err;
            }
        }
        if (vclHrdParamPresentFlag) {
            status_t err = parseSubLayerHrdParameters(subPicHrdParamsPresentFlag, cpbCntMinus1, reader);
            if (err != OK) {
                ALOGE("error parsing Sub layer HRD Parameters (VCL)");
                return err;
            }
        }
    }
    return reader->overRead() ? ERROR_MALFORMED : OK;
}

status_t HevcParameterSets::parseSubLayerHrdParameters(const bool subPicHrdParamsPresentFlag, const uint8_t cpbCntMinus1, NALBitReader *reader) {
    uint8_t cpbCnt = cpbCntMinus1 + 1;
    for (size_t i = 0; i < cpbCnt; i++) {
        // Skip bit_rate_value_minus1[i]
        parseUEWithFallback(reader, 0U);
        // Skip cpb_size_value_minus1[i]
        parseUEWithFallback(reader, 0U);
        if (subPicHrdParamsPresentFlag) {
            // Skip cpb_size_du_value_minus1[i]
            parseUEWithFallback(reader, 0U);
            // Skip bit_rate_du_value_minus1[i]
            parseUEWithFallback(reader, 0U);
        }
        // Skip cbr_flag[i]
        reader->skipBits(1);
    }
    return reader->overRead() ? ERROR_MALFORMED : OK;
}

status_t HevcParameterSets::parseVpsExtension(const uint8_t maxLayersMinus1, const bool baseLayerInternalFlag, NALBitReader& reader) {
    if (maxLayersMinus1 > 0 && baseLayerInternalFlag) {
        parseProfileTierLevel(0, 0, reader, 0);
    }
    bool splittingFlag = reader.getBits(1);
    bool scalabilityMaskFlag[16];
    std::fill_n(scalabilityMaskFlag, 16, 0);
    uint8_t numScalabilityTypes = 0;
    for (size_t i = 0; i < 16; i++) {
        scalabilityMaskFlag[i] = reader.getBits(1);
        numScalabilityTypes += scalabilityMaskFlag[i];
    }
    uint8_t dimensionIdLenMinus1[16];
    std::fill_n(dimensionIdLenMinus1, 16, 0);
    for (size_t j = 0; j < (numScalabilityTypes - splittingFlag); j++) {
        dimensionIdLenMinus1[j] = reader.getBits(3);
    }
    // F.7.4.3.1, maxNumLayers = 63
    uint8_t dimBitOffset[63];
    std::fill_n(dimBitOffset, 63, 0);
    if (splittingFlag) {
        for (size_t j = 1; j < numScalabilityTypes - 1; j++) {
            for (size_t dimIdx = 0; dimIdx <= j-1; dimIdx++) {
                dimBitOffset[j] += (dimensionIdLenMinus1[dimIdx] + 1);
            }
        }
        dimensionIdLenMinus1[numScalabilityTypes - 1]
                            = 5 - dimBitOffset[numScalabilityTypes -1];
        dimBitOffset[numScalabilityTypes] = 6;
    }
    bool nuhLayerIdPresentFlag = reader.getBits(1);
    uint8_t MaxLayersMinus1 = std::min((uint8_t)63, maxLayersMinus1);
    uint8_t layerIdInNuh[63];
    // F.7.4.3.1
    // For any value of i in the range of 0 to MaxLayersMinus1, inclusive,
    // when not present, the value of layer_id_in_nuh[i] is inferred to be equal to i.
    for (size_t i = 0; i < 63; i++) {
        layerIdInNuh[i] = i;
    }
    uint8_t dimensionId[63][16];
    for (size_t i = 0; i < 63; i++) {
        std::fill_n(dimensionId[i], 16, 0);
    }
    for (size_t i = 1; i <= MaxLayersMinus1; i++) {
        if (nuhLayerIdPresentFlag) {
            // layer_id_in_nuh[i]
            layerIdInNuh[i] = reader.getBits(6);
        }
        if (!splittingFlag) {
            for (size_t j = 0; j < numScalabilityTypes; j++) {
                // dimension_id[i][j]
                dimensionId[i][j] = reader.getBits(dimensionIdLenMinus1[j] + 1);
            }
        } else { // F.7.4.3.1
            for (size_t j = 0; j < numScalabilityTypes; j++) {
                dimensionId[i][j] = ((layerIdInNuh[i] & ((1 << dimBitOffset[j+1]) - 1)) >> dimBitOffset[j]);
            }
        }
    }
    // derive NumViews (please refer to dimension_id[i] semantic for more details.)
    uint8_t NumViews = 1;
    uint8_t ScalabilityId[62][16], ViewOrderIdx[62]; //  DepthLayerFlag[62], DependencyId[62], AuxId[62];
    for (size_t i = 0; i <= MaxLayersMinus1; i++) {
        uint8_t lId = layerIdInNuh[i];
        for (size_t smIdx=0, j=0; smIdx < 16; smIdx++) {
            if (scalabilityMaskFlag[smIdx]) {
                ScalabilityId[i][smIdx] = dimensionId[i][j++];
            } else {
                ScalabilityId[i][smIdx] = 0;
            }
        }
        // DepthLayerFlag[lId] = ScalabilityId[i][0];
         ViewOrderIdx[lId] = ScalabilityId[i][1];
        // DependencyId[lId] = ScalabilityId[i][2];
        // AuxId[lId] = ScalabilityId[i][3];
        if (i > 0) {
            uint8_t newViewFlag = 1;
            for (size_t j = 0; j < i; j++) {
                if (ViewOrderIdx[lId] == ViewOrderIdx[layerIdInNuh[j]]) {
                    newViewFlag = 0;
                }
            }
            NumViews += newViewFlag;
        }
    }
    mParams.add(kNumViews, NumViews);

    return reader.overRead() ? ERROR_MALFORMED : OK;
}

size_t HevcParameterSets::numBitsParsedExpGolomb(uint8_t symbol) {
    uint8_t value = symbol + 1;
    size_t counter = 0;
    while (value != 0) {
        ++counter;
        value >>= 1;
    }
    return counter * 2 - 1;
}

status_t HevcParameterSets::makeHvcc(uint8_t *hvcc, size_t *hvccSize, size_t nalSizeLength) {
    if (hvcc == NULL || hvccSize == NULL
            || (nalSizeLength != 4 && nalSizeLength != 2)) {
        return BAD_VALUE;
    }
    // ISO 14496-15: HEVC file format
    size_t size = 23;  // 23 bytes in the header
    size_t numOfArrays = 0;
    const size_t numNalUnits = getNumNalUnits();
    for (size_t i = 0; i < ARRAY_SIZE(kHevcNalUnitTypes); ++i) {
        uint8_t type = kHevcNalUnitTypes[i];
        size_t numNalus = getNumNalUnitsOfType(type);
        if (numNalus == 0) {
            continue;
        }
        ++numOfArrays;
        size += 3;
        for (size_t j = 0; j < numNalUnits; ++j) {
            if (getType(j) != type) {
                continue;
            }
            size += 2 + getSize(j);
        }
    }
    uint8_t generalProfileSpace, generalTierFlag, generalProfileIdc;
    if (!findParam8(kGeneralProfileSpace, &generalProfileSpace)
            || !findParam8(kGeneralTierFlag, &generalTierFlag)
            || !findParam8(kGeneralProfileIdc, &generalProfileIdc)) {
        return ERROR_MALFORMED;
    }
    uint32_t compatibilityFlags;
    uint64_t constraintIdcFlags;
    if (!findParam32(kGeneralProfileCompatibilityFlags, &compatibilityFlags)
            || !findParam64(kGeneralConstraintIndicatorFlags, &constraintIdcFlags)) {
        return ERROR_MALFORMED;
    }
    uint8_t generalLevelIdc;
    if (!findParam8(kGeneralLevelIdc, &generalLevelIdc)) {
        return ERROR_MALFORMED;
    }
    uint8_t chromaFormatIdc, bitDepthLumaMinus8, bitDepthChromaMinus8;
    if (!findParam8(kChromaFormatIdc, &chromaFormatIdc)
            || !findParam8(kBitDepthLumaMinus8, &bitDepthLumaMinus8)
            || !findParam8(kBitDepthChromaMinus8, &bitDepthChromaMinus8)) {
        return ERROR_MALFORMED;
    }
    if (size > *hvccSize) {
        return NO_MEMORY;
    }
    *hvccSize = size;

    uint8_t *header = hvcc;
    header[0] = 1;
    header[1] = (generalProfileSpace << 6) | (generalTierFlag << 5) | generalProfileIdc;
    header[2] = (compatibilityFlags >> 24) & 0xff;
    header[3] = (compatibilityFlags >> 16) & 0xff;
    header[4] = (compatibilityFlags >> 8) & 0xff;
    header[5] = compatibilityFlags & 0xff;
    header[6] = (constraintIdcFlags >> 40) & 0xff;
    header[7] = (constraintIdcFlags >> 32) & 0xff;
    header[8] = (constraintIdcFlags >> 24) & 0xff;
    header[9] = (constraintIdcFlags >> 16) & 0xff;
    header[10] = (constraintIdcFlags >> 8) & 0xff;
    header[11] = constraintIdcFlags & 0xff;
    header[12] = generalLevelIdc;
    // FIXME: parse min_spatial_segmentation_idc.
    header[13] = 0xf0;
    header[14] = 0;
    // FIXME: derive parallelismType properly.
    header[15] = 0xfc;
    header[16] = 0xfc | chromaFormatIdc;
    header[17] = 0xf8 | bitDepthLumaMinus8;
    header[18] = 0xf8 | bitDepthChromaMinus8;
    // FIXME: derive avgFrameRate
    header[19] = 0;
    header[20] = 0;
    // constantFrameRate, numTemporalLayers, temporalIdNested all set to 0.
    header[21] = nalSizeLength - 1;
    header[22] = numOfArrays;
    header += 23;
    for (size_t i = 0; i < ARRAY_SIZE(kHevcNalUnitTypes); ++i) {
        uint8_t type = kHevcNalUnitTypes[i];
        size_t numNalus = getNumNalUnitsOfType(type);
        if (numNalus == 0) {
            continue;
        }
        // array_completeness set to 1.
        header[0] = type | 0x80;
        header[1] = (numNalus >> 8) & 0xff;
        header[2] = numNalus & 0xff;
        header += 3;
        for (size_t j = 0; j < numNalUnits; ++j) {
            if (getType(j) != type) {
                continue;
            }
            header[0] = (getSize(j) >> 8) & 0xff;
            header[1] = getSize(j) & 0xff;
            if (!write(j, header + 2, size - (header - (uint8_t *)hvcc))) {
                return NO_MEMORY;
            }
            header += (2 + getSize(j));
        }
    }
    CHECK_EQ(header - size, hvcc);
    return OK;
}

status_t HevcParameterSets::makeHvcc_l(uint8_t *hvcc, size_t *hvccSize, size_t nalSizeLength) {
    if (hvcc == NULL || hvccSize == NULL
            || (nalSizeLength != 4 && nalSizeLength != 2)) {
        return BAD_VALUE;
    }
    // ISO 14496-15: HEVC file format
    size_t size = 23;  // 23 bytes in the header
    size_t numOfArrays = 0;
    const size_t numNalUnits = getNumNalUnits();
    for (size_t i = 0; i < ARRAY_SIZE(kHevcNalUnitTypes); ++i) {
        uint8_t type = kHevcNalUnitTypes[i];
        size_t numNalus = getNumNalUnitsOfType(type, 0);
        if (numNalus == 0) {
            continue;
        }
        ++numOfArrays;
        size += 3;
        for (size_t j = 0; j < numNalUnits; ++j) {
            if (getType(j) != type || getLayerId(j) != 0) {
                continue;
            }
            size += 2 + getSize(j);
        }
    }
    uint8_t generalProfileSpace, generalTierFlag, generalProfileIdc;
    if (!findParam8(kGeneralProfileSpace, &generalProfileSpace)
            || !findParam8(kGeneralTierFlag, &generalTierFlag)
            || !findParam8(kGeneralProfileIdc, &generalProfileIdc)) {
        return ERROR_MALFORMED;
    }
    uint32_t compatibilityFlags;
    uint64_t constraintIdcFlags;
    if (!findParam32(kGeneralProfileCompatibilityFlags, &compatibilityFlags)
            || !findParam64(kGeneralConstraintIndicatorFlags, &constraintIdcFlags)) {
        return ERROR_MALFORMED;
    }
    uint8_t generalLevelIdc;
    if (!findParam8(kGeneralLevelIdc, &generalLevelIdc)) {
        return ERROR_MALFORMED;
    }
    uint8_t chromaFormatIdc, bitDepthLumaMinus8, bitDepthChromaMinus8;
    if (!findParam8(kChromaFormatIdc, &chromaFormatIdc)
            || !findParam8(kBitDepthLumaMinus8, &bitDepthLumaMinus8)
            || !findParam8(kBitDepthChromaMinus8, &bitDepthChromaMinus8)) {
        return ERROR_MALFORMED;
    }
    if (size > *hvccSize) {
        return NO_MEMORY;
    }
    *hvccSize = size;

    uint8_t *header = hvcc;
    header[0] = 1;
    header[1] = (generalProfileSpace << 6) | (generalTierFlag << 5) | generalProfileIdc;
    header[2] = (compatibilityFlags >> 24) & 0xff;
    header[3] = (compatibilityFlags >> 16) & 0xff;
    header[4] = (compatibilityFlags >> 8) & 0xff;
    header[5] = compatibilityFlags & 0xff;
    header[6] = (constraintIdcFlags >> 40) & 0xff;
    header[7] = (constraintIdcFlags >> 32) & 0xff;
    header[8] = (constraintIdcFlags >> 24) & 0xff;
    header[9] = (constraintIdcFlags >> 16) & 0xff;
    header[10] = (constraintIdcFlags >> 8) & 0xff;
    header[11] = constraintIdcFlags & 0xff;
    header[12] = generalLevelIdc;
    // FIXME: parse min_spatial_segmentation_idc.
    header[13] = 0xf0;
    header[14] = 0;
    // FIXME: derive parallelismType properly.
    header[15] = 0xfc;
    header[16] = 0xfc | chromaFormatIdc;
    header[17] = 0xf8 | bitDepthLumaMinus8;
    header[18] = 0xf8 | bitDepthChromaMinus8;
    // FIXME: derive avgFrameRate
    header[19] = 0;
    header[20] = 0;
    // constantFrameRate, numTemporalLayers, temporalIdNested all set to 0.
    header[21] = nalSizeLength - 1;
    header[22] = numOfArrays;
    header += 23;
    for (size_t i = 0; i < ARRAY_SIZE(kHevcNalUnitTypes); ++i) {
        uint8_t type = kHevcNalUnitTypes[i];
        size_t numNalus = getNumNalUnitsOfType(type, 0);
        if (numNalus == 0) {
            continue;
        }
        // array_completeness set to 1.
        header[0] = type | 0x80;
        header[1] = (numNalus >> 8) & 0xff;
        header[2] = numNalus & 0xff;
        header += 3;
        for (size_t j = 0; j < numNalUnits; ++j) {
            if (getType(j) != type || getLayerId(j) != 0) {
                continue;
            }
            header[0] = (getSize(j) >> 8) & 0xff;
            header[1] = getSize(j) & 0xff;
            if (!write(j, header + 2, size - (header - (uint8_t *)hvcc))) {
                return NO_MEMORY;
            }
            header += (2 + getSize(j));
        }
    }
    CHECK_EQ(header - size, hvcc);
    return OK;
}

status_t HevcParameterSets::makeLhvc(uint8_t *lhvc, size_t *lhvcSize,
        size_t nalSizeLength) {
    ALOGV("makeLhvc() start");
    if (lhvc == NULL || lhvcSize == NULL
            || (nalSizeLength != 4 && nalSizeLength != 2)) {
        ALOGE("nalSizeLength error");
        return BAD_VALUE;
    }
    // ISO 14496-15: HEVC file format
    size_t size = 6;  // 6 bytes in the header
    size_t numOfArrays = 0;
    const size_t numNalUnits = getNumNalUnits();
    for (size_t i = 0; i < ARRAY_SIZE(kHevcNalUnitTypes); ++i) {
        uint8_t type = kHevcNalUnitTypes[i];
        size_t numNalus = getNumNalUnitsOfType(type, 1);
        if (numNalus == 0) {
            continue;
        }
        ++numOfArrays;
        size += 3;
        for (size_t j = 0; j < numNalUnits; ++j) {
            if (getType(j) != type || getLayerId(j) != 1) {
                continue;
            }
            size += 2 + getSize(j);
        }
    }

    if (size > *lhvcSize) {
        ALOGE("size : %d, numNalus : %d", size, numNalUnits);
        return NO_MEMORY;
    }
    *lhvcSize = size;

    uint8_t *header = lhvc;
    header[0] = 1;
    // FIXME: derive parallelismType properly.
    header[1] = 0xf0;
    header[2] = 0x00;
    // FIXME: derive parallelismType properly.
    header[3] = 0xfc;
    // FIXME: derive numTemporalLayers, temporalIdNested properly.
    header[4] = 0xc8 | (nalSizeLength -1);
    header[5] = numOfArrays;
    header += 6;
    for (size_t i = 0; i < ARRAY_SIZE(kHevcNalUnitTypes); ++i) {
        uint8_t type = kHevcNalUnitTypes[i];
        // Suppose only stereo video is available (2-view only)
        size_t numNalus = getNumNalUnitsOfType(type, 1);
        if (numNalus == 0) {
            continue;
        }
        // array_completeness set to 1.
        header[0] = type | 0x80;
        header[1] = (numNalus >> 8) & 0xff;
        header[2] = numNalus & 0xff;
        header += 3;
        for (size_t j = 0; j < numNalUnits; ++j) {
            if (getType(j) != type || getLayerId(j) != 1) {
                continue;
            }
            header[0] = (getSize(j) >> 8) & 0xff;
            header[1] = getSize(j) & 0xff;
            if (!write(j, header + 2, size - (header - (uint8_t *)lhvc))) {
                return NO_MEMORY;
            }
            header += (2 + getSize(j));
        }
    }
    CHECK_EQ(header - size, lhvc);
    return OK;
}

status_t HevcParameterSets::makeHero(uint8_t *hero) {
    ALOGV("makeHero()");

    // Please refer to Stereo-video-isobmff-extensions for details.
    uint8_t leftViewId;
    if (!findParam8(kSeiLeftViewId, &leftViewId)) {
        *hero = 0; // meaning none
    } else {
        if(leftViewId == 0) {
            *hero = 1; // left view is the hero
        } else if (leftViewId == 1) {
            *hero = 2; // right view is the hero
        } else {
            return ERROR_MALFORMED;
        }
    }

    ALOGV("hero : %#04x", *hero);
    return OK;
}

status_t HevcParameterSets::parseSeiMessage(const uint8_t *data, size_t size) {
    uint32_t payloadType = 0;
    uint32_t payloadSize = 0;
    size_t byteRead = 0;
    status_t err = OK;
    while (data[0] == 0xff) {
        payloadType += 255;
        ++byteRead;
        ++data;
    }
    payloadType += data[0];
    ++data;
    ++byteRead;
    while (data[0] == 0xff) {
        payloadSize += 255;
        ++byteRead;
        ++data;
    }
    payloadSize += data[0];
    ++data;
    ++byteRead;

    if (payloadType == 176) { // three dimensional reference info SEI
        err = parseThreeDimensionalReferenceInfoSei(data, size - byteRead);
    }
    return err;
}

status_t HevcParameterSets::parseThreeDimensionalReferenceInfoSei(const uint8_t *data, size_t size) {
    ALOGV("three-dimensional reference displays info()");

    NALBitReader reader(data, size);
    // prec_ref_display_width
    uint32_t prec_ref_display_width = parseUEWithFallback(&reader, 0U);

    if (reader.getBits(1)) { // ref_viewing_distance_flag
        parseUEWithFallback(&reader, 0U);
    }
    uint8_t numRefDisp = parseUEWithFallback(&reader, 0U); // num_ref_displays_minus1
    if (numRefDisp) {
        // FIXME : Currenly only stereo video is allowed.
        return ERROR_MALFORMED;
    }
    for (uint8_t i = 0; i<=numRefDisp; ++i) {
        uint8_t leftViewId = parseUEWithFallback(&reader, 0U); // left_view_id[i]
        // if left_view_id[i] is 0, left view is primary,
        // otherwise a right view video is a base view.
        mParams.add(kSeiLeftViewId, leftViewId);
    }
    return OK;
}

bool HevcParameterSets::IsHevcIDR(const uint8_t *data, size_t size) {
    bool foundIDR = false;
    const uint8_t *nalStart;
    size_t nalSize;
    while (!foundIDR && getNextNALUnit(&data, &size, &nalStart, &nalSize, true) == OK) {
        if (nalSize == 0) {
            ALOGE("Encountered zero-length HEVC NAL");
            return false;
        }

        uint8_t nalType = (nalStart[0] & 0x7E) >> 1;
        switch (nalType) {
            case kHevcNalUnitTypeCodedSliceIdr:
            case kHevcNalUnitTypeCodedSliceIdrNoLP:
            case kHevcNalUnitTypeCodedSliceCra:
                foundIDR = true;
                break;
        }
    }

    return foundIDR;
}

// indicates whether the current bitstream is mv-hevc bitstream
bool HevcParameterSets::IsMvHevc() {
    if (mIsLhevc) {
        return true;
    } else {
        uint8_t numViews = 1;
        if (findParam8(kNumViews, &numViews)) {
            ALOGV("%s", numViews > 1 ? "[MV-HEVC] This bitstream is stereo video."
            : "[MV-HEVC] This bitstream is single view video.");
            return numViews > 1;
        }
    }
    return false;
}
}  // namespace android
