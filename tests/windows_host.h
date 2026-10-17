#pragma once
#include "../src/win/tsf_compat.h"
#include <textstor.h>
using namespace atypoi::win;

// Installation-free integration host: real Windows document contexts and ranges;
// only key-sink subscription is adapted because Windows requires a registered TIP.
class TestManager final:public ITfThreadMgr,public ITfKeystrokeMgr {
    LONG refs_=1;Ptr<ITfThreadMgr> real_;
public:
    explicit TestManager(ITfThreadMgr* real):real_(real){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==__uuidof(ITfThreadMgr))*out=static_cast<ITfThreadMgr*>(this);else if(id==__uuidof(ITfKeystrokeMgr))*out=static_cast<ITfKeystrokeMgr*>(this);else return real_->QueryInterface(id,out);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Activate(TfClientId* p) override{return real_->Activate(p);}
    HRESULT STDMETHODCALLTYPE Deactivate() override{return real_->Deactivate();}
    HRESULT STDMETHODCALLTYPE CreateDocumentMgr(ITfDocumentMgr** p) override{return real_->CreateDocumentMgr(p);}
    HRESULT STDMETHODCALLTYPE EnumDocumentMgrs(IEnumTfDocumentMgrs** p) override{return real_->EnumDocumentMgrs(p);}
    HRESULT STDMETHODCALLTYPE GetFocus(ITfDocumentMgr** p) override{return real_->GetFocus(p);}
    HRESULT STDMETHODCALLTYPE SetFocus(ITfDocumentMgr* p) override{return real_->SetFocus(p);}
    HRESULT STDMETHODCALLTYPE AssociateFocus(HWND h,ITfDocumentMgr* n,ITfDocumentMgr** p) override{return real_->AssociateFocus(h,n,p);}
    HRESULT STDMETHODCALLTYPE IsThreadFocus(BOOL* p) override{return real_->IsThreadFocus(p);}
    HRESULT STDMETHODCALLTYPE GetFunctionProvider(REFCLSID c,ITfFunctionProvider** p) override{return real_->GetFunctionProvider(c,p);}
    HRESULT STDMETHODCALLTYPE EnumFunctionProviders(IEnumTfFunctionProviders** p) override{return real_->EnumFunctionProviders(p);}
    HRESULT STDMETHODCALLTYPE GetGlobalCompartment(ITfCompartmentMgr** p) override{return real_->GetGlobalCompartment(p);}
    HRESULT STDMETHODCALLTYPE AdviseKeyEventSink(TfClientId,ITfKeyEventSink*,BOOL) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE UnadviseKeyEventSink(TfClientId) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetForeground(CLSID* p) override{*p=serviceId;return S_OK;}
    HRESULT STDMETHODCALLTYPE TestKeyDown(WPARAM,LPARAM,BOOL*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE TestKeyUp(WPARAM,LPARAM,BOOL*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE KeyDown(WPARAM,LPARAM,BOOL*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE KeyUp(WPARAM,LPARAM,BOOL*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetPreservedKey(ITfContext*,const TF_PRESERVEDKEY*,GUID*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE IsPreservedKey(REFGUID,const TF_PRESERVEDKEY*,BOOL*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE PreserveKey(TfClientId,REFGUID,const TF_PRESERVEDKEY*,const WCHAR*,ULONG) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE UnpreserveKey(REFGUID,const TF_PRESERVEDKEY*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetPreservedKeyDescription(REFGUID,const WCHAR*,ULONG) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetPreservedKeyDescription(REFGUID,BSTR*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SimulatePreservedKey(ITfContext*,REFGUID,BOOL*) override{return E_NOTIMPL;}
};
class TestStore final:public ITextStoreACP {
    LONG refs_=1;Ptr<ITextStoreACPSink> sink_;bool locked_=false;
public:
    std::wstring text;LONG start=0,end=0;bool readOnly=false,failSelection=false;
    HWND window=nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=__uuidof(ITextStoreACP))return E_NOINTERFACE;*out=static_cast<ITextStoreACP*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID id,IUnknown* value,DWORD) override{if(id!=__uuidof(ITextStoreACPSink))return E_INVALIDARG;return query(value,sink_);}
    HRESULT STDMETHODCALLTYPE UnadviseSink(IUnknown*) override{sink_.reset();return S_OK;}
    HRESULT STDMETHODCALLTYPE RequestLock(DWORD flags,HRESULT* result) override{if(!result)return E_POINTER;if(locked_){*result=TS_E_SYNCHRONOUS;return S_OK;}if(!sink_)return E_FAIL;locked_=true;*result=sink_->OnLockGranted(flags);locked_=false;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetStatus(TS_STATUS* status) override{*status={};status->dwDynamicFlags=readOnly?TS_SD_READONLY:0;return S_OK;}
    HRESULT STDMETHODCALLTYPE QueryInsert(LONG a,LONG b,ULONG,LONG* x,LONG* y) override{*x=a;*y=b;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSelection(ULONG,ULONG count,TS_SELECTION_ACP* selection,ULONG* fetched) override{*fetched=count?1:0;if(count)*selection={start,end,{TS_AE_END,FALSE}};return S_OK;}
    HRESULT STDMETHODCALLTYPE SetSelection(ULONG count,const TS_SELECTION_ACP* selection) override{if(failSelection)return E_FAIL;if(count){start=selection[0].acpStart;end=selection[0].acpEnd;}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetText(LONG a,LONG b,WCHAR* output,ULONG requested,ULONG* returned,TS_RUNINFO* runs,ULONG runCount,ULONG* runsReturned,LONG* next) override {
        if(a<0||a>static_cast<LONG>(text.size()))return TS_E_INVALIDPOS;if(b<0)b=static_cast<LONG>(text.size());b=std::clamp(b,a,static_cast<LONG>(text.size()));
        ULONG count=std::min(requested,static_cast<ULONG>(b-a));if(count&&output)std::copy_n(text.data()+a,count,output);
        if(returned)*returned=count;if(runsReturned)*runsReturned=runCount&&b>a?1:0;if(runCount&&runs&&b>a)runs[0]={static_cast<ULONG>(b-a),TS_RT_PLAIN};if(next)*next=a+count;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetText(DWORD,LONG a,LONG b,const WCHAR* value,ULONG count,TS_TEXTCHANGE* change) override {
        if(a<0||b<a||b>static_cast<LONG>(text.size()))return TS_E_INVALIDPOS;
        text.replace(static_cast<size_t>(a),static_cast<size_t>(b-a),value,count);start=end=a+static_cast<LONG>(count);if(change)*change={a,b,end};return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFormattedText(LONG,LONG,IDataObject**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetEmbedded(LONG,REFGUID,REFIID,IUnknown**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE QueryInsertEmbedded(const GUID*,const FORMATETC*,BOOL* value) override{*value=FALSE;return S_OK;}
    HRESULT STDMETHODCALLTYPE InsertEmbedded(DWORD,LONG,LONG,IDataObject*,TS_TEXTCHANGE*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE InsertTextAtSelection(DWORD flags,const WCHAR* value,ULONG count,LONG* a,LONG* b,TS_TEXTCHANGE* change) override {
        LONG oldStart=start,oldEnd=end;if(a)*a=start;if(b)*b=end;
        if(flags&TS_IAS_QUERYONLY)return S_OK;return SetText(0,oldStart,oldEnd,value,count,change);
    }
    HRESULT STDMETHODCALLTYPE InsertEmbeddedAtSelection(DWORD,IDataObject*,LONG*,LONG*,TS_TEXTCHANGE*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RequestSupportedAttrs(DWORD,ULONG,const TS_ATTRID*) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE RequestAttrsAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE RequestAttrsTransitioningAtPosition(LONG,ULONG,const TS_ATTRID*,DWORD) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE FindNextAttrTransition(LONG,LONG halt,ULONG,const TS_ATTRID*,DWORD,LONG* next,BOOL* found,LONG* offset) override{*next=halt;*found=FALSE;*offset=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE RetrieveRequestedAttrs(ULONG,TS_ATTRVAL*,ULONG* fetched) override{*fetched=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEndACP(LONG* endValue) override{*endValue=static_cast<LONG>(text.size());return S_OK;}
    HRESULT STDMETHODCALLTYPE GetActiveView(TsViewCookie* view) override{*view=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetACPFromPoint(TsViewCookie,const POINT*,DWORD,LONG* result) override{*result=start;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTextExt(TsViewCookie,LONG,LONG,RECT* rect,BOOL* clipped) override{*rect={0,0,100,24};*clipped=FALSE;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetScreenExt(TsViewCookie,RECT* rect) override{*rect={0,0,800,600};return S_OK;}
    HRESULT STDMETHODCALLTYPE GetWnd(TsViewCookie,HWND* out) override{*out=window;return S_OK;}
    void insertHost(wchar_t character) {LONG old=end;text.insert(static_cast<size_t>(start),1,character);start=end=start+1;if(sink_){TS_TEXTCHANGE change{old,old,end};sink_->OnTextChange(0,&change);sink_->OnSelectionChange();}}
    void setReadOnly(bool value) {readOnly=value;if(sink_)sink_->OnStatusChange(value?TS_SD_READONLY:0);}
};
