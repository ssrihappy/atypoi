#pragma once
#include "platform.h"
#include <tlhelp32.h>

namespace atypoi::win::host {
enum class InputService { Unknown, NotLoaded, Current, Outdated };
inline InputService inspect(DWORD pid,const std::wstring& expected) {
    if(!pid||expected.empty())return InputService::Unknown;
    HANDLE snapshot=INVALID_HANDLE_VALUE;
    for(int attempt=0;attempt<3;++attempt){snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(snapshot!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;}
    if(snapshot==INVALID_HANDLE_VALUE)return InputService::Unknown;
    MODULEENTRY32W entry{};entry.dwSize=sizeof entry;InputService result=InputService::NotLoaded;
    if(Module32FirstW(snapshot,&entry))do {
        if(lowerPath(entry.szModule)==L"atypoi.ime.dll"){
            // A host can keep an old module mapped after also loading the new one.
            // Do not let enumeration order hide the restart requirement.
            if(lowerPath(entry.szExePath)!=lowerPath(expected)){result=InputService::Outdated;break;}
            result=InputService::Current;
        }
    }while(Module32NextW(snapshot,&entry));
    else result=InputService::Unknown;
    CloseHandle(snapshot);return result;
}
class RestartNotices {
    std::vector<DWORD> shown_;
public:
    bool observe(DWORD pid,InputService state){
        if(!pid||state==InputService::Unknown)return false;
        auto found=std::find(shown_.begin(),shown_.end(),pid);
        if(state!=InputService::Outdated){if(found!=shown_.end())shown_.erase(found);return false;}
        if(found!=shown_.end())return false;
        if(shown_.size()==32)shown_.erase(shown_.begin());
        shown_.push_back(pid);return true;
    }
};
}
