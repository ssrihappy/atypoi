#pragma once
#include "../src/win/accessible_input.h"
using namespace atypoi::win;

// Privacy fixtures expose only role/state/focus. Any request for actual content
// increments contentReads, so the regression detects accidental text inspection.
class TestAccessible final:public IAccessible {
    LONG refs_=1;
public:
    LONG role=ROLE_SYSTEM_TEXT,state=STATE_SYSTEM_FOCUSED|STATE_SYSTEM_FOCUSABLE;
    HRESULT focusResult=S_OK,roleResult=S_OK,stateResult=S_OK;
    VARTYPE focusType=VT_I4,roleType=VT_I4,stateType=VT_I4;
    bool cycle=false;int contentReads=0,focusReads=0;Ptr<IAccessible> child;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch&&id!=IID_IAccessible)return E_NOINTERFACE;*out=static_cast<IAccessible*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accParent(IDispatch**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accChildCount(LONG*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accChild(VARIANT,IDispatch**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accName(VARIANT,BSTR*) override{++contentReads;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accValue(VARIANT,BSTR*) override{++contentReads;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT,BSTR*) override{++contentReads;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accRole(VARIANT,VARIANT* out) override{VariantInit(out);if(roleResult!=S_OK)return roleResult;out->vt=roleType;if(roleType==VT_I4)out->lVal=role;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_accState(VARIANT,VARIANT* out) override{VariantInit(out);if(stateResult!=S_OK)return stateResult;out->vt=stateType;if(stateType==VT_I4)out->lVal=state;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT,BSTR*) override{++contentReads;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR*,VARIANT,LONG*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT,BSTR*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* out) override{++focusReads;VariantInit(out);if(focusResult!=S_OK)return focusResult;if(child||cycle){out->vt=VT_DISPATCH;out->pdispVal=child?static_cast<IDispatch*>(child.get()):static_cast<IDispatch*>(this);out->pdispVal->AddRef();}else{out->vt=focusType;if(focusType==VT_I4)out->lVal=CHILDID_SELF;}return S_OK;}
    HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT,BSTR*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE accSelect(LONG,VARIANT) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE accLocation(LONG*,LONG*,LONG*,LONG*,VARIANT) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE accNavigate(LONG,VARIANT,VARIANT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE accHitTest(LONG,LONG,VARIANT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE put_accName(VARIANT,BSTR) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE put_accValue(VARIANT,BSTR) override{return E_NOTIMPL;}
};
class TestUnknownAlias final:public IUnknown {
    LONG refs_=1;Ptr<IUnknown> object_;
public:
    explicit TestUnknownAlias(IUnknown* object):object_(object){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{return object_->QueryInterface(id,out);}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
};
struct AccessibleWindowFixture {
    IAccessible* accessible=nullptr;int acquisitions=0;
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){
        if(message==WM_NCCREATE)SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        auto self=reinterpret_cast<AccessibleWindowFixture*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_GETOBJECT&&static_cast<LONG>(l)==OBJID_CLIENT&&self){++self->acquisitions;return LresultFromObject(IID_IAccessible,w,self->accessible);}
        return DefWindowProcW(window,message,w,l);
    }
};
