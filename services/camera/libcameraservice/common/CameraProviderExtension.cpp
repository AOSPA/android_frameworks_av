/*
 * Copyright 2024 The LibreMobileOS Foundation
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

#ifndef TARGET_PROVIDES_CAMERA_PROVIDER_EXT_LIB

#include "common/CameraProviderExtension.h"

bool supportsTorchStrengthControlExt() {
    return false;
}

int32_t getTorchDefaultStrengthLevelExt() {
    return 0;
}

int32_t getTorchMaxStrengthLevelExt() {
    return 0;
}

int32_t getTorchStrengthLevelExt() {
    return 0;
}

void setTorchStrengthLevelExt(__unused int32_t torchStrength) {
    // Nothing
}

#endif
