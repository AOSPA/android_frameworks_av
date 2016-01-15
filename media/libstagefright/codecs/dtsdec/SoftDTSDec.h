/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef SOFT_DTS_DEC_H_
#define SOFT_DTS_DEC_H_

#include "SimpleSoftOMXComponent.h"

namespace android {

#define DTS_M8_OMX_LIB "libomx-dts.so"

typedef OMX_ERRORTYPE (*fPtrInit)();
typedef OMX_ERRORTYPE (*fPtrGetHandle)(
                                OMX_OUT OMX_HANDLETYPE* pHandle,
                                OMX_IN  OMX_STRING cComponentName,
                                OMX_IN  OMX_PTR pAppData,
                                OMX_IN  OMX_CALLBACKTYPE* pCallBacks);
typedef OMX_ERRORTYPE (*fPtrFreeHandle)(
                                OMX_IN  OMX_HANDLETYPE hComponent);
typedef OMX_ERRORTYPE (*fPtrDeinit)();

struct SoftDTSDec : public SimpleSoftOMXComponent
{
    SoftDTSDec( const char *name,
                const OMX_CALLBACKTYPE *callbacks,
                OMX_PTR appData,
                OMX_COMPONENTTYPE **component );

    OMX_ERRORTYPE sendCommand(OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR data);

    OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, OMX_PTR params);
    OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index, const OMX_PTR params);

    OMX_ERRORTYPE getConfig(OMX_INDEXTYPE index, OMX_PTR params);
    OMX_ERRORTYPE setConfig(OMX_INDEXTYPE index, const OMX_PTR params);

    OMX_ERRORTYPE getExtensionIndex(const char *name, OMX_INDEXTYPE *index);

    OMX_ERRORTYPE useBuffer(OMX_BUFFERHEADERTYPE **buffer,
                            OMX_U32 portIndex,
                            OMX_PTR appPrivate,
                            OMX_U32 size,
                            OMX_U8 *ptr);

    OMX_ERRORTYPE allocateBuffer(OMX_BUFFERHEADERTYPE **buffer,
                                 OMX_U32 portIndex,
                                 OMX_PTR appPrivate,
                                 OMX_U32 size);

    OMX_ERRORTYPE freeBuffer(OMX_U32 portIndex,
                             OMX_BUFFERHEADERTYPE *buffer);

    OMX_ERRORTYPE emptyThisBuffer(OMX_BUFFERHEADERTYPE *buffer);

    OMX_ERRORTYPE fillThisBuffer(OMX_BUFFERHEADERTYPE *buffer);

    OMX_ERRORTYPE getState(OMX_STATETYPE *state);

protected:
    ~SoftDTSDec();

private:
    OMX_HANDLETYPE mComponentHandle; // handle to DTS OMX component instance
    void *mOmxLibHandle;

    DISALLOW_EVIL_CONSTRUCTORS(SoftDTSDec);
};

}  // namespace android

#endif  // SOFT_DTS_DEC_H_
