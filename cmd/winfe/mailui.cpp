#include "mailui.h"
#include "classic.h"
#include "frontend.h"
#include "msgcom.h"
#include "resource.h"
#include <uxtheme.h>

static int mail_clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

static bool mail_image(CImageList &images, CWnd *owner, UINT resource, bool header = false)
{
    CBitmap source, scaled;
    if (!source.LoadBitmap(resource)) return false;
    BITMAP bitmap; source.GetBitmap(&bitmap);
    int inset = header ? 1 : 0;
    int size = ClassicUI::Scale(owner, header ? 14 : 16);
    CClientDC screen(owner);
    CDC input, output;
    input.CreateCompatibleDC(&screen); output.CreateCompatibleDC(&screen);
    if (!scaled.CreateCompatibleBitmap(&screen, size, size)) return false;
    CBitmap *old_input = input.SelectObject(&source);
    CBitmap *old_output = output.SelectObject(&scaled);
    output.SetStretchBltMode(COLORONCOLOR);
    output.FillSolidRect(0,0,size,size,ClassicUI::ArtworkKey);
    int width=bitmap.bmWidth-inset*2,height=bitmap.bmHeight-inset*2;
    int drawn_width=width>=height?size:MulDiv(size,width,height);
    int drawn_height=height>=width?size:MulDiv(size,height,width);
    output.StretchBlt((size-drawn_width)/2,(size-drawn_height)/2,drawn_width,drawn_height,
        &input,inset,inset,width,height,SRCCOPY);
    input.SelectObject(old_input); output.SelectObject(old_output);
    return images.Add(&scaled, ClassicUI::ArtworkKey) >= 0;
}

BEGIN_MESSAGE_MAP(MailPaneDivider, CWnd)
 ON_WM_PAINT()
 ON_WM_ERASEBKGND()
 ON_WM_SETCURSOR()
 ON_WM_LBUTTONDOWN()
 ON_WM_LBUTTONUP()
 ON_WM_MOUSEMOVE()
 ON_WM_CAPTURECHANGED()
 ON_WM_CANCELMODE()
END_MESSAGE_MAP()

