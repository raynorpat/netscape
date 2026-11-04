#include <afxwin.h>
#include <afxext.h>
#include <afxcoll.h>
#include <shlwapi.h>
#include <map>
#include <algorithm>
#include <string>
#include <vector>
#ifdef _DEBUG
#include <crtdbg.h>
static int debug_report(int type, char *message, int *)
{
    if (message) { fputs(message, stderr); fflush(stderr); }
    if (type == _CRT_ASSERT) ExitProcess(3);
    return TRUE;
}
static void invalid_parameter(const wchar_t *expression, const wchar_t *function,
                              const wchar_t *file, unsigned int line, uintptr_t)
{
    fwprintf(stderr, L"Invalid parameter: %ls, function %ls, %ls:%u\n",
             expression ? expression : L"", function ? function : L"",
             file ? file : L"", line);
    fflush(stderr);
    ExitProcess(3);
}
#endif
#include "resource.h"
#include "legacy/genedit.h"
#include "legacy/cmdparse.h"
#include "frontend.h"

class BrowserFrame;
static bool smoke_mode = false;
static std::string smoke_error;
static std::vector<BrowserFrame *> browser_windows;

class BrowserView : public CWnd {
public:
    BrowserFrame *frame = nullptr;
    CDC *drawing = nullptr;
    COLORREF background = RGB(255,255,255);
    int scroll_x = 0, scroll_y = 0, document_width = 0, document_height = 0;
    unsigned drawn_runs = 0;
    std::string drawn_text;
    void Render(CDC &dc, int width, int height);
protected:
    afx_msg void OnPaint();
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar *bar);
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar *bar);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()
};

class BrowserFrame : public CFrameWnd {
public:
    BrowserView view;
    CGenericEdit address;
    CButton go;
    CStatusBar status;
    MWContext context;
    bool initialized = false, completed = false;
    int load_status = 0;
    std::string current_url;
    std::vector<std::string> history;
    size_t history_position = 0;
    void Navigate(const char *url, bool remember = true);
    BOOL PreTranslateMessage(MSG *message) override;
    void PostNcDestroy() override { delete this; }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnOpen();
    afx_msg void OnGo();
    afx_msg void OnStop();
    afx_msg void OnReload();
    afx_msg void OnBack();
    afx_msg void OnForward();
    afx_msg void OnAbout();
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(BrowserFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_TIMER()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_COMMAND(ID_BROWSER_OPEN, OnOpen)
 ON_COMMAND(ID_NAV_GO, OnGo)
 ON_COMMAND(ID_NAV_STOP, OnStop)
 ON_COMMAND(ID_NAV_RELOAD, OnReload)
 ON_COMMAND(ID_NAV_BACK, OnBack)
 ON_COMMAND(ID_NAV_FORWARD, OnForward)
 ON_COMMAND(ID_BROWSER_ABOUT, OnAbout)
END_MESSAGE_MAP()
BEGIN_MESSAGE_MAP(BrowserView, CWnd)
 ON_WM_PAINT()
 ON_WM_VSCROLL()
 ON_WM_HSCROLL()
 ON_WM_MOUSEWHEEL()
 ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

static BrowserFrame *frame_for(MWContext *context)
{
    return context ? reinterpret_cast<BrowserFrame *>(context->fe.cx) : nullptr;
}

static std::string as_url(const char *input)
{
    char full[32768], url[32768];
    DWORD size = sizeof(url);
    if (PathFileExistsA(input) && GetFullPathNameA(input, sizeof(full), full, nullptr)
        && SUCCEEDED(UrlCreateFromPathA(full, url, &size, 0))) return url;
    if (strchr(input, ':')) return input;
    return std::string("http://") + input;
}

int BrowserFrame::OnCreate(LPCREATESTRUCT create)
{
    static UINT indicators[] = {ID_SEPARATOR};
    if (CFrameWnd::OnCreate(create) == -1) return -1;
    if (!address.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                        CRect(8,8,700,32), this, ID_ADDRESS)) return -1;
    address.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
    if (!go.Create("Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    CRect(710,8,770,32), this, ID_NAV_GO)) return -1;
    view.frame = this;
    if (!view.CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW)),
                       "Document", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                       CRect(0,40,800,600), this, 112)) return -1;
    if (!status.Create(this) || !status.SetIndicators(indicators, 1)) return -1;
    WFE_InitContext(&context, this);
    initialized = true;
    SetTimer(1, 10, nullptr);
    return 0;
}

