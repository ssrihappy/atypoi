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
            result=lowerPath(entry.szExePath)==lowerPath(expected)?InputService::Current:InputService::Outdated;break;
        }
    }while(Module32NextW(snapshot,&entry));
    else result=InputService::Unknown;
    CloseHandle(snapshot);return result;
}
}
