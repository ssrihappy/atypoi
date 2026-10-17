#pragma once
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <msctf.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "../core/engine.h"

namespace atypoi::win {
inline constexpr GUID serviceId={0x64a5e233,0x36c2,0x49f3,{0x90,0x51,0xb1,0xea,0x68,0x34,0x01,0x26}};
inline constexpr GUID profileId={0xa6b90145,0x1f95,0x4e40,{0xb5,0xb7,0x63,0x1d,0xfa,0xe2,0x73,0x91}};
inline constexpr GUID displayId={0x6ad0c905,0x478c,0x4e01,{0x95,0xf3,0x1e,0x8b,0xa7,0xc4,0x5b,0x20}};
inline constexpr GUID immersiveCategory={0x13a016df,0x560b,0x46cd,{0x94,0x7a,0x4c,0x3a,0xf1,0xe0,0xe3,0x5d}};
inline constexpr GUID systrayCategory={0x25504fb4,0x7bab,0x4bc1,{0x9c,0x69,0xcf,0x81,0x89,0x0f,0x0e,0xf5}};
inline constexpr GUID uiLessCategory={0x49d2f9cf,0x1f5e,0x11d7,{0xa6,0xd3,0x00,0x06,0x5b,0x84,0x43,0x5c}};
inline constexpr LANGID languageId=0x0412;
inline constexpr wchar_t appClass[]=L"Atypoi.Tray.1";
inline constexpr wchar_t settingsKey[]=L"Software\\Atypoi";
inline constexpr wchar_t version[]=L"0.1.5-dev";
inline constexpr size_t maxApps=32;

template<class T> class Ptr {
    T* p_=nullptr;
public:
    Ptr()=default;
    explicit Ptr(T* p,bool add=true):p_(p) { if(p_&&add)p_->AddRef(); }
    Ptr(const Ptr& x):Ptr(x.p_) {}
    Ptr(Ptr&& x) noexcept:p_(x.p_) {x.p_=nullptr;}
    ~Ptr(){ reset(); }
    Ptr& operator=(Ptr x) {std::swap(p_,x.p_);return *this;}
    T* get() const {return p_;}
    T* operator->() const {return p_;}
    explicit operator bool() const {return p_!=nullptr;}
    T** put(){reset();return &p_;}
    void reset(){if(p_)p_->Release();p_=nullptr;}
    T* detach(){auto p=p_;p_=nullptr;return p;}
};
template<class T> HRESULT query(IUnknown* p,Ptr<T>& out) {return p?p->QueryInterface(__uuidof(T),reinterpret_cast<void**>(out.put())):E_POINTER;}
inline std::wstring guidString(REFGUID guid) {wchar_t text[40];StringFromGUID2(guid,text,40);return text;}
inline std::wstring modulePath(HMODULE module=nullptr) {wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);return n?std::wstring(path,n):L"";}
inline std::wstring parentPath(const std::wstring& path) {auto i=path.find_last_of(L"\\/");return i==std::wstring::npos?L".":path.substr(0,i);}
inline std::wstring firstArgument() {int count=0;auto values=CommandLineToArgvW(GetCommandLineW(),&count);std::wstring argument=values&&count>1?values[1]:L"";if(values)LocalFree(values);return argument;}
inline std::wstring fileName(const std::wstring& path) {auto i=path.find_last_of(L"\\/");return path.substr(i==std::wstring::npos?0:i+1);}
inline std::wstring lowerPath(std::wstring path) {CharLowerBuffW(path.data(),static_cast<DWORD>(path.size()));return path;}
inline std::wstring processPath(DWORD pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return {};
    wchar_t path[32768];DWORD n=32768;bool ok=QueryFullProcessImageNameW(process,0,path,&n);CloseHandle(process);
    return ok?lowerPath(std::wstring(path,n)):L"";
}
inline std::wstring sid() {
    HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {};
    DWORD n=0;GetTokenInformation(token,TokenUser,nullptr,0,&n);std::vector<BYTE> bytes(n);
    if(!GetTokenInformation(token,TokenUser,bytes.data(),n,&n)){CloseHandle(token);return {};}
    CloseHandle(token);LPWSTR text=nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&text))return {};
    std::wstring result=text;LocalFree(text);return result;
}
inline std::wstring objectName(const wchar_t* suffix) {
#ifdef ATYPOI_TESTING
    return L"Local\\Atypoi."+sid()+L".Tests."+suffix;
#else
    return L"Local\\Atypoi."+sid()+L"."+suffix;
#endif
}

