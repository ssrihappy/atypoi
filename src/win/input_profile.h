#pragma once
#include "platform.h"

namespace atypoi::win {
inline bool activateInputProfile(){
    Ptr<ITfInputProcessorProfiles> profiles;Ptr<ITfInputProcessorProfileMgr> manager;
    auto hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfiles),reinterpret_cast<void**>(profiles.put()));
    if(FAILED(hr)||FAILED(query(profiles.get(),manager)))return false;
    TF_INPUTPROCESSORPROFILE previous{};
    const bool restore=SUCCEEDED(manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&previous));
    if(FAILED(profiles->EnableLanguageProfile(serviceId,languageId,profileId,TRUE)))return false;
    hr=profiles->ChangeCurrentLanguage(languageId);
    if(SUCCEEDED(hr))hr=manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,languageId,serviceId,profileId,nullptr,0x20000001); // FORSESSION | ENABLEPROFILE
    if(FAILED(hr)){
        if(restore&&previous.langid){
            profiles->ChangeCurrentLanguage(previous.langid);
            manager->ActivateProfile(previous.dwProfileType,previous.langid,previous.clsid,previous.guidProfile,previous.hkl,0x20000000);
        }
        return false;
    }
    // Only the initiating user's legacy entry is disabled, after activation.
    profiles->EnableLanguageProfile(serviceId,legacyLanguageId,profileId,FALSE);
    return true;
}
inline bool legacyInputProfileActive(){
    Ptr<ITfInputProcessorProfileMgr> manager;TF_INPUTPROCESSORPROFILE active{};
    return SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfileMgr),reinterpret_cast<void**>(manager.put())))&&
        SUCCEEDED(manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active))&&active.clsid==serviceId&&active.guidProfile==profileId&&active.langid==legacyLanguageId;
}
}
