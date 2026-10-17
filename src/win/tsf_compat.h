#pragma once
#include "platform.h"
#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL 0
#endif
#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM 0
#endif
// Public Windows SDK COM contracts omitted by some MinGW header versions.
#ifndef __ITfTextInputProcessorEx_INTERFACE_DEFINED__
MIDL_INTERFACE("6e4e2102-f9cd-433d-b496-303ce03a6507") ITfTextInputProcessorEx : public ITfTextInputProcessor {
    virtual HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr*,TfClientId,DWORD)=0;
};
__CRT_UUID_DECL(ITfTextInputProcessorEx,0x6e4e2102,0xf9cd,0x433d,0xb4,0x96,0x30,0x3c,0xe0,0x3a,0x65,0x07)
#endif
#ifndef __ITfDisplayAttributeProvider_INTERFACE_DEFINED__
MIDL_INTERFACE("fee47777-163c-4769-996a-6e9c50ad8f54") ITfDisplayAttributeProvider : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(REFGUID,ITfDisplayAttributeInfo**)=0;
};
__CRT_UUID_DECL(ITfDisplayAttributeProvider,0xfee47777,0x163c,0x4769,0x99,0x6a,0x6e,0x9c,0x50,0xad,0x8f,0x54)
#endif
