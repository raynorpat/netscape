#include "classic.h"
#include <map>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "msimg32.lib")

static int window_dpi(CWnd *window)
{
    typedef UINT (WINAPI *DpiFunction)(HWND);
    static DpiFunction get_dpi = (DpiFunction)GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow");
    return window && window->GetSafeHwnd() && get_dpi ? get_dpi(window->GetSafeHwnd()) : 96;
}

int ClassicUI::Scale(CWnd *window, int value) { return MulDiv(value, window_dpi(window), 96); }

CFont *ClassicUI::Font(CWnd *window)
{
    static std::map<int, std::unique_ptr<CFont>> fonts;
    int dpi = window_dpi(window);
    auto &font = fonts[dpi];
    if (!font) {
        font.reset(new CFont);
        font->CreateFont(-MulDiv(8, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         NONANTIALIASED_QUALITY, DEFAULT_PITCH, "MS Sans Serif");
    }
    return font.get();
}

static BOOL CALLBACK classic_child(HWND window, LPARAM font)
{
    SendMessage(window, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}

void ClassicUI::Apply(CWnd *window)
{
    if (!window || !window->GetSafeHwnd()) return;
    HFONT font = (HFONT)Font(window)->GetSafeHandle();
    classic_child(window->GetSafeHwnd(), (LPARAM)font);
    EnumChildWindows(window->GetSafeHwnd(), classic_child, (LPARAM)font);
}

void ClassicUI::SetWindowIcon(CWnd *window,UINT resource)
{
    for(int icon_size_index=0;icon_size_index<2;++icon_size_index) {
        HICON icon=(HICON)LoadImage(AfxGetInstanceHandle(),MAKEINTRESOURCE(resource),IMAGE_ICON,
            GetSystemMetrics(icon_size_index?SM_CXSMICON:SM_CXICON),GetSystemMetrics(icon_size_index?SM_CYSMICON:SM_CYICON),LR_SHARED);
        if(icon) window->SetIcon(icon,icon_size_index?FALSE:TRUE);
    }
}

HBRUSH ClassicUI::ControlColor(CDC *dc, CWnd *, UINT type)
{
    bool paper = type == CTLCOLOR_EDIT || type == CTLCOLOR_LISTBOX;
    dc->SetTextColor(GetSysColor(paper ? COLOR_WINDOWTEXT : COLOR_BTNTEXT));
    dc->SetBkColor(GetSysColor(paper ? COLOR_WINDOW : COLOR_BTNFACE));
    return GetSysColorBrush(paper ? COLOR_WINDOW : COLOR_BTNFACE);
}

void ClassicButton::SetBitmapResource(UINT resource)
{
    artwork.DeleteObject();
    artwork.LoadBitmap(resource);
    disabled_artwork.DeleteObject(); disabled_artwork.LoadBitmap(resource+100);
    disabled_mask.DeleteObject();
    if(artwork.GetSafeHandle()) {
        BITMAP bitmap;artwork.GetBitmap(&bitmap);
        CDC source,mask;source.CreateCompatibleDC(nullptr);mask.CreateCompatibleDC(nullptr);
        if(disabled_mask.CreateBitmap(bitmap.bmWidth,bitmap.bmHeight,1,1,nullptr)) {
            CBitmap *old_source=source.SelectObject(&artwork),*old_mask=mask.SelectObject(&disabled_mask);
            COLORREF old_background=source.SetBkColor(ClassicUI::ArtworkKey);
            mask.BitBlt(0,0,bitmap.bmWidth,bitmap.bmHeight,&source,0,0,SRCCOPY);
            source.SetBkColor(RGB(255,255,255));
            mask.BitBlt(0,0,bitmap.bmWidth,bitmap.bmHeight,&source,0,0,SRCPAINT);
            source.SetBkColor(old_background);source.SelectObject(old_source);mask.SelectObject(old_mask);
        }
    }
    if (GetSafeHwnd()) Invalidate();
}

void ClassicButton::DrawItem(LPDRAWITEMSTRUCT item)
{
    CDC dc; dc.Attach(item->hDC);
    CRect area(item->rcItem), inside(area);
    bool down = (item->itemState & ODS_SELECTED) != 0;
    bool disabled = (item->itemState & ODS_DISABLED) != 0;
    dc.FillSolidRect(area, ClassicUI::Face());
    dc.DrawFrameControl(area,DFC_BUTTON,DFCS_BUTTONPUSH | (down?DFCS_PUSHED:0) | (disabled?DFCS_INACTIVE:0));
    inside.DeflateRect(3,2);
    if (down) inside.OffsetRect(1,1);
    if (artwork.GetSafeHandle() && presentation!=1) {
        CBitmap *image = disabled && disabled_artwork.GetSafeHandle() ? &disabled_artwork : &artwork;
        BITMAP bitmap; image->GetBitmap(&bitmap);
        int width=ClassicUI::Scale(this,bitmap.bmWidth),height=ClassicUI::Scale(this,bitmap.bmHeight);
        int x = inside.CenterPoint().x - width / 2, y = presentation==0 ? inside.CenterPoint().y-height/2 : inside.top + ClassicUI::Scale(this, 1);
        CDC memory; memory.CreateCompatibleDC(&dc);
        CBitmap *old = memory.SelectObject(image);
        if(disabled && disabled_mask.GetSafeHandle()) dc.DrawState(CPoint(x,y),CSize(width,height),&disabled_mask,DST_BITMAP|DSS_DISABLED);
        else TransparentBlt(dc,x,y,width,height,memory,0,0,bitmap.bmWidth,bitmap.bmHeight,ClassicUI::ArtworkKey);
        memory.SelectObject(old);
        inside.top = y + height + ClassicUI::Scale(this, 1);
    }
    CString caption; GetWindowText(caption);
    CFont *old_font = dc.SelectObject(ClassicUI::Font(this));
    dc.SetBkMode(TRANSPARENT);
    if (disabled && presentation!=0) {
        CRect shadow(inside); shadow.OffsetRect(1,1);
        dc.SetTextColor(GetSysColor(COLOR_3DHIGHLIGHT));
        dc.DrawText(caption, shadow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    dc.SetTextColor(GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
    if(presentation!=0) dc.DrawText(caption, inside, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS) { CRect focus(area); focus.DeflateRect(3,3); dc.DrawFocusRect(focus); }
    dc.SelectObject(old_font); dc.Detach();
}

BEGIN_MESSAGE_MAP(ClassicDialog,CDialog)
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
END_MESSAGE_MAP()
BOOL ClassicDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    if(GetDlgItem(IDOK)) { accept.SubclassDlgItem(IDOK,this);accept.SetButtonStyle(BS_OWNERDRAW); }
    if(GetDlgItem(IDCANCEL)) { cancel.SubclassDlgItem(IDCANCEL,this);cancel.SetButtonStyle(BS_OWNERDRAW); }
    ClassicUI::Apply(this);return TRUE;
}
BOOL ClassicDialog::OnEraseBkgnd(CDC *dc)
{
    CRect area;GetClientRect(area);dc->FillSolidRect(area,ClassicUI::Face());return TRUE;
}
HBRUSH ClassicDialog::OnCtlColor(CDC *dc,CWnd *window,UINT type) { return ClassicUI::ControlColor(dc,window,type); }

BOOL ClassicDialog::PreTranslateMessage(MSG *message)
{
    if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN) {
        SendMessage(WM_COMMAND,GetFocus()==&cancel ? IDCANCEL : IDOK);return TRUE;
    }
    return CDialog::PreTranslateMessage(message);
}
