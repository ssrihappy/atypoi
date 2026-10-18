#include "../win/tsf_compat.h"
#include "../win/accessible_input.h"
#include <inputscope.h>
#include <new>
#include <functional>
#include <array>

using namespace atypoi;
using namespace atypoi::win;
namespace {
HMODULE module=nullptr;
LONG objects=0;
constexpr GUID inputScopeId={0x1713dd5a,0x68e7,0x4a5b,{0x9a,0xf6,0x59,0x2a,0x59,0x5c,0x77,0x8d}};
struct Counted {Counted(){InterlockedIncrement(&objects);}~Counted(){InterlockedDecrement(&objects);}};

class Attribute final:public ITfDisplayAttributeInfo,Counted {
    LONG refs_=1;TF_DISPLAYATTRIBUTE value_{};
public:
    Attribute(){Reset();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==__uuidof(ITfDisplayAttributeInfo))*out=static_cast<ITfDisplayAttributeInfo*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetGUID(GUID* id) override{if(!id)return E_POINTER;*id=displayId;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDescription(BSTR* text) override{if(!text)return E_POINTER;*text=SysAllocString(L"Atypoi 조합 중");return *text?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetAttributeInfo(TF_DISPLAYATTRIBUTE* value) override{if(!value)return E_POINTER;*value=value_;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetAttributeInfo(const TF_DISPLAYATTRIBUTE* value) override{if(!value)return E_POINTER;value_=*value;return S_OK;}
    HRESULT STDMETHODCALLTYPE Reset() override{value_={};value_.lsStyle=TF_LS_SOLID;value_.crLine.type=TF_CT_SYSCOLOR;value_.crLine.nIndex=COLOR_WINDOWTEXT;value_.bAttr=TF_ATTR_INPUT;return S_OK;}
};
class Attributes final:public IEnumTfDisplayAttributeInfo,Counted {
    LONG refs_=1;bool done_=false;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==__uuidof(IEnumTfDisplayAttributeInfo))*out=static_cast<IEnumTfDisplayAttributeInfo*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Clone(IEnumTfDisplayAttributeInfo** out) override{if(!out)return E_POINTER;auto p=new(std::nothrow) Attributes;if(!p)return E_OUTOFMEMORY;p->done_=done_;*out=p;return S_OK;}
    HRESULT STDMETHODCALLTYPE Next(ULONG count,ITfDisplayAttributeInfo** out,ULONG* fetched) override{if(!out||(!fetched&&count!=1))return E_POINTER;if(fetched)*fetched=0;if(!count)return S_OK;if(done_)return S_FALSE;*out=new(std::nothrow) Attribute;if(!*out)return E_OUTOFMEMORY;done_=true;if(fetched)*fetched=1;return count==1?S_OK:S_FALSE;}
    HRESULT STDMETHODCALLTYPE Reset() override{done_=false;return S_OK;}
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override{if(!count)return S_OK;bool old=done_;done_=true;return !old&&count==1?S_OK:S_FALSE;}
};
class Edit final:public ITfEditSession,Counted {
    LONG refs_=1;std::function<HRESULT(TfEditCookie)> action_;
public:
    explicit Edit(std::function<HRESULT(TfEditCookie)> action):action_(std::move(action)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==__uuidof(ITfEditSession))*out=static_cast<ITfEditSession*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie cookie) override{try{return action_(cookie);}catch(...){return E_FAIL;}}
};
HRESULT request(ITfContext* context,TfClientId client,std::function<HRESULT(TfEditCookie)> action,DWORD flags=TF_ES_SYNC|TF_ES_READWRITE) {
    if(!context)return E_POINTER;auto edit=new(std::nothrow) Edit(std::move(action));if(!edit)return E_OUTOFMEMORY;
    HRESULT result=E_FAIL;auto hr=context->RequestEditSession(client,edit,flags,&result);edit->Release();return FAILED(hr)?hr:result;
}
bool compartment(IUnknown* owner,REFGUID id) {
    Ptr<ITfCompartmentMgr> manager;Ptr<ITfCompartment> value;if(FAILED(query(owner,manager))||FAILED(manager->GetCompartment(id,value.put())))return false;
    VARIANT v;VariantInit(&v);bool yes=SUCCEEDED(value->GetValue(&v))&&v.vt==VT_I4&&v.lVal!=0;VariantClear(&v);return yes;
}
class Service final:public ITfTextInputProcessorEx,public ITfKeyEventSink,public ITfCompositionSink,public ITfThreadMgrEventSink,public ITfTextEditSink,public ITfDisplayAttributeProvider,Counted {
    LONG refs_=1;
    Ptr<ITfThreadMgr> manager_;
    Ptr<ITfContext> context_;
    Ptr<ITfComposition> composition_;
    Ptr<ITfSource> contextSource_;
    TfClientId client_=TF_CLIENTID_NULL;
    DWORD threadCookie_=TF_INVALID_COOKIE,textCookie_=TF_INVALID_COOKIE;
    TfGuidAtom attribute_=TF_INVALID_GUIDATOM;
    std::unique_ptr<Engine> engine_;
    std::unique_ptr<Session> session_;
    HANDLE mapping_=nullptr;
    Shared* shared_=nullptr;
    HWND timer_=nullptr;
    HKL keyboard_=nullptr;
    std::wstring path_;
    std::array<bool,256> consumed_{};
    bool secure_=false,editing_=false,protectedToken_=false,applied_=false,fault_=false;
    accessible::QtInput qtInput_;
    int fieldSupported_=-1;
    LONG modeSeen_=-1;
    DWORD lastInput_=0;

