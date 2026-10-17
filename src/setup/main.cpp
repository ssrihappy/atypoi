#include "../win/platform.h"
#include "../win/ui.h"
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <filesystem>
#include <fstream>

using namespace atypoi::win;
namespace fs=std::filesystem;
namespace {
constexpr wchar_t uninstallKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Atypoi";
constexpr wchar_t installKey[]=L"Software\\Atypoi";
HINSTANCE instance;
HWND window,autoStart,runNow,removeData;
bool uninstalling=false;
UINT windowDpi=96;
HFONT bodyFont=nullptr,headingFont=nullptr;

bool elevated(){HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;TOKEN_ELEVATION value{};DWORD n=0;bool ok=GetTokenInformation(token,TokenElevation,&value,sizeof value,&n);CloseHandle(token);return ok&&value.TokenIsElevated;}
std::wstring root() {PWSTR value=nullptr;auto hr=SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&value);if(FAILED(hr))throw std::runtime_error("Program Files unavailable");std::wstring path=std::wstring(value)+L"\\Atypoi";CoTaskMemFree(value);return path;}
std::wstring readString(HKEY hive,const wchar_t* key,const wchar_t* name){wchar_t value[32768]{};DWORD bytes=sizeof value;if(RegGetValueW(hive,key,name,RRF_RT_REG_SZ,nullptr,value,&bytes)!=ERROR_SUCCESS)return {};return value;}
void setString(HKEY hive,const wchar_t* key,const wchar_t* name,const std::wstring& value){HKEY h=nullptr;LONG error=RegCreateKeyExW(hive,key,0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&h,nullptr);if(error==ERROR_SUCCESS){error=RegSetValueExW(h,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*2));RegCloseKey(h);}if(error!=ERROR_SUCCESS)throw std::runtime_error("Registry update failed");}
std::wstring payloadHash(std::string_view bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD objectSize=0,done=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 unavailable");
    BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<BYTE*>(&objectSize),sizeof objectSize,&done,0);std::vector<BYTE> object(objectSize);BYTE digest[32]{};
    NTSTATUS status=BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0);
    if(status>=0)status=BCryptHashData(hash,reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0);
    if(status>=0)status=BCryptFinishHash(hash,digest,sizeof digest,0);
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)throw std::runtime_error("SHA256 failed");
    std::wstring result;for(auto b:digest){result+=L"0123456789abcdef"[b>>4];result+=L"0123456789abcdef"[b&15];}return result;
}
void writeFile(const fs::path& path,std::string_view data) {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(data.data(),static_cast<std::streamsize>(data.size()));out.flush();if(!out)throw std::runtime_error("Payload write failed");}
[[maybe_unused]] bool signedFile(const std::wstring& path){GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;WINTRUST_FILE_INFO file{};file.cbStruct=sizeof file;file.pcwszFilePath=path.c_str();WINTRUST_DATA data{};data.cbStruct=sizeof data;data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;LONG result=WinVerifyTrust(nullptr,&action,&data);data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&data);return result==ERROR_SUCCESS;}
HRESULT registerDll(const std::wstring& dll,bool install){HMODULE library=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library)return HRESULT_FROM_WIN32(GetLastError());auto fn=reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(library,install?"DllRegisterServer":"DllUnregisterServer"));auto result=fn?fn():E_NOINTERFACE;FreeLibrary(library);return result;}
bool reparse(const fs::path& path){auto attributes=GetFileAttributesW(path.c_str());return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT);}
void ensureSafeRoot(const fs::path& path){if(reparse(path)||reparse(path/L"versions"))throw std::runtime_error("Installation directory must not be a reparse point");}
int installFiles() {
    if(!elevated())return ERROR_ELEVATION_REQUIRED;
    auto base=fs::path(root());ensureSafeRoot(base);
    auto exe=resource(instance,201),dll=resource(instance,202),notices=resource(instance,203);
    if(exe.empty()||dll.empty()||notices.empty())throw std::runtime_error("Installer payload missing");
    auto id=payloadHash(exe+dll);auto destination=base/L"versions"/id;
    if(reparse(destination))throw std::runtime_error("Unsafe version directory");
    auto previous=readString(HKEY_LOCAL_MACHINE,installKey,L"CurrentDirectory");
    bool same=lowerPath(previous)==lowerPath(destination.wstring());
    bool created=!fs::exists(destination);
    if(!created&&!same)throw std::runtime_error("Version already exists. Remove incomplete installation first.");
    fs::create_directories(destination);
    try {
        if(created){writeFile(destination/L"Atypoi.exe",exe);writeFile(destination/L"Atypoi.IME.dll",dll);writeFile(destination/L"ThirdPartyNotices.txt",notices);}
        else { // Same-version repair validates the installed bytes instead of overwriting a loaded module.
            auto read=[](const fs::path& p){std::ifstream in(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
            if(payloadHash(read(destination/L"Atypoi.exe")+read(destination/L"Atypoi.IME.dll"))!=id)throw std::runtime_error("Installed payload differs; repair cannot overwrite an active version");
        }
#ifdef ATYPOI_RELEASE
        if(!signedFile((destination/L"Atypoi.exe").wstring())||!signedFile((destination/L"Atypoi.IME.dll").wstring()))throw std::runtime_error("Publisher signature validation failed");
#endif
        auto hr=registerDll((destination/L"Atypoi.IME.dll").wstring(),true);
        if(FAILED(hr))throw std::runtime_error("TSF registration failed: "+std::to_string(static_cast<unsigned long>(hr)));
        auto setupTarget=base/L"Atypoi.Setup.exe";
        auto source=modulePath();
        if(lowerPath(source)!=lowerPath(setupTarget.wstring())&&!CopyFileW(source.c_str(),setupTarget.c_str(),FALSE))throw std::runtime_error("Cannot update uninstaller");
        setString(HKEY_LOCAL_MACHINE,uninstallKey,L"DisplayName",L"Atypoi 한영 자동 입력");
        setString(HKEY_LOCAL_MACHINE,uninstallKey,L"DisplayVersion",version);
        setString(HKEY_LOCAL_MACHINE,uninstallKey,L"Publisher",L"Atypoi (development build)");
        setString(HKEY_LOCAL_MACHINE,uninstallKey,L"InstallLocation",base.wstring());
        setString(HKEY_LOCAL_MACHINE,uninstallKey,L"DisplayIcon",(destination/L"Atypoi.exe").wstring()+L",0");
        setString(HKEY_LOCAL_MACHINE,uninstallKey,L"UninstallString",L"\""+setupTarget.wstring()+L"\" --uninstall");
        setString(HKEY_LOCAL_MACHINE,installKey,L"CurrentDirectory",destination.wstring());
        return 0;
    } catch(...) {
        if(!previous.empty()&&lowerPath(previous)!=lowerPath(destination.wstring()))registerDll(previous+L"\\Atypoi.IME.dll",true);
        else if(previous.empty())registerDll((destination/L"Atypoi.IME.dll").wstring(),false);
        if(created){for(auto name:{L"Atypoi.exe",L"Atypoi.IME.dll",L"ThirdPartyNotices.txt"}){std::error_code ignored;fs::remove(destination/name,ignored);}std::error_code ignored;fs::remove(destination,ignored);}
        throw;
    }
}
bool removeKnown(const fs::path& path) {if(!fs::exists(path))return false;if(reparse(path))throw std::runtime_error("Refusing to remove reparse point");if(DeleteFileW(path.c_str()))return false;if(!MoveFileExW(path.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT))throw std::runtime_error("Cannot schedule removal");return true;}
int uninstallFiles() {
    if(!elevated())return ERROR_ELEVATION_REQUIRED;
    auto base=fs::path(root());ensureSafeRoot(base);auto current=readString(HKEY_LOCAL_MACHINE,installKey,L"CurrentDirectory");
    // Never execute a DLL from an arbitrary path supplied through registry tampering.
    if(!current.empty()) {
        auto versionPath=fs::path(current);if(versionPath.parent_path()!=base/L"versions"||reparse(versionPath))throw std::runtime_error("Invalid installed path");
        auto hr=registerDll((versionPath/L"Atypoi.IME.dll").wstring(),false);if(FAILED(hr))throw std::runtime_error("Input service unregister failed");
    }
    bool reboot=false;
    if(fs::exists(base/L"versions"))for(const auto& entry:fs::directory_iterator(base/L"versions")) {
        auto name=entry.path().filename().wstring();
        if(name.size()!=64||!std::all_of(name.begin(),name.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');}))continue;
        if(reparse(entry.path()))throw std::runtime_error("Unsafe installed version");
        for(auto file:{L"Atypoi.exe",L"Atypoi.IME.dll",L"ThirdPartyNotices.txt"})reboot=removeKnown(entry.path()/file)||reboot;
        if(!RemoveDirectoryW(entry.path().c_str())){MoveFileExW(entry.path().c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);reboot=true;}
    }
    RemoveDirectoryW((base/L"versions").c_str());
    reboot=removeKnown(base/L"Atypoi.Setup.exe")||reboot;
    if(!RemoveDirectoryW(base.c_str()))MoveFileExW(base.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE,uninstallKey);RegDeleteTreeW(HKEY_LOCAL_MACHINE,installKey);return reboot?3010:0;
}
void launch(const std::wstring& path,const wchar_t* args){ShellExecuteW(window,L"open",path.c_str(),args,nullptr,SW_SHOWNORMAL);}
void shortcut(const std::wstring& target,bool remove) {
    PWSTR programs=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Programs,0,nullptr,&programs)))return;
    auto path=std::wstring(programs)+L"\\Atypoi.lnk";CoTaskMemFree(programs);
    if(remove){DeleteFileW(path.c_str());return;}
    Ptr<IShellLinkW> link;if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,reinterpret_cast<void**>(link.put())))){link->SetPath(target.c_str());link->SetDescription(L"Atypoi 한영 자동 입력");Ptr<IPersistFile> persist;if(SUCCEEDED(query(link.get(),persist)))persist->Save(path.c_str(),TRUE);}
}
void runWorker() {
    EnableWindow(GetDlgItem(window,1),FALSE);
    auto self=modulePath();SHELLEXECUTEINFOW info{};info.cbSize=sizeof info;info.fMask=SEE_MASK_NOCLOSEPROCESS;info.hwnd=window;info.lpVerb=L"runas";info.lpFile=self.c_str();info.lpParameters=uninstalling?L"--uninstall-worker":L"--install-worker";info.nShow=SW_HIDE;
    if(!ShellExecuteExW(&info)){EnableWindow(GetDlgItem(window,1),TRUE);MessageBoxW(window,L"설치 권한 요청이 취소되었거나 실행하지 못했습니다.",L"Atypoi",MB_ICONINFORMATION);return;}
    // Responsive parent while the privileged worker handles only machine-wide files/registration.
    while(MsgWaitForMultipleObjects(1,&info.hProcess,FALSE,INFINITE,QS_ALLINPUT)==WAIT_OBJECT_0+1){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_CLOSE)continue;TranslateMessage(&msg);DispatchMessageW(&msg);}}
    DWORD result=1;GetExitCodeProcess(info.hProcess,&result);CloseHandle(info.hProcess);EnableWindow(GetDlgItem(window,1),TRUE);
    if(result!=0&&result!=3010){MessageBoxW(window,L"설치 작업을 완료하지 못했습니다. 기존 입력기를 계속 사용할 수 있습니다.\n관리자 작업 창의 오류를 확인하세요.",L"Atypoi",MB_ICONERROR);return;}
    if(uninstalling){setStartup(false);shortcut(L"",true);if(SendMessageW(removeData,BM_GETCHECK,0,0)==BST_CHECKED)RegDeleteTreeW(HKEY_CURRENT_USER,settingsKey);}
    else {
        // Only stop the old tray after installation succeeds, so cancelling UAC
        // or a rolled-back update leaves the existing session usable.
        if(auto tray=FindWindowW(appClass,nullptr)) {
            DWORD pid=0;GetWindowThreadProcessId(tray,&pid);HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,pid);
            SendMessageTimeoutW(tray,WM_COMMAND,116,0,SMTO_ABORTIFHUNG,1000,nullptr);
            if(process){WaitForSingleObject(process,5000);CloseHandle(process);}
        }
        auto directory=readString(HKEY_LOCAL_MACHINE,installKey,L"CurrentDirectory");auto executable=directory+L"\\Atypoi.exe";
        if(SendMessageW(autoStart,BM_GETCHECK,0,0)==BST_CHECKED)setStartup(true,executable);else setStartup(false);
        shortcut(executable,false);
        // Profile enabling is intentionally performed in the initiating user's process.
        Ptr<ITfInputProcessorProfiles> profiles;
        if(SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfiles),reinterpret_cast<void**>(profiles.put()))))profiles->EnableLanguageProfile(serviceId,languageId,profileId,TRUE);
        if(SendMessageW(runNow,BM_GETCHECK,0,0)==BST_CHECKED)launch(executable,L"");
    }
    MessageBoxW(window,uninstalling?(result==3010?L"제거했습니다. 사용 중인 입력기 파일은 재시작 후 삭제됩니다.":L"Atypoi를 제거했습니다."):L"설치했습니다. Win + Space에서 Atypoi를 선택하세요.\n목록에 없으면 로그아웃 후 다시 로그인하세요.\n설정에서 사용할 앱을 추가할 수 있습니다.\n업데이트한 경우 입력 중인 앱을 다시 여세요.",L"Atypoi",MB_OK|MB_ICONINFORMATION);DestroyWindow(window);
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN)return atypoi::ui::color(w,l);
    if(msg==WM_DRAWITEM&&atypoi::ui::drawButton(l))return TRUE;
    if(msg==WM_PAINT){PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);atypoi::ui::clear(hwnd,dc);atypoi::ui::mark(hwnd,dc,32,30,56);atypoi::ui::card(hwnd,dc,32,144,536,192);EndPaint(hwnd,&ps);return 0;}
    if(msg==WM_DPICHANGED){
        struct Scale{HWND parent;UINT old,next;};Scale s{hwnd,windowDpi,HIWORD(w)};
        EnumChildWindows(hwnd,[](HWND child,LPARAM value)->BOOL{auto& s=*reinterpret_cast<Scale*>(value);RECT r;GetWindowRect(child,&r);MapWindowPoints(nullptr,s.parent,reinterpret_cast<POINT*>(&r),2);SetWindowPos(child,nullptr,MulDiv(r.left,s.next,s.old),MulDiv(r.top,s.next,s.old),MulDiv(r.right-r.left,s.next,s.old),MulDiv(r.bottom-r.top,s.next,s.old),SWP_NOZORDER|SWP_NOACTIVATE);return TRUE;},reinterpret_cast<LPARAM>(&s));
        windowDpi=s.next;DeleteObject(bodyFont);DeleteObject(headingFont);bodyFont=atypoi::ui::font(14,FW_NORMAL,windowDpi);headingFont=atypoi::ui::font(32,FW_SEMIBOLD,windowDpi,L"Segoe UI");
        EnumChildWindows(hwnd,[](HWND child,LPARAM)->BOOL{SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetDlgCtrlID(child)==10?headingFont:bodyFont),TRUE);return TRUE;},0);
        auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);InvalidateRect(hwnd,nullptr,TRUE);return 0;
    }
    if(msg==WM_COMMAND&&LOWORD(w)==1){if(uninstalling)if(auto tray=FindWindowW(appClass,nullptr))SendMessageTimeoutW(tray,WM_COMMAND,116,0,SMTO_ABORTIFHUNG,1000,nullptr);runWorker();return 0;}
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR,int){
    instance=app;std::wstring args=firstArgument();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(args==L"--install-worker"||args==L"--uninstall-worker") {
        try {int result=args==L"--install-worker"?installFiles():uninstallFiles();CoUninitialize();return result;}
        catch(const std::exception& ex){MessageBoxW(nullptr,atypoi::fromUtf8(ex.what()).c_str(),L"Atypoi 설치 오류",MB_ICONERROR);CoUninitialize();return 1;}
    }
    if(args==L"--verify-payload") {auto exe=resource(app,201),dll=resource(app,202),notice=resource(app,203);return exe.starts_with("MZ")&&dll.starts_with("MZ")&&!notice.empty()?0:1;}
    uninstalling=args==L"--uninstall";
    INITCOMMONCONTROLSEX cc{sizeof cc,ICC_STANDARD_CLASSES};InitCommonControlsEx(&cc);
    WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=app;cls.lpszClassName=L"Atypoi.Setup";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(app,MAKEINTRESOURCEW(101));RegisterClassW(&cls);
    windowDpi=GetDpiForSystem();auto p=[](int n){return MulDiv(n,windowDpi,96);};
    window=CreateWindowExW(WS_EX_CONTROLPARENT,cls.lpszClassName,uninstalling?L"Atypoi 제거":L"Atypoi 설치",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,p(616),p(493),nullptr,nullptr,app,nullptr);
    bodyFont=atypoi::ui::font(14,FW_NORMAL,windowDpi);headingFont=atypoi::ui::font(32,FW_SEMIBOLD,windowDpi,L"Segoe UI");
    auto create=[&](const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){auto child=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,p(x),p(y),p(width),p(height),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app,nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(id==10?headingFont:bodyFont),TRUE);return child;};
    atypoi::ui::outer(create(L"STATIC",L"atypoi",0,108,25,400,44,10));
    auto sub=create(L"STATIC",uninstalling?L"다음에 또 만나요.":L"한영 전환 없이, 생각의 흐름대로.",0,110,75,458,24,0);atypoi::ui::outer(sub);atypoi::ui::secondary(sub);
    create(L"STATIC",uninstalling?L"Atypoi 입력기와 트레이 앱을 제거합니다.":L"한글과 영어를 알아서. 지금 시작해 보세요.",0,56,165,488,28,0);
    auto note=create(L"STATIC",uninstalling?L"사용 중인 입력기 파일은 재시작 후 정리될 수 있습니다.":L"입력기 등록에는 관리자 권한이 필요합니다.\n기존 Windows 입력기와 함께 사용할 수 있습니다.",0,56,204,488,48,0);atypoi::ui::secondary(note);
    if(uninstalling)removeData=create(L"BUTTON",L"이 사용자의 앱별 설정도 삭제",BS_AUTOCHECKBOX|WS_TABSTOP,56,270,488,28,4);
    else {
        autoStart=create(L"BUTTON",L"Windows 로그인 시 자동 실행",BS_AUTOCHECKBOX|WS_TABSTOP,56,264,488,28,2);
        runNow=create(L"BUTTON",L"설치 완료 후 실행",BS_AUTOCHECKBOX|WS_TABSTOP,56,298,488,28,3);
        auto installed=!readString(HKEY_LOCAL_MACHINE,installKey,L"CurrentDirectory").empty();SendMessageW(autoStart,BM_SETCHECK,installed?(startupEnabled()?BST_CHECKED:BST_UNCHECKED):BST_CHECKED,0);SendMessageW(runNow,BM_SETCHECK,BST_CHECKED,0);
    }
    auto buildText=std::wstring(version)+L" · 개발 빌드\n게시자 서명 미적용";
    auto build=create(L"STATIC",buildText.c_str(),0,32,366,310,50,0);atypoi::ui::outer(build);atypoi::ui::secondary(build);
    auto action=create(L"BUTTON",uninstalling?L"제거하기":L"설치하고 시작하기",BS_OWNERDRAW|WS_TABSTOP,360,370,208,42,1);atypoi::ui::styleButton(action,true);
    ShowWindow(window,SW_SHOW);MSG message;while(GetMessageW(&message,nullptr,0,0)>0){if(IsDialogMessageW(window,&message))continue;TranslateMessage(&message);DispatchMessageW(&message);}
    DeleteObject(bodyFont);DeleteObject(headingFont);CoUninitialize();return 0;
}
