// An owned, synthetic IMM editor. It exercises Windows' real TSF-to-IMM bridge
// without Qt dependencies, network access, chat history or message transmission.
#include "accessible_host.h"
#include <imm.h>
#include <filesystem>

namespace {
std::wstring committed,composing;
TestAccessible field;
Ptr<IClassFactory> factory;
DWORD factoryCookie=0;
HMODULE inputService=nullptr;
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_GETOBJECT&&static_cast<LONG>(l)==OBJID_CLIENT)return LresultFromObject(IID_IAccessible,w,&field);
    if(message==WM_GETTEXT){
        auto value=committed+composing;auto count=std::min<size_t>(value.size(),w?w-1:0);
        if(w){std::copy_n(value.data(),count,reinterpret_cast<wchar_t*>(l));reinterpret_cast<wchar_t*>(l)[count]=0;}return count;
    }
    if(message==WM_GETTEXTLENGTH)return committed.size()+composing.size();
    if(message==WM_APP+1){
        Ptr<ITfInputProcessorProfiles> profiles;Ptr<ITfInputProcessorProfileMgr> manager;
        auto hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfiles),reinterpret_cast<void**>(profiles.put()));
        if(SUCCEEDED(hr))hr=query(profiles.get(),manager);
        if(SUCCEEDED(hr))hr=profiles->ChangeCurrentLanguage(languageId);
        // Thread only: do not change the user's session profile or preferences.
        if(SUCCEEDED(hr))hr=manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,languageId,serviceId,profileId,nullptr,0);
        return hr;
    }
    if(message==WM_APP+2){if(!composing.empty())return FALSE;committed.clear();InvalidateRect(window,nullptr,TRUE);return TRUE;}
    if(message==WM_APP+3){field.state=STATE_SYSTEM_FOCUSED|STATE_SYSTEM_FOCUSABLE|static_cast<LONG>(w);return TRUE;}
    if(message==WM_IME_STARTCOMPOSITION){composing.clear();return 0;}
    if(message==WM_IME_COMPOSITION){
        auto context=ImmGetContext(window);
        auto read=[&](DWORD flag){
            LONG bytes=ImmGetCompositionStringW(context,flag,nullptr,0);
            std::wstring text(bytes>0?bytes/sizeof(wchar_t):0,L'\0');
            if(bytes>0)ImmGetCompositionStringW(context,flag,text.data(),bytes);return text;
        };
        if(l&GCS_RESULTSTR){committed+=read(GCS_RESULTSTR);composing.clear();}
        if(l&GCS_COMPSTR)composing=read(GCS_COMPSTR);
        ImmReleaseContext(window,context);InvalidateRect(window,nullptr,TRUE);return 0;
    }
    if(message==WM_IME_ENDCOMPOSITION){composing.clear();InvalidateRect(window,nullptr,TRUE);return 0;}
    if(message==WM_CHAR){
        if(w==VK_BACK){if(!committed.empty())committed.pop_back();}
        else if(w>=32)committed+=static_cast<wchar_t>(w);
        InvalidateRect(window,nullptr,TRUE);return 0;
    }
    if(message==WM_LBUTTONDOWN){SetFocus(window);return 0;}
    if(message==WM_PAINT){
        PAINTSTRUCT paint;auto dc=BeginPaint(window,&paint);auto text=committed+composing;
        TextOutW(dc,20,40,text.data(),static_cast<int>(text.size()));EndPaint(window,&paint);return 0;
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 1;
    auto argument=firstArgument();if(argument.empty())return 2;
    auto path=std::filesystem::absolute(argument).wstring();
    inputService=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!inputService)return 3;
    auto getClass=reinterpret_cast<HRESULT(WINAPI*)(REFCLSID,REFIID,void**)>(GetProcAddress(inputService,"DllGetClassObject"));
    if(!getClass||FAILED(getClass(serviceId,IID_IClassFactory,reinterpret_cast<void**>(factory.put())))||
        FAILED(CoRegisterClassObject(serviceId,factory.get(),CLSCTX_INPROC_SERVER,REGCLS_MULTIPLEUSE,&factoryCookie)))return 4;
    WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=instance;cls.lpszClassName=L"Qt6103QWindowIcon";
    cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&cls);
    auto window=CreateWindowW(cls.lpszClassName,L"Atypoi synthetic IMM verification",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,800,200,nullptr,nullptr,instance,nullptr);
    if(!window)return 5;
    SetFocus(window);MSG message;
    while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    CoRevokeClassObject(factoryCookie);factory.reset();
    // TSF may retain service objects until COM thread teardown.
    CoUninitialize();return field.contentReads==0?0:6;
}
