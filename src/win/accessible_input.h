#pragma once
#include "platform.h"
#include <oleacc.h>

namespace atypoi::win::accessible {
inline bool qtWindow(std::wstring name){
    name=lowerPath(std::move(name));if(!name.starts_with(L"qt"))return false;
    size_t end=2;while(end<name.size()&&name[end]>=L'0'&&name[end]<=L'9')++end;
    return end>2&&(name.substr(end)==L"qwindowicon"||name.substr(end)==L"qwindowowndcicon");
}
// Read only focus, role and state. Never request accessible text, names or values.
inline bool focusedEditable(IAccessible* root){
    Ptr<IAccessible> current(root);if(!current)return false;
    for(int depth=0;depth<16;++depth){
        VARIANT focus;VariantInit(&focus);auto hr=current->get_accFocus(&focus);
        if(hr!=S_OK){VariantClear(&focus);return false;}
        if(focus.vt==VT_DISPATCH&&focus.pdispVal){
            Ptr<IAccessible> next;query(focus.pdispVal,next);VariantClear(&focus);
            if(!next)return false;current=std::move(next);continue;
        }
        if(focus.vt!=VT_I4||focus.lVal<CHILDID_SELF){VariantClear(&focus);return false;}
        VARIANT role,state;VariantInit(&role);VariantInit(&state);
        auto roleHr=current->get_accRole(focus,&role),stateHr=current->get_accState(focus,&state);
        constexpr LONG blocked=STATE_SYSTEM_PROTECTED|STATE_SYSTEM_READONLY|STATE_SYSTEM_UNAVAILABLE|STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN;
        bool allowed=roleHr==S_OK&&stateHr==S_OK&&role.vt==VT_I4&&state.vt==VT_I4&&role.lVal==ROLE_SYSTEM_TEXT&&(state.lVal&STATE_SYSTEM_FOCUSED)&&!(state.lVal&blocked);
        VariantClear(&focus);VariantClear(&role);VariantClear(&state);return allowed;
    }
    return false;
}
class QtInput {
    HWND window_=nullptr;Ptr<IAccessible> root_;bool verified_=false,acquiring_=false;
public:
    void reset(){window_=nullptr;root_.reset();verified_=false;}
    void update(HWND window){
        verified_=false;if(acquiring_)return;
        DWORD pid=0;if(window)GetWindowThreadProcessId(window,&pid);
        wchar_t name[128]{};if(window)GetClassNameW(window,name,128);
        if(pid!=GetCurrentProcessId()||!qtWindow(name)){reset();return;}
        if(window_!=window){reset();window_=window;}
        if(!root_){
            acquiring_=true;
            auto hr=AccessibleObjectFromWindow(window,OBJID_CLIENT,IID_IAccessible,reinterpret_cast<void**>(root_.put()));
            acquiring_=false;if(FAILED(hr))return;
        }
        verified_=focusedEditable(root_.get());
    }
    // The proof is refreshed before every key, outside the TSF edit lock.
    bool permits(HWND window)const{return verified_&&window_==window&&GetFocus()==window;}
};
}
