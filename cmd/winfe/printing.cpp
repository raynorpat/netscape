/* Native printing retains the original LO/GDI drawing commands in EMFs. */
#include "printing.h"
#include "classic.h"
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace {
enum { PRINT_COMMAND = 57607, SETUP_COMMAND = 57605, PREVIEW_COMMAND = 57609,
       PRINT_PREVIOUS = 4740, PRINT_NEXT, PRINT_ZOOM, PRINT_START, PRINT_LABEL,
       SETUP_PAPER, SETUP_ORIENTATION, SETUP_MARGIN, PRINT_TEST = 4770 };

/* Hundredths of a millimetre, matching PAGESETUPDLG and the EMF frame.
 * Letter and margin defaults follow XL_InitializePrintSetup. */
struct PageSettings {
    int width = 21590, height = 27940;
    int left = 1905, right = 1905, top = 2540, bottom = 2540;
    int orientation = DMORIENT_PORTRAIT, paper = DMPAPER_LETTER;
};
struct PageGeometry {
    int width = 0, height = 0, left = 0, top = 0;
    int content_width = 0, content_height = 0, logical_height = 0, count = 0;
};
struct Pages {
    std::vector<HENHMETAFILE> files;
    std::vector<int> tops, heights;
    PageGeometry geometry;
    ~Pages() { clear(); }
    void clear() { for (auto page : files) DeleteEnhMetaFile(page); files.clear(); tops.clear(); heights.clear(); }
};
struct Printer {
    HGLOBAL mode = nullptr, names = nullptr;
    ~Printer() { if (mode) GlobalFree(mode); if (names) GlobalFree(names); }
};
static Printer saved_printer;

static bool valid_settings(const PageSettings &s)
{
    return s.width >= 2500 && s.width <= 200000 && s.height >= 2500 && s.height <= 200000 &&
        s.left >= 0 && s.left <= 200000 && s.right >= 0 && s.right <= 200000 &&
        s.top >= 0 && s.top <= 200000 && s.bottom >= 0 && s.bottom <= 200000 &&
        s.left + s.right < (s.orientation == DMORIENT_LANDSCAPE ? s.height : s.width) - 500 &&
        s.top + s.bottom < (s.orientation == DMORIENT_LANDSCAPE ? s.width : s.height) - 500 &&
        (s.orientation == DMORIENT_PORTRAIT || s.orientation == DMORIENT_LANDSCAPE) &&
        s.paper > 0 && s.paper <= 32767;
}

static std::string settings_path() { return std::string(WFE_ProfileDirectory()) + "\\printing.ini"; }

static PageSettings load_settings(const char *supplied_path=nullptr)
{
    PageSettings s;
    std::string path = supplied_path ? supplied_path : settings_path();
    struct Value { const char *key; int *value; } values[] = {
        {"PaperWidth", &s.width}, {"PaperHeight", &s.height}, {"Left", &s.left}, {"Right", &s.right},
        {"Top", &s.top}, {"Bottom", &s.bottom}, {"Orientation", &s.orientation}, {"Paper", &s.paper}
    };
    for (const auto &value : values)
        *value.value = (int)GetPrivateProfileIntA("Printing", value.key, *value.value, path.c_str());
    return valid_settings(s) ? s : PageSettings();
}

static bool save_settings(const PageSettings &s,const char *supplied_path=nullptr)
{
    if (!valid_settings(s)) return false;
    std::string path = supplied_path ? supplied_path : settings_path();
    size_t slash=path.find_last_of("\\/");
    if(slash==std::string::npos) return false;
    std::string directory=path.substr(0,slash+1);
    char temporary[MAX_PATH];
    if (!GetTempFileNameA(directory.c_str(), "npr", 0, temporary)) return false;
    char buffer[512];
    int length = sprintf_s(buffer, "[Printing]\r\nPaperWidth=%d\r\nPaperHeight=%d\r\n"
        "Left=%d\r\nRight=%d\r\nTop=%d\r\nBottom=%d\r\nOrientation=%d\r\nPaper=%d\r\n",
        s.width, s.height, s.left, s.right, s.top, s.bottom, s.orientation, s.paper);
    HANDLE file = CreateFileA(temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    bool good = file != INVALID_HANDLE_VALUE && length > 0 &&
        WriteFile(file, buffer, (DWORD)length, &written, nullptr) && written == (DWORD)length && FlushFileBuffers(file);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (good) good = MoveFileExA(temporary, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!good) DeleteFileA(temporary);
    else WritePrivateProfileStringA(nullptr,nullptr,nullptr,path.c_str());
    return good;
}

static bool same_settings(const PageSettings &a,const PageSettings &b)
{
    return a.width==b.width && a.height==b.height && a.left==b.left && a.right==b.right &&
        a.top==b.top && a.bottom==b.bottom && a.orientation==b.orientation && a.paper==b.paper;
}

static HGLOBAL copy_global(HGLOBAL original)
{
    SIZE_T length = original ? GlobalSize(original) : 0;
    if (!length) return nullptr;
    void *source = GlobalLock(original);
    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, length);
    void *destination = copy ? GlobalLock(copy) : nullptr;
    if (source && destination) memcpy(destination, source, length);
    if (destination) GlobalUnlock(copy);
    if (source) GlobalUnlock(original);
    if (!source || !destination) { if (copy) GlobalFree(copy); return nullptr; }
    return copy;
}

static void release_dialog(PRINTDLGA &dialog)
{
    if (dialog.hDC) DeleteDC(dialog.hDC);
    if (dialog.hDevMode) GlobalFree(dialog.hDevMode);
    if (dialog.hDevNames) GlobalFree(dialog.hDevNames);
    dialog.hDC = nullptr; dialog.hDevMode = dialog.hDevNames = nullptr;
}

static void remember_printer(HGLOBAL mode, HGLOBAL names)
{
    HGLOBAL next_mode = copy_global(mode), next_names = copy_global(names);
    if (mode && !next_mode) { if (next_names) GlobalFree(next_names); return; }
    if (names && !next_names) { if (next_mode) GlobalFree(next_mode); return; }
    if (saved_printer.mode) GlobalFree(saved_printer.mode);
    if (saved_printer.names) GlobalFree(saved_printer.names);
    saved_printer.mode = next_mode; saved_printer.names = next_names;
}

static void apply_settings(HGLOBAL memory, const PageSettings &s)
{
    DEVMODEA *mode = memory ? (DEVMODEA *)GlobalLock(memory) : nullptr;
    if (mode) {
        mode->dmFields |= DM_ORIENTATION | DM_PAPERSIZE;
        mode->dmOrientation = (short)s.orientation; mode->dmPaperSize = (short)s.paper;
        if (s.paper == DMPAPER_USER) {
            mode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
            mode->dmPaperWidth = (short)(s.width / 10); mode->dmPaperLength = (short)(s.height / 10);
        } else mode->dmFields &= ~(DM_PAPERWIDTH | DM_PAPERLENGTH);
        GlobalUnlock(memory);
    }
}

static PRINTDLGA printer_dialog(HWND owner, const PageSettings &settings)
{
    PRINTDLGA dialog = {};
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = owner;
    if (saved_printer.mode && saved_printer.names) {
        dialog.hDevMode = copy_global(saved_printer.mode); dialog.hDevNames = copy_global(saved_printer.names);
    } else {
        dialog.Flags = PD_RETURNDEFAULT | PD_NOWARNING;
        PrintDlgA(&dialog);
    }
    apply_settings(dialog.hDevMode, settings);
    dialog.Flags = PD_RETURNDC | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE | PD_HIDEPRINTTOFILE;
    dialog.nMinPage = 1; dialog.nMaxPage = 65535;
    dialog.nFromPage = 1; dialog.nToPage = 65535; dialog.nCopies = 1;
    return dialog;
}

static bool page_geometry(const PageSettings &settings, int document_width, int document_height,
                          HDC printer, PageGeometry &page)
{
    if (!valid_settings(settings) || document_width <= 0 || document_height <= 0) return false;
    page.width = settings.orientation == DMORIENT_LANDSCAPE ? settings.height : settings.width;
    page.height = settings.orientation == DMORIENT_LANDSCAPE ? settings.width : settings.height;
    page.left = settings.left; page.top = settings.top;
    int right = settings.right, bottom = settings.bottom;
    if (printer) {
        int dx = GetDeviceCaps(printer, LOGPIXELSX), dy = GetDeviceCaps(printer, LOGPIXELSY);
        int pw = GetDeviceCaps(printer, PHYSICALWIDTH), ph = GetDeviceCaps(printer, PHYSICALHEIGHT);
        int ox = GetDeviceCaps(printer, PHYSICALOFFSETX), oy = GetDeviceCaps(printer, PHYSICALOFFSETY);
        int rw = GetDeviceCaps(printer, HORZRES), rh = GetDeviceCaps(printer, VERTRES);
        if (dx <= 0 || dy <= 0 || pw <= 0 || ph <= 0 || rw <= 0 || rh <= 0) return false;
        page.width = MulDiv(pw, 2540, dx); page.height = MulDiv(ph, 2540, dy);
        page.left = (std::max)(page.left, MulDiv(ox, 2540, dx));
        page.top = (std::max)(page.top, MulDiv(oy, 2540, dy));
        right = (std::max)(right, MulDiv(pw - ox - rw, 2540, dx));
        bottom = (std::max)(bottom, MulDiv(ph - oy - rh, 2540, dy));
    }
    page.content_width = page.width - page.left - right;
    page.content_height = page.height - page.top - bottom;
    if (page.content_width <= 0 || page.content_height <= 0) return false;
    __int64 logical_height = (__int64)document_width * page.content_height / page.content_width;
    if (logical_height < 1 || logical_height > INT_MAX) return false;
    page.logical_height = (int)logical_height;
    __int64 count = ((__int64)document_height + page.logical_height - 1) / page.logical_height;
    if (count < 1 || count > 10000) return false;
    page.count = (int)count;
    return true;
}

typedef bool (*RenderPage)(MWContext *, HDC, int, int, int);
static bool record_pages(MWContext *context, const PageSettings &settings, HDC printer,
                         Pages &pages, std::string &error, RenderPage render = WFE_PrintDocumentPage)
{
    pages.clear();
    int width = 0, height = 0;
    if (!context || !WFE_PrintDocumentSize(context, &width, &height) ||
        !page_geometry(settings, width, height, printer, pages.geometry)) {
        error = "The document or page margins do not leave a printable page."; return false;
    }
    PageGeometry &page = pages.geometry;
    HDC screen = GetDC(nullptr);
    if (!screen) { error = "Could not create a print preview drawing context."; return false; }
    int dx = GetDeviceCaps(screen, LOGPIXELSX), dy = GetDeviceCaps(screen, LOGPIXELSY);
    int top = 0;
    while (top < height && pages.files.size() < 10000) {
        int maximum = (std::min)(page.logical_height, height-top);
        int slice = WFE_PrintPageHeight(context, top, maximum);
        if (slice <= 0 || slice > maximum) { error = "Could not determine a valid document page break."; break; }
        RECT frame = {0, 0, page.width, page.height};
        HDC dc = CreateEnhMetaFileA(screen, nullptr, &frame, "Netscape\0Document page\0\0");
        if (!dc) { error = "Could not allocate a vector print page."; break; }
        int saved = SaveDC(dc);
        RECT paper = {0, 0, MulDiv(page.width, dx, 2540), MulDiv(page.height, dy, 2540)};
        FillRect(dc, &paper, (HBRUSH)GetStockObject(WHITE_BRUSH));
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, width, page.logical_height, nullptr);
        SetViewportExtEx(dc, MulDiv(page.content_width, dx, 2540), MulDiv(page.content_height, dy, 2540), nullptr);
        SetViewportOrgEx(dc, MulDiv(page.left, dx, 2540), MulDiv(page.top, dy, 2540), nullptr);
        IntersectClipRect(dc, 0, 0, width, slice);
        bool good = saved != 0 && render(context, dc, top, width, slice);
        if (saved) RestoreDC(dc, saved);
        HENHMETAFILE metafile = CloseEnhMetaFile(dc);
        if (!good || !metafile) {
            if (metafile) DeleteEnhMetaFile(metafile);
            error = "The document renderer could not finish a print page."; break;
        }
        pages.files.push_back(metafile);
        pages.tops.push_back(top); pages.heights.push_back(slice); top += slice;
    }
    ReleaseDC(nullptr, screen);
    if (top != height) { if(error.empty()) error="The document exceeds the supported print page count.";pages.clear(); return false; }
    page.count = (int)pages.files.size();
    return true;
}