void BrowserFrame::OnSize(UINT type, int width, int height)
{
    CFrameWnd::OnSize(type, width, height);
    if (!view.GetSafeHwnd()) return;
    address.MoveWindow(8, 8, (std::max)(width - 84, 20), 24);
    go.MoveWindow((std::max)(width - 68, 8), 8, 60, 24);
    view.MoveWindow(0, 40, width, (std::max)(height - 62, 1));
}
BOOL BrowserFrame::PreTranslateMessage(MSG *message)
{
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN
        && GetFocus() == &address) { OnGo(); return TRUE; }
    return CFrameWnd::PreTranslateMessage(message);
}
void BrowserFrame::Navigate(const char *input, bool remember)
{
    current_url = as_url(input);
    address.SetWindowText(current_url.c_str());
    if (remember) {
        if (!history.empty()) history.resize(history_position + 1);
        history.push_back(current_url);
        history_position = history.size() - 1;
    }
    completed = false;
    view.drawn_runs = 0;
    view.drawn_text.clear();
    WFE_Load(&context, current_url.c_str());
}
void BrowserFrame::OnTimer(UINT_PTR timer) { if (timer == 1) WFE_Pump(); else CFrameWnd::OnTimer(timer); }
void BrowserFrame::OnGo() { CString text; address.GetWindowText(text); if (!text.IsEmpty()) Navigate(text); }
void BrowserFrame::OnOpen() { CFileDialog dialog(TRUE, "html", nullptr, OFN_FILEMUSTEXIST, "Documents|*.html;*.htm;*.txt|All files|*.*||", this); if (dialog.DoModal() == IDOK) Navigate(dialog.GetPathName()); }
void BrowserFrame::OnStop() { NET_InterruptWindow(&context); }
void BrowserFrame::OnReload() { if (!current_url.empty()) Navigate(current_url.c_str(), false); }
void BrowserFrame::OnBack() { if (history_position > 0) { --history_position; Navigate(history[history_position].c_str(), false); } }
void BrowserFrame::OnForward() { if (history_position + 1 < history.size()) { ++history_position; Navigate(history[history_position].c_str(), false); } }
void BrowserFrame::OnAbout() { Navigate("about:"); }
void BrowserFrame::OnClose()
{
    CFrameWnd::OnClose();
}
void BrowserFrame::OnDestroy()
{
    KillTimer(1);
    if (initialized) { WFE_DestroyContext(&context); initialized = false; }
    auto found = std::find(browser_windows.begin(), browser_windows.end(), this);
    if (found != browser_windows.end()) browser_windows.erase(found);
    if (AfxGetApp()->m_pMainWnd == this && !browser_windows.empty())
        AfxGetApp()->m_pMainWnd = browser_windows.front();
    CFrameWnd::OnDestroy();
}

void BrowserView::Render(CDC &dc, int width, int height)
{
    dc.FillSolidRect(0, 0, width, height, background);
    drawing = &dc;
    int saved = dc.SaveDC();
    dc.SetViewportOrg(-scroll_x, -scroll_y);
    LO_RefreshArea(&frame->context, scroll_x, scroll_y, width, height);
    dc.RestoreDC(saved);
    drawing = nullptr;
}
void BrowserView::OnPaint() { CPaintDC dc(this); CRect area; GetClientRect(area); if (frame->initialized) Render(dc, area.Width(), area.Height()); }
static int scroll_position(int current, UINT code, UINT position, int maximum, int page)
{
    if (code == SB_TOP) current = 0;
    else if (code == SB_BOTTOM) current = maximum;
    else if (code == SB_LINEUP) current -= 24;
    else if (code == SB_LINEDOWN) current += 24;
    else if (code == SB_PAGEUP) current -= page;
    else if (code == SB_PAGEDOWN) current += page;
    else if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) current = position;
    return (std::max)(0, (std::min)(current, (std::max)(0, maximum - page)));
}
void BrowserView::OnVScroll(UINT code, UINT position, CScrollBar *) { CRect area; GetClientRect(area); scroll_y = scroll_position(scroll_y, code, position, document_height, area.Height()); SetScrollPos(SB_VERT, scroll_y); Invalidate(); }
void BrowserView::OnHScroll(UINT code, UINT position, CScrollBar *) { CRect area; GetClientRect(area); scroll_x = scroll_position(scroll_x, code, position, document_width, area.Width()); SetScrollPos(SB_HORZ, scroll_x); Invalidate(); }
BOOL BrowserView::OnMouseWheel(UINT, short delta, CPoint) { OnVScroll(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0, nullptr); return TRUE; }
void BrowserView::OnLButtonUp(UINT, CPoint point)
{
    LO_Element *element = LO_XYToElement(&frame->context, point.x + scroll_x, point.y + scroll_y);
    if (element && element->type == LO_TEXT && element->lo_text.anchor_href) {
        char *address = (char *)element->lo_text.anchor_href->anchor;
        if (address) frame->Navigate(address);
    }
}