struct AppRule {wchar_t path[520]{};LONG enabled=0;LONG strict=-1;LONG enter=-1;};
struct Config {LONG mode=0;LONG strict=0;LONG enter=1;LONG shortcut=1;LONG appCount=0;AppRule apps[maxApps]{};};
struct Shared {
    volatile LONG sequence=0;
    DWORD magic=0x41545931;
    volatile LONG heartbeat=0;
    volatile LONG running=0;
    Config config;
    volatile LONG foregroundPid=0;
    volatile LONG status=0;
    volatile LONG statusTick=0;
    // Content-free diagnostics: counts, stage and HRESULT only (never keys/text).
    volatile LONG testedKeys=0,handledKeys=0,diagnosticStage=0,diagnosticHr=0;
    volatile LONG bufferLength=0,bindings=0,terminations=0,selectionEnds=0,focusLosses=0;
};
static_assert(sizeof(Config)<40000);
inline bool alive(const Shared* state) {
    return state && state->magic==0x41545931 && state->running && static_cast<DWORD>(GetTickCount()-static_cast<DWORD>(state->heartbeat))<2500;
}
inline bool readConfig(const Shared* state,Config& config) {
    if(!alive(state))return false;
    for(int attempt=0;attempt<3;++attempt) {
        LONG start=state->sequence;if(start&1)continue;
        MemoryBarrier();std::memcpy(&config,&state->config,sizeof config);MemoryBarrier();
        if(start==state->sequence)return config.appCount>=0 && config.appCount<=static_cast<LONG>(maxApps);
    }
    return false;
}
inline void writeConfig(Shared* state,const Config& config) {InterlockedIncrement(&state->sequence);MemoryBarrier();std::memcpy(&state->config,&config,sizeof config);MemoryBarrier();InterlockedIncrement(&state->sequence);}
struct Policy {Mode mode=Mode::Paused;bool strict=false;bool enter=true;LONG shortcut=0;bool enabled=false;};
inline Policy policy(const Config& config,const std::wstring& path) {
    Policy p{static_cast<Mode>(std::clamp(config.mode,0L,3L)),config.strict!=0,config.enter!=0,config.shortcut,false};
    // These hosts have been exercised with physical Windows keyboard events.
    p.enabled=fileName(path)==L"atypoi.exe" || fileName(path)==L"atypoi-host.exe";
    if(fileName(path)==L"notepad.exe"&&path.find(L"\\windowsapps\\microsoft.windowsnotepad_")!=std::wstring::npos)p.enabled=true;
    if(path.ends_with(L"\\google\\chrome\\application\\chrome.exe"))p.enabled=true;
    for(LONG i=0;i<config.appCount;++i) if(path==config.apps[i].path) {
        const auto& r=config.apps[i];p.enabled=r.enabled!=0;if(r.strict>=0)p.strict=r.strict!=0;if(r.enter>=0)p.enter=r.enter!=0;break;
    }
    if(!p.enabled)p.mode=Mode::Paused;return p;
}
inline std::string resource(HMODULE module,int id) {
    auto found=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);if(!found)return {};
    auto loaded=LoadResource(module,found);auto ptr=static_cast<const char*>(LockResource(loaded));
    return ptr?std::string(ptr,SizeofResource(module,found)):std::string{};
}
inline std::unique_ptr<Engine> loadEngine(HMODULE module) {return std::make_unique<Engine>(resource(module,101),resource(module,102),resource(module,103));}
inline bool readDword(HKEY key,const wchar_t* name,DWORD& value) {DWORD size=sizeof value;return RegGetValueW(key,nullptr,name,RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS;}
inline bool saveConfig(const Config& config) {
    HKEY key=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,settingsKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    DWORD schema=1;auto result=RegSetValueExW(key,L"ConfigV1",0,REG_BINARY,reinterpret_cast<const BYTE*>(&config),sizeof config);
    RegSetValueExW(key,L"Schema",0,REG_DWORD,reinterpret_cast<BYTE*>(&schema),sizeof schema);RegCloseKey(key);return result==ERROR_SUCCESS;
}
inline Config loadConfig() {
    Config config;DWORD type=0,size=sizeof config;
    auto result=RegGetValueW(HKEY_CURRENT_USER,settingsKey,L"ConfigV1",RRF_RT_REG_BINARY,&type,&config,&size);
    if(result==ERROR_FILE_NOT_FOUND)return {};
    if(result!=ERROR_SUCCESS || size!=sizeof config || config.appCount<0 || config.appCount>static_cast<LONG>(maxApps) || config.mode<0 || config.mode>3 || config.shortcut<0 || config.shortcut>2) {
        // Preserve a malformed value before the next save replaces it.
        DWORD bytes=0,kind=0;
        if(RegGetValueW(HKEY_CURRENT_USER,settingsKey,L"ConfigV1",RRF_RT_ANY,&kind,nullptr,&bytes)==ERROR_SUCCESS && bytes<=1024*1024) {
            std::vector<BYTE> backup(bytes);HKEY key=nullptr;
            if(RegGetValueW(HKEY_CURRENT_USER,settingsKey,L"ConfigV1",RRF_RT_ANY,&kind,backup.data(),&bytes)==ERROR_SUCCESS && RegOpenKeyExW(HKEY_CURRENT_USER,settingsKey,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS) {RegSetValueExW(key,L"ConfigV1.Corrupt",0,kind,backup.data(),bytes);RegCloseKey(key);}
        }
        return {};
    }
    for(auto& app:config.apps)app.path[519]=0;
    return config;
}
inline bool startupEnabled() {wchar_t path[32768];DWORD size=sizeof path;return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"Atypoi",RRF_RT_REG_SZ,nullptr,path,&size)==ERROR_SUCCESS;}
inline bool startupBlocked() {BYTE value[32]{};DWORD size=sizeof value;return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",L"Atypoi",RRF_RT_REG_BINARY,nullptr,value,&size)==ERROR_SUCCESS && size>0 && (value[0]==3 || value[0]==7);}
inline bool setStartup(bool enabled,const std::wstring& executable=modulePath()) {
    HKEY key=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    auto text=L"\""+executable+L"\" --background";
    LONG result=enabled?RegSetValueExW(key,L"Atypoi",0,REG_SZ,reinterpret_cast<const BYTE*>(text.c_str()),static_cast<DWORD>((text.size()+1)*sizeof(wchar_t))):RegDeleteValueW(key,L"Atypoi");
    RegCloseKey(key);return result==ERROR_SUCCESS || (!enabled&&result==ERROR_FILE_NOT_FOUND);
}
}
