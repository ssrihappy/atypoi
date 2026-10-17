#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>

// Shared, dependency-free visual language for the tray and installer.
namespace atypoi::ui {
inline bool highContrast(){HIGHCONTRASTW hc{};hc.cbSize=sizeof hc;SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof hc,&hc,0);return (hc.dwFlags&HCF_HIGHCONTRASTON)!=0;}
inline COLORREF background(){return highContrast()?GetSysColor(COLOR_WINDOW):RGB(246,247,251);}
inline COLORREF surface(){return GetSysColor(COLOR_WINDOW);}
inline COLORREF ink(){return highContrast()?GetSysColor(COLOR_WINDOWTEXT):RGB(29,33,49);}
inline COLORREF muted(){return highContrast()?GetSysColor(COLOR_WINDOWTEXT):RGB(104,111,130);}
inline COLORREF accent(){return highContrast()?GetSysColor(COLOR_HIGHLIGHT):RGB(103,80,228);}
inline COLORREF line(){return highContrast()?GetSysColor(COLOR_WINDOWTEXT):RGB(222,225,235);}
inline int px(HWND w,int n){auto dpi=GetDpiForWindow(w);return MulDiv(n,dpi?dpi:96,96);}
inline HFONT font(int size,int weight=FW_NORMAL,UINT dpi=96,const wchar_t* face=L"맑은 고딕") {return CreateFontW(-MulDiv(size,dpi,96),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);}
inline void round(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius=12){auto b=CreateSolidBrush(fill);auto p=CreatePen(PS_SOLID,1,border);auto ob=SelectObject(dc,b),op=SelectObject(dc,p);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);}
inline void card(HWND w,HDC dc,int x,int y,int width,int height){RECT r{px(w,x),px(w,y),px(w,x+width),px(w,y+height)};round(dc,r,surface(),line(),px(w,20));}
inline void clear(HWND w,HDC dc){RECT r;GetClientRect(w,&r);auto b=CreateSolidBrush(background());FillRect(dc,&r,b);DeleteObject(b);}
inline HICON icon(HINSTANCE instance,int size){return static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,size,size,LR_DEFAULTCOLOR));}
inline void mark(HWND w,HDC dc,int x,int y,int size){auto i=icon(reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(w,GWLP_HINSTANCE)),px(w,size));if(i){DrawIconEx(dc,px(w,x),px(w,y),i,px(w,size),px(w,size),0,nullptr,DI_NORMAL);DestroyIcon(i);}}
inline LRESULT color(WPARAM wp,LPARAM lp){auto dc=reinterpret_cast<HDC>(wp);auto child=reinterpret_cast<HWND>(lp);auto bg=GetPropW(child,L"Atypoi.Outer")?background():surface();SetBkColor(dc,bg);SetTextColor(dc,GetPropW(child,L"Atypoi.Muted")?muted():ink());SetDCBrushColor(dc,bg);return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));}
inline void outer(HWND w){SetPropW(w,L"Atypoi.Outer",reinterpret_cast<HANDLE>(1));}
inline void secondary(HWND w){SetPropW(w,L"Atypoi.Muted",reinterpret_cast<HANDLE>(1));}
inline LRESULT CALLBACK buttonProc(HWND w,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(msg==WM_MOUSEMOVE&&!GetPropW(w,L"Atypoi.Hover")){SetPropW(w,L"Atypoi.Hover",reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,w,0};TrackMouseEvent(&track);InvalidateRect(w,nullptr,TRUE);}
    if(msg==WM_MOUSELEAVE){RemovePropW(w,L"Atypoi.Hover");InvalidateRect(w,nullptr,TRUE);}
    if(msg==WM_NCDESTROY){RemoveWindowSubclass(w,buttonProc,1);RemovePropW(w,L"Atypoi.Hover");}
    return DefSubclassProc(w,msg,wp,lp);
}
inline void styleButton(HWND w,bool primary=false){if(primary)SetPropW(w,L"Atypoi.Primary",reinterpret_cast<HANDLE>(1));SetWindowSubclass(w,buttonProc,1,0);}
inline void selected(HWND w,bool yes){if(yes)SetPropW(w,L"Atypoi.Selected",reinterpret_cast<HANDLE>(1));else RemovePropW(w,L"Atypoi.Selected");InvalidateRect(w,nullptr,TRUE);}
inline bool drawButton(LPARAM lp){auto d=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(d->CtlType!=ODT_BUTTON)return false;
    bool active=GetPropW(d->hwndItem,L"Atypoi.Primary")||GetPropW(d->hwndItem,L"Atypoi.Selected");bool disabled=d->itemState&ODS_DISABLED;
    auto fill=active?accent():surface();if(disabled)fill=background();else if(!active&&(GetPropW(d->hwndItem,L"Atypoi.Hover")||(d->itemState&ODS_SELECTED)))fill=background();
    RECT r=d->rcItem;SetDCBrushColor(d->hDC,background());FillRect(d->hDC,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));round(d->hDC,r,fill,active&&!disabled?fill:line(),px(d->hwndItem,12));
    wchar_t caption[256];GetWindowTextW(d->hwndItem,caption,256);auto old=SelectObject(d->hDC,reinterpret_cast<HFONT>(SendMessageW(d->hwndItem,WM_GETFONT,0,0)));SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,disabled?GetSysColor(COLOR_GRAYTEXT):active?GetSysColor(COLOR_HIGHLIGHTTEXT):ink());DrawTextW(d->hDC,caption,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if((d->itemState&ODS_FOCUS)&&!(d->itemState&ODS_NOFOCUSRECT)){InflateRect(&r,-4,-4);DrawFocusRect(d->hDC,&r);}SelectObject(d->hDC,old);return true;
}
}