    static LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto self=reinterpret_cast<Service*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<Service*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(message==WM_TIMER&&self)self->tick();
        return DefWindowProcW(hwnd,message,w,l);
    }
    void connect() {
        if(shared_)return;
        mapping_=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,objectName(L"State").c_str());
        if(mapping_){shared_=static_cast<Shared*>(MapViewOfFile(mapping_,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(Shared)));if(!shared_){CloseHandle(mapping_);mapping_=nullptr;}}
    }
    Policy currentPolicy() {
        Config config;if(secure_||!readConfig(shared_,config))return {};
        return policy(config,path_);
    }
    void report(LONG status) {
        if(!shared_)return;DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);if(pid!=GetCurrentProcessId())return;
        InterlockedExchange(&shared_->foregroundPid,static_cast<LONG>(pid));InterlockedExchange(&shared_->status,status);InterlockedExchange(&shared_->statusTick,static_cast<LONG>(GetTickCount()));
    }
    void tick() {
        if(!editing_)qtInput_.update(GetFocus());
        connect();auto p=currentPolicy();
        if(p.mode==Mode::Paused||static_cast<LONG>(p.mode)!=modeSeen_) {finishAsync();if(session_)session_->reset();protectedToken_=false;modeSeen_=static_cast<LONG>(p.mode);}
        if(session_ && GetTickCount()-lastInput_>30000)session_->resetContext();
        report(fault_?6:!p.enabled?5:fieldSupported_==0?7:static_cast<LONG>(p.mode)+1);
    }
    void unbind() {
        if(contextSource_&&textCookie_!=TF_INVALID_COOKIE)contextSource_->UnadviseSink(textCookie_);
        textCookie_=TF_INVALID_COOKIE;contextSource_.reset();context_.reset();
    }
    void bind(ITfContext* context) {
        if(sameObject(context_.get(),context))return;
        if(shared_)InterlockedIncrement(&shared_->bindings);
        finishAsync();unbind();context_=Ptr<ITfContext>(context);protectedToken_=false;
        if(session_)session_->reset();fault_=false;fieldSupported_=-1;
        if(context_&&SUCCEEDED(query(context_.get(),contextSource_)))contextSource_->AdviseSink(__uuidof(ITfTextEditSink),static_cast<ITfTextEditSink*>(this),&textCookie_);
    }
    void finishAsync() {
        if(!composition_)return;
        auto composition=composition_;auto context=context_;composition_.reset();if(session_)session_->commit();
        request(context.get(),client_,[composition,context](TfEditCookie cookie){
            Ptr<ITfRange> range;Ptr<ITfProperty> property;
            if(SUCCEEDED(composition->GetRange(range.put()))&&SUCCEEDED(context->GetProperty(GUID_PROP_ATTRIBUTE,property.put())))property->Clear(cookie,range.get());
            return composition->EndComposition(cookie);
        },TF_ES_ASYNCDONTCARE|TF_ES_READWRITE);
    }
    HRESULT finish(TfEditCookie cookie) {
        if(!composition_){session_->commit();return S_OK;}
        auto composition=composition_;Ptr<ITfRange> range;Ptr<ITfProperty> property;
        if(SUCCEEDED(composition->GetRange(range.put()))&&SUCCEEDED(context_->GetProperty(GUID_PROP_ATTRIBUTE,property.put())))property->Clear(cookie,range.get());
        composition_.reset(); // EndComposition may synchronously call our sink.
        auto hr=composition->EndComposition(cookie);
        if(SUCCEEDED(hr))session_->commit();else composition_=composition;
        return hr;
    }
    bool safe(TfEditCookie cookie,ITfContext* context) {
        auto stage=[&](LONG n){if(shared_)InterlockedExchange(&shared_->diagnosticStage,n);};stage(10);
        if(fault_)return false;
        stage(11);
        TF_STATUS status{};if(FAILED(context->GetStatus(&status))||(status.dwDynamicFlags&TF_SD_READONLY))return false;
        stage(12);
        if(compartment(context,GUID_COMPARTMENT_KEYBOARD_DISABLED)||compartment(context,GUID_COMPARTMENT_EMPTYCONTEXT))return false;
        Ptr<ITfContextView> view;HWND hwnd=nullptr;
        if(SUCCEEDED(context->GetActiveView(view.put())))view->GetWnd(&hwnd);
        bool nativeText=false,qtText=false;
        if(hwnd) {
            HWND focus=GetFocus();if(focus&&(focus==hwnd||IsChild(hwnd,focus)))hwnd=focus;
            wchar_t cls[128]{};GetClassNameW(hwnd,cls,128);std::wstring name=lowerPath(cls);
            nativeText=name==L"edit"||name.starts_with(L"richedit");
            if(nativeText&&(GetWindowLongPtrW(hwnd,GWL_STYLE)&ES_PASSWORD))return false;
            if(accessible::qtWindow(name)){
                qtText=qtInput_.permits(hwnd);
                if(!qtText)return false;
            }
        }
        TF_SELECTION selection{};ULONG count=0;
        stage(13);
        if(FAILED(context->GetSelection(cookie,TF_DEFAULT_SELECTION,1,&selection,&count))||!count)return false;
        Ptr<ITfRange> range(selection.range,false);Ptr<ITfReadOnlyProperty> property;
        if(SUCCEEDED(context->GetAppProperty(inputScopeId,property.put()))) {
            stage(14);
            VARIANT value;VariantInit(&value);
            auto scopeHr=property->GetValue(cookie,range.get(),&value);stage(FAILED(scopeHr)?scopeHr:1000+value.vt);
            if(SUCCEEDED(scopeHr)&&value.vt==VT_UNKNOWN&&value.punkVal) {
                stage(15);
                Ptr<ITfInputScope> scope;
                if(SUCCEEDED(query(value.punkVal,scope))) {
                    InputScope* scopes=nullptr;UINT n=0;
                    if(SUCCEEDED(scope->GetInputScopes(&scopes,&n))) {
                        if((n&&!scopes)||n>128){CoTaskMemFree(scopes);VariantClear(&value);return false;}
                        stage(n?100+scopes[0]:16);
                        bool forbidden=false;
                        for(UINT i=0;i<n;++i) if(scopes[i]==IS_PASSWORD||scopes[i]==IS_EMAIL_SMTPEMAILADDRESS||scopes[i]==IS_URL||scopes[i]==IS_NUMBER||scopes[i]==IS_DIGITS||scopes[i]==IS_TELEPHONE_FULLTELEPHONENUMBER)forbidden=true;
                        CoTaskMemFree(scopes);VariantClear(&value);return n>0?!forbidden:nativeText||qtText;
                    }
                }
                VariantClear(&value);return false;
            }
            if(qtText&&(FAILED(scopeHr)||value.vt!=VT_EMPTY)){VariantClear(&value);return false;}
            VariantClear(&value);
        }
        // Qt's IMM bridge can return VT_EMPTY for InputScope. Accept only an
        // independently verified focused editable control; other custom hosts
        // still require a scope. Explicit forbidden scopes above always win.
        return nativeText||qtText;
    }
    HRESULT render(TfEditCookie cookie) {
        Ptr<ITfRange> range;
        if(!composition_) {
            Ptr<ITfInsertAtSelection> insertion;auto hr=query(context_.get(),insertion);
            if(FAILED(hr))return hr;
            hr=insertion->InsertTextAtSelection(cookie,TF_IAS_QUERYONLY,nullptr,0,range.put());
            if(FAILED(hr)||!range)return FAILED(hr)?hr:E_FAIL;
            Ptr<ITfContextComposition> compositions;hr=query(context_.get(),compositions);if(FAILED(hr))return hr;
            hr=compositions->StartComposition(cookie,range.get(),static_cast<ITfCompositionSink*>(this),composition_.put());if(FAILED(hr)||!composition_)return FAILED(hr)?hr:E_FAIL;
        }
        // Edit the composition's range and use a separate clone for the caret.
        {auto hr=composition_->GetRange(range.put());if(FAILED(hr))return hr;}
        const auto& text=session_->display().text;auto hr=range->SetText(cookie,0,text.c_str(),static_cast<LONG>(text.size()));if(FAILED(hr))return hr;
        applied_=true; // Never replay an OS key after its text was already inserted.
        Ptr<ITfProperty> property;
        if(attribute_!=TF_INVALID_GUIDATOM && SUCCEEDED(context_->GetProperty(GUID_PROP_ATTRIBUTE,property.put()))) {
            VARIANT value;VariantInit(&value);value.vt=VT_I4;value.lVal=attribute_;property->SetValue(cookie,range.get(),&value);
        }
        Ptr<ITfRange> caret;hr=range->Clone(caret.put());if(FAILED(hr))return hr;
        caret->Collapse(cookie,TF_ANCHOR_END);
        TF_SELECTION selection{caret.get(),{TF_AE_NONE,FALSE}};
        hr=context_->SetSelection(cookie,1,&selection);
        if(session_->empty())return finish(cookie);
        return hr;
    }
    char character(WPARAM key,LPARAM param) {
        BYTE state[256];if(!GetKeyboardState(state))return 0;
        wchar_t text[4]{};int n=ToUnicodeEx(static_cast<UINT>(key),(static_cast<UINT>(param)>>16)&255,state,text,4,4,keyboard_);
        return n==1&&text[0]>=32&&text[0]<127?static_cast<char>(text[0]):0;
    }
    bool switching(WPARAM key,const Policy& p) {
        bool ctrl=GetKeyState(VK_CONTROL)&0x8000,shift=GetKeyState(VK_SHIFT)&0x8000,alt=GetKeyState(VK_MENU)&0x8000,win=(GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000;
        if(alt||win)return false;
        return p.shortcut==0?(key==VK_SPACE&&ctrl&&shift):p.shortcut==1?(key==VK_F9&&!ctrl&&!shift):false;
    }
    bool modifiers() {return ((GetKeyState(VK_CONTROL)|GetKeyState(VK_MENU)|GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)!=0;}
    bool modifierKey(WPARAM key) {return key==VK_SHIFT||key==VK_CONTROL||key==VK_MENU||key==VK_LWIN||key==VK_RWIN||key==VK_LSHIFT||key==VK_RSHIFT||key==VK_LCONTROL||key==VK_RCONTROL||key==VK_LMENU||key==VK_RMENU;}
    HRESULT insertCharacter(TfEditCookie cookie,wchar_t character,BOOL* eaten){
        Ptr<ITfInsertAtSelection> insertion;auto hr=query(context_.get(),insertion);
        if(FAILED(hr))return hr;
        Ptr<ITfRange> inserted;hr=insertion->InsertTextAtSelection(cookie,0,&character,1,inserted.put());
        if(FAILED(hr))return hr;
        applied_=true;*eaten=TRUE;if(!inserted)return E_FAIL;
        hr=inserted->Collapse(cookie,TF_ANCHOR_END);if(FAILED(hr))return hr;
        TF_SELECTION selection{inserted.get(),{TF_AE_NONE,FALSE}};
        return context_->SetSelection(cookie,1,&selection);
    }
    HRESULT key(TfEditCookie cookie,ITfContext* context,WPARAM vk,LPARAM param,BOOL* eaten) {
        *eaten=FALSE;auto p=currentPolicy();
        fieldSupported_=safe(cookie,context)?1:0;
        if(!fieldSupported_||p.mode==Mode::Paused) {report(fault_?6:!p.enabled?5:7);finish(cookie);session_->reset();return S_OK;}
        report(static_cast<LONG>(p.mode)+1);
        if(switching(vk,p)&&!session_->empty()){session_->cycle();auto hr=render(cookie);*eaten=SUCCEEDED(hr);return hr;}
        if(modifiers()||vk==VK_ESCAPE||vk==VK_TAB||vk==VK_LEFT||vk==VK_RIGHT||vk==VK_UP||vk==VK_DOWN||vk==VK_HOME||vk==VK_END||vk==VK_DELETE||vk==VK_HANGUL) {
            auto hr=finish(cookie);session_->resetContext();protectedToken_=false;return hr;
        }
        if(vk==VK_BACK&&!session_->empty()) {session_->backspace(p.mode,p.strict);auto hr=render(cookie);*eaten=SUCCEEDED(hr);return hr;}
        if(vk==VK_RETURN) {bool had=!session_->empty();auto hr=finish(cookie);*eaten=SUCCEEDED(hr)&&had&&p.enter;protectedToken_=false;session_->resetContext();return hr;}
        if(vk==VK_SPACE) {protectedToken_=false;auto hr=finish(cookie);return FAILED(hr)?hr:insertCharacter(cookie,L' ',eaten);}
        char c=character(vk,param);
        if(protectedToken_)return S_OK;
        if(c && ((c>='a'&&c<='z')||(c>='A'&&c<='Z'))) {
            if(session_->keys().size()>=Session::maxKeys){auto hr=finish(cookie);if(FAILED(hr))return hr;}
            if(!session_->append(c,p.mode,p.strict))return E_FAIL;
            if(shared_)InterlockedExchange(&shared_->bufferLength,static_cast<LONG>(session_->keys().size()));
            auto hr=render(cookie);*eaten=SUCCEEDED(hr);return hr;
        }
        if(c) {
            bool protect=(c>='0'&&c<='9')||c=='@'||c=='_'||c=='/'||c=='\\'||c==':'||c=='\''||c=='-';
            if(protect) {
                if(!session_->empty()) {
                    // Restore the raw token before committing the structural delimiter.
                    for(int i=0;i<3&&session_->display().language!=Language::English;++i)session_->cycle();
                    auto hr=render(cookie);if(FAILED(hr))return hr;
                }
                protectedToken_=true;
            }
            auto hr=finish(cookie);if(c=='.'||c=='!'||c=='?')session_->resetContext();return FAILED(hr)?hr:insertCharacter(cookie,static_cast<wchar_t>(c),eaten);
        }
        return finish(cookie);
    }
public:
    ~Service(){Deactivate();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==IID_IUnknown||id==__uuidof(ITfTextInputProcessor)||id==__uuidof(ITfTextInputProcessorEx))*out=static_cast<ITfTextInputProcessorEx*>(this);
        else if(id==__uuidof(ITfKeyEventSink))*out=static_cast<ITfKeyEventSink*>(this);
        else if(id==__uuidof(ITfCompositionSink))*out=static_cast<ITfCompositionSink*>(this);
        else if(id==__uuidof(ITfThreadMgrEventSink))*out=static_cast<ITfThreadMgrEventSink*>(this);
        else if(id==__uuidof(ITfTextEditSink))*out=static_cast<ITfTextEditSink*>(this);
        else if(id==__uuidof(ITfDisplayAttributeProvider))*out=static_cast<ITfDisplayAttributeProvider*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* mgr,TfClientId client) override{return ActivateEx(mgr,client,0);}
    HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* mgr,TfClientId client,DWORD flags) override {
        if(!mgr)return E_INVALIDARG;
        try {
            manager_=Ptr<ITfThreadMgr>(mgr);client_=client;secure_=(flags&TF_TMF_SECUREMODE)!=0;
            engine_=loadEngine(module);if(!engine_->ready()){Deactivate();return E_FAIL;}session_=std::make_unique<Session>(*engine_);
            path_=lowerPath(modulePath());keyboard_=LoadKeyboardLayoutW(L"00000409",KLF_NOTELLSHELL);
            Ptr<ITfKeystrokeMgr> keys;auto hr=query(mgr,keys);if(FAILED(hr)){Deactivate();return hr;}
            hr=keys->AdviseKeyEventSink(client,static_cast<ITfKeyEventSink*>(this),TRUE);if(FAILED(hr)){Deactivate();return hr;}
            Ptr<ITfSource> source;if(SUCCEEDED(query(mgr,source)))source->AdviseSink(__uuidof(ITfThreadMgrEventSink),static_cast<ITfThreadMgrEventSink*>(this),&threadCookie_);
            Ptr<ITfCategoryMgr> category;if(SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfCategoryMgr),reinterpret_cast<void**>(category.put()))))category->RegisterGUID(displayId,&attribute_);
            connect();WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=module;wc.lpszClassName=L"Atypoi.IME.State";RegisterClassW(&wc);
            timer_=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,this);if(timer_)SetTimer(timer_,1,500,nullptr);
            Ptr<ITfDocumentMgr> doc;Ptr<ITfContext> context;if(SUCCEEDED(mgr->GetFocus(doc.put()))&&doc&&SUCCEEDED(doc->GetTop(context.put())))bind(context.get());
            tick();return S_OK;
        }catch(...){Deactivate();return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE Deactivate() override {
        qtInput_.reset();
        if(timer_){DestroyWindow(timer_);timer_=nullptr;}
        finishAsync();unbind();
        if(manager_) {
            Ptr<ITfKeystrokeMgr> keys;if(SUCCEEDED(query(manager_.get(),keys)))keys->UnadviseKeyEventSink(client_);
            Ptr<ITfSource> source;if(threadCookie_!=TF_INVALID_COOKIE&&SUCCEEDED(query(manager_.get(),source)))source->UnadviseSink(threadCookie_);
        }
        threadCookie_=TF_INVALID_COOKIE;manager_.reset();client_=TF_CLIENTID_NULL;
        if(shared_){UnmapViewOfFile(shared_);shared_=nullptr;}if(mapping_){CloseHandle(mapping_);mapping_=nullptr;}
        session_.reset();engine_.reset();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override {if(!foreground){if(shared_)InterlockedIncrement(&shared_->focusLosses);finishAsync();if(session_)session_->reset();}else tick();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext* context,WPARAM vk,LPARAM param,BOOL* eaten) override {
        if(shared_)InterlockedIncrement(&shared_->testedKeys);
        if(!eaten)return E_POINTER;*eaten=FALSE;if(!session_||secure_||modifierKey(vk))return S_OK;
        TF_STATUS status{};if(!context||FAILED(context->GetStatus(&status))||(status.dwDynamicFlags&TF_SD_READONLY)){fieldSupported_=0;return S_OK;}
        auto p=currentPolicy();if(!session_->empty()){*eaten=TRUE;return S_OK;}
        if(protectedToken_&&(vk==VK_SPACE||vk==VK_RETURN||vk==VK_ESCAPE||vk==VK_TAB||vk==VK_LEFT||vk==VK_RIGHT||modifiers())){*eaten=TRUE;return S_OK;}
        if(p.mode==Mode::Paused||p.mode==Mode::English||modifiers()||protectedToken_)return S_OK;
        qtInput_.update(GetFocus());
        // Hosts such as Chromium suppress WM_CHAR once TestKeyDown accepts a key.
        // Reject unsupported fields here, before claiming the physical keystroke.
        bool supported=false;
        auto hr=request(context,client_,[&](TfEditCookie cookie){supported=safe(cookie,context);return S_OK;},TF_ES_SYNC|TF_ES_READ);
        fieldSupported_=SUCCEEDED(hr)&&supported?1:0;
        if(!fieldSupported_){report(7);return S_OK;}
        char c=character(vk,param);*eaten=(c>='a'&&c<='z')||(c>='A'&&c<='Z');return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext* context,WPARAM vk,LPARAM param,BOOL* eaten) override {
        if(shared_)InterlockedIncrement(&shared_->handledKeys);
        if(!eaten)return E_POINTER;*eaten=FALSE;if(!session_||modifierKey(vk))return S_OK;
        try {
            qtInput_.update(GetFocus());
            bind(context);if(GetTickCount()-lastInput_>30000)session_->resetContext();lastInput_=GetTickCount();
            auto before=*session_;editing_=true;applied_=false;
            auto hr=request(context,client_,[&](TfEditCookie cookie){return key(cookie,context,vk,param,eaten);});editing_=false;
            if(shared_)InterlockedExchange(&shared_->diagnosticHr,hr);
            if(FAILED(hr)){*eaten=applied_?TRUE:FALSE;if(!applied_)*session_=before;finishAsync();session_->reset();fault_=true;report(6);}
            if(vk<256)consumed_[vk]=*eaten!=FALSE;
        }catch(...){editing_=false;*eaten=applied_?TRUE:FALSE;finishAsync();if(session_)session_->reset();fault_=true;report(6);}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext*,WPARAM key,LPARAM,BOOL* eaten) override {if(!eaten)return E_POINTER;*eaten=key<256&&consumed_[key];return S_OK;}
    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext*,WPARAM key,LPARAM,BOOL* eaten) override {if(!eaten)return E_POINTER;*eaten=key<256&&consumed_[key];if(key<256)consumed_[key]=false;return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext*,REFGUID,BOOL* eaten) override {if(!eaten)return E_POINTER;*eaten=FALSE;return S_OK;}
    HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie,ITfComposition* composition) override {if(shared_)InterlockedIncrement(&shared_->terminations);if(composition_.get()==composition){composition_.reset();if(session_)session_->reset();}return S_OK;}
    HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr*) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr*) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnSetFocus(ITfDocumentMgr* doc,ITfDocumentMgr*) override {Ptr<ITfContext> context;if(doc)doc->GetTop(context.put());bind(context.get());return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext* context) override {bind(context);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext* context) override {if(sameObject(context_.get(),context)){finishAsync();unbind();if(session_)session_->reset();}return S_OK;}
    HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext* context,TfEditCookie cookie,ITfEditRecord* record) override {
        if(editing_||!composition_||!sameObject(context_.get(),context))return S_OK;
        BOOL changed=FALSE;if(FAILED(record->GetSelectionStatus(&changed))||!changed)return S_OK;
        TF_SELECTION selection{};ULONG n=0;Ptr<ITfRange> range;
        if(SUCCEEDED(context->GetSelection(cookie,TF_DEFAULT_SELECTION,1,&selection,&n))&&n) {
            Ptr<ITfRange> selected(selection.range,false);BOOL empty=FALSE;LONG position=1;
            selected->IsEmpty(cookie,&empty);
            if(SUCCEEDED(composition_->GetRange(range.put())))selected->CompareStart(cookie,range.get(),TF_ANCHOR_END,&position);
            if(!empty||position!=0){if(shared_)InterlockedIncrement(&shared_->selectionEnds);finishAsync();session_->reset();protectedToken_=false;}
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) override {if(!out)return E_POINTER;*out=new(std::nothrow) Attributes;return *out?S_OK:E_OUTOFMEMORY;}
    HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(REFGUID id,ITfDisplayAttributeInfo** out) override {if(!out)return E_POINTER;*out=nullptr;if(id!=displayId)return E_INVALIDARG;*out=new(std::nothrow) Attribute;return *out?S_OK:E_OUTOFMEMORY;}
};
class Factory final:public IClassFactory,Counted {
    LONG refs_=1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IClassFactory)return E_NOINTERFACE;*out=static_cast<IClassFactory*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs_);}
    ULONG STDMETHODCALLTYPE Release() override{auto n=InterlockedDecrement(&refs_);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;auto service=new(std::nothrow) Service;if(!service)return E_OUTOFMEMORY;auto hr=service->QueryInterface(id,out);service->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override{if(lock)InterlockedIncrement(&objects);else InterlockedDecrement(&objects);return S_OK;}
};
HRESULT registration(bool install) {
    auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return initialized;
    Ptr<ITfInputProcessorProfiles> profiles;Ptr<ITfCategoryMgr> categories;
    auto hr=CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfInputProcessorProfiles),reinterpret_cast<void**>(profiles.put()));
    if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_TF_CategoryMgr,nullptr,CLSCTX_INPROC_SERVER,__uuidof(ITfCategoryMgr),reinterpret_cast<void**>(categories.put()));
    auto key=L"Software\\Classes\\CLSID\\"+guidString(serviceId);
    if(SUCCEEDED(hr)&&install) {
        HKEY handle=nullptr;auto path=modulePath(module);auto error=RegCreateKeyExW(HKEY_LOCAL_MACHINE,(key+L"\\InprocServer32").c_str(),0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&handle,nullptr);
        if(error!=ERROR_SUCCESS)hr=HRESULT_FROM_WIN32(error);
        else {error=RegSetValueExW(handle,nullptr,0,REG_SZ,reinterpret_cast<const BYTE*>(path.c_str()),static_cast<DWORD>((path.size()+1)*2));if(error==ERROR_SUCCESS)error=RegSetValueExW(handle,L"ThreadingModel",0,REG_SZ,reinterpret_cast<const BYTE*>(L"Apartment"),20);RegCloseKey(handle);hr=HRESULT_FROM_WIN32(error);}
        if(SUCCEEDED(hr))hr=profiles->Register(serviceId);
        constexpr wchar_t description[]=L"Atypoi 한영 자동 입력";
        if(SUCCEEDED(hr))hr=profiles->AddLanguageProfile(serviceId,languageId,profileId,description,static_cast<ULONG>(wcslen(description)),path.c_str(),static_cast<ULONG>(path.size()),0);
        if(SUCCEEDED(hr))hr=categories->RegisterCategory(serviceId,GUID_TFCAT_TIP_KEYBOARD,serviceId);
        if(SUCCEEDED(hr))hr=categories->RegisterCategory(serviceId,GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,serviceId);
        // Remove a capability accidentally advertised by the intermediate dev build.
        constexpr GUID legacySecureCategory={0x49d2f9ce,0x1f5e,0x11d7,{0xa6,0xd3,0x00,0x06,0x5b,0x84,0x43,0x5c}};
        categories->UnregisterCategory(serviceId,legacySecureCategory,serviceId);
        for(auto category:{immersiveCategory,systrayCategory,uiLessCategory})if(SUCCEEDED(hr))hr=categories->RegisterCategory(serviceId,category,serviceId);
        if(FAILED(hr)){for(auto category:{GUID_TFCAT_TIP_KEYBOARD,GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,immersiveCategory,systrayCategory,uiLessCategory})categories->UnregisterCategory(serviceId,category,serviceId);profiles->Unregister(serviceId);RegDeleteTreeW(HKEY_LOCAL_MACHINE,key.c_str());}
    } else if(SUCCEEDED(hr)) {
        categories->UnregisterCategory(serviceId,GUID_TFCAT_TIP_KEYBOARD,serviceId);
        categories->UnregisterCategory(serviceId,GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,serviceId);
        for(auto category:{immersiveCategory,systrayCategory,uiLessCategory})categories->UnregisterCategory(serviceId,category,serviceId);
        profiles->RemoveLanguageProfile(serviceId,languageId,profileId);
        hr=profiles->Unregister(serviceId);RegDeleteTreeW(HKEY_LOCAL_MACHINE,key.c_str());
    }
    profiles.reset();categories.reset();if(SUCCEEDED(initialized))CoUninitialize();return hr;
}
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID cls,REFIID id,void** out){if(!out)return E_POINTER;*out=nullptr;if(cls!=serviceId)return CLASS_E_CLASSNOTAVAILABLE;auto f=new(std::nothrow) Factory;if(!f)return E_OUTOFMEMORY;auto hr=f->QueryInterface(id,out);f->Release();return hr;}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return objects?S_FALSE:S_OK;}
extern "C" __declspec(dllexport) HRESULT WINAPI DllRegisterServer(){try{return registration(true);}catch(...){return E_FAIL;}}
extern "C" __declspec(dllexport) HRESULT WINAPI DllUnregisterServer(){try{return registration(false);}catch(...){return E_FAIL;}}