struct NativeTemplate {
    DLGTEMPLATE dialog; WORD menu, window_class, title;
    NativeTemplate() : dialog(), menu(0), window_class(0), title(0) {
        dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
        dialog.cx = 300; dialog.cy = 210;
    }
};
static CWnd *owner_window(MWContext *context)
{
    HWND window = context ? WFE_DocumentWindow(context) : nullptr;
    return IsWindow(window) ? CWnd::FromHandle(GetAncestor(window, GA_ROOT)) : nullptr;
}

/* Native Page Setup fallback is usable even when Windows has no printer. */
class PageSetupFallback : public ClassicDialog {
public:
    PageSettings value;
    bool automatic, initialized = false;
    CStatic labels[8]; CComboBox paper, orientation;
    CEdit margin[4]; ClassicButton accept_button, cancel_button;
    PageSetupFallback(const PageSettings &settings, bool cancel_test) : value(settings), automatic(cancel_test) { }
protected:
    BOOL PreTranslateMessage(MSG *message) override {
        if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN) {
            if(GetFocus()==&cancel_button) OnCancel();else OnOK();return TRUE;
        }
        return CDialog::PreTranslateMessage(message);
    }
    BOOL OnInitDialog() override {
        ClassicDialog::OnInitDialog(); SetWindowText("Page Setup");
        CRect outer(0,0,ClassicUI::Scale(this,365),ClassicUI::Scale(this,270));
        AdjustWindowRectEx(&outer, GetStyle(), FALSE, GetExStyle());
        SetWindowPos(nullptr,0,0,outer.Width(),outer.Height(),SWP_NOMOVE|SWP_NOZORDER);
        auto r = [this](int x,int y,int w,int h) {return CRect(ClassicUI::Scale(this,x),ClassicUI::Scale(this,y),ClassicUI::Scale(this,x+w),ClassicUI::Scale(this,y+h));};
        labels[0].Create("&Paper:",WS_CHILD|WS_VISIBLE,r(15,20,92,22),this);
        paper.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,r(112,16,230,110),this,SETUP_PAPER);
        paper.AddString("Letter (8.5 x 11 in)"); paper.AddString("Legal (8.5 x 14 in)"); paper.AddString("A4 (210 x 297 mm)");
        paper.SetCurSel(value.paper==DMPAPER_A4?2:value.paper==DMPAPER_LEGAL?1:0);
        labels[1].Create("&Orientation:",WS_CHILD|WS_VISIBLE,r(15,55,92,22),this);
        orientation.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,r(112,51,230,90),this,SETUP_ORIENTATION);
        orientation.AddString("Portrait");orientation.AddString("Landscape");orientation.SetCurSel(value.orientation==DMORIENT_LANDSCAPE?1:0);
        labels[2].Create("Margins (inches)",WS_CHILD|WS_VISIBLE,r(15,91,300,22),this);
        const char *names[] = {"&Left:","&Right:","&Top:","&Bottom:"};
        int values[] = {value.left,value.right,value.top,value.bottom};
        for (int n=0;n<4;++n) {
            int x=n%2?192:15,y=122+(n/2)*37;
            labels[n+3].Create(names[n],WS_CHILD|WS_VISIBLE,r(x,y,70,22),this);
            margin[n].Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,r(x+72,y-3,77,25),this,SETUP_MARGIN+n);
            char text[32];sprintf_s(text,"%.3f",values[n]/2540.0);margin[n].SetWindowText(text);margin[n].LimitText(16);
        }
        accept_button.Create("OK",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,r(177,224,77,28),this,IDOK);
        cancel_button.Create("Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,r(265,224,77,28),this,IDCANCEL);
        ClassicUI::Apply(this); CenterWindow(); initialized=true;
        if (automatic) PostMessage(WM_COMMAND,IDCANCEL);
        return TRUE;
    }
    void OnOK() override {
        PageSettings next=value;
        int selected=paper.GetCurSel();
        next.paper=selected==2?DMPAPER_A4:selected==1?DMPAPER_LEGAL:DMPAPER_LETTER;
        next.width=selected==2?21000:21590;next.height=selected==2?29700:selected==1?35560:27940;
        next.orientation=orientation.GetCurSel()==1?DMORIENT_LANDSCAPE:DMORIENT_PORTRAIT;
        int *values[]={&next.left,&next.right,&next.top,&next.bottom};
        for(int n=0;n<4;++n) {
            CString text;margin[n].GetWindowText(text);char *end=nullptr;
            double inches=strtod(text,&end);
            bool parsed=end!=(LPCSTR)text;
            while(end && (*end==' '||*end=='\t')) ++end;
            if(!parsed || !end || *end || !std::isfinite(inches) || inches<0 || inches>70) {
                MessageBox("Enter a nonnegative margin in inches.","Page Setup",MB_OK|MB_ICONEXCLAMATION);margin[n].SetFocus();return;
            }
            *values[n]=(int)(inches*2540.0+0.5);
        }
        if(!valid_settings(next)) {MessageBox("The margins must leave at least 5 mm of printable width and height.","Page Setup",MB_OK|MB_ICONEXCLAMATION);return;}
        value=next;EndDialog(IDOK);
    }
};

