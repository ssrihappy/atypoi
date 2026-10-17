#include "../src/win/platform.h"
#include <cstdio>
using namespace atypoi::win;
int wmain(int argc,wchar_t** argv) {
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 Ptr<ITfInputProcessorProfileMgr> mgr;
 auto hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfileMgr),reinterpret_cast<void**>(mgr.put()));
 if(FAILED(hr)){printf("manager %08lx\n",hr);return 1;}
 if(argc==4 && std::wstring(argv[1])==L"activate-profile") {
  GUID cls{},profile{};hr=CLSIDFromString(argv[2],&cls);
  if(SUCCEEDED(hr))hr=CLSIDFromString(argv[3],&profile);
  if(SUCCEEDED(hr))hr=mgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,languageId,cls,profile,nullptr,0x20000000);
  printf("ActivateSelected %08lx\n",hr);
 }
 if(argc>1 && std::wstring(argv[1])==L"activate-default") {
  Ptr<ITfInputProcessorProfiles> profiles;query(mgr.get(),profiles);GUID cls{},profile{};
  hr=profiles->GetDefaultLanguageProfile(languageId,GUID_TFCAT_TIP_KEYBOARD,&cls,&profile);
  if(SUCCEEDED(hr))hr=mgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,languageId,cls,profile,nullptr,0x20000000);
  printf("ActivateDefault %08lx\n",hr);
 }
 if(argc>1 && std::wstring(argv[1])==L"activate") {
  hr=mgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,languageId,serviceId,profileId,nullptr,0x20000000 /* TF_IPPMF_FORSESSION */);
  printf("ActivateProfile %08lx\n",hr);
 }
 TF_INPUTPROCESSORPROFILE active{};
 hr=mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active);
 wprintf(L"active hr=%08lx type=%lu lang=%04x service=%ls profile=%ls flags=%lu caps=%08lx\n",hr,active.dwProfileType,active.langid,guidString(active.clsid).c_str(),guidString(active.guidProfile).c_str(),active.dwFlags,active.dwCaps);
 Ptr<IEnumTfInputProcessorProfiles> profiles;
 hr=mgr->EnumProfiles(languageId,profiles.put());
 if(SUCCEEDED(hr)){TF_INPUTPROCESSORPROFILE p{};ULONG fetched=0;while(profiles->Next(1,&p,&fetched)==S_OK)wprintf(L"profile service=%ls profile=%ls flags=%lu hkl=%p\n",guidString(p.clsid).c_str(),guidString(p.guidProfile).c_str(),p.dwFlags,p.hkl);}
 auto config=loadConfig();printf("config mode=%ld apps=%ld\n",config.mode,config.appCount);
 HANDLE mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,objectName(L"State").c_str());
 if(mapping){auto s=static_cast<const Shared*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(Shared)));if(s){printf("state alive=%d pid=%ld status=%ld tested=%ld handled=%ld stage=%ld hr=%08lx\n",alive(s),s->foregroundPid,s->status,s->testedKeys,s->handledKeys,s->diagnosticStage,s->diagnosticHr);printf("buffer=%ld bind=%ld term=%ld selection=%ld focusloss=%ld\n",s->bufferLength,s->bindings,s->terminations,s->selectionEnds,s->focusLosses);UnmapViewOfFile(s);}CloseHandle(mapping);}
 return 0;
}