static HFONT text_font(LO_TextAttr *attribute)
{
    static const int heights[] = {10,10,12,16,18,24,32,48};
    int size = attribute ? (std::max)(0, (std::min)(7, (int)attribute->size)) : 3;
    int mask = attribute ? attribute->fontmask : 0;
    int attributes = attribute ? attribute->attrmask : 0;
    const char *face = attribute && attribute->font_face ? attribute->font_face : (mask & LO_FONT_FIXED ? "Consolas" : "Times New Roman");
    return CreateFontA(-heights[size], 0, 0, 0, mask & LO_FONT_BOLD ? FW_BOLD : FW_NORMAL,
                      !!(mask & LO_FONT_ITALIC), !!(attributes & LO_ATTR_UNDERLINE),
                      !!(attributes & LO_ATTR_STRIKEOUT), ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
}

extern "C" void WFE_TextMetrics(MWContext *context, LO_TextStruct *text, LO_TextInfo *info)
{
    BrowserFrame *frame = frame_for(context);
    CClientDC dc(frame ? &frame->view : nullptr);
    HFONT font = text_font(text->text_attr);
    HGDIOBJ previous = SelectObject(dc, font);
    TEXTMETRICA metrics;
    SIZE size = {0,0};
    GetTextMetricsA(dc, &metrics);
    if (text->text && text->text_len > 0) GetTextExtentPoint32A(dc, (char *)text->text, text->text_len, &size);
    info->max_width = size.cx;
    info->ascent = (int16)metrics.tmAscent;
    info->descent = (int16)metrics.tmDescent;
    info->lbearing = 0;
    info->rbearing = (int16)size.cx;
    SelectObject(dc, previous);
    DeleteObject(font);
}
extern "C" void WFE_Text(MWContext *context, LO_TextStruct *text, int32 start, int32 end, int background)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || !frame->view.drawing || !text->text) return;
    start = (std::max)(0L, start);
    end = (std::min)((long)text->text_len, end);
    if (end <= start) return;
    CDC &dc = *frame->view.drawing;
    HFONT font = text_font(text->text_attr);
    HGDIOBJ previous = SelectObject(dc, font);
    SIZE offset = {0,0};
    if (start) GetTextExtentPoint32A(dc, (char *)text->text, start, &offset);
    LO_TextAttr *attribute = text->text_attr;
    if (attribute) {
        SetTextColor(dc, RGB(attribute->fg.red, attribute->fg.green, attribute->fg.blue));
        SetBkColor(dc, RGB(attribute->bg.red, attribute->bg.green, attribute->bg.blue));
    }
    SetBkMode(dc, background ? OPAQUE : TRANSPARENT);
    TextOutA(dc, text->x + text->x_offset + offset.cx, text->y + text->y_offset,
             (char *)text->text + start, end - start);
    SelectObject(dc, previous);
    DeleteObject(font);
    if (smoke_mode) {
        frame->view.drawn_text.append((char *)text->text + start, end - start);
        frame->view.drawn_text += ' ';
        ++frame->view.drawn_runs;
    }
}
extern "C" void WFE_Rect(MWContext *context, int32 x, int32 y, int32 width, int32 height, unsigned long color, int fill)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || !frame->view.drawing) return;
    CRect area(x,y,x+width,y+height);
    if (fill) frame->view.drawing->FillSolidRect(area, color);
    else { CBrush brush(color); frame->view.drawing->FrameRect(area, &brush); }
}
extern "C" void WFE_Status(MWContext *context, const char *message) { BrowserFrame *frame = frame_for(context); if (frame && frame->status.GetSafeHwnd()) frame->status.SetPaneText(0, message ? message : ""); }
extern "C" void WFE_Title(MWContext *context, const char *title) { BrowserFrame *frame = frame_for(context); if (frame) { std::string caption = std::string(title ? title : "Untitled") + " - Netscape 3.02"; frame->SetWindowText(caption.c_str()); } }
extern "C" void WFE_Invalidate(MWContext *context) { BrowserFrame *frame = frame_for(context); if (frame && frame->view.GetSafeHwnd()) frame->view.Invalidate(); }
extern "C" void WFE_WindowSize(MWContext *context, int32 *width, int32 *height) { BrowserFrame *frame = frame_for(context); CRect area(0,0,800,600); if (frame) frame->view.GetClientRect(area); *width = area.Width(); *height = area.Height(); }
extern "C" void WFE_DocumentSize(MWContext *context, int32 width, int32 height) { BrowserFrame *frame = frame_for(context); if (frame) { frame->view.document_width = width; frame->view.document_height = height; frame->view.SetScrollRange(SB_VERT, 0, height); frame->view.SetScrollRange(SB_HORZ, 0, width); } }
extern "C" void WFE_Scroll(MWContext *context, int32 x, int32 y) { BrowserFrame *frame = frame_for(context); if (frame) { frame->view.scroll_x = x; frame->view.scroll_y = y; WFE_Invalidate(context); } }
extern "C" void WFE_Position(MWContext *context, int32 *x, int32 *y) { BrowserFrame *frame = frame_for(context); *x = frame ? frame->view.scroll_x : 0; *y = frame ? frame->view.scroll_y : 0; }
extern "C" void WFE_Background(MWContext *context, unsigned long color) { BrowserFrame *frame = frame_for(context); if (frame) frame->view.background = color; }
extern "C" void WFE_Unsupported(MWContext *context, const char *feature) { std::string message = std::string(feature) + " is not available in this Windows port yet."; OutputDebugStringA(message.c_str()); WFE_Status(context, message.c_str()); }
extern "C" int WFE_Message(MWContext *context, const char *message, int confirm) { if (smoke_mode) { smoke_error = message ? message : ""; return 0; } BrowserFrame *frame = frame_for(context); return MessageBoxA(frame ? frame->GetSafeHwnd() : nullptr, message ? message : "", "Netscape", confirm ? MB_YESNO | MB_ICONQUESTION : MB_OK | MB_ICONINFORMATION) == IDYES; }
extern "C" void WFE_Complete(MWContext *context, int status) { BrowserFrame *frame = frame_for(context); if (frame) { frame->completed = true; frame->load_status = status; WFE_Invalidate(context); } }
extern "C" MWContext *WFE_NewWindow(const char *address)
{
    BrowserFrame *frame = new BrowserFrame;
    if (!frame->Create(nullptr, "Netscape 3.02", WS_OVERLAPPEDWINDOW, CRect(80,80,1100,800), nullptr, MAKEINTRESOURCE(IDR_MAIN))) return nullptr;
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_MAIN), TRUE);
    frame->LoadAccelTable(MAKEINTRESOURCE(IDR_MAIN));
    browser_windows.push_back(frame);
    if (!AfxGetApp()->m_pMainWnd) AfxGetApp()->m_pMainWnd = frame;
    if (!smoke_mode) frame->ShowWindow(SW_SHOW);
    if (address) frame->Navigate(address);
    return &frame->context;
}
extern "C" void WFE_CloseWindow(MWContext *context) { BrowserFrame *frame = frame_for(context); if (frame) frame->SendMessage(WM_CLOSE); }