static int fallback_setup(MWContext *context, PageSettings &settings, bool cancel_test, bool *initialized=nullptr)
{
    NativeTemplate layout;
    PageSetupFallback dialog(settings,cancel_test);
    if(!dialog.InitModalIndirect(&layout.dialog,owner_window(context))) return -1;
    INT_PTR result=dialog.DoModal();
    if(initialized) *initialized=dialog.initialized;
    if(result==IDOK) settings=dialog.value;
    return (int)result;
}

static void show_page_setup(MWContext *context)
{
    PageSettings settings=load_settings();CWnd *owner=owner_window(context);
    PRINTDLGA printer=printer_dialog(owner?owner->GetSafeHwnd():nullptr,settings);
    bool accepted=false;
    if(printer.hDevMode && printer.hDevNames) {
        PAGESETUPDLGA dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=printer.hwndOwner;
        dialog.hDevMode=printer.hDevMode;dialog.hDevNames=printer.hDevNames;
        dialog.Flags=PSD_MARGINS|PSD_INHUNDREDTHSOFMILLIMETERS|PSD_NOWARNING;
        dialog.rtMargin={settings.left,settings.top,settings.right,settings.bottom};
        accepted=PageSetupDlgA(&dialog)!=FALSE;
        printer.hDevMode=dialog.hDevMode;printer.hDevNames=dialog.hDevNames;
        if(accepted) {
            settings.left=dialog.rtMargin.left;settings.right=dialog.rtMargin.right;
            settings.top=dialog.rtMargin.top;settings.bottom=dialog.rtMargin.bottom;
            DEVMODEA *mode=dialog.hDevMode?(DEVMODEA *)GlobalLock(dialog.hDevMode):nullptr;
            if(mode) {settings.orientation=mode->dmOrientation;settings.paper=mode->dmPaperSize;GlobalUnlock(dialog.hDevMode);}
            settings.width=(std::min)(dialog.ptPaperSize.x,dialog.ptPaperSize.y);
            settings.height=(std::max)(dialog.ptPaperSize.x,dialog.ptPaperSize.y);
        } else if(CommDlgExtendedError()) WFE_Message(context,"Windows could not open Page Setup. Check the selected printer.",0);
    } else accepted=fallback_setup(context,settings,false)==IDOK;
    if(accepted) {
        if(!save_settings(settings)) WFE_Message(context,"Could not save the page settings in your profile.",0);
        else {remember_printer(printer.hDevMode,printer.hDevNames);WFE_Status(context,"Page settings saved.");}
    }
    release_dialog(printer);
}

