#include "../src/win/tsf_compat.h"
#include "windows_host.h"
#include "accessible_host.h"
#include "../src/win/update.h"
#include "../src/win/host_version.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
using namespace atypoi;
using namespace atypoi::win;
namespace {
int failures=0,checks=0;
void check(bool condition,const std::string& message){++checks;if(!condition){++failures;std::cerr<<"FAIL: "<<message<<"\n";}}
std::string file(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error(path);return {std::istreambuf_iterator<char>(f),{}};}
void expectText(const std::wstring& actual,const std::wstring& expected,const std::string& label){check(actual==expected,label+" expected="+toUtf8(expected)+" actual="+toUtf8(actual));}
std::string twoSetKeys(std::wstring_view word) {
    const std::vector<std::string> initial={"r","R","s","e","E","f","a","q","Q","t","T","d","w","W","c","z","x","v","g"};
    const std::vector<std::string> vowel={"k","o","i","O","j","p","u","P","h","hk","ho","hl","y","n","nj","np","nl","b","m","ml","l"};
    const std::vector<std::string> final={"","r","R","rt","s","sw","sg","e","f","fr","fa","fq","ft","fx","fv","fg","a","q","qt","t","T","d","w","c","z","x","v","g"};
    std::string out;for(auto c:word){if(c<0xac00||c>0xd7a3)throw std::runtime_error("Expected Hangul fixture");auto n=c-0xac00;out+=initial[n/588]+vowel[(n%588)/28]+final[n%28];}return out;
}
void pastTenseRegression(const Engine& engine) {
    for(auto stem:{L"만들었",L"먹었",L"읽었",L"앉았",L"닮았",L"괜찮았",L"알았",L"몰랐",L"맛있었",L"재미있었",L"아름다웠",L"걸었",L"도왔",L"없었",L"핥았",L"깎았",L"묶었",L"빨랐",L"기뻤",L"슬펐",L"됐",L"되었",L"썼",L"있었",L"뒀",L"줬",L"꺼냈",L"나왔",L"갔",L"봤",L"씻었",L"쫓았",L"쌓았",L"섞었",L"갚았",L"놓았",L"많았",L"밝았",L"넓었",L"젊었",L"옳았",L"끓었",L"싫었",L"끊었",L"읽혔",L"끓였",L"바뀌었",L"뛰었",L"깨졌",L"잠갔",L"치렀",L"골랐",L"들었",L"불렀",L"푸르렀",L"따뜻했",L"깨끗했",L"필요했",L"정확했",L"시작했",L"종료했"})
    for(auto ending:{L"다",L"어요",L"습니다",L"고",L"는데",L"지만",L"었어요",L"겠어요"}) {
        auto word=std::wstring(stem)+ending;auto input=twoSetKeys(word);auto label="past tense "+toUtf8(word);
        expectText(compose(input).text,word,label+" composition");
        expectText(engine.decide(input).text,word,label+" isolated");
        expectText(engine.decide(input,Language::English).text,word,label+" after English");
        for(bool englishFirst:{false,true}) {
            Session session(engine);if(englishFirst){for(char c:std::string("hello"))session.append(c,Mode::Auto,false);session.commit();}
            for(char c:input)session.append(c,Mode::Auto,false);
            expectText(session.display().text,word,label+" live");
            session.backspace(Mode::Auto,false);input.pop_back();
            expectText(session.display().text,compose(input).text,label+" backspace");
            auto original=twoSetKeys(word);session.append(original.back(),Mode::Auto,false);input=original;
            expectText(session.commit(),word,label+" retype and commit");
        }
    }
    for(auto input:{"hello","sunlight","leaf","GoT","got","Top","top","Pop","Root","ROOT","TOP","POT","TREE","POWER","PROPER","WROTE","API","HTTP","QWERTY","The","the"}){
        Session session(engine);for(char c:std::string("hello"))session.append(c,Mode::Auto,false);session.commit();for(char c:std::string(input))session.append(c,Mode::Auto,false);
        expectText(session.commit(),fromUtf8(input),"English after shifted-evidence change "+std::string(input));
    }
    for(auto [input,expected]:std::vector<std::pair<std::string,std::wstring>>{{"qkRNl",L"바뀌"},{"qkRNldjTek",L"바뀌었다"},{"dlTDjTek",L"있었다"},{"aksemfdjTDjdy",L"만들었어요"}}){
        expectText(compose(input).text,expected,"held Shift composition");
        expectText(engine.decide(input,Language::English).text,expected,"held Shift after English");
        Session live(engine);for(char c:std::string(input))live.append(c,Mode::Auto,false);expectText(live.commit(),expected,"held Shift session");
    }
    for(auto input:{"TONE","POINT","ROOTS","TREE","POWER","TOP","QWERTY","GoT","API","tjsA","RMS","RNS","TKS"})expectText(engine.decide(input,Language::English).text,fromUtf8(input),"uppercase English protected");
}
void integration(ITfTextInputProcessorEx* service,ITfThreadMgr* manager,TfClientId client){
    HANDLE mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),objectName(L"State").c_str());
    if(!mapping||GetLastError()==ERROR_ALREADY_EXISTS){if(mapping)CloseHandle(mapping);throw std::runtime_error("Close Atypoi before integration tests; tests do not alter a running app's state");}
    auto shared=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
    Config config;config.shortcut=0;config.appCount=1;wcscpy_s(config.apps[0].path,lowerPath(modulePath()).c_str());config.apps[0].enabled=1;
    shared->magic=0x41545931;shared->running=1;shared->heartbeat=static_cast<LONG>(GetTickCount());writeConfig(shared,config);
    Ptr<TestManager> proxy(new TestManager(manager),false);
    auto activation=service->ActivateEx(proxy.get(),client,0);check(SUCCEEDED(activation),"activate DLL with isolated key subscription adapter");
    Ptr<ITfDocumentMgr> doc;manager->CreateDocumentMgr(doc.put());Ptr<TestStore> store(new TestStore,false);
    store->window=CreateWindowExW(0,L"EDIT",L"",WS_POPUP,0,0,400,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Ptr<ITfContext> context;TfEditCookie owner=0;check(SUCCEEDED(doc->CreateContext(client,0,static_cast<ITextStoreACP*>(store.get()),context.put(),&owner)),"real TSF context with owned text store");
    doc->Push(context.get());manager->SetFocus(doc.get());Ptr<ITfKeyEventSink> keys;query(service,keys);
    BYTE original[256];GetKeyboardState(original);
    auto press=[&](WPARAM vk,char c=0,bool ctrl=false,bool shift=false){
        BYTE state[256]{};if(ctrl)state[VK_CONTROL]=0x80;if(shift)state[VK_SHIFT]=0x80;SetKeyboardState(state);
        shared->heartbeat=static_cast<LONG>(GetTickCount());BOOL test=FALSE,eaten=FALSE;LPARAM param=static_cast<LPARAM>(MapVirtualKeyW(static_cast<UINT>(vk),MAPVK_VK_TO_VSC))<<16;
        keys->OnTestKeyDown(context.get(),vk,param,&test);if(test)keys->OnKeyDown(context.get(),vk,param,&eaten);
        if(!eaten&&c)store->insertHost(static_cast<unsigned char>(c));BOOL up=FALSE;keys->OnKeyUp(context.get(),vk,param,&up);return eaten!=FALSE;
    };
    auto type=[&](const std::string& value){auto layout=LoadKeyboardLayoutW(L"00000409",KLF_NOTELLSHELL);for(char c:value){SHORT mapped=VkKeyScanExW(c,layout);press(LOBYTE(mapped),c,false,(HIBYTE(mapped)&1)!=0);}};
    type("dkssud hello gksrmf ");expectText(store->text,L"안녕 hello 한글 ","TSF composition + boundary integration");
    type("githubdp APIfmf");expectText(store->text,L"안녕 hello 한글 github에 API를","TSF mixed words");
    check(press(VK_RETURN),"first Enter consumed to commit");check(!press(VK_RETURN),"second Enter passes to host");
    type(" dkssud");
    BOOL modifierEaten=TRUE;keys->OnKeyDown(context.get(),VK_CONTROL,0,&modifierEaten);check(!modifierEaten,"direct Ctrl event preserves composition");
    modifierEaten=TRUE;keys->OnKeyDown(context.get(),VK_SHIFT,0,&modifierEaten);check(!modifierEaten,"direct Shift event preserves composition");
    press(VK_SPACE,0,true,true);expectText(store->text,L"안녕 hello 한글 github에 API를 dkssud","TSF candidate toggle");press(VK_SPACE,' ');
    type("gksrnr");press(VK_BACK);expectText(store->text,L"안녕 hello 한글 github에 API를 dkssud 한구","TSF jamo backspace");
    press(VK_RETURN);auto before=store->text;store->setReadOnly(true);check(!press('A'),"read-only text bypassed");expectText(store->text,before,"read-only text preserved");store->setReadOnly(false);
    auto oldStyle=GetWindowLongPtrW(store->window,GWL_STYLE);SetWindowLongPtrW(store->window,GWL_STYLE,oldStyle|ES_PASSWORD);check(!press('A'),"password edit bypassed");expectText(store->text,before,"password edit unchanged by IME");SetWindowLongPtrW(store->window,GWL_STYLE,oldStyle);
    type(" https://github.com abc123 dkssud ");expectText(store->text,before+L" https://github.com abc123 안녕 ","protected URL and number token recover on Space");
    type("dkssud");auto firstText=store->text;
    Ptr<ITfDocumentMgr> secondDoc;manager->CreateDocumentMgr(secondDoc.put());Ptr<TestStore> secondStore(new TestStore,false);secondStore->window=CreateWindowExW(0,L"EDIT",L"",WS_POPUP,0,0,400,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Ptr<ITfContext> secondContext;TfEditCookie secondOwner=0;check(SUCCEEDED(secondDoc->CreateContext(client,0,static_cast<ITextStoreACP*>(secondStore.get()),secondContext.put(),&secondOwner)),"second input context");secondDoc->Push(secondContext.get());manager->SetFocus(secondDoc.get());
    auto firstStore=store;auto firstContext=context;store=secondStore;context=secondContext;
    type("hello ");expectText(store->text,L"hello ","new field has no old composition");expectText(firstStore->text,firstText,"old field composition retained on focus change");
    manager->SetFocus(doc.get());store=firstStore;context=firstContext;secondDoc->Pop(TF_POPF_ALL);secondContext.reset();secondDoc.reset();DestroyWindow(secondStore->window);secondStore.reset();
    press(VK_SPACE,' ');type("hello");check(!press('Z',0,true),"Ctrl+Z preserved for host");press(VK_SPACE,' ');
    auto beforeFinals=store->text;
    type("sunlightdl tjsaudgkrp leaffmf gkfxrh dlTdjTek");expectText(store->text,beforeFinals+L"sunlight이 선명하게 leaf를 핥고 있었다","TSF requested mixed sentence with Shift finals");
    press(VK_BACK);expectText(store->text,beforeFinals+L"sunlight이 선명하게 leaf를 핥고 있었ㄷ","TSF Backspace keeps unfinished Korean syllable");
    type("k ");type("hello goT");expectText(store->text,beforeFinals+L"sunlight이 선명하게 leaf를 핥고 있었다 hello 했","TSF tense final after English");
    press(VK_BACK);expectText(store->text,beforeFinals+L"sunlight이 선명하게 leaf를 핥고 있었다 hello 해","TSF remove tense final");
    press(VK_RETURN);press(VK_SPACE,' ');
    auto beforePast=store->text;type("aksemfdjTek ");expectText(store->text,beforePast+L"만들었다 ","TSF made with Shift final");
    type("hello RoRmtgoTek");expectText(store->text,beforePast+L"만들었다 hello 깨끗했다","TSF shifted consonants inside past tense after English");
    press(VK_BACK);expectText(store->text,beforePast+L"만들었다 hello 깨끗했ㄷ","TSF past ending backspace");type("k ");
    auto beforeHeldShift=store->text;type("qkRNldjTek ");expectText(store->text,beforeHeldShift+L"바뀌었다 ","TSF ordinary key while Shift is held after doubled consonant");
    {
        press(VK_SPACE,' ');
        auto savedStore=store;auto savedContext=context;auto previousFocus=GetFocus();
        Ptr<TestAccessible> field(new TestAccessible,false);AccessibleWindowFixture fixture{field.get()};
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=AccessibleWindowFixture::procedure;cls.lpszClassName=L"Qt6103QWindowIcon";RegisterClassW(&cls);
        Ptr<TestStore> qtStore(new TestStore,false);qtStore->emptyInputScope=true;qtStore->window=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,300,100,nullptr,nullptr,cls.hInstance,&fixture);SetFocus(qtStore->window);
        Ptr<ITfDocumentMgr> qtDoc;manager->CreateDocumentMgr(qtDoc.put());Ptr<ITfContext> qtContext;TfEditCookie qtOwner=0;
        check(SUCCEEDED(qtDoc->CreateContext(client,0,static_cast<ITextStoreACP*>(qtStore.get()),qtContext.put(),&qtOwner)),"Qt-style host has a real TSF context without InputScope");
        qtDoc->Push(qtContext.get());manager->SetFocus(qtDoc.get());store=qtStore;context=qtContext;
        check(GetFocus()==qtStore->window,"Qt TSF fixture retains native window focus");
        type("g");auto acquisitions=fixture.acquisitions;type("ksrmf aksemfdjTek ");
        if(store->text!=L"한글 만들었다 ")std::cerr<<"Qt diagnostics: status="<<shared->status<<" stage="<<shared->diagnosticStage<<" hr="<<shared->diagnosticHr<<" focused="<<(GetFocus()==qtStore->window)<<" focusReads="<<field->focusReads<<" acquisitions="<<fixture.acquisitions<<"\n";
        expectText(store->text,L"한글 만들었다 ","TSF unscoped Qt field composes Korean and shifted finals");
        check(acquisitions>0&&fixture.acquisitions==acquisitions,"TSF Qt composition reuses accessible provider between keys");
        auto beforeProtected=store->text;field->state|=STATE_SYSTEM_PROTECTED;check(!press('A'),"Qt password input bypasses TSF conversion");expectText(store->text,beforeProtected,"Qt password state preserves text store");
        field->state=STATE_SYSTEM_FOCUSED|STATE_SYSTEM_READONLY;check(!press('A'),"Qt accessible read-only field bypasses conversion");
        field->state=STATE_SYSTEM_FOCUSED;field->role=ROLE_SYSTEM_CLIENT;check(!press('A'),"unknown Qt role bypasses conversion");
        field->role=ROLE_SYSTEM_TEXT;type("gksrmf ");expectText(store->text,beforeProtected+L"한글 ","Qt plain text recovers after protected input");
        type("gksrmf");check(press(VK_SPACE,' '),"claimed boundary Space is inserted by the TIP");
        expectText(store->text,beforeProtected+L"한글 한글 ","claimed Space is preserved exactly once");
        type("hello");check(press(VK_OEM_PERIOD,'.'),"claimed punctuation is inserted by the TIP");
        expectText(store->text,beforeProtected+L"한글 한글 hello.","claimed punctuation is preserved exactly once");
        check(field->contentReads==0,"TSF Qt conversion never reads accessible input contents");
        manager->SetFocus(doc.get());store=savedStore;context=savedContext;SetFocus(previousFocus);qtDoc->Pop(TF_POPF_ALL);qtContext.reset();qtDoc.reset();DestroyWindow(qtStore->window);qtStore.reset();UnregisterClassW(cls.lpszClassName,cls.hInstance);
    }
    store->failSelection=true;auto beforeFailure=store->text;check(press('D','d'),"key consumed after text inserted despite selection failure");expectText(store->text,beforeFailure+L"d","no duplicated key on partial edit failure");store->failSelection=false;
    config.mode=3;writeConfig(shared,config);check(!press('A'),"pause bypasses input");config.mode=0;writeConfig(shared,config);
    type(" dkssud");shared->running=0;check(!press('X'),"tray exit bypasses input");
    SetKeyboardState(original);service->Deactivate();manager->SetFocus(nullptr);doc->Pop(TF_POPF_ALL);context.reset();doc.reset();keys.reset();DestroyWindow(store->window);store.reset();proxy.reset();UnmapViewOfFile(shared);CloseHandle(mapping);
}
}
int wmain(){
    try {
        Engine engine(file("data/english_words.txt"),file("data/korean_words.txt"),file("data/korean_syllables.txt"));check(engine.ready(),"full dictionaries loaded");
        pastTenseRegression(engine);
        check(accessible::qtWindow(L"Qt6103QWindowIcon")&&accessible::qtWindow(L"Qt5QWindowOwnDCIcon"),"Qt window classes across versions recognized");
        for(auto name:{L"edit",L"QtQuickText",L"QtQWindowIcon",L"Qt6103QWindowIconExtra",L"Chrome_WidgetWin_1"})check(!accessible::qtWindow(name),"other custom windows do not gain Qt fallback");
        Ptr<TestAccessible> editable(new TestAccessible,false),container(new TestAccessible,false);
        Ptr<TestUnknownAlias> alias(new TestUnknownAlias(editable.get()),false);
        check(sameObject(editable.get(),alias.get()),"different interface wrappers retain canonical COM identity");
        check(!sameObject(editable.get(),container.get()),"different COM objects are not merged");
        check(!sameObject(editable.get(),nullptr)&&sameObject(nullptr,nullptr),"null COM identity is handled safely");
        check(accessible::focusedEditable(editable.get()),"focused accessible plain text accepted without reading content");
        for(auto blocked:{STATE_SYSTEM_PROTECTED,STATE_SYSTEM_READONLY,STATE_SYSTEM_UNAVAILABLE,STATE_SYSTEM_INVISIBLE,STATE_SYSTEM_OFFSCREEN}){editable->state=STATE_SYSTEM_FOCUSED|blocked;check(!accessible::focusedEditable(editable.get()),"protected or unavailable Qt input rejected");}
        editable->state=STATE_SYSTEM_FOCUSABLE;check(!accessible::focusedEditable(editable.get()),"unfocused text rejected");editable->state=STATE_SYSTEM_FOCUSED;
        editable->role=ROLE_SYSTEM_CLIENT;check(!accessible::focusedEditable(editable.get()),"generic client is not editable evidence");editable->role=ROLE_SYSTEM_TEXT;
        editable->focusResult=E_FAIL;check(!accessible::focusedEditable(editable.get()),"accessibility focus failure fails closed");editable->focusResult=S_OK;
        editable->roleResult=E_FAIL;check(!accessible::focusedEditable(editable.get()),"accessibility role failure fails closed");editable->roleResult=S_OK;
        editable->stateResult=E_FAIL;check(!accessible::focusedEditable(editable.get()),"accessibility state failure fails closed");editable->stateResult=S_OK;
        editable->stateType=VT_EMPTY;check(!accessible::focusedEditable(editable.get()),"missing accessibility state rejected");editable->stateType=VT_I4;
        editable->focusType=VT_EMPTY;check(!accessible::focusedEditable(editable.get()),"missing accessible focus rejected");editable->focusType=VT_I4;
        container->child=Ptr<IAccessible>(editable.get());check(accessible::focusedEditable(container.get()),"Qt focused child dispatch resolved");
        editable->state|=STATE_SYSTEM_PROTECTED;check(!accessible::focusedEditable(container.get()),"password focused child remains protected");editable->state=STATE_SYSTEM_FOCUSED;
        container->child.reset();container->cycle=true;check(!accessible::focusedEditable(container.get()),"cyclic accessible focus terminates safely");
        check(editable->contentReads==0&&container->contentReads==0,"privacy: no accessible values names or content requested");
        check(host::inspect(0,L"unused")==host::InputService::Unknown,"invalid host inspection is unknown");
        host::RestartNotices notices;
        check(!notices.observe(0,host::InputService::Outdated),"invalid process does not generate a restart notice");
        check(notices.observe(101,host::InputService::Outdated),"old input service generates a restart notice");
        check(!notices.observe(101,host::InputService::Outdated),"repeated old-host polling does not spam notices");
        check(!notices.observe(101,host::InputService::Unknown)&&!notices.observe(101,host::InputService::Outdated),"temporary inspection failure retains notice history");
        check(notices.observe(102,host::InputService::Outdated)&&!notices.observe(101,host::InputService::Outdated),"switching apps preserves per-host notice history");
        check(!notices.observe(101,host::InputService::Current)&&notices.observe(101,host::InputService::Outdated),"recovered host can report a later stale module");
        check(!notices.observe(101,host::InputService::NotLoaded)&&notices.observe(101,host::InputService::Outdated),"unloaded host clears stale notice history");
        CreateDirectoryW(L"build\\host-version",nullptr);check(CopyFileW(L"build\\Atypoi.TestIME.dll",L"build\\host-version\\Atypoi.IME.dll",FALSE),"prepare isolated host-version module");
        auto hostDll=LoadLibraryW(L"build\\host-version\\Atypoi.IME.dll");check(hostDll!=nullptr,"load isolated host-version module");
        if(hostDll){wchar_t loaded[32768]{};GetModuleFileNameW(hostDll,loaded,32768);check(host::inspect(GetCurrentProcessId(),loaded)==host::InputService::Current,"detect matching installed input service");check(host::inspect(GetCurrentProcessId(),L"C:\\different-version\\Atypoi.IME.dll")==host::InputService::Outdated,"detect previously loaded input service");FreeLibrary(hostDll);}
        DeleteFileW(L"build\\host-version\\Atypoi.IME.dll");RemoveDirectoryW(L"build\\host-version");
        CreateDirectoryW(L"build\\host-old",nullptr);CreateDirectoryW(L"build\\host-new",nullptr);
        check(CopyFileW(L"build\\Atypoi.TestIME.dll",L"build\\host-old\\Atypoi.IME.dll",FALSE)&&CopyFileW(L"build\\Atypoi.TestIME.dll",L"build\\host-new\\Atypoi.IME.dll",FALSE),"prepare coexisting input service modules");
        auto oldModule=LoadLibraryW(L"build\\host-old\\Atypoi.IME.dll"),newModule=LoadLibraryW(L"build\\host-new\\Atypoi.IME.dll");
        check(oldModule&&newModule&&oldModule!=newModule,"map both host input service versions");
        if(oldModule&&newModule){
            wchar_t oldPath[32768]{},newPath[32768]{};GetModuleFileNameW(oldModule,oldPath,32768);GetModuleFileNameW(newModule,newPath,32768);
            check(host::inspect(GetCurrentProcessId(),oldPath)==host::InputService::Outdated,"later stale module is not hidden by first current module");
            check(host::inspect(GetCurrentProcessId(),newPath)==host::InputService::Outdated,"earlier stale module is detected alongside current module");
        }
        if(oldModule)FreeLibrary(oldModule);if(newModule)FreeLibrary(newModule);
        DeleteFileW(L"build\\host-old\\Atypoi.IME.dll");DeleteFileW(L"build\\host-new\\Atypoi.IME.dll");RemoveDirectoryW(L"build\\host-old");RemoveDirectoryW(L"build\\host-new");
        check(update::sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 standard test vector");
        auto manifest=update::parse("version=0.2.0\nurl=https://example.com/setup.exe\nsha256="+std::string(64,'a')+"\n");check(manifest.version==L"0.2.0","update manifest parser");
        check(update::versionParts(L"0.2.0")>update::versionParts(L"0.1.0-dev"),"update version ordering");
        check(update::newer(L"0.1.6",L"0.1.6-dev"),"stable release upgrades same-version development build");
        check(!update::newer(L"0.1.6",L"0.1.6")&&!update::newer(L"0.1.5",L"0.1.6"),"no repeated update or downgrade");
        for(auto invalid:{L"1.2.3.4",L"1.2.3-evil",L"1.2.3/other",L"-1.2.3",L"1.2.65536",L"01.2.3",L"1. 2.3",L"1.2."}){bool rejected=false;try{update::versionParts(invalid);}catch(...){rejected=true;}check(rejected,"malformed update version rejected");}
        auto officialManifest=update::parse("version=0.1.6\nurl=https://github.com/ssrihappy/atypoi/releases/download/v0.1.6/Atypoi.Setup.exe\nsha256="+std::string(64,'a')+"\n");
        check(update::official(officialManifest),"official versioned installer accepted");
        for(auto url:{L"https://github.com/other/atypoi/releases/download/v0.1.6/Atypoi.Setup.exe",L"https://github.com/ssrihappy/atypoi/releases/download/v0.1.5/Atypoi.Setup.exe",L"https://github.com/ssrihappy/atypoi/releases/download/v0.1.6/Atypoi.Setup.exe?other",L"https://github.com.evil.invalid/ssrihappy/atypoi/releases/download/v0.1.6/Atypoi.Setup.exe"}){officialManifest.url=url;check(!update::official(officialManifest),"unofficial update installer rejected");}
        check(update::publisherAllowed("","",true),"unsigned official GitHub release allowed with user consent");
        check(!update::publisherAllowed("","",false),"unsigned custom feed rejected");
        check(update::publisherAllowed("trusted","trusted",true),"matching publisher accepted");
        check(!update::publisherAllowed("trusted","",true)&&!update::publisherAllowed("trusted","different",true),"signed installations reject unsigned or different publisher updates");
        for(auto invalid:{"version=0.2.0\nurl=http://example.com/a\nsha256=abc", "version=0.2.0\nversion=0.3.0\nurl=https://example.com/a\nsha256=abc"}) {bool rejected=false;try{update::parse(invalid);}catch(...){rejected=true;}check(rejected,"invalid update feed rejected");}
        const std::vector<std::pair<std::string,std::wstring>> compositions={
            {"dkssud",L"안녕"},{"gksrmf",L"한글"},{"gksrnr",L"한국"},{"rkqtk",L"갑사"},{"rkqt",L"값"},{"rkqtdl",L"값이"},
            {"rhk",L"과"},{"rhos",L"괜"},{"rhl",L"괴"},{"rnj",L"궈"},{"rnp",L"궤"},{"rnl",L"귀"},{"rml",L"긔"},
            {"Rk",L"까"},{"Ek",L"따"},{"Qk",L"빠"},{"Tk",L"싸"},{"Wk",L"짜"},{"dO",L"얘"},{"dP",L"예"},
            {"zz",L"ㅋㅋ"},{"gg",L"ㅎㅎ"},{"bb",L"ㅠㅠ"},{"nn",L"ㅜㅜ"},{"k",L"ㅏ"},{"r",L"ㄱ"},
            {"dkssudgktpdy",L"안녕하세요"},{"rkatkgkqslek",L"감사합니다"},{"qkRNl",L"바뀌"}};
        for(const auto& [keys,expected]:compositions)expectText(compose(keys).text,expected,"compose "+keys);
        for(auto [keys,expected]:std::vector<std::pair<std::string,std::wstring>>{{"dlT",L"있"},{"dlTek",L"있다"},{"dlTdjTek",L"있었다"},{"goT",L"했"},{"goTek",L"했다"},{"TjTek",L"썼다"},{"qhkTek",L"봤다"},{"rkTek",L"갔다"},{"gkfxrh",L"핥고"}}){
            expectText(compose(keys).text,expected,"tense/compound final composition "+keys);
            expectText(engine.decide(keys,Language::English).text,expected,"tense/compound final after English "+keys);
        }
        Session sentence(engine);std::wstring sentenceText;
        for(char c:std::string("sunlightdl tjsaudgkrp leaffmf gkfxrh dlTdjTek")){if(c==' ')sentenceText+=sentence.commit()+L" ";else sentence.append(c,Mode::Auto,false);}
        sentenceText+=sentence.commit();expectText(sentenceText,L"sunlight이 선명하게 leaf를 핥고 있었다","requested mixed sentence");
        Session tense(engine);
        for(char c:std::string("dlTdjT"))tense.append(c,Mode::Auto,false);
        expectText(tense.display().text,L"있었","tense final live composition");
        tense.append('e',Mode::Auto,false);expectText(tense.display().text,L"있었ㄷ","unfinished next syllable stays Korean");
        tense.append('k',Mode::Auto,false);expectText(tense.display().text,L"있었다","tense final completed word");
        for(auto expected:{L"있었ㄷ",L"있었",L"있",L""}){tense.backspace(Mode::Auto,false);expectText(tense.display().text,expected,"tense final Backspace remains Korean");}
        for(char c:std::string("dlT"))tense.append(c,Mode::Auto,false);
        for(auto expected:{L"이",L"ㅇ",L""}){tense.backspace(Mode::Auto,false);expectText(tense.display().text,expected,"single syllable tense final Backspace");}
        for(auto keys:{"sunlight","leaf","hello","world","got","GoT","TOP","ROOT","TWO","API","the","The","that","with","great","testing","shift","gift","soft","tjsA"}){Session english(engine);for(char c:std::string(keys))english.append(c,Mode::Auto,false);expectText(english.display().text,fromUtf8(keys),"English recovers after partial Korean "+std::string(keys));}
        const std::vector<std::string> cho={"r","R","s","e","E","f","a","q","Q","t","T","d","w","W","c","z","x","v","g"};
        const std::vector<std::string> jung={"k","o","i","O","j","p","u","P","h","hk","ho","hl","y","n","nj","np","nl","b","m","ml","l"};
        const std::vector<std::string> jong={"","r","R","rt","s","sw","sg","e","f","fr","fa","fq","ft","fx","fv","fg","a","q","qt","t","T","d","w","c","z","x","v","g"};
        for(size_t c=0;c<19;++c)for(size_t v=0;v<21;++v)for(size_t f=0;f<28;++f)expectText(compose(cho[c]+jung[v]+jong[f]).text,std::wstring(1,static_cast<wchar_t>(0xac00+(c*21+v)*28+f)),"all 11172 Hangul syllables");
        for(auto [keys,expected]:std::vector<std::pair<std::string,std::wstring>>{{"dkssud",L"안녕"},{"gksrmf",L"한글"},{"hello",L"hello"},{"githubdp",L"github에"},{"APIfmf",L"API를"},{"API",L"API"},{"Hello",L"Hello"},{"go",L"go"},{"https",L"https"},{"nn",L"ㅜㅜ"}})expectText(engine.decide(keys).text,expected,"decide "+keys);
        for(auto key:{"hello_world","sample@example.invalid","D:\\examples\\sample","https://github.com","abc123"})expectText(engine.decide(key).text,fromUtf8(key),"protected token");
        Session session(engine);for(char c:std::string("gksrnr"))session.append(c,Mode::Korean,false);
        for(auto expected:{L"한구",L"한ㄱ",L"한",L""}){session.backspace(Mode::Korean,false);expectText(session.display().text,expected,"backspace Korean");}
        for(char c:std::string("dkssud"))session.append(c,Mode::Auto,false);expectText(session.display().text,L"안녕","live preview");session.cycle();expectText(session.display().text,L"dkssud","cycle raw");session.append('g',Mode::Auto,false);expectText(session.display().text,L"dkssudg","forced English retained");expectText(session.commit(),L"dkssudg","commit preview unchanged");check(session.empty(),"commit clears raw buffer");
        session.reset();for(size_t i=0;i<Session::maxKeys;++i)check(session.append('a',Mode::English,false),"bounded append");check(!session.append('a',Mode::English,false),"buffer bounded");session.reset();
        std::mt19937 random(47);std::string alphabet="abcdefghijklmnopqrstuvwxyzQWERTOP";
        for(int n=0;n<2000;++n){std::string token;for(int i=0;i<1+static_cast<int>(random()%40);++i)token+=alphabet[random()%alphabet.size()];auto composed=compose(token);check(composed.text.size()==composed.boundaries.size(),"composition ranges aligned");check(std::is_sorted(composed.boundaries.begin(),composed.boundaries.end()),"composition range order");check(toUtf8(fromUtf8(toUtf8(composed.text)))==toUtf8(composed.text),"UTF8 roundtrip");auto d=engine.decide(token);check(!d.text.empty(),"classifier never drops token");}
        Config config;check(!policy(config,L"c:\\windows\\notepad.exe").enabled,"unverified app disabled");wcscpy_s(config.apps[0].path,L"c:\\test.exe");config.appCount=1;config.apps[0].enabled=1;config.apps[0].strict=1;config.apps[0].enter=0;auto p=policy(config,L"c:\\test.exe");check(p.enabled&&p.strict&&!p.enter,"app overrides");
        Shared shared;shared.running=1;shared.heartbeat=static_cast<LONG>(GetTickCount());writeConfig(&shared,config);Config read;check(readConfig(&shared,read),"shared config snapshot");shared.heartbeat=static_cast<LONG>(GetTickCount()-3000);check(!readConfig(&shared,read),"dead tray stops input");shared.heartbeat=static_cast<LONG>(GetTickCount());shared.running=0;check(!readConfig(&shared,read),"exit stops input");
        // Exercise the actual DLL COM contracts without installing or selecting an IME.
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        {
            Ptr<TestAccessible> field(new TestAccessible,false);AccessibleWindowFixture fixture{field.get()};
            WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=AccessibleWindowFixture::procedure;cls.lpszClassName=L"Qt6103QWindowIcon";RegisterClassW(&cls);
            auto window=CreateWindowW(cls.lpszClassName,L"",WS_POPUP,0,0,100,40,nullptr,nullptr,cls.hInstance,&fixture);
            auto previous=GetFocus();SetFocus(window);check(window&&GetFocus()==window,"owned custom Qt-style accessibility window focused");
            accessible::QtInput proof;proof.update(window);check(proof.permits(window),"Qt input proof uses actual Windows accessibility provider");
            auto acquisitions=fixture.acquisitions;check(acquisitions>0,"Qt provider acquired through WM_GETOBJECT");
            for(int i=0;i<20;++i)proof.update(window);
            check(fixture.acquisitions==acquisitions,"Qt provider cached rather than reacquired during every key");
            field->state|=STATE_SYSTEM_PROTECTED;proof.update(window);check(!proof.permits(window),"live Qt password state invalidates prior plain-text proof");
            field->state=STATE_SYSTEM_FOCUSED|STATE_SYSTEM_READONLY;proof.update(window);check(!proof.permits(window),"live Qt read-only state invalidates proof");
            field->state=STATE_SYSTEM_FOCUSED;proof.update(window);check(proof.permits(window),"Qt plain text resumes after protected field");
            check(field->contentReads==0,"native Qt safety proof never requests field contents");
            proof.reset();check(!proof.permits(window),"cleared Qt proof cannot authorize old window");SetFocus(previous);DestroyWindow(window);UnregisterClassW(cls.lpszClassName,cls.hInstance);
        }
        HMODULE dll=LoadLibraryW(L"build\\Atypoi.TestIME.dll");check(dll!=nullptr,"load isolated IME DLL");
        if(dll){
            auto get=reinterpret_cast<HRESULT(WINAPI*)(REFCLSID,REFIID,void**)>(GetProcAddress(dll,"DllGetClassObject"));
            auto unload=reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(dll,"DllCanUnloadNow"));
            check(get&&unload,"COM exports");
            if(get&&unload){Ptr<IClassFactory> factory;check(SUCCEEDED(get(serviceId,IID_IClassFactory,reinterpret_cast<void**>(factory.put()))),"class factory");Ptr<ITfTextInputProcessorEx> service;
                if(factory){check(SUCCEEDED(factory->CreateInstance(nullptr,__uuidof(ITfTextInputProcessorEx),reinterpret_cast<void**>(service.put()))),"create TSF service");}
                if(service){Ptr<ITfDisplayAttributeProvider> provider;check(SUCCEEDED(query(service.get(),provider)),"display attribute provider");if(provider){Ptr<ITfDisplayAttributeInfo> info;check(SUCCEEDED(provider->GetDisplayAttributeInfo(displayId,info.put())),"composition attribute");TF_DISPLAYATTRIBUTE attribute{};if(info){info->GetAttributeInfo(&attribute);check(attribute.lsStyle==TF_LS_SOLID,"composition underline");}}
                    Ptr<ITfThreadMgr> manager;auto hr=CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfThreadMgr),reinterpret_cast<void**>(manager.put()));check(SUCCEEDED(hr),"real Windows TSF manager");if(manager){TfClientId client=0;check(SUCCEEDED(manager->Activate(&client)),"activate TSF manager");integration(service.get(),manager.get(),client);manager->Deactivate();}
                }
                service.reset();factory.reset();check(unload()==S_OK,"DLL releases COM references");
            }
            FreeLibrary(dll);
        }
        CoUninitialize();
        std::vector<double> timings;timings.reserve(10000);
        const std::vector<std::string> samples={"hello","dkssudgktpdy","githubdp","rkatkgkqslek","gksrmf","APIfmf","world","go"};
        for(int i=0;i<10000;++i){auto start=std::chrono::steady_clock::now();auto result=engine.decide(samples[i%samples.size()]);auto end=std::chrono::steady_clock::now();timings.push_back(std::chrono::duration<double,std::milli>(end-start).count());if(result.text.empty())++failures;}
        std::sort(timings.begin(),timings.end());std::cout<<"Decision latency p95="<<timings[9500]<<" ms over 10000 decisions\n";check(timings[9500]<5,"engine p95 budget");
        std::cout<<checks<<" checks; "<<failures<<" failures\n";return failures?1:0;
    }catch(const std::exception& ex){std::cerr<<"ERROR: "<<ex.what()<<"\n";return 2;}
}