struct NativeTimer { TimeoutCallbackFunction callback; void *closure; ULONGLONG due; };
static std::map<UINT_PTR, NativeTimer> timers;
static void CALLBACK timer_callback(HWND, UINT, UINT_PTR identifier, DWORD)
{
    auto found = timers.find(identifier);
    if (found == timers.end() || GetTickCount64() < found->second.due) return;
    NativeTimer timer = found->second;
    KillTimer(nullptr, identifier);
    timers.erase(found);
    timer.callback(timer.closure);
}
extern "C" void *WFE_SetTimer(TimeoutCallbackFunction callback, void *closure, uint32 milliseconds) { UINT_PTR identifier = SetTimer(nullptr, 0, (std::max)(1UL, milliseconds), timer_callback); if (!identifier) return nullptr; timers[identifier] = {callback, closure, GetTickCount64() + milliseconds}; return reinterpret_cast<void *>(identifier); }
extern "C" void WFE_KillTimer(void *timer) { UINT_PTR identifier = reinterpret_cast<UINT_PTR>(timer); KillTimer(nullptr, identifier); timers.erase(identifier); }

extern "C" int WFE_RunSmokeTest(const char *report, const char *url)
{
    std::string path = std::string(WFE_ProfileDirectory()) + "\\frontend-smoke.html";
    FILE *file = fopen(path.c_str(), "wb");
    if (!file) return 2;
    fputs("<html><head><title>Native frontend smoke</title></head><body><h1>Windows core renderer</h1><p>Hello <b>Netscape</b> 3.02.</p><ul><li>Native list</li></ul><img src='placeholder.png'><a href='about:'>About this port</a></body></html>", file);
    fclose(file);
    MWContext *context = WFE_NewWindow(nullptr);
    if (!context) return 3;
    BrowserFrame *frame = frame_for(context);
    frame->Navigate(url ? url : path.c_str());
    ULONGLONG deadline = GetTickCount64() + 10000;
    while (!frame->completed && GetTickCount64() < deadline) {
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessage(&message); }
        WFE_Pump();
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    CClientDC screen(&frame->view);
    CDC memory; memory.CreateCompatibleDC(&screen);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1024, 768);
    CBitmap *previous = memory.SelectObject(&bitmap);
    frame->view.Render(memory, 1024, 768);
    memory.SelectObject(previous);
    CString caption; frame->GetWindowText(caption);
    bool passed = frame->completed && frame->load_status >= 0 && frame->view.drawn_runs > 0
        && frame->view.drawn_text.find("Windows core renderer") != std::string::npos
        && frame->view.drawn_text.find("Netscape") != std::string::npos
        && caption.Find("Native frontend smoke") >= 0;
    const bool completed = frame->completed;
    const int load_status = frame->load_status;
    const unsigned drawn_runs = frame->view.drawn_runs;
    const std::string drawn_text = frame->view.drawn_text;
    BrowserFrame *survivor = frame_for(WFE_NewWindow(nullptr));
    frame->SendMessage(WM_CLOSE);
    bool survived = survivor && IsWindow(survivor->GetSafeHwnd())
        && AfxGetApp()->m_pMainWnd == survivor;
    MSG quit;
    if (PeekMessage(&quit, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE)) survived = false;
    passed = passed && survived;
    if (survivor) survivor->SendMessage(WM_CLOSE);
    file = fopen(report, "wb");
    if (file) {
        fprintf(file, "passed=%d\ncompleted=%d\nstatus=%d\ntext_runs=%u\nwindow_survived=%d\ntitle=%s\ntext=%s\nerror=%s\n",
                passed, completed, load_status, drawn_runs, survived,
                (const char *)caption, drawn_text.c_str(), smoke_error.c_str());
        fclose(file);
    } else passed = false;
    DeleteFileA(path.c_str());
    return passed ? 0 : 4;
}