class PrintProgress : public ClassicDialog {
public:
    bool cancelled=false;
    MWContext *context=nullptr;
    HWND owner=nullptr,source=nullptr;
    CStatic label;ClassicButton stop;
    bool continuing() {
        if(cancelled) return false;
        if(!context || !XP_IsContextInList(context) || !IsWindow(source) ||
            WFE_DocumentWindow(context)!=source || !IsWindow(owner) || !IsWindow(GetSafeHwnd())) cancelled=true;
        return !cancelled;
    }
    BOOL OnInitDialog() override {
        ClassicDialog::OnInitDialog();SetWindowText("Printing");
        label.Create("Preparing print job...",WS_CHILD|WS_VISIBLE,CRect(14,16,365,46),this);
        stop.Create("Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(277,55,365,84),this,IDCANCEL);
        CRect outer(0,0,382,100);AdjustWindowRectEx(&outer,GetStyle(),FALSE,GetExStyle());
        SetWindowPos(nullptr,0,0,outer.Width(),outer.Height(),SWP_NOMOVE|SWP_NOZORDER);ClassicUI::Apply(this);CenterWindow();return TRUE;
    }
    void OnCancel() override {
        cancelled=true;
        if(IsWindow(stop.GetSafeHwnd())) stop.EnableWindow(FALSE);
        if(IsWindow(label.GetSafeHwnd())) label.SetWindowText("Cancelling print job...");
    }
    void OnOK() override { }
    afx_msg void OnDestroy() {cancelled=true;ClassicDialog::OnDestroy();}
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(PrintProgress,ClassicDialog)
 ON_WM_DESTROY()
END_MESSAGE_MAP()
static PrintProgress *active_job=nullptr;
static BOOL CALLBACK abort_print(HDC,int)
{
    PrintProgress *job=active_job;
    if(!job || !job->continuing()) return FALSE;
    MSG message;
    while(PeekMessage(&message,nullptr,0,0,PM_REMOVE)) {
        if(message.message==WM_QUIT) {PostQuitMessage((int)message.wParam);job->cancelled=true;break;}
        if(active_job!=job || !job->continuing()) return FALSE;
        BOOL handled=IsDialogMessage(job->GetSafeHwnd(),&message);
        if(active_job!=job || !job->continuing()) return FALSE;
        if(!handled) {TranslateMessage(&message);DispatchMessage(&message);}
        if(active_job!=job || !job->continuing()) return FALSE;
    }
    return active_job==job && job->continuing();
}

static bool source_alive(MWContext *context,HWND source)
{
    return context && XP_IsContextInList(context) && IsWindow(source) && WFE_DocumentWindow(context)==source;
}

static bool owner_close_smoke(MWContext *context)
{
    if(active_job) return false;
    HWND source=WFE_DocumentWindow(context);CWnd fixture;
    if(!source_alive(context,source) || !fixture.CreateEx(0,AfxRegisterWndClass(0),"Print owner cancellation fixture",
        WS_OVERLAPPEDWINDOW,CRect(0,0,400,160),nullptr,0)) return false;
    HWND owner=fixture.GetSafeHwnd();NativeTemplate layout;PrintProgress progress;
    progress.context=context;progress.owner=owner;progress.source=source;
    if(!progress.CreateIndirect(&layout.dialog,&fixture)) {fixture.DestroyWindow();return false;}
    HWND progress_window=progress.GetSafeHwnd();
    ::EnableWindow(owner,FALSE);active_job=&progress;
    bool before=progress.continuing();
    /* Disabled owners still receive posted WM_CLOSE (for example File/Exit in
     * another browser). Exercise the real modeless window and abort pump. */
    ::PostMessage(owner,WM_CLOSE,0,0);
    bool stopped=abort_print(nullptr,0)==FALSE;
    bool closed=!IsWindow(owner) && !IsWindow(progress_window);
    bool cancelled=progress.cancelled;active_job=nullptr;
    if(IsWindow(progress.GetSafeHwnd())) progress.DestroyWindow();
    if(IsWindow(owner)) fixture.DestroyWindow();
    return before && stopped && closed && cancelled && source_alive(context,source);
}

static void print_document(MWContext *context,CWnd *modal_owner=nullptr)
{
    if(active_job) return;
    PageSettings settings=load_settings();
    HWND owner=nullptr;
    {CWnd *initial_owner=modal_owner?modal_owner:owner_window(context);owner=initial_owner?initial_owner->GetSafeHwnd():nullptr;}
    HWND source=WFE_DocumentWindow(context);
    PRINTDLGA dialog=printer_dialog(owner,settings);
    bool accepted=PrintDlgA(&dialog)!=FALSE;
    if(!source_alive(context,source) || !IsWindow(owner)) {release_dialog(dialog);return;}
    if(!accepted) {
        DWORD error=CommDlgExtendedError();release_dialog(dialog);
        if(error) {char text[160];sprintf_s(text,"Windows could not open the Print dialog (error %lu). Check that a printer is installed.",(unsigned long)error);WFE_Message(context,text,0);}
        else WFE_Status(context,"Printing cancelled.");
        return;
    }
    remember_printer(dialog.hDevMode,dialog.hDevNames);
    Pages pages;std::string error;
    if(!dialog.hDC || !record_pages(context,settings,dialog.hDC,pages,error)) {
        release_dialog(dialog);WFE_Message(context,error.empty()?"The printer did not provide a drawing context.":error.c_str(),0);return;
    }
    int first=1,last=(int)pages.files.size();
    if(dialog.Flags&PD_PAGENUMS) {first=(std::max)(1,(int)dialog.nFromPage);last=(std::min)(last,(int)dialog.nToPage);}
    if(first>last) {release_dialog(dialog);WFE_Message(context,"The selected page range is outside this document.",0);return;}
    NativeTemplate layout;PrintProgress progress;
    progress.context=context;progress.owner=owner;progress.source=source;
    if(!progress.CreateIndirect(&layout.dialog,CWnd::FromHandle(owner))) {
        release_dialog(dialog);if(source_alive(context,source)) WFE_Message(context,"Could not create the print cancellation window.",0);return;
    }
    bool owner_enabled=IsWindowEnabled(owner)!=FALSE;if(owner_enabled) ::EnableWindow(owner,FALSE);
    active_job=&progress;
    if(progress.continuing()) {progress.ShowWindow(SW_SHOW);progress.UpdateWindow();}
    DOCINFOA document={};document.cbSize=sizeof(document);document.lpszDocName="Netscape Document";
    bool started=false,good=progress.continuing() && SetAbortProc(dialog.hDC,abort_print)>0;
    if(good) {started=StartDocA(dialog.hDC,&document)>0;good=started && progress.continuing();}
    for(int n=first;good && n<=last;++n) {
        if(!abort_print(dialog.hDC,0)) {good=false;break;}
        char status[96];sprintf_s(status,"Printing page %d of %d...",n,last);progress.label.SetWindowText(status);
        good=progress.continuing() && StartPage(dialog.hDC)>0;
        good=good && progress.continuing();
        if(good) {
            int saved=SaveDC(dialog.hDC);SetMapMode(dialog.hDC,MM_TEXT);SetWindowOrgEx(dialog.hDC,0,0,nullptr);SetViewportOrgEx(dialog.hDC,0,0,nullptr);
            int x=-GetDeviceCaps(dialog.hDC,PHYSICALOFFSETX),y=-GetDeviceCaps(dialog.hDC,PHYSICALOFFSETY);
            RECT paper={x,y,x+GetDeviceCaps(dialog.hDC,PHYSICALWIDTH),y+GetDeviceCaps(dialog.hDC,PHYSICALHEIGHT)};
            good=saved!=0 && PlayEnhMetaFile(dialog.hDC,pages.files[n-1],&paper)!=FALSE && progress.continuing();
            if(saved) RestoreDC(dialog.hDC,saved);
            if(good) good=EndPage(dialog.hDC)>0 && progress.continuing();
        }
    }
    if(started) {
        if(good && progress.continuing()) {good=EndDoc(dialog.hDC)>0 && progress.continuing();if(!good) AbortDoc(dialog.hDC);}
        else AbortDoc(dialog.hDC);
    }
    bool cancelled=!progress.continuing();active_job=nullptr;
    if(IsWindow(progress.GetSafeHwnd())) progress.DestroyWindow();
    /* An unrestricted abort-procedure dispatch can delete a CFrameWnd. Retain
     * only handles across that boundary and never dereference the old owner. */
    if(owner_enabled && source_alive(context,source) && IsWindow(owner)) {
        ::EnableWindow(owner,TRUE);if(IsWindow(owner)) ::SetActiveWindow(owner);
    }
    release_dialog(dialog);
    if(!source_alive(context,source)) return;
    if(cancelled) WFE_Status(context,"Printing cancelled.");
    else if(!good) WFE_Message(context,"Windows could not complete the print job. Check the printer and its queue.",0);
    else WFE_Status(context,"Document sent to the printer.");
}

static bool capture_preview(CWnd *window,const char *path)
{
    CRect area;window->GetWindowRect(area);
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=area.Width();info.bmiHeader.biHeight=-area.Height();
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    CClientDC screen(window);CDC memory;
    if(!memory.CreateCompatibleDC(&screen)) return false;
    void *pixels=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap) return false;
    HGDIOBJ old=SelectObject(memory,bitmap);
    window->RedrawWindow(nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    bool captured=PrintWindow(window->GetSafeHwnd(),memory,0)!=FALSE;
    DWORD bytes=(DWORD)area.Width()*(DWORD)area.Height()*4;
    BITMAPFILEHEADER header={};header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+bytes;
    FILE *file=nullptr;fopen_s(&file,path,"wb");bool written=false;
    if(file) {
        written=fwrite(&header,sizeof(header),1,file)==1 && fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1 &&
            fwrite(pixels,bytes,1,file)==1;
        written=fclose(file)==0 && written;
    }
    SelectObject(memory,old);DeleteObject(bitmap);return captured && written;
}

class PrintPreview : public CDialog {
public:
    MWContext *context;Pages &pages;int current=0,scroll_x=0,scroll_y=0;bool zoom=false;
    bool automatic=false,exercised=false,played=false;
    bool test_posted=false,test_visible=false,first_painted=false,next_painted=false,captured=false,zoom_captured=false;
    std::string snapshot_path,zoom_snapshot_path;
    ClassicButton previous,next,print,close,magnify;CStatic label;
    PrintPreview(MWContext *source,Pages &snapshot):context(source),pages(snapshot) { }
protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();SetWindowText("Print Preview");
        CRect work;SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
        SetWindowPos(nullptr,0,0,(std::min)(1000,work.Width()-30),(std::min)(820,work.Height()-30),SWP_NOMOVE|SWP_NOZORDER);
        previous.Create("&Previous",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(10,10,100,38),this,PRINT_PREVIOUS);
        next.Create("&Next",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(108,10,198,38),this,PRINT_NEXT);
        magnify.Create("&Zoom In",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(206,10,296,38),this,PRINT_ZOOM);
        print.Create("&Print...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(304,10,394,38),this,PRINT_START);
        close.Create("Close",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(402,10,492,38),this,IDCANCEL);
        label.Create("",WS_CHILD|WS_VISIBLE,CRect(508,16,900,38),this,PRINT_LABEL);
        ClassicUI::Apply(this);CenterWindow();update_page();return TRUE;
    }
    void update_page() {
        char text[96];sprintf_s(text,"Page %d of %u",current+1,(unsigned)pages.files.size());
        if(label.GetSafeHwnd()) label.SetWindowText(text);
        if(previous.GetSafeHwnd()) previous.EnableWindow(current>0);
        if(next.GetSafeHwnd()) next.EnableWindow(current+1<(int)pages.files.size());
        scroll_x=scroll_y=0;Invalidate();
    }
    BOOL OnCommand(WPARAM command,LPARAM control) override {
        switch(LOWORD(command)) {
        case PRINT_PREVIOUS:if(current>0) {--current;update_page();}return TRUE;
        case PRINT_NEXT:if(current+1<(int)pages.files.size()) {++current;update_page();}return TRUE;
        case PRINT_ZOOM:zoom=!zoom;magnify.SetWindowText(zoom?"&Zoom Out":"&Zoom In");update_page();return TRUE;
        case PRINT_START:if(XP_IsContextInList(context)) print_document(context,this);return TRUE;
        case PRINT_TEST:
            if(automatic) {
                test_visible=IsWindowVisible()!=FALSE;
                Invalidate();UpdateWindow();first_painted=played;
                if(!snapshot_path.empty()) captured=capture_preview(this,snapshot_path.c_str());
                SendMessage(WM_COMMAND,PRINT_NEXT);SendMessage(WM_COMMAND,PRINT_ZOOM);
                Invalidate();UpdateWindow();next_painted=played;
                if(!zoom_snapshot_path.empty()) zoom_captured=capture_preview(this,zoom_snapshot_path.c_str());
                exercised=test_visible && first_painted && next_painted && current==1 && zoom;
                EndDialog(IDCANCEL);
            }
            return TRUE;
        }
        return CDialog::OnCommand(command,control);
    }
    void OnOK() override { }
    BOOL PreTranslateMessage(MSG *message) override {
        if(message->message==WM_KEYDOWN) {
            if(message->wParam==VK_NEXT) {SendMessage(WM_COMMAND,PRINT_NEXT);return TRUE;}
            if(message->wParam==VK_PRIOR) {SendMessage(WM_COMMAND,PRINT_PREVIOUS);return TRUE;}
            if(message->wParam==VK_HOME) {current=0;update_page();return TRUE;}
            if(message->wParam==VK_END) {current=(int)pages.files.size()-1;update_page();return TRUE;}
        }
        return CDialog::PreTranslateMessage(message);
    }
    void draw_preview(CDC &dc) {
        CRect client;GetClientRect(client);dc.FillSolidRect(client,RGB(112,112,112));
        dc.FillSolidRect(0,0,client.Width(),48,ClassicUI::Face);
        int w=pages.geometry.width,h=pages.geometry.height;
        double scale=zoom?96.0/2540.0:(std::min)((client.Width()-40.0)/w,(client.Height()-78.0)/h);
        int pw=(std::max)(1,(int)(w*scale)),ph=(std::max)(1,(int)(h*scale));
        int xrange=(std::max)(0,pw+40-client.Width()),yrange=(std::max)(0,ph+78-client.Height());
        scroll_x=(std::min)(scroll_x,xrange);scroll_y=(std::min)(scroll_y,yrange);
        SCROLLINFO sx={sizeof(sx),SIF_RANGE|SIF_PAGE|SIF_POS,0,pw+39,(UINT)(std::max)(1,client.Width()),scroll_x};
        SCROLLINFO sy={sizeof(sy),SIF_RANGE|SIF_PAGE|SIF_POS,0,ph+77,(UINT)(std::max)(1,client.Height()),scroll_y};
        SetScrollInfo(SB_HORZ,&sx,TRUE);SetScrollInfo(SB_VERT,&sy,TRUE);
        int x=(std::max)(20,(client.Width()-pw)/2)-scroll_x,y=62-scroll_y;
        int saved=dc.SaveDC();dc.IntersectClipRect(0,49,client.Width(),client.Height());
        dc.FillSolidRect(x+4,y+4,pw,ph,RGB(48,48,48));RECT page={x,y,x+pw,y+ph};
        played=PlayEnhMetaFile(dc.GetSafeHdc(),pages.files[current],&page)!=FALSE;
        if(!played) dc.FillSolidRect(&page,RGB(255,255,255));
        dc.RestoreDC(saved);
    }
    afx_msg void OnPaint() {CPaintDC dc(this);draw_preview(dc);}
    afx_msg LRESULT OnPrintClient(WPARAM target,LPARAM) {
        if(target) draw_preview(*CDC::FromHandle((HDC)target));return 0;
    }
    afx_msg void OnShowWindow(BOOL show,UINT status) {
        CDialog::OnShowWindow(show,status);
        /* DoModal shows on idle. A command posted from OnInitDialog can run
         * before that first show and therefore before WM_PAINT is possible. */
        if(show && automatic && !test_posted) {test_posted=true;PostMessage(WM_COMMAND,PRINT_TEST);}
    }
    void scroll(int bar,UINT code,UINT position) {
        int &offset=bar==SB_HORZ?scroll_x:scroll_y;
        SCROLLINFO info={sizeof(info),SIF_ALL};GetScrollInfo(bar,&info);
        switch(code) {case SB_LINEUP:offset-=40;break;case SB_LINEDOWN:offset+=40;break;
        case SB_PAGEUP:offset-=(int)info.nPage;break;case SB_PAGEDOWN:offset+=(int)info.nPage;break;
        case SB_THUMBTRACK:offset=info.nTrackPos;break;case SB_THUMBPOSITION:offset=(int)position;break;case SB_TOP:offset=0;break;case SB_BOTTOM:offset=info.nMax;break;}
        offset=(std::max)(0,(std::min)(offset,info.nMax-(int)info.nPage+1));SetScrollPos(bar,offset,TRUE);Invalidate();
    }
    afx_msg void OnVScroll(UINT code,UINT position,CScrollBar *) {scroll(SB_VERT,code,position);}
    afx_msg void OnHScroll(UINT code,UINT position,CScrollBar *) {scroll(SB_HORZ,code,position);}
    afx_msg BOOL OnMouseWheel(UINT,short delta,CPoint) {scroll(SB_VERT,delta>0?SB_LINEUP:SB_LINEDOWN,0);return TRUE;}
    afx_msg void OnSize(UINT type,int x,int y) {CDialog::OnSize(type,x,y);if(label.GetSafeHwnd()) Invalidate();}
    afx_msg void OnGetMinMaxInfo(MINMAXINFO *info) {
        CDialog::OnGetMinMaxInfo(info);info->ptMinTrackSize.x=660;info->ptMinTrackSize.y=360;
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(PrintPreview,CDialog)
 ON_WM_PAINT()
 ON_MESSAGE(WM_PRINTCLIENT,OnPrintClient)
 ON_WM_SHOWWINDOW()
 ON_WM_VSCROLL()
 ON_WM_HSCROLL()
 ON_WM_MOUSEWHEEL()
 ON_WM_SIZE()
 ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

static void preview_document(MWContext *context)
{
    Pages pages;std::string error;PageSettings settings=load_settings();
    CWnd *owner=owner_window(context);
    PRINTDLGA printer=printer_dialog(owner?owner->GetSafeHwnd():nullptr,settings);
    /* Information contexts do not spool a job. A missing printer simply uses
     * the selected physical page dimensions for printer-independent preview. */
    HDC reference=nullptr;
    DEVMODEA *mode=printer.hDevMode?(DEVMODEA *)GlobalLock(printer.hDevMode):nullptr;
    DEVNAMES *names=printer.hDevNames?(DEVNAMES *)GlobalLock(printer.hDevNames):nullptr;
    if(mode && names) reference=CreateICA((char *)names+names->wDriverOffset,
        (char *)names+names->wDeviceOffset,(char *)names+names->wOutputOffset,mode);
    if(names) GlobalUnlock(printer.hDevNames);if(mode) GlobalUnlock(printer.hDevMode);
    bool good=record_pages(context,settings,reference,pages,error);
    if(reference) DeleteDC(reference);release_dialog(printer);
    if(!good) {WFE_Message(context,error.c_str(),0);return;}
    NativeTemplate layout;layout.dialog.style|=WS_THICKFRAME|WS_MAXIMIZEBOX|WS_HSCROLL|WS_VSCROLL;
    PrintPreview preview(context,pages);
    if(preview.InitModalIndirect(&layout.dialog,owner_window(context))) preview.DoModal();
}

struct VectorRecords {int text=0,bitmap=0;};
static int CALLBACK count_records(HDC,HANDLETABLE *,const ENHMETARECORD *record,int,LPARAM parameter)
{
    VectorRecords &counts=*(VectorRecords *)parameter;
    switch(record->iType) {
    case EMR_EXTTEXTOUTA:case EMR_EXTTEXTOUTW:case EMR_POLYTEXTOUTA:case EMR_POLYTEXTOUTW:++counts.text;break;
    case EMR_STRETCHDIBITS:case EMR_BITBLT:case EMR_STRETCHBLT:++counts.bitmap;break;
    }
    return 1;
}
static bool failed_renderer(MWContext *,HDC,int,int,int) {return false;}
}

bool WFE_PrintCommand(MWContext *context,UINT command)
{
    if(command!=PRINT_COMMAND && command!=SETUP_COMMAND && command!=PREVIEW_COMMAND) return false;
    if(!context || !XP_IsContextInList(context) || active_job) return true;
    if(command==SETUP_COMMAND) show_page_setup(context);
    else if(command==PREVIEW_COMMAND) preview_document(context);
    else print_document(context);
    return true;
}

bool WFE_PrintUpdate(MWContext *context,CCmdUI *ui)
{
    if(!ui || (ui->m_nID!=PRINT_COMMAND && ui->m_nID!=SETUP_COMMAND && ui->m_nID!=PREVIEW_COMMAND)) return false;
    int width=0,height=0;
    bool enabled=context && XP_IsContextInList(context) && !active_job &&
        (ui->m_nID==SETUP_COMMAND || WFE_PrintDocumentSize(context,&width,&height));
    ui->Enable(enabled);return true;
}

int WFE_PrintSmoke(MWContext *context,const char *report)
{
    PageSettings settings;Pages pages;std::string error;
    int width=0,height=0;bool size=WFE_PrintDocumentSize(context,&width,&height);
    bool rendered=record_pages(context,settings,nullptr,pages,error);
    bool pagination=rendered && size && pages.files.size()>1;
    int covered=0;
    for(size_t n=0;n<pages.files.size();++n) {
        pagination=pagination && pages.tops[n]==covered && pages.heights[n]>0 && pages.heights[n]<=pages.geometry.logical_height;
        covered+=pages.heights[n];
    }
    pagination=pagination && covered==height;
    bool content=rendered;int text_count=0,bitmap_count=0;
    for(auto page:pages.files) {
        VectorRecords counts;ENHMETAHEADER header={};
        bool valid=GetEnhMetaFileHeader(page,sizeof(header),&header)!=0 && header.nBytes>sizeof(header) &&
            EnumEnhMetaFile(nullptr,page,count_records,&counts,nullptr)!=FALSE;
        content=content && valid && counts.text>0;text_count+=counts.text;bitmap_count+=counts.bitmap;
    }
    PageSettings invalid=settings;invalid.left=invalid.width;
    Pages rejected;std::string rejected_error;
    bool invalid_geometry=!record_pages(context,invalid,nullptr,rejected,rejected_error) && rejected.files.empty();
    bool render_failure=!record_pages(context,settings,nullptr,rejected,rejected_error,failed_renderer) && rejected.files.empty();
    PageSettings before=load_settings(),cancelled=before;bool initialized=false;
    int result=fallback_setup(context,cancelled,true,&initialized);
    bool setup_cancel=result==IDCANCEL && initialized && same_settings(cancelled,before);
    bool owner_close_cancel=owner_close_smoke(context);
    bool persistence=false,atomic_failure=false;
    if(report) {
        /* This file belongs to the smoke fixture, never the user's page setup.
         * A reader denying FILE_SHARE_DELETE exercises a real failed rename. */
        std::string probe_path=std::string(report)+".settings.ini";
        PageSettings probe=settings;probe.paper=DMPAPER_A4;probe.width=21000;probe.height=29700;
        probe.orientation=DMORIENT_LANDSCAPE;probe.left=1270;probe.top=3810;
        persistence=save_settings(probe,probe_path.c_str()) && same_settings(probe,load_settings(probe_path.c_str()));
        HANDLE locked=CreateFileA(probe_path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(locked!=INVALID_HANDLE_VALUE) {
            PageSettings changed=probe;changed.left+=254;
            bool rejected=!save_settings(changed,probe_path.c_str());CloseHandle(locked);
            atomic_failure=rejected && same_settings(probe,load_settings(probe_path.c_str()));
        }
        persistence=persistence && !save_settings(invalid,probe_path.c_str()) && same_settings(probe,load_settings(probe_path.c_str()));
    }
    bool preview_controls=false,preview_visible=false,preview_first_paint=false,preview_next_paint=false,preview_capture=false;
    std::string preview_path=report?std::string(report)+".preview.bmp":"";
    std::string preview_zoom_path=report?std::string(report)+".preview-zoom.bmp":"";
    if(rendered && pages.files.size()>1) {
        NativeTemplate layout;layout.dialog.style|=WS_THICKFRAME|WS_MAXIMIZEBOX|WS_HSCROLL|WS_VSCROLL;
        PrintPreview preview(context,pages);preview.automatic=true;
        preview.snapshot_path=preview_path;preview.zoom_snapshot_path=preview_zoom_path;
        if(preview.InitModalIndirect(&layout.dialog,owner_window(context))) preview.DoModal();
        preview_controls=preview.exercised;
        preview_visible=preview.test_visible;preview_first_paint=preview.first_painted;preview_next_paint=preview.next_painted;
        preview_capture=preview.captured && preview.zoom_captured;
    }
    bool artifacts=true;
    if(report && rendered && !pages.files.empty()) {
        std::string first=std::string(report)+".first.emf",last=std::string(report)+".last.emf";
        HENHMETAFILE a=CopyEnhMetaFileA(pages.files.front(),first.c_str()),b=CopyEnhMetaFileA(pages.files.back(),last.c_str());
        artifacts=a && b;if(a) DeleteEnhMetaFile(a);if(b) DeleteEnhMetaFile(b);
    }
    FILE *file=nullptr;if(report) fopen_s(&file,report,"wb");
    bool passed=pagination && content && invalid_geometry && render_failure && setup_cancel && owner_close_cancel && persistence && atomic_failure && preview_controls && preview_capture && artifacts && file;
    if(file) {
        fprintf(file,"pagination=%d\nvector_content=%d\ninvalid_geometry=%d\nrender_failure=%d\npage_setup_cancel=%d\n"
            "owner_close_cancel=%d\nsettings_persistence=%d\natomic_write_failure=%d\npreview_controls=%d\npreview_visible=%d\npreview_first_paint=%d\npreview_next_paint=%d\n"
            "preview_capture=%d\npreview_path=%s\npreview_zoom_path=%s\npages=%u\ntext_records=%d\nbitmap_records=%d\nemf_artifacts=%d\nerror=%s\n",pagination,content,invalid_geometry,render_failure,setup_cancel,
            owner_close_cancel,persistence,atomic_failure,preview_controls,preview_visible,preview_first_paint,preview_next_paint,preview_capture,
            preview_path.c_str(),preview_zoom_path.c_str(),(unsigned)pages.files.size(),text_count,bitmap_count,artifacts,error.c_str());fclose(file);
    }
    return passed?1:0;
}