bool MailPaneDivider::Create(MailPaneUI *ui, CWnd *owner, bool direction, UINT id)
{
    panes = ui; vertical = direction;
    return CWnd::CreateEx(0, AfxRegisterWndClass(0), "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, CRect(0,0,0,0), owner, id) != FALSE;
}

void MailPaneDivider::OnPaint()
{
    CPaintDC dc(this); CRect area; GetClientRect(area);
    dc.FillSolidRect(area, ClassicUI::Face());
    dc.DrawEdge(area,EDGE_RAISED,BF_RECT);
}

BOOL MailPaneDivider::OnEraseBkgnd(CDC *) { return TRUE; }

BOOL MailPaneDivider::OnSetCursor(CWnd *, UINT, UINT)
{
    ::SetCursor(::LoadCursor(nullptr, vertical ? IDC_SIZEWE : IDC_SIZENS));
    return TRUE;
}

void MailPaneDivider::OnLButtonDown(UINT, CPoint point)
{
    grab_offset = vertical ? point.x : point.y;
    SetCapture(); dragging = GetCapture() == this;
}

void MailPaneDivider::OnMouseMove(UINT, CPoint point)
{
    if (dragging && GetCapture() == this) {
        ClientToScreen(&point);
        panes->DragDivider(vertical, point, grab_offset);
    } else dragging = false;
}

void MailPaneDivider::OnLButtonUp(UINT flags, CPoint point)
{
    OnMouseMove(flags, point);
    OnCancelMode();
}

void MailPaneDivider::OnCaptureChanged(CWnd *window)
{
    dragging = false;
    CWnd::OnCaptureChanged(window);
}

void MailPaneDivider::OnCancelMode()
{
    dragging = false;
    if (GetCapture() == this) ::ReleaseCapture();
    CWnd::OnCancelMode();
}

MailPaneUI::~MailPaneUI()
{
    if (folder_divider.GetSafeHwnd()) folder_divider.DestroyWindow();
    if (reader_divider.GetSafeHwnd()) reader_divider.DestroyWindow();
    if (folders && folders->GetSafeHwnd()) {
        folders->GetHeaderCtrl()->SetImageList(nullptr);
        folders->SetImageList(nullptr, LVSIL_SMALL);
    }
    if (messages && messages->GetSafeHwnd()) {
        messages->GetHeaderCtrl()->SetImageList(nullptr);
        messages->SetImageList(nullptr, LVSIL_SMALL);
    }
}

bool MailPaneUI::Attach(CWnd *frame, CListCtrl *folder_list, CListCtrl *message_list,
                        CWnd *view, bool news)
{
    if (owner || !frame || !frame->GetSafeHwnd() || !folder_list
        || !folder_list->GetSafeHwnd() || !message_list || !message_list->GetSafeHwnd()
        || !view || !view->GetSafeHwnd()) return false;
    owner = frame; folders = folder_list; messages = message_list; reader = view; news_mode = news;
    folder_width = ClassicUI::Scale(owner, news?303:333);
    int size = ClassicUI::Scale(owner, 16);
    if (!folder_images.Create(size, size, ILC_COLOR24 | ILC_MASK, 2, 1)
        || !message_images.Create(size, size, ILC_COLOR24 | ILC_MASK, 3, 1)
        || !mail_image(folder_images, owner, 540)
        || !mail_image(folder_images, owner, 541)
        || !mail_image(message_images, owner, 542)
        || !mail_image(message_images, owner, 543)
        || !mail_image(message_images, owner, 544)
        || !folder_divider.Create(this, owner, true, FolderDividerID)
        || !reader_divider.Create(this, owner, false, ReaderDividerID)) return false;
    if(news && (!mail_image(folder_images,owner,550) || !mail_image(folder_images,owner,551)
        || !mail_image(folder_images,owner,552) || !mail_image(folder_images,owner,553))) return false;
    folders->SetImageList(&folder_images, LVSIL_SMALL);
    messages->SetImageList(&message_images, LVSIL_SMALL);
    folders->SetExtendedStyle(folders->GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_SUBITEMIMAGES);
    messages->SetExtendedStyle(messages->GetExtendedStyle()
                              | LVS_EX_FULLROWSELECT | LVS_EX_SUBITEMIMAGES);
    folders->SetBkColor(RGB(255,255,255)); folders->SetTextBkColor(RGB(255,255,255));
    messages->SetBkColor(RGB(255,255,255)); messages->SetTextBkColor(RGB(255,255,255));
    while (folders->DeleteColumn(0)) {}
    while (messages->DeleteColumn(0)) {}
    folders->InsertColumn(0, news ? "News Server" : "Mail Folder");
    folders->InsertColumn(1, news ? "" : "Unread", news ? LVCFMT_CENTER : LVCFMT_RIGHT);
    folders->InsertColumn(2, news ? "Unread" : "Total", LVCFMT_RIGHT);
    messages->InsertColumn(SenderColumn, "Sender");
    messages->InsertColumn(PriorityColumn, "", LVCFMT_CENTER);
    messages->InsertColumn(FlagColumn, "", LVCFMT_CENTER);
    messages->InsertColumn(SubjectColumn, "Subject");
    messages->InsertColumn(DateColumn, "Date");
    ClassicUI::Apply(folders); ClassicUI::Apply(messages);
    SetWindowTheme(folders->GetHeaderCtrl()->GetSafeHwnd(),L"",L"");
    SetWindowTheme(messages->GetHeaderCtrl()->GetSafeHwnd(),L"",L"");
    int header_size = ClassicUI::Scale(owner, 14);
    if (!header_images.Create(header_size, header_size, ILC_COLOR24 | ILC_MASK, 3, 1)
        || !mail_image(header_images, owner, IDB_NEWS_SUBSCRIBED, true)
        || !mail_image(header_images, owner, IDB_MAIL_PRIORITY_HEADER, true)
        || !mail_image(header_images, owner, IDB_MAIL_FLAG, true)) return false;
    messages->GetHeaderCtrl()->SetImageList(&header_images);
    HDITEM heading = {}; heading.mask = HDI_FORMAT | HDI_IMAGE;
    heading.fmt = HDF_IMAGE | HDF_CENTER; heading.iImage = 1;
    messages->GetHeaderCtrl()->SetItem(PriorityColumn, &heading);
    heading.iImage = 2; messages->GetHeaderCtrl()->SetItem(FlagColumn, &heading);
    if (news) {
        folders->GetHeaderCtrl()->SetImageList(&header_images);
        heading.iImage = 0; folders->GetHeaderCtrl()->SetItem(1, &heading);
    }
    LOGFONT font;ClassicUI::Font(owner)->GetLogFont(&font);font.lfWeight=FW_BOLD;unread_font.CreateFontIndirect(&font);
    return true;
}

void MailPaneUI::Arrange(int toolbar_bottom, int status_height)
{
    if (!owner || !owner->GetSafeHwnd() || !folders->GetSafeHwnd()
        || !messages->GetSafeHwnd() || !reader->GetSafeHwnd()) return;
    CRect area; owner->GetClientRect(area);
    top = mail_clamp(toolbar_bottom, 0, area.bottom);
    footer = mail_clamp(status_height, 0, area.bottom - top);
    int width = area.Width(), height = area.bottom - footer - top;
    int divider = ClassicUI::Scale(owner, 5);
    int vertical_size = mail_clamp(divider, 0, width);
    int horizontal_size = mail_clamp(divider, 0, height);
    int list_space = width - vertical_size, body_space = height - horizontal_size;
    int folder_min = ClassicUI::Scale(owner, 110), message_min = ClassicUI::Scale(owner, 160);
    if (list_space < folder_min + message_min) {
        folder_min = MulDiv(list_space, 110, 270);
        message_min = list_space - folder_min;
    }
    int pane_min = ClassicUI::Scale(owner, 70);
    if (body_space < pane_min * 2) pane_min = body_space / 2;
    /* Original Mail reserves 333px for folder/count columns; News uses 303px.
       Both keep 203px lists on larger windows and clamp below. */
    if(folder_width==-1 && width>0) folder_width=ClassicUI::Scale(owner,news_mode?303:333);
    if (top_height == -1 && height > 0)
        top_height = ClassicUI::Scale(owner,203);
    int folder_size = mail_clamp(folder_width, folder_min, list_space - message_min);
    int list_height = mail_clamp(top_height, pane_min, body_space - pane_min);
    folder_bounds.SetRect(0, top, folder_size, top + list_height);
    message_bounds.SetRect(folder_size + vertical_size, top, width, top + list_height);
    reader_bounds.SetRect(0, top + list_height + horizontal_size, width, area.bottom - footer);
    folders->MoveWindow(folder_bounds); messages->MoveWindow(message_bounds);
    reader->MoveWindow(reader_bounds);
    folder_divider.MoveWindow(folder_size, top, vertical_size, list_height);
    reader_divider.MoveWindow(0, top + list_height, width, horizontal_size);
    arranged = true;
    FitColumns();
}

void MailPaneUI::FitColumns()
{
    if (!folders || !folders->GetSafeHwnd() || !messages || !messages->GetSafeHwnd()) return;
    CRect area; folders->GetClientRect(area);
    int width = area.Width();
    int unread = ClassicUI::Scale(owner, news_mode ? 16 : 42), total = ClassicUI::Scale(owner, news_mode ? 42 : 46);
    int label_min = ClassicUI::Scale(owner, 60);
    if (width < label_min + unread + total) {
        int numeric = width > label_min ? width - label_min : 0;
        unread = MulDiv(numeric, news_mode ? 16 : 42, news_mode?58:88); total = numeric - unread;
    }
    folders->SetColumnWidth(0, width - unread - total);
    folders->SetColumnWidth(1, unread); folders->SetColumnWidth(2, total);
    messages->GetClientRect(area); width = area.Width();
    int sender = ClassicUI::Scale(owner, 150), status = ClassicUI::Scale(owner, 16);
    int subject = ClassicUI::Scale(owner, 150), date_min = ClassicUI::Scale(owner, 70);
    int nominal = sender + status * 2 + subject + date_min;
    if (width < nominal) {
        sender = MulDiv(width, 150, 402); status = MulDiv(width, 16, 402);
        subject = MulDiv(width, 150, 402);
    }
    messages->SetColumnWidth(SenderColumn, sender);
    messages->SetColumnWidth(PriorityColumn, status); messages->SetColumnWidth(FlagColumn, status);
    messages->SetColumnWidth(SubjectColumn, subject);
    messages->SetColumnWidth(DateColumn, width-sender-status*2-subject);
}

int MailPaneUI::FolderImage(unsigned long flags) const
{
    if(news_mode && (flags&MSG_FOLDER_FLAG_NEWS_HOST)) return flags&MSG_FOLDER_FLAG_ELIDED?0:2;
    if(news_mode && (flags&MSG_FOLDER_FLAG_NEWSGROUP) && !(flags&MSG_FOLDER_FLAG_DIRECTORY)) return 3;
    return (flags & MSG_FOLDER_FLAG_DIRECTORY) && !(flags & MSG_FOLDER_FLAG_ELIDED) ? 1 : 0;
}

void MailPaneUI::SortColumn(int column,bool reverse)
{
    CHeaderCtrl *header=messages->GetHeaderCtrl();
    for(int i=0;i<header->GetItemCount();++i) {
        HDITEM item={};item.mask=HDI_FORMAT;header->GetItem(i,&item);
        item.fmt&=~(HDF_SORTUP|HDF_SORTDOWN);
        if(i==column) item.fmt|=reverse?HDF_SORTUP:HDF_SORTDOWN;
        header->SetItem(i,&item);
    }
}

int MailPaneUI::MessageImage(unsigned long flags) const
{
    return flags & MSG_FLAG_READ ? 0 : 1;
}

void MailPaneUI::SetDividers(int width, int height)
{
    /* Only an explicit -1 restore requests the initial list height. */
    folder_width = width; top_height = height == -1 ? -1 : height < 0 ? 0 : height;
    if (arranged) Arrange(top, footer);
}

void MailPaneUI::DragDivider(bool vertical, CPoint point, int offset)
{
    if (!owner || !owner->GetSafeHwnd()) return;
    owner->ScreenToClient(&point);
    if (vertical) folder_width = point.x - offset;
    else {
        int height = point.y - offset - top;
        top_height = height < 0 ? 0 : height;
    }
    Arrange(top, footer);
}

int MailPaneUI::SubscriptionImage(unsigned long flags) const
{
    return flags&MSG_FOLDER_FLAG_DIRECTORY ? I_IMAGENONE : flags&MSG_FOLDER_FLAG_SUBSCRIBED ? 4 : 5;
}

bool MailPaneUI::NewsExpansionHit(int row,CPoint point) const
{
    if(!news_mode || row<0) return false;
    DWORD_PTR data=folders->GetItemData(row);
    int left=ClassicUI::Scale(owner,4+int(data>>16)*16);
    return (data&MSG_FOLDER_FLAG_DIRECTORY) && point.x>=left && point.x<left+ClassicUI::Scale(owner,10);
}

void MailPaneUI::DrawNewsFolder(NMLVCUSTOMDRAW *draw,LRESULT *result)
{
    *result=CDRF_DODEFAULT;
    if(draw->nmcd.dwDrawStage==CDDS_PREPAINT) { *result=CDRF_NOTIFYITEMDRAW;return; }
    if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) { *result=CDRF_NOTIFYSUBITEMDRAW;return; }
    if(draw->nmcd.dwDrawStage!=(CDDS_ITEMPREPAINT|CDDS_SUBITEM) || draw->iSubItem) return;
    int row=(int)draw->nmcd.dwItemSpec;DWORD_PTR data=folders->GetItemData(row);
    unsigned flags=(unsigned)(data&0xffff);int depth=(int)(data>>16);
    CRect area;folders->GetSubItemRect(row,0,LVIR_BOUNDS,area);area.right=folders->GetColumnWidth(0);
    CDC *dc=CDC::FromHandle(draw->nmcd.hdc);int saved=dc->SaveDC();dc->IntersectClipRect(area);
    bool selected=(folders->GetItemState(row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
    bool focused=folders->GetFocus()==folders;
    dc->FillSolidRect(area,GetSysColor(selected?(focused?COLOR_HIGHLIGHT:COLOR_BTNFACE):COLOR_WINDOW));
    dc->SetTextColor(GetSysColor(selected&&focused?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT));dc->SetBkMode(TRANSPARENT);
    auto scale=[this](int n){return ClassicUI::Scale(owner,n);};
    int x=area.left+scale(4+depth*16),middle=area.CenterPoint().y;
    CPen guide(PS_DOT,1,GetSysColor(selected&&focused?COLOR_HIGHLIGHTTEXT:COLOR_3DSHADOW));dc->SelectObject(&guide);
    if(depth||!news_mode) {
        int stem=depth?x-scale(12):x+scale(4);
        bool continues=row+1<folders->GetItemCount()&&int(folders->GetItemData(row+1)>>16)>=depth;
        dc->MoveTo(stem,area.top);dc->LineTo(stem,continues?area.bottom:middle);
        dc->MoveTo(stem,middle);dc->LineTo(x+scale(12),middle);
    }
    if(flags&MSG_FOLDER_FLAG_DIRECTORY) {
        CRect glyph(x,middle-scale(4),x+scale(9),middle+scale(5));dc->FillSolidRect(glyph,RGB(255,255,255));
        dc->Draw3dRect(glyph,RGB(128,128,128),RGB(128,128,128));
        dc->FillSolidRect(x+scale(2),middle,scale(5),1,RGB(0,0,0));
        if(flags&MSG_FOLDER_FLAG_ELIDED) dc->FillSolidRect(x+scale(4),middle-scale(2),1,scale(5),RGB(0,0,0));
    }
    int image=FolderImage(flags);folder_images.Draw(dc,image,CPoint(x+scale(12),middle-scale(8)),ILD_TRANSPARENT);
    CRect label=area;label.left=x+scale(news_mode?40:36);
    bool unread=news_mode&&!(flags&MSG_FOLDER_FLAG_DIRECTORY)&&folders->GetItemText(row,2)!="0";
    dc->SelectObject(unread?&unread_font:ClassicUI::Font(owner));
    CString text=folders->GetItemText(row,0);dc->DrawText(text,label,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    if(selected&&folders->GetFocus()==folders) { CRect focus=area;focus.DeflateRect(1,1);dc->DrawFocusRect(focus); }
    dc->RestoreDC(saved);*result=CDRF_SKIPDEFAULT;
}