class NetscapeApplication : public CWinApp {
public:
    BOOL InitInstance() override {
        CWinApp::InitInstance();
        SetRegistryKey("NetscapeLegacy");
        CStringList arguments;
        if (strlen(m_lpCmdLine) >= 511) return FALSE;
        CCmdParse parser(m_lpCmdLine, arguments);
        if (!parser.ProcessCmdLine()) return FALSE;
        POSITION position = arguments.GetHeadPosition();
        CString first = position ? arguments.GetNext(position) : "about:";
        smoke_mode = first == "--smoke-test";
        if (smoke_mode) {
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _DEBUG
            _CrtSetReportHook(debug_report);
            _set_invalid_parameter_handler(invalid_parameter);
#endif
        }
        if (!WFE_Init()) { m_nExitCode = 2; return FALSE; }
        backend_initialized = true;
        if (smoke_mode) {
            CString report = position ? arguments.GetNext(position) : "frontend-smoke.txt";
            CString url = position ? arguments.GetNext(position) : "";
            m_nExitCode = WFE_RunSmokeTest(report, url.IsEmpty() ? nullptr : (const char *)url);
            WFE_Shutdown();
            backend_initialized = false;
            return FALSE;
        }
        return WFE_NewWindow(first) != nullptr;
    }
    int ExitInstance() override {
        while (!browser_windows.empty()) browser_windows.back()->DestroyWindow();
        if (backend_initialized) WFE_Shutdown();
        for (const auto &timer : timers) KillTimer(nullptr, timer.first);
        timers.clear();
        int result = CWinApp::ExitInstance();
        return smoke_mode || m_nExitCode ? m_nExitCode : result;
    }
private:
    int m_nExitCode = 0;
    bool backend_initialized = false;
};
static NetscapeApplication application;
