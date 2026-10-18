#include "../win/platform.h"
#include "../win/update.h"
#include "../win/ui.h"
#include "../win/host_version.h"
#include "../win/input_profile.h"
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <imm.h>
#include <richedit.h>
#include <fstream>
#include <thread>

using namespace atypoi;
using namespace atypoi::win;
namespace {
constexpr UINT trayMessage=WM_APP+1;
constexpr int idMode=101,idStartup=102,idStrict=103,idEnter=104,idShortcut=105,idApps=106,idBrowse=107,idRemove=108,idPolicyEnabled=109,idPolicyStrict=110,idPolicyEnter=111,idSaveRule=112,idTest=113,idWindows=114,idPause=115,idExit=116,idUpdate=117,idAddForeground=118,idActivate=119,idRaw=201,idOutput=202;
HINSTANCE instance;
HWND mainWindow=nullptr,testWindow=nullptr;
HWND modeBox,shortcutBox,startupBox,strictBox,enterBox,appList,ruleEnabled,ruleStrict,ruleEnter,statusLabel;
HFONT font=nullptr,titleFont=nullptr,sectionFont=nullptr;
std::vector<HWND> generalControls,appControls;
int creatingPage=0;
bool appsPage=false;
HWND appPathLabel=nullptr;
HANDLE mapping=nullptr,mutex=nullptr;
Shared* state=nullptr;
Config config;
std::unique_ptr<Engine> engine;
std::wstring foregroundPath;
std::wstring installedIme;
DWORD lastHostPid=0,hostProbeTick=0;
bool outdatedHost=false;
host::RestartNotices restartNotices;
enum class Notice {Update,Restart};
Notice latestNotice=Notice::Update;
UINT taskbarCreated;
HICON trayIcon=nullptr;
LONG lastStatus=-1;
bool demoMode=false;
std::atomic_bool updateCancelled{false};
std::thread updateWorker;
update::Result updateResult;
bool automaticUpdates=true,backgroundUpdate=false;
ULONGLONG nextUpdateCheck=0;
std::vector<HWND> controls;
bool activateProfile() {
    return activateInputProfile();
}

std::wstring text(HWND window) {int n=GetWindowTextLengthW(window);std::wstring value(n+1,L'\0');GetWindowTextW(window,value.data(),n+1);value.resize(n);return value;}
HWND control(HWND parent,const wchar_t* cls,const wchar_t* caption,DWORD style,int x,int y,int width,int height,int id=0) {
    UINT dpi=GetDpiForWindow(parent);if(!dpi)dpi=96;
    x=MulDiv(x,dpi,96);y=MulDiv(y,dpi,96);width=MulDiv(width,dpi,96);height=MulDiv(height,dpi,96);
    auto hwnd=CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,x,y,width,height,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    if(creatingPage==1)generalControls.push_back(hwnd);if(creatingPage==2)appControls.push_back(hwnd);
    return hwnd;
}
HWND label(HWND parent,const wchar_t* caption,int x,int y,int width,int height=24) {return control(parent,L"STATIC",caption,0,x,y,width,height);}
HWND button(HWND parent,const wchar_t* caption,int x,int y,int width,int id) {auto b=control(parent,L"BUTTON",caption,WS_TABSTOP|BS_OWNERDRAW,x,y,width,36,id);ui::styleButton(b,id==idActivate||id==idSaveRule);return b;}
HWND check(HWND parent,const wchar_t* caption,int x,int y,int width,int id) {return control(parent,L"BUTTON",caption,WS_TABSTOP|BS_AUTOCHECKBOX,x,y,width,28,id);}
HWND combo(HWND parent,int x,int y,int width,int id,std::initializer_list<const wchar_t*> values) {
    auto box=control(parent,WC_COMBOBOXW,L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,x,y,width,180,id);
    for(auto value:values)SendMessageW(box,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));return box;
}
void select(HWND combo,LONG index){SendMessageW(combo,CB_SETCURSEL,index,0);}
void checked(HWND box,bool value){SendMessageW(box,BM_SETCHECK,value?BST_CHECKED:BST_UNCHECKED,0);}
bool isChecked(HWND box){return SendMessageW(box,BM_GETCHECK,0,0)==BST_CHECKED;}
void publish() {
    writeConfig(state,config);
    if(!demoMode&&!saveConfig(config))MessageBoxW(mainWindow,L"설정을 저장하지 못했습니다. 현재 실행에는 적용됩니다.",L"Atypoi",MB_ICONWARNING);
}
void refreshApps() {
    SendMessageW(appList,LB_RESETCONTENT,0,0);
    for(LONG i=0;i<config.appCount;++i) {auto label=std::wstring(config.apps[i].enabled?L"사용  ·  ":L"제외  ·  ")+fileName(config.apps[i].path);SendMessageW(appList,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
    SetWindowTextW(appPathLabel,config.appCount?L"앱을 선택하면 개별 설정을 바꿀 수 있습니다.":L"아직 추가한 앱이 없습니다. 메모장과 Chrome은 기본 지원합니다.");
    for(auto id:{idRemove,idPolicyEnabled,idPolicyStrict,idPolicyEnter,idSaveRule})EnableWindow(GetDlgItem(mainWindow,id),FALSE);
}
void loadRule() {
    auto index=SendMessageW(appList,LB_GETCURSEL,0,0);if(index==LB_ERR||index>=config.appCount)return;
    auto& r=config.apps[index];checked(ruleEnabled,r.enabled!=0);select(ruleStrict,r.strict+1);select(ruleEnter,r.enter+1);
    SetWindowTextW(appPathLabel,r.path);
    for(auto id:{idRemove,idPolicyEnabled,idPolicyStrict,idPolicyEnter,idSaveRule})EnableWindow(GetDlgItem(mainWindow,id),TRUE);
}
void addApp(std::wstring path) {
    if(path.empty())return;path=lowerPath(path);
    if(path.size()>=520){MessageBoxW(mainWindow,L"이 버전은 519자 이하의 앱 경로를 지원합니다.",L"Atypoi",MB_ICONWARNING);return;}
    for(LONG i=0;i<config.appCount;++i)if(path==config.apps[i].path){SendMessageW(appList,LB_SETCURSEL,i,0);loadRule();return;}
    if(config.appCount>=static_cast<LONG>(maxApps)){MessageBoxW(mainWindow,L"최대 32개 앱을 등록할 수 있습니다.",L"Atypoi",MB_ICONWARNING);return;}
    auto& rule=config.apps[config.appCount++];rule={};wcscpy_s(rule.path,path.c_str());rule.enabled=1;
    publish();refreshApps();SendMessageW(appList,LB_SETCURSEL,config.appCount-1,0);loadRule();
}
HICON makeIcon(LONG status) {
    int size=GetSystemMetrics(SM_CXSMICON);HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
    HBITMAP color=CreateCompatibleBitmap(screen,size,size),mask=CreateBitmap(size,size,1,1,nullptr);auto old=SelectObject(dc,color);
    RECT rect{0,0,size,size};FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    ICONINFO brandInfo{};auto brand=ui::icon(instance,size);if(brand){GetIconInfo(brand,&brandInfo);DrawIconEx(dc,0,0,brand,size,size,0,nullptr,DI_NORMAL);DestroyIcon(brand);}
    // The recognizable brand stays constant; a badge and tooltip convey state.
    auto badge=CreateSolidBrush(status==1?RGB(85,224,174):(status==4||status==8)?RGB(255,196,85):status==6?RGB(244,101,113):RGB(211,214,229));auto oldBrush=SelectObject(dc,badge);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,size*2/3,size*2/3,size,size);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(badge);SelectObject(dc,old);
    auto oldMask=SelectObject(dc,mask);PatBlt(dc,0,0,size,size,BLACKNESS);
    if(brandInfo.hbmMask){auto source=CreateCompatibleDC(screen);auto previous=SelectObject(source,brandInfo.hbmMask);BitBlt(dc,0,0,size,size,source,0,0,SRCCOPY);SelectObject(source,previous);DeleteDC(source);}
    oldBrush=SelectObject(dc,GetStockObject(BLACK_BRUSH));oldPen=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,size*2/3,size*2/3,size,size);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);SelectObject(dc,oldMask);
    if(brandInfo.hbmColor)DeleteObject(brandInfo.hbmColor);if(brandInfo.hbmMask)DeleteObject(brandInfo.hbmMask);
    ICONINFO info{TRUE,0,0,mask,color};auto icon=CreateIconIndirect(&info);DeleteObject(color);DeleteObject(mask);DeleteDC(dc);ReleaseDC(nullptr,screen);return icon;
}
std::wstring statusText(LONG status) {
    if(config.mode==3)return L"일시정지 — 직접 입력을 전달합니다";
    switch(status){case 1:return L"자동 — 한글과 영어를 판단합니다";case 2:return L"한글 고정 — 두벌식으로 입력합니다";case 3:return L"영문 고정 — 영어를 그대로 입력합니다";case 5:return L"현재 앱은 자동 판단 제외 상태입니다";case 6:return L"입력 오류 — 자동 판단을 중지했습니다";case 7:return L"보호·미지원 입력란 — 원래 키를 전달합니다";case 8:return L"이전 입력기 사용 중 — 앱을 완전히 종료 후 다시 여세요";default:return L"입력기 선택 필요 — 오른쪽 버튼으로 켜세요";}
}
void tray(bool add=false) {
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    LONG status=state->status;
    if(static_cast<DWORD>(state->foregroundPid)!=foreground||GetTickCount()-static_cast<DWORD>(state->statusTick)>2000)status=0;
    if(GetForegroundWindow()==mainWindow){
        Ptr<ITfInputProcessorProfileMgr> profiles;TF_INPUTPROCESSORPROFILE active{};
        if(SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfileMgr),reinterpret_cast<void**>(profiles.put())))&&SUCCEEDED(profiles->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&active)))status=IsEqualGUID(active.clsid,serviceId)?config.mode+1:0;
    }
    if(outdatedHost&&(foreground==lastHostPid||GetForegroundWindow()==mainWindow))status=8;
    if(config.mode==3)status=4;
    auto message=statusText(status);
    if(status==8&&!foregroundPath.empty())message=fileName(foregroundPath)+L" · 재시작 필요 (이전 입력기)";
    if(text(statusLabel)!=message)SetWindowTextW(statusLabel,message.c_str());
    if(!add && lastStatus==status)return;lastStatus=status;
    auto next=makeIcon(status);
    NOTIFYICONDATAW data{};data.cbSize=sizeof data;data.hWnd=mainWindow;data.uID=1;data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;data.uCallbackMessage=trayMessage;data.hIcon=next;wcsncpy_s(data.szTip,(L"Atypoi · "+message).c_str(),_TRUNCATE);
    Shell_NotifyIconW(add?NIM_ADD:NIM_MODIFY,&data);
    if(add){data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);}
    if(trayIcon)DestroyIcon(trayIcon);trayIcon=next;
}
void showSettings(){
    ShowWindow(mainWindow,SW_SHOWNORMAL);
    // The first call can honor the launcher's SW_HIDE instead of our command.
    if(!IsWindowVisible(mainWindow))ShowWindow(mainWindow,SW_SHOWNORMAL);
    SetForegroundWindow(mainWindow);
}
void scaleWindow(HWND window,WPARAM dpi,LPARAM rectPointer) {
    UINT old=static_cast<UINT>(reinterpret_cast<UINT_PTR>(GetPropW(window,L"Atypoi.Dpi")));if(!old)old=96;
    UINT next=HIWORD(dpi);if(!next)next=96;
    struct Scale {HWND parent;UINT old,next;};Scale scale{window,old,next};
    EnumChildWindows(window,[](HWND child,LPARAM pointer)->BOOL {
        auto& s=*reinterpret_cast<Scale*>(pointer);if(GetParent(child)!=s.parent)return TRUE;
        RECT r;GetWindowRect(child,&r);MapWindowPoints(nullptr,s.parent,reinterpret_cast<POINT*>(&r),2);
        SetWindowPos(child,nullptr,MulDiv(r.left,s.next,s.old),MulDiv(r.top,s.next,s.old),MulDiv(r.right-r.left,s.next,s.old),MulDiv(r.bottom-r.top,s.next,s.old),SWP_NOZORDER|SWP_NOACTIVATE);
        auto oldFont=reinterpret_cast<HFONT>(SendMessageW(child,WM_GETFONT,0,0));LOGFONTW lf{};
        if(oldFont&&GetObjectW(oldFont,sizeof lf,&lf)){lf.lfHeight=MulDiv(lf.lfHeight,s.next,s.old);auto resized=CreateFontIndirectW(&lf);auto owned=reinterpret_cast<HFONT>(GetPropW(child,L"Atypoi.Font"));SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(resized),TRUE);SetPropW(child,L"Atypoi.Font",resized);if(owned)DeleteObject(owned);}
        return TRUE;
    },reinterpret_cast<LPARAM>(&scale));
    SetPropW(window,L"Atypoi.Dpi",reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(next)));
    auto rect=reinterpret_cast<RECT*>(rectPointer);SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
}
void releaseFonts(HWND window){EnumChildWindows(window,[](HWND child,LPARAM)->BOOL{auto owned=RemovePropW(child,L"Atypoi.Font");if(owned)DeleteObject(owned);return TRUE;},0);RemovePropW(window,L"Atypoi.Dpi");}
void updateCheck(bool background=false) {
    if(updateWorker.joinable()){if(!background)MessageBoxW(mainWindow,L"업데이트를 확인하고 있습니다.",L"업데이트",MB_OK);return;}
    if(!updateResult.path.empty()){if(!background){backgroundUpdate=false;PostMessageW(mainWindow,WM_APP+2,0,0);}return;}
    backgroundUpdate=background;
    updateCancelled=false;EnableWindow(GetDlgItem(mainWindow,idUpdate),FALSE);
    updateWorker=std::thread([]{
        try{updateResult=update::check(updateCancelled);}
        catch(const std::exception& e){updateResult={L"업데이트를 확인하지 못했습니다.\n"+fromUtf8(e.what()),{},{}};}
        if(!updateCancelled)PostMessageW(mainWindow,WM_APP+2,0,0);
    });
}
void menu() {
    HMENU popup=CreatePopupMenu();AppendMenuW(popup,MF_STRING,idPause,config.mode==3?L"자동 판단 다시 켜기":L"일시정지");
    AppendMenuW(popup,MF_STRING,idMode,L"설정 열기");AppendMenuW(popup,MF_STRING,idTest,L"입력 테스트");
    auto app=L"이 앱 등록: "+fileName(foregroundPath);AppendMenuW(popup,MF_STRING|(foregroundPath.empty()?MF_GRAYED:0),idAddForeground,app.c_str());
    AppendMenuW(popup,MF_SEPARATOR,0,nullptr);AppendMenuW(popup,MF_STRING|(automaticUpdates?MF_CHECKED:0),124,L"자동 업데이트 확인");AppendMenuW(popup,MF_STRING,idUpdate,L"업데이트 확인");AppendMenuW(popup,MF_STRING,idExit,L"종료");
    POINT pos;GetCursorPos(&pos);SetForegroundWindow(mainWindow);auto chosen=TrackPopupMenu(popup,TPM_RETURNCMD|TPM_RIGHTBUTTON,pos.x,pos.y,0,mainWindow,nullptr);DestroyMenu(popup);PostMessageW(mainWindow,WM_NULL,0,0);if(chosen)SendMessageW(mainWindow,WM_COMMAND,chosen,0);
}
void preview(HWND window) {
    auto raw=text(GetDlgItem(window,idRaw));std::wstring output;
    try {
        auto input=toUtf8(raw);size_t start=0;
        for(size_t i=0;i<=input.size();++i)if(i==input.size()||input[i]==' ') {
            auto word=input.substr(start,i-start);bool ascii=std::all_of(word.begin(),word.end(),[](unsigned char c){return c<128;});
            output+=ascii?engine->decide(word,{},config.strict!=0).text:fromUtf8(word);if(i<input.size())output+=L' ';start=i+1;
        }
    } catch(...){output=L"입력 샘플을 해석할 수 없습니다.";}
    SetWindowTextW(GetDlgItem(window,idOutput),output.c_str());
}
LRESULT CALLBACK testProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORBTN||message==WM_CTLCOLOREDIT)return ui::color(w,l);
    if(message==WM_PAINT){PAINTSTRUCT ps;auto dc=BeginPaint(window,&ps);ui::clear(window,dc);ui::mark(window,dc,32,28,44);ui::card(window,dc,32,118,664,210);ui::card(window,dc,32,346,664,174);EndPaint(window,&ps);return 0;}
    if(message==WM_CREATE) {
        SetPropW(window,L"Atypoi.Dpi",reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(GetDpiForWindow(window))));
        auto heading=label(window,L"직접 써보세요",92,26,500,38);ui::outer(heading);SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
        auto sub=label(window,L"한글과 영어, 하나의 입력 흐름으로.",94,72,580);ui::outer(sub);ui::secondary(sub);
        auto actualTitle=label(window,L"실제 입력",56,136,120);SendMessageW(actualTitle,WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE);
        ui::secondary(label(window,L"Win + Space로 Atypoi를 선택하고 아래 문장을 입력해 보세요.",56,167,620));
        auto actual=control(window,MSFTEDIT_CLASS,L"",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER,56,202,616,102,203);
        SendMessageW(actual,EM_SETEDITSTYLE,SES_USECTF,SES_USECTF);
        auto previewTitle=label(window,L"변환 미리보기",56,364,250);SendMessageW(previewTitle,WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE);
        ui::secondary(label(window,L"영문 키 입력",56,404,110));
        auto raw=control(window,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|WS_BORDER,178,400,494,30,idRaw);SendMessageW(raw,EM_SETLIMITTEXT,2048,0);ImmAssociateContextEx(raw,nullptr,0);
        ui::secondary(label(window,L"판단 결과",56,453,110));
        control(window,L"EDIT",L"",WS_TABSTOP|ES_READONLY|ES_MULTILINE,178,450,494,46,idOutput);
        auto sample=label(window,L"sunlight이 선명하게 leaf를 핥고 있었다",56,195,616,26);ui::secondary(sample);
        SetWindowPos(actual,nullptr,ui::px(window,56),ui::px(window,230),ui::px(window,616),ui::px(window,74),SWP_NOZORDER|SWP_NOACTIVATE);
        auto foot=label(window,L"입력은 이 PC에서 처리되며 저장되지 않습니다.",32,542,664);ui::outer(foot);ui::secondary(foot);
        SetWindowTextW(raw,L"sunlightdl tjsaudgkrp leaffmf gkfxrh dlTdjTek");return 0;
    }
    if(message==WM_COMMAND&&LOWORD(w)==idRaw&&HIWORD(w)==EN_CHANGE){preview(window);return 0;}
    if(message==WM_CLOSE){releaseFonts(window);DestroyWindow(window);return 0;}
    if(message==WM_DPICHANGED){scaleWindow(window,w,l);return 0;}
    if(message==WM_DESTROY){testWindow=nullptr;return 0;}
    return DefWindowProcW(window,message,w,l);
}
void openTest() {
    if(testWindow){ShowWindow(testWindow,SW_SHOW);SetForegroundWindow(testWindow);return;}
    auto dpi=GetDpiForSystem();testWindow=CreateWindowExW(WS_EX_CONTROLPARENT,L"Atypoi.Test",L"Atypoi 입력 테스트",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,MulDiv(744,dpi,96),MulDiv(627,dpi,96),nullptr,nullptr,instance,nullptr);ShowWindow(testWindow,SW_SHOW);preview(testWindow);
}
void showPage(bool apps){appsPage=apps;for(auto c:generalControls)ShowWindow(c,apps?SW_HIDE:SW_SHOW);for(auto c:appControls)ShowWindow(c,apps?SW_SHOW:SW_HIDE);ui::selected(GetDlgItem(mainWindow,120),!apps);ui::selected(GetDlgItem(mainWindow,121),apps);InvalidateRect(mainWindow,nullptr,TRUE);}
void createSettings(HWND window) {
    auto heading=label(window,L"atypoi",94,22,250,44);ui::outer(heading);SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    auto sub=label(window,L"한영 전환 없이, 생각의 흐름대로.",96,70,550);ui::outer(sub);ui::secondary(sub);
    button(window,L"일반",32,110,116,120);button(window,L"앱별 설정",160,110,130,121);
    auto hint=label(window,L"AI + typo",582,118,130);ui::outer(hint);ui::secondary(hint);
    creatingPage=1;
    auto statusTitle=label(window,L"입력 상태",56,182,430);SendMessageW(statusTitle,WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE);
    statusLabel=control(window,L"STATIC",L"입력 상태 확인 중",0,56,214,440,22,123);ui::secondary(statusLabel);
    button(window,L"Atypoi 입력기 켜기",510,188,178,idActivate);
    auto inputTitle=label(window,L"입력 방식",56,278,350);SendMessageW(inputTitle,WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE);
    ui::secondary(label(window,L"입력 모드",56,315,280));ui::secondary(label(window,L"후보 전환 단축키",374,315,300));
    modeBox=combo(window,56,345,278,idMode,{L"자동 판단",L"한글 고정",L"영문 고정",L"일시정지"});select(modeBox,config.mode);
    shortcutBox=combo(window,374,345,314,idShortcut,{L"Ctrl + Shift + Space",L"F9  (권장)",L"사용 안 함"});select(shortcutBox,config.shortcut);
    strictBox=check(window,L"영어 원문 우선으로 판단",56,392,298,idStrict);checked(strictBox,config.strict!=0);
    enterBox=check(window,L"첫 Enter는 확정만 하기",374,392,314,idEnter);checked(enterBox,config.enter!=0);
    startupBox=check(window,L"Windows 로그인 시 자동 실행",56,467,632,idStartup);checked(startupBox,startupEnabled());
    ui::secondary(label(window,startupBlocked()?L"Windows 시작 앱에서 비활성화되어 있습니다.":L"설정 창을 닫아도 시계 옆 트레이에서 함께합니다.",56,505,632));
    creatingPage=2;
    auto appTitle=label(window,L"앱마다 편안한 입력",56,184,632);SendMessageW(appTitle,WM_SETFONT,reinterpret_cast<WPARAM>(sectionFont),TRUE);
    ui::secondary(label(window,L"메모장·Chrome은 기본 지원합니다. 다른 앱은 직접 추가하세요.",56,216,632));
    appList=control(window,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|WS_BORDER|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,56,250,632,110,idApps);
    SendMessageW(appList,LB_SETITEMHEIGHT,0,ui::px(window,28));
    appPathLabel=control(window,L"STATIC",L"",SS_PATHELLIPSIS,56,370,632,22);ui::secondary(appPathLabel);
    button(window,L"실행 파일 추가",56,405,174,idBrowse);button(window,L"최근 사용 앱 추가",242,405,218,idAddForeground);button(window,L"선택 삭제",472,405,216,idRemove);
    ruleEnabled=check(window,L"선택한 앱에서 자동 판단 사용",56,449,600,idPolicyEnabled);
    ruleStrict=combo(window,56,490,206,idPolicyStrict,{L"판단: 기본 설정",L"판단: 보통",L"판단: 영어 우선"});select(ruleStrict,0);
    ruleEnter=combo(window,274,490,214,idPolicyEnter,{L"Enter: 기본 설정",L"Enter: 바로 전달",L"Enter: 확정만"});select(ruleEnter,0);
    button(window,L"앱 설정 저장",502,486,186,idSaveRule);
    creatingPage=0;
    auto privacy=label(window,L"내 PC에서만 처리 · 입력 기록은 저장하지 않습니다.",32,563,680);ui::outer(privacy);ui::secondary(privacy);
    button(window,L"입력 테스트",32,604,148,idTest);button(window,L"Windows 입력 설정",192,604,178,idWindows);button(window,L"업데이트",382,604,142,idUpdate);button(window,L"프로그램 종료",536,604,176,idExit);
    auto footer=std::wstring(version)+L"  ·  Made for your flow.";
    auto foot=label(window,footer.c_str(),32,658,330);ui::outer(foot);ui::secondary(foot);
    auto repository=control(window,WC_LINK,L"<a href=\"https://github.com/ssrihappy/atypoi\">https://github.com/ssrihappy/atypoi</a>",WS_TABSTOP|LWS_TRANSPARENT|LWS_RIGHT,378,658,334,24,122);ui::outer(repository);
    refreshApps();showPage(false);
}
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    // Transparent native controls ask the parent to paint their background.
    if(message==WM_ERASEBKGND){ui::clear(window,reinterpret_cast<HDC>(w));return TRUE;}
    if(message==WM_NOTIFY){auto event=reinterpret_cast<NMHDR*>(l);if(event->idFrom==122&&(event->code==NM_CLICK||event->code==NM_RETURN)){ShellExecuteW(window,L"open",L"https://github.com/ssrihappy/atypoi",nullptr,nullptr,SW_SHOWNORMAL);return 0;}}
    if(message==WM_APP+2){
        if(updateWorker.joinable())updateWorker.join();EnableWindow(GetDlgItem(window,idUpdate),TRUE);
        if(backgroundUpdate){
            if(!updateResult.path.empty()){
                NOTIFYICONDATAW notice{};notice.cbSize=sizeof notice;notice.hWnd=window;notice.uID=1;notice.uFlags=NIF_INFO;notice.dwInfoFlags=NIIF_INFO;
                wcscpy_s(notice.szInfoTitle,L"Atypoi 업데이트 준비 완료");wcscpy_s(notice.szInfo,L"새 버전을 다운로드했습니다. 알림 또는 설정의 업데이트 버튼을 눌러 설치하세요.");latestNotice=Notice::Update;Shell_NotifyIconW(NIM_MODIFY,&notice);
            }
            return 0;
        }
        if(updateResult.path.empty())MessageBoxW(window,updateResult.message.c_str(),L"업데이트",MB_OK|MB_ICONINFORMATION);
        else if(MessageBoxW(window,(updateResult.version+L"\n\n"+updateResult.message).c_str(),L"업데이트",MB_YESNO|MB_ICONQUESTION)==IDYES)ShellExecuteW(window,L"open",updateResult.path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        else {DeleteFileW(updateResult.path.c_str());updateResult={};}
        return 0;
    }
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORBTN||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX)return ui::color(w,l);
    if(message==WM_DRAWITEM&&ui::drawButton(l))return TRUE;
    if(message==WM_PAINT){PAINTSTRUCT ps;auto dc=BeginPaint(window,&ps);ui::clear(window,dc);ui::mark(window,dc,32,28,48);if(appsPage)ui::card(window,dc,32,166,680,382);else{ui::card(window,dc,32,166,680,82);ui::card(window,dc,32,260,680,174);ui::card(window,dc,32,450,680,98);}EndPaint(window,&ps);return 0;}
    if(message==WM_CREATE){mainWindow=window;SetPropW(window,L"Atypoi.Dpi",reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(GetDpiForWindow(window))));createSettings(window);SetTimer(window,1,500,nullptr);return 0;}
    if(message==taskbarCreated){tray(true);return 0;}
    if(message==trayMessage) {
        auto action=LOWORD(l);if(action==NIN_BALLOONUSERCLICK){if(latestNotice==Notice::Restart)showSettings();else updateCheck();}else if(action==WM_CONTEXTMENU||action==WM_RBUTTONUP)menu();else if(action==NIN_SELECT||action==NIN_KEYSELECT||action==WM_LBUTTONDBLCLK)showSettings();return 0;
    }
    if(message==WM_TIMER) {
        if(!demoMode&&automaticUpdates&&GetTickCount64()>=nextUpdateCheck){nextUpdateCheck=GetTickCount64()+24ULL*60*60*1000;updateCheck(true);}
        InterlockedExchange(&state->heartbeat,static_cast<LONG>(GetTickCount()));
        DWORD pid=0;auto active=GetForegroundWindow();GetWindowThreadProcessId(active,&pid);
        if(pid&&pid!=GetCurrentProcessId()){
            auto path=processPath(pid);
            if(!path.empty()&&fileName(path)!=L"explorer.exe"){
                foregroundPath=path;
                if(!demoMode&&(pid!=lastHostPid||GetTickCount()-hostProbeTick>3000)){
                    auto state=host::inspect(pid,installedIme);
                    if(state!=host::InputService::Unknown||pid!=lastHostPid)outdatedHost=state==host::InputService::Outdated;
                    if(restartNotices.observe(pid,state)){
                        NOTIFYICONDATAW notice{};notice.cbSize=sizeof notice;notice.hWnd=window;notice.uID=1;notice.uFlags=NIF_INFO;notice.dwInfoFlags=NIIF_WARNING;
                        wcscpy_s(notice.szInfoTitle,L"입력하던 앱을 다시 시작해 주세요");
                        auto message=fileName(path)+L"에 이전 Atypoi 입력기가 남아 있습니다. 작업을 저장한 뒤 앱을 트레이에서도 완전히 종료하고 다시 실행하세요.";
                        wcsncpy_s(notice.szInfo,message.c_str(),_TRUNCATE);latestNotice=Notice::Restart;Shell_NotifyIconW(NIM_MODIFY,&notice);
                    }
                    lastHostPid=pid;hostProbeTick=GetTickCount();
                }
            }
        }
        tray();return 0;
    }
    if(message==WM_COMMAND) {
        int id=LOWORD(w),event=HIWORD(w);
        if(id==120||id==121){showPage(id==121);return 0;}
        if(id==124){
            DWORD enabled=automaticUpdates?0:1;
            if(demoMode||RegSetKeyValueW(HKEY_CURRENT_USER,settingsKey,L"AutomaticUpdates",REG_DWORD,&enabled,sizeof enabled)==ERROR_SUCCESS){automaticUpdates=enabled!=0;nextUpdateCheck=GetTickCount64()+10000;}
            else MessageBoxW(window,L"자동 업데이트 설정을 저장하지 못했습니다.",L"업데이트",MB_OK|MB_ICONWARNING);
            return 0;
        }
        if(id==idMode){if(event==CBN_SELCHANGE){config.mode=static_cast<LONG>(SendMessageW(modeBox,CB_GETCURSEL,0,0));publish();lastStatus=-1;tray();}else showSettings();}
        else if(id==idShortcut&&event==CBN_SELCHANGE){config.shortcut=static_cast<LONG>(SendMessageW(shortcutBox,CB_GETCURSEL,0,0));publish();}
        else if(id==idStrict){config.strict=isChecked(strictBox);publish();}
        else if(id==idEnter){config.enter=isChecked(enterBox);publish();}
        else if(id==idStartup){if(demoMode||!setStartup(isChecked(startupBox)))checked(startupBox,startupEnabled());}
        else if(id==idApps&&event==LBN_SELCHANGE)loadRule();
        else if(id==idBrowse){wchar_t path[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof dialog;dialog.hwndOwner=window;dialog.lpstrFilter=L"Windows 프로그램 (*.exe)\0*.exe\0\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;if(GetOpenFileNameW(&dialog))addApp(path);}
        else if(id==idRemove){auto selected=SendMessageW(appList,LB_GETCURSEL,0,0);if(selected!=LB_ERR&&selected<config.appCount){for(LONG i=static_cast<LONG>(selected);i+1<config.appCount;++i)config.apps[i]=config.apps[i+1];config.apps[--config.appCount]={};publish();refreshApps();}}
        else if(id==idSaveRule){auto selected=SendMessageW(appList,LB_GETCURSEL,0,0);if(selected!=LB_ERR&&selected<config.appCount){auto& r=config.apps[selected];r.enabled=isChecked(ruleEnabled);r.strict=static_cast<LONG>(SendMessageW(ruleStrict,CB_GETCURSEL,0,0))-1;r.enter=static_cast<LONG>(SendMessageW(ruleEnter,CB_GETCURSEL,0,0))-1;publish();refreshApps();SendMessageW(appList,LB_SETCURSEL,selected,0);loadRule();}}
        else if(id==idAddForeground){showSettings();showPage(true);addApp(foregroundPath);}
        else if(id==idTest)openTest();
        else if(id==idActivate){if(!activateProfile())MessageBoxW(window,L"입력기를 켜지 못했습니다. 설치 후 Win + Space에서 Atypoi를 선택하세요.",L"Atypoi",MB_ICONWARNING);else {lastStatus=-1;tray();}}
        else if(id==idWindows)ShellExecuteW(window,L"open",L"ms-settings:typing",nullptr,nullptr,SW_SHOWNORMAL);
        else if(id==idUpdate)updateCheck();
        else if(id==idPause){config.mode=config.mode==3?0:3;select(modeBox,config.mode);writeConfig(state,config);lastStatus=-1;tray();}
        else if(id==idExit){releaseFonts(window);DestroyWindow(window);}
        return 0;
    }
    if(message==WM_CLOSE){ShowWindow(window,SW_HIDE);return 0;}
    if(message==WM_QUERYENDSESSION)return TRUE;
    if(message==WM_ENDSESSION&&w){InterlockedExchange(&state->running,0);return 0;}
    if(message==WM_DESTROY){updateCancelled=true;InterlockedExchange(&state->running,0);NOTIFYICONDATAW data{};data.cbSize=sizeof data;data.hWnd=window;data.uID=1;Shell_NotifyIconW(NIM_DELETE,&data);if(testWindow)DestroyWindow(testWindow);PostQuitMessage(0);return 0;}
    if(message==WM_DPICHANGED){scaleWindow(window,w,l);return 0;}
    return DefWindowProcW(window,message,w,l);
}
bool enableProfile() {
    Ptr<ITfInputProcessorProfiles> profiles;
    auto hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfiles),reinterpret_cast<void**>(profiles.put()));
    return SUCCEEDED(hr)&&SUCCEEDED(profiles->EnableLanguageProfile(serviceId,languageId,profileId,TRUE));
}
}
int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR,int) {
    instance=app;std::wstring args=firstArgument();
    if(args==L"--enable-profile"){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);bool ok=enableProfile();CoUninitialize();return ok?0:1;}
    if(args==L"--startup-on")return setStartup(true)?0:1;
    if(args==L"--startup-off")return setStartup(false)?0:1;
    if(args==L"--quit"){if(auto window=FindWindowW(appClass,nullptr))PostMessageW(window,WM_COMMAND,idExit,0);return 0;}
    demoMode=args.find(L"--demo")!=std::wstring::npos;
    auto executable=modulePath();installedIme=executable.substr(0,executable.find_last_of(L"\\/"))+L"\\Atypoi.IME.dll";
    mutex=CreateMutexW(nullptr,TRUE,objectName(L"Tray").c_str());
    if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(auto window=FindWindowW(appClass,nullptr)){ShowWindow(window,SW_SHOW);SetForegroundWindow(window);}CloseHandle(mutex);return 0;}
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(!demoMode&&legacyInputProfileActive())activateProfile();
    LoadLibraryW(L"Msftedit.dll");
    INITCOMMONCONTROLSEX controlsInit{sizeof controlsInit,ICC_STANDARD_CLASSES|ICC_LINK_CLASS};InitCommonControlsEx(&controlsInit);
    try{engine=loadEngine(instance);}catch(...){MessageBoxW(nullptr,L"입력 모델을 불러올 수 없습니다.",L"Atypoi",MB_ICONERROR);return 1;}
    if(!engine->ready())return 1;
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),objectName(L"State").c_str());
    if(!mapping)return 1;state=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));if(!state)return 1;
    config=demoMode?Config{}:loadConfig();if(config.mode==3)config.mode=0;
    DWORD automatic=1,size=sizeof automatic;RegGetValueW(HKEY_CURRENT_USER,settingsKey,L"AutomaticUpdates",RRF_RT_REG_DWORD,nullptr,&automatic,&size);automaticUpdates=automatic!=0;nextUpdateCheck=GetTickCount64()+10000;
    state->magic=0x41545931;InterlockedExchange(&state->sequence,0);writeConfig(state,config);InterlockedExchange(&state->heartbeat,static_cast<LONG>(GetTickCount()));InterlockedExchange(&state->running,1);
    auto dpi=GetDpiForSystem();font=ui::font(14,FW_NORMAL,dpi);titleFont=ui::font(34,FW_SEMIBOLD,dpi,L"Segoe UI");sectionFont=ui::font(16,FW_BOLD,dpi);
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=app;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(app,MAKEINTRESOURCEW(101));wc.lpszClassName=appClass;RegisterClassW(&wc);
    wc.lpfnWndProc=testProc;wc.lpszClassName=L"Atypoi.Test";RegisterClassW(&wc);
    mainWindow=CreateWindowExW(WS_EX_CONTROLPARENT,appClass,L"Atypoi 설정",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,MulDiv(760,dpi,96),MulDiv(735,dpi,96),nullptr,nullptr,app,nullptr);
    if(!mainWindow)return 1;
    tray(true);
    if(args.find(L"--background")==std::wstring::npos)showSettings();
    // TSF must see keystrokes before IsDialogMessage/TranslateMessage consumes them.
    Ptr<ITfThreadMgr> threadManager;Ptr<ITfKeystrokeMgr> keystrokes;TfClientId hostClient=0;
    if(SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfThreadMgr),reinterpret_cast<void**>(threadManager.put())))&&SUCCEEDED(threadManager->Activate(&hostClient)))query(threadManager.get(),keystrokes);
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){
        BOOL eaten=FALSE;
        if(keystrokes&&(message.message==WM_KEYDOWN||message.message==WM_KEYUP)) {
            bool down=message.message==WM_KEYDOWN;
            auto hr=down?keystrokes->TestKeyDown(message.wParam,message.lParam,&eaten):keystrokes->TestKeyUp(message.wParam,message.lParam,&eaten);
            if(SUCCEEDED(hr)&&eaten){eaten=FALSE;hr=down?keystrokes->KeyDown(message.wParam,message.lParam,&eaten):keystrokes->KeyUp(message.wParam,message.lParam,&eaten);if(SUCCEEDED(hr)&&eaten)continue;}
        }
        if(testWindow&&IsDialogMessageW(testWindow,&message))continue;if(IsDialogMessageW(mainWindow,&message))continue;TranslateMessage(&message);DispatchMessageW(&message);
    }
    keystrokes.reset();if(threadManager&&hostClient)threadManager->Deactivate();threadManager.reset();
    if(updateWorker.joinable()){updateWorker.join();if(!updateResult.path.empty())DeleteFileW(updateResult.path.c_str());}
    if(trayIcon)DestroyIcon(trayIcon);DeleteObject(font);DeleteObject(titleFont);DeleteObject(sectionFont);UnmapViewOfFile(state);CloseHandle(mapping);ReleaseMutex(mutex);CloseHandle(mutex);engine.reset();CoUninitialize();return 0;
}
