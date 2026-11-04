#include <afxwin.h>
#include "editor.h"
#ifdef EDITOR
#include "editor_resource.h"
#include "printing.h"
#include "editor-create.h"
#include "editor-publish.h"
#include "resource.h"
#include "menu_resource.h"
#include "classic.h"
#include "find.h"
#include "window-menu.h"
#include "edt.h"
#include "pa_tags.h"
#include <afxdlgs.h>
#include <afxcmn.h>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

namespace {
class GoldChrome;
std::map<MWContext *,std::unique_ptr<GoldChrome>> editors;
const struct { UINT command; TagType tag; const char *label; } paragraph_styles[] = {
    {36012,P_PARAGRAPH,"Normal"}, {36005,P_HEADER_1,"Heading 1"},
    {36006,P_HEADER_2,"Heading 2"}, {36007,P_HEADER_3,"Heading 3"},
    {36008,P_HEADER_4,"Heading 4"}, {36009,P_HEADER_5,"Heading 5"},
    {36010,P_HEADER_6,"Heading 6"}, {36013,P_ADDRESS,"Address"},
    {36017,P_PREFORMAT,"Formatted"}, {36023,P_LIST_ITEM,"List Item"},
    {36025,P_DESC_TITLE,"Description Title"}, {36026,P_DESC_TEXT,"Description Text"}
};

bool ready(MWContext *context)
{
    return context && context->is_editor && EDT_HaveEditBuffer(context) && !EDT_IsBlocked(context);
}

ED_TextFormat style_bit(UINT command)
{
    switch(command) {
    case ID_EDITOR_BOLD: return TF_BOLD;
    case ID_EDITOR_ITALIC: return TF_ITALIC;
    case ID_EDITOR_FIXED: return TF_FIXED;
    case ID_EDITOR_SUPER: return TF_SUPER;
    case ID_EDITOR_SUB: return TF_SUB;
    case ID_EDITOR_STRIKE: return TF_STRIKEOUT;
    case ID_EDITOR_BLINK: return TF_BLINK;
    case ID_EDITOR_UNDERLINE: return TF_UNDERLINE;
    }
    return TF_NONE;
}

bool style_checked(MWContext *context,ED_TextFormat bit)
{
    EDT_CharacterData *data=EDT_GetCharacterData(context);
    bool checked=data && (data->mask&bit) && (data->values&bit);
    if(data) EDT_FreeCharacterData(data);
    return checked;
}

void toggle_style(MWContext *context,ED_TextFormat bit)
{
    EDT_CharacterData *data=EDT_NewCharacterData();
    if(!data) return;
    data->mask=bit;data->values=style_checked(context,bit) ? 0 : bit;
    EDT_SetCharacterData(context,data);EDT_FreeCharacterData(data);
}

class GoldProperties : public ClassicDialog {
public:
    CString title, labels[3], values[3];
    GoldProperties(CWnd *owner,const char *caption):ClassicDialog(IDD_EDITOR_PROPERTIES,owner),title(caption) {}
protected:
    BOOL OnInitDialog() override {
        ClassicDialog::OnInitDialog();SetWindowText(title);
        for(int i=0;i<3;i++) {
            SetDlgItemText(IDC_EDITOR_LABEL1+i,labels[i]);
            SetDlgItemText(IDC_EDITOR_VALUE1+i,values[i]);
            if(labels[i].IsEmpty()) {
                GetDlgItem(IDC_EDITOR_LABEL1+i)->ShowWindow(SW_HIDE);
                GetDlgItem(IDC_EDITOR_VALUE1+i)->ShowWindow(SW_HIDE);
            }
        }
        return TRUE;
    }
    void OnOK() override {
        for(int i=0;i<3;i++) GetDlgItemText(IDC_EDITOR_VALUE1+i,values[i]);
        ClassicDialog::OnOK();
    }
};

/* The Gold strips contain 20 by 18 pixel cells, wider than Navigator icons. */
class GoldButton : public ClassicButton {
public:
    CBitmap artwork, disabled_mask;
    void SetArtwork(UINT resource) {
        artwork.DeleteObject();disabled_mask.DeleteObject();
        if(!artwork.LoadBitmap(resource)) return;
        CDC source,mask;source.CreateCompatibleDC(nullptr);mask.CreateCompatibleDC(nullptr);
        if(!disabled_mask.CreateBitmap(20,18,1,1,nullptr)) return;
        CBitmap *old_source=source.SelectObject(&artwork),*old_mask=mask.SelectObject(&disabled_mask);
        /* DrawState's color-to-mono conversion treats the gray sprite background
         * as ink. Build its mask explicitly: face and white highlight pixels
         * are transparent; the remaining pixels form the embossed glyph. */
        COLORREF old_background=source.SetBkColor(ClassicUI::Face);
        mask.BitBlt(0,0,20,18,&source,0,0,SRCCOPY);
        source.SetBkColor(RGB(255,255,255));
        mask.BitBlt(0,0,20,18,&source,0,0,SRCPAINT);
        source.SetBkColor(old_background);
        source.SelectObject(old_source);mask.SelectObject(old_mask);
    }
    void DrawItem(LPDRAWITEMSTRUCT item) override {
        CDC dc;dc.Attach(item->hDC);CRect area(item->rcItem),inside(area);
        bool down=(item->itemState&ODS_SELECTED)!=0,disabled=(item->itemState&ODS_DISABLED)!=0;
        dc.FillSolidRect(area,ClassicUI::Face);
        dc.Draw3dRect(area,down ? RGB(128,128,128) : RGB(255,255,255),down ? RGB(255,255,255) : RGB(128,128,128));
        inside.DeflateRect(1,1);
        dc.Draw3dRect(inside,down ? RGB(64,64,64) : ClassicUI::Face,down ? ClassicUI::Face : RGB(64,64,64));
        if(artwork.GetSafeHandle()) {
            int width=ClassicUI::Scale(this,20),height=ClassicUI::Scale(this,18);
            int x=area.CenterPoint().x-width/2+(down ? 1 : 0),y=area.CenterPoint().y-height/2+(down ? 1 : 0);
            if(disabled && disabled_mask.GetSafeHandle()) dc.DrawState(CPoint(x,y),CSize(width,height),&disabled_mask,DST_BITMAP|DSS_DISABLED);
            else {CDC memory;memory.CreateCompatibleDC(&dc);CBitmap *old=memory.SelectObject(&artwork);
                dc.StretchBlt(x,y,width,height,&memory,0,0,20,18,SRCCOPY);memory.SelectObject(old);}
        }
        if(item->itemState&ODS_FOCUS) {inside.DeflateRect(2,2);dc.DrawFocusRect(inside);}
        dc.Detach();
    }
};

class GoldChrome : public CWnd {
public:
    MWContext *context=nullptr;
    CComboBox paragraph, size;
    CToolTipCtrl tips;
    std::vector<std::unique_ptr<GoldButton>> buttons;
    std::vector<int> rows;
    bool visible[3]={true,true,true};
    bool preview_after_save=false;
    bool Create(MWContext *value,CWnd *owner) {
        context=value;
        if(!CreateEx(0,AfxRegisterWndClass(0,LoadCursor(NULL,IDC_ARROW)),"Gold editor toolbars",
                     WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,CRect(0,0,0,0),owner,ID_EDITOR_CHROME)) return false;
        tips.Create(this,TTS_ALWAYSTIP);
        static const struct {const char *label;UINT command;int icon;} file[] = {
            {"New",ID_EDITOR_NEW,0},{"Open",ID_EDITOR_OPEN,1},{"Save",ID_EDITOR_SAVE,2},
            {"Browse",ID_EDITOR_BROWSE,3},{"Cut",ID_EDIT_CUT,4},{"Copy",ID_EDIT_COPY,5},
            {"Paste",ID_EDIT_PASTE,6},{"Print",ID_FILE_PRINT,7},{"Find",ID_EDITOR_FIND,8},
            {"Publish",ID_EDITOR_PUBLISH,9}
        };
        for(const auto &item:file) Add(item.label,item.command,IDB_GOLD_FILE_FIRST+item.icon,0);
        paragraph.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,CRect(0,0,0,180),this,ID_EDITOR_STYLE_COMBO);
        for(const auto &style:paragraph_styles) paragraph.AddString(style.label);
        size.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,CRect(0,0,0,160),this,ID_EDITOR_SIZE_COMBO);
        const char *sizes[]={"-2","-1","+0","+1","+2","+3","+4"};
        for(auto label:sizes) size.AddString(label);
        static const struct {const char *label;UINT command;int icon;} character[] = {
            {"Smaller",ID_EDITOR_SMALLER,0},{"Larger",ID_EDITOR_LARGER,1},
            {"Bold",ID_EDITOR_BOLD,2},{"Italic",ID_EDITOR_ITALIC,3},{"Fixed Width",ID_EDITOR_FIXED,4},
            {"Text Color",ID_EDITOR_COLOR,5},{"Link",ID_EDITOR_LINK,6},{"Remove Links",ID_EDITOR_UNLINK,7},
            {"Target",ID_EDITOR_TARGET,8},{"Image",ID_EDITOR_IMAGE,9},{"Horizontal Line",ID_EDITOR_HRULE,10},
            {"Table",ID_EDITOR_TABLE,11},{"Table Properties",ID_EDITOR_TABLE_PROPERTIES,12}
        };
        for(const auto &item:character) Add(item.label,item.command,IDB_GOLD_CHAR_FIRST+item.icon,1);
        static const struct {const char *label;UINT command;} format[] = {
            {"Bulleted List",ID_EDITOR_BULLETS},{"Numbered List",ID_EDITOR_NUMBERS},
            {"Remove Indent",ID_EDITOR_OUTDENT},{"Indent",ID_EDITOR_INDENT},
            {"Align Left",ID_EDITOR_ALIGN_LEFT},{"Center",ID_EDITOR_ALIGN_CENTER},{"Align Right",ID_EDITOR_ALIGN_RIGHT}
        };
        for(int i=0;i<7;i++) Add(format[i].label,format[i].command,IDB_GOLD_PARA_FIRST+i,2);
        ClassicUI::Apply(this);Update();return true;
    }
    void Add(const char *label,UINT command,UINT bitmap,int row) {
        std::unique_ptr<GoldButton> button(new GoldButton);
        button->Create(label,WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,CRect(0,0,0,0),this,command);
        button->SetArtwork(bitmap);
        tips.AddTool(button.get(),label);buttons.push_back(std::move(button));rows.push_back(row);
    }
    int Arrange(int width) {
        int y=0;
        const int order[]={0,2,1};
        for(int index=0;index<3;index++) {
            int row=order[index];
            int height=ClassicUI::Scale(this,row==1 ? 38 : 33),x=ClassicUI::Scale(this,8);
            if(row==2) x=ClassicUI::Scale(this,136);
            for(size_t i=0;i<buttons.size();i++) if(rows[i]==row) {
                buttons[i]->ShowWindow(visible[row] ? SW_SHOWNA : SW_HIDE);
                if(visible[row]) {
                    if(row==1 && buttons[i]->GetDlgCtrlID()==ID_EDITOR_BOLD) x+=ClassicUI::Scale(this,48);
                    int w=ClassicUI::Scale(this,27);
                    buttons[i]->MoveWindow(x,y+ClassicUI::Scale(this,4),w,ClassicUI::Scale(this,26));x+=ClassicUI::Scale(this,30);
                }
            }
            if(row==2) {
                paragraph.ShowWindow(visible[row] ? SW_SHOWNA : SW_HIDE);
                if(visible[row]) paragraph.MoveWindow(ClassicUI::Scale(this,8),y+ClassicUI::Scale(this,5),ClassicUI::Scale(this,118),ClassicUI::Scale(this,230));
            }
            if(row==1) {
                size.ShowWindow(visible[row] ? SW_SHOWNA : SW_HIDE);
                if(visible[row]) size.MoveWindow(ClassicUI::Scale(this,68),y+ClassicUI::Scale(this,5),ClassicUI::Scale(this,40),ClassicUI::Scale(this,180));
            }
            if(visible[row]) y+=height;
        }
        MoveWindow(0,0,width,y);Invalidate();return y;
    }
    void Update() {
        bool active=ready(context);
        for(auto &button:buttons) {
            CCmdUI command;command.m_nID=button->GetDlgCtrlID();command.m_pOther=button.get();
            if(!WFE_EditorUpdate(context,&command)) button->EnableWindow(TRUE);
        }
        paragraph.EnableWindow(active);size.EnableWindow(active && EDT_CanSetCharacterAttribute(context));
        if(!active) return;
        TagType tag=EDT_GetParagraphFormatting(context);
        for(size_t i=0;i<sizeof(paragraph_styles)/sizeof(paragraph_styles[0]);i++)
            if(paragraph_styles[i].tag==tag) paragraph.SetCurSel((int)i);
        size.SetCurSel(EDT_GetFontSize(context)-1);
        for(auto &button:buttons) {
            ED_TextFormat bit=style_bit(button->GetDlgCtrlID());
            if(bit) button->SetState(style_checked(context,bit));
        }
    }
protected:
    BOOL OnCommand(WPARAM command,LPARAM control) override {
        UINT id=LOWORD(command);
        if(HIWORD(command)==CBN_SELENDOK && ready(context)) {
            if(id==ID_EDITOR_STYLE_COMBO && paragraph.GetCurSel()>=0)
                EDT_MorphContainer(context,paragraph_styles[paragraph.GetCurSel()].tag);
            else if(id==ID_EDITOR_SIZE_COMBO && size.GetCurSel()>=0)
                EDT_SetFontSize(context,size.GetCurSel()+1);
            else return CWnd::OnCommand(command,control);
            WFE_EditorFormattingChanged(context);::SetFocus(WFE_DocumentWindow(context));WFE_EditorRefreshCaret(context);return TRUE;
        }
        if(HIWORD(command)==0 && WFE_EditorCommand(context,id)) return TRUE;
        return GetParent()->SendMessage(WM_COMMAND,command,control)!=0;
    }
    BOOL PreTranslateMessage(MSG *message) override {tips.RelayEvent(message);return CWnd::PreTranslateMessage(message);}
    afx_msg BOOL OnEraseBkgnd(CDC *dc) {
        CRect area;GetClientRect(area);dc->FillSolidRect(area,ClassicUI::Face);
        int y=0;
        const int order[]={0,2,1};
        for(int index=0;index<3;index++) if(visible[order[index]]) {
            int row=order[index];
            dc->FillSolidRect(0,y,area.Width(),1,RGB(255,255,255));
            y+=ClassicUI::Scale(this,row==1 ? 38 : 33);
            dc->FillSolidRect(0,y-1,area.Width(),1,RGB(128,128,128));
        }
        dc->FillSolidRect(0,area.bottom-4,area.Width(),1,RGB(255,0,0));
        dc->FillSolidRect(0,area.bottom-2,area.Width(),1,RGB(0,128,128));
        return TRUE;
    }
    afx_msg HBRUSH OnCtlColor(CDC *dc,CWnd *window,UINT type) {return ClassicUI::ControlColor(dc,window,type);}
    afx_msg void OnTimer(UINT_PTR timer) {
        if(timer==1 && preview_after_save && !WFE_EditorSaving(context)) {
            preview_after_save=false;KillTimer(1);Update();
            if(WFE_EditorSaveStatus(context)==ED_ERROR_NONE) WFE_NewWindow(WFE_CurrentDocumentURL(context));
        }
        CWnd::OnTimer(timer);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(GoldChrome,CWnd)
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
 ON_WM_TIMER()
END_MESSAGE_MAP()

GoldChrome *chrome(MWContext *context)
{
    auto it=editors.find(context);return it==editors.end() ? nullptr : it->second.get();
}
CWnd *owner(MWContext *context) {GoldChrome *ui=chrome(context);return ui ? ui->GetParent() : CWnd::FromHandle(WFE_DocumentWindow(context));}

void changed(MWContext *context)
{
    WFE_EditorFormattingChanged(context);
    HWND document=WFE_DocumentWindow(context);
    if(document) ::SetFocus(document);
    WFE_EditorRefreshCaret(context);
}

bool properties(MWContext *context,UINT command)
{
    if(command==ID_EDITOR_LINK || command==ID_EDITOR_LINK_PROPERTIES) {
        GoldProperties dialog(owner(context),"Link Properties");
        dialog.labels[0]="Link to:";
        if(!EDT_CanSetHREF(context)) dialog.labels[1]="Link text:";
        char *url=EDT_GetHREF(context),*text=EDT_GetHREFText(context);
        dialog.values[0]=url ? url : "";dialog.values[1]=text ? text : "";
        /* EDT_GetHREF returns the engine's borrowed link-manager string. */
        if(text) XP_FREE(text);
        if(dialog.DoModal()==IDOK && (!dialog.values[0].IsEmpty() || EDT_CanSetHREF(context))) {
            CString address=dialog.values[0],title=dialog.values[1];
            if(EDT_CanSetHREF(context)) EDT_SetHREF(context,address.IsEmpty() ? nullptr : address.GetBuffer());
            else {if(title.IsEmpty()) title=address;char *href=address.GetBuffer(),*label=title.GetBuffer();EDT_PasteHREF(context,&href,&label,1);}
        }
        return true;
    }
    if(command==ID_EDITOR_IMAGE || command==ID_EDITOR_IMAGE_PROPERTIES) {
        bool edit=command==ID_EDITOR_IMAGE_PROPERTIES;
        EDT_ImageData *data=edit ? EDT_GetImageData(context) : EDT_NewImageData();
        if(!data) return true;
        GoldProperties dialog(owner(context),"Image Properties");
        dialog.labels[0]="Image location:";dialog.labels[1]="Alternate text:";dialog.labels[2]="Width (pixels):";
        dialog.values[0]=data->pSrc ? data->pSrc : "";dialog.values[1]=data->pAlt ? data->pAlt : "";dialog.values[2].Format("%ld",(long)data->iWidth);
        if(dialog.DoModal()==IDOK && !dialog.values[0].IsEmpty()) {
            if(data->pSrc) XP_FREE(data->pSrc);if(data->pAlt) XP_FREE(data->pAlt);
            data->pSrc=XP_STRDUP((LPCSTR)dialog.values[0]);data->pAlt=XP_STRDUP((LPCSTR)dialog.values[1]);
            data->iWidth=max(0,atoi(dialog.values[2]));data->bWidthPercent=FALSE;
            if(edit) EDT_SetImageData(context,data,TRUE);else EDT_InsertImage(context,data,TRUE);
        }
        EDT_FreeImageData(data);return true;
    }
    if(command==ID_EDITOR_TABLE || command==ID_EDITOR_TABLE_PROPERTIES) {
        bool edit=command==ID_EDITOR_TABLE_PROPERTIES;
        EDT_TableData *data=edit ? EDT_GetTableData(context) : EDT_NewTableData();
        if(!data) return true;
        GoldProperties dialog(owner(context),"Table Properties");
        dialog.labels[0]="Rows:";dialog.labels[1]="Columns:";dialog.labels[2]="Border width:";
        dialog.values[0].Format("%ld",(long)data->iRows);dialog.values[1].Format("%ld",(long)data->iColumns);dialog.values[2].Format("%ld",(long)data->iBorderWidth);
        if(dialog.DoModal()==IDOK) {
            int rows=atoi(dialog.values[0]),columns=atoi(dialog.values[1]),border=atoi(dialog.values[2]);
            if(rows>0 && rows<=100 && columns>0 && columns<=100 && border>=0 && border<=100) {
                data->iRows=rows;data->iColumns=columns;data->iBorderWidth=border;
                if(edit) EDT_SetTableData(context,data);else EDT_InsertTable(context,data);
            } else ::MessageBox(owner(context)->GetSafeHwnd(),"Rows and columns must be between 1 and 100; border width must be between 0 and 100.","Table Properties",MB_OK|MB_ICONEXCLAMATION);
        }
        EDT_FreeTableData(data);return true;
    }
    if(command==ID_EDITOR_HRULE_PROPERTIES) {
        EDT_HorizRuleData *data=EDT_GetHorizRuleData(context);if(!data) return true;
        GoldProperties dialog(owner(context),"Horizontal Line Properties");
        dialog.labels[0]="Width (percent):";dialog.labels[1]="Height (pixels):";
        dialog.values[0].Format("%ld",(long)data->iWidth);dialog.values[1].Format("%ld",(long)data->size);
        if(dialog.DoModal()==IDOK) {
            data->iWidth=max(1,min(100,atoi(dialog.values[0])));data->bWidthPercent=TRUE;
            data->size=max(1,min(100,atoi(dialog.values[1])));EDT_SetHorizRuleData(context,data);
        }
        EDT_FreeHorizRuleData(data);return true;
    }
    if(command==ID_EDITOR_DOCUMENT_PROPERTIES) {
        EDT_PageData *data=EDT_GetPageData(context);if(!data) return true;
        GoldProperties dialog(owner(context),"Document Properties");
        dialog.labels[0]="Title:";dialog.labels[1]="Background image:";
        dialog.values[0]=data->pTitle ? data->pTitle : "";dialog.values[1]=data->pBackgroundImage ? data->pBackgroundImage : "";
        if(dialog.DoModal()==IDOK) {
            if(data->pTitle) XP_FREE(data->pTitle);if(data->pBackgroundImage) XP_FREE(data->pBackgroundImage);
            data->pTitle=XP_STRDUP((LPCSTR)dialog.values[0]);data->pBackgroundImage=dialog.values[1].IsEmpty() ? nullptr : XP_STRDUP((LPCSTR)dialog.values[1]);
            EDT_SetPageData(context,data);
        }
        EDT_FreePageData(data);return true;
    }
    if(command==ID_EDITOR_TARGET || command==ID_EDITOR_TARGET_PROPERTIES || command==ID_EDITOR_TAG || command==ID_EDITOR_TAG_PROPERTIES) {
        bool target=command==ID_EDITOR_TARGET || command==ID_EDITOR_TARGET_PROPERTIES;
        bool edit=command==ID_EDITOR_TARGET_PROPERTIES || command==ID_EDITOR_TAG_PROPERTIES;
        GoldProperties dialog(owner(context),target ? "Target Properties" : "HTML Tag Properties");
        dialog.labels[0]=target ? "Target name:" : "HTML tag:";
        char *text=edit ? (target ? EDT_GetTargetData(context) : EDT_GetUnknownTagData(context)) : nullptr;
        dialog.values[0]=text ? text : "";if(text) XP_FREE(text);
        if(dialog.DoModal()==IDOK && !dialog.values[0].IsEmpty()) {
            char *value=dialog.values[0].GetBuffer();
            if(target) {if(edit) EDT_SetTargetData(context,value);else EDT_InsertTarget(context,value);}
            else if(EDT_ValidateTag(value,FALSE)==ED_TAG_OK) {if(edit) EDT_SetUnknownTagData(context,value);else EDT_InsertUnknownTag(context,value);}
            else WFE_Message(context,"The HTML tag has unmatched brackets or quotes.",0);
        }
        return true;
    }
    if(command==ID_EDITOR_TEXT_PROPERTIES) {
        GoldProperties dialog(owner(context),"Text Properties");dialog.labels[0]="Font size (1-7):";dialog.values[0].Format("%d",EDT_GetFontSize(context));
        if(dialog.DoModal()==IDOK) EDT_SetFontSize(context,max(1,min(7,atoi(dialog.values[0]))));
        return true;
    }
    return false;
}

UINT clipboard_format() {static UINT format=RegisterClipboardFormatA("NETSCAPE_EDITOR");return format;}
HGLOBAL clipboard_block(const char *data,size_t length)
{
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,length+1);
    if(memory) {void *bytes=GlobalLock(memory);if(!bytes) {GlobalFree(memory);return nullptr;}memcpy(bytes,data,length);GlobalUnlock(memory);}
    return memory;
}

bool copy_selection(MWContext *context,bool cut)
{
    char *text=nullptr;XP_HUGE_CHAR_PTR html=nullptr;int32 text_length=0,html_length=0;
    if(EDT_CopySelection(context,&text,&text_length,&html,&html_length)!=EDT_COP_OK) return false;
    HGLOBAL plain=text ? clipboard_block(text,text_length) : nullptr;
    HGLOBAL rich=html ? clipboard_block(html,html_length) : nullptr;
    if(text) XP_FREE(text);if(html) XP_HUGE_FREE(html);
    bool success=false;
    if(plain && rich && OpenClipboard(WFE_DocumentWindow(context))) {
        if(EmptyClipboard()) {
            bool a=SetClipboardData(CF_TEXT,plain)!=nullptr;if(a) plain=nullptr;
            bool b=SetClipboardData(clipboard_format(),rich)!=nullptr;if(b) rich=nullptr;
            success=a && b;
        }
        CloseClipboard();
    }
    if(plain) GlobalFree(plain);if(rich) GlobalFree(rich);
    /* Delete only after both clipboard representations have been accepted. */
    if(cut && success) EDT_DeleteChar(context);
    return success;
}

bool paste_selection(MWContext *context)
{
    if(!OpenClipboard(WFE_DocumentWindow(context))) return false;
    UINT external=RegisterClipboardFormatA("HTML Format"),format=0;
    if(IsClipboardFormatAvailable(clipboard_format())) format=clipboard_format();
    else if(IsClipboardFormatAvailable(external)) format=external;
    else if(IsClipboardFormatAvailable(CF_TEXT)) format=CF_TEXT;
    else if(IsClipboardFormatAvailable(CF_UNICODETEXT)) format=CF_UNICODETEXT;
    HANDLE memory=format ? GetClipboardData(format) : nullptr;
    const char *bytes=memory ? (const char *)GlobalLock(memory) : nullptr;
    std::vector<char> contents;
    if(bytes) {
        size_t length=GlobalSize(memory);
        if(format==CF_UNICODETEXT) {
            int count=0;const wchar_t *wide=(const wchar_t *)bytes;
            while((size_t)(count+1)*sizeof(wchar_t)<=length && wide[count]) ++count;
            int size=WideCharToMultiByte(CP_ACP,0,wide,count,nullptr,0,nullptr,nullptr);
            contents.resize(size+1);if(size) WideCharToMultiByte(CP_ACP,0,wide,count,contents.data(),size,nullptr,nullptr);
        } else {contents.assign(bytes,bytes+length);contents.push_back(0);}
        GlobalUnlock(memory);
    }
    CloseClipboard();if(contents.empty()) return false;
    if(format==clipboard_format()) return EDT_PasteHTML(context,contents.data())==EDT_COP_OK;
    if(format==external) {
        std::string html(contents.data());size_t begin=html.find("<!--StartFragment-->"),end=html.find("<!--EndFragment-->");
        if(begin!=std::string::npos && end!=std::string::npos && end>=begin+20) html=html.substr(begin+20,end-begin-20);
        std::vector<char> fragment(html.begin(),html.end());fragment.push_back(0);
        return EDT_PasteExternalHTML(context,fragment.data())==EDT_COP_OK;
    }
    return EDT_PasteText(context,contents.data())==EDT_COP_OK;
}

bool unavailable(UINT command)
{
    switch(command) {
    case 32810:case 32884:case 32883:case 32878:case 42293:case 42294:case 32839:
        return true;
    }
    return (command>=33115 && command<=34377 && command!=33115 && command!=ID_EDITOR_SELECT_ALL);
}
}

bool WFE_EditorCreateUI(MWContext *context,CWnd *owner)
{
    if(chrome(context)) return true;
    std::unique_ptr<GoldChrome> ui(new GoldChrome);
    if(!ui->Create(context,owner)) return false;
    editors[context]=std::move(ui);return true;
}
int WFE_EditorArrange(MWContext *context,int width) {GoldChrome *ui=chrome(context);return ui ? ui->Arrange(width) : 0;}
void WFE_EditorDestroyUI(MWContext *context) {auto it=editors.find(context);if(it!=editors.end()) {it->second->DestroyWindow();editors.erase(it);}}
extern "C" void WFE_EditorDocumentLoaded(MWContext *context) {GoldChrome *ui=chrome(context);if(ui) ui->Update();WFE_EditorRefreshCaret(context);}
extern "C" void WFE_EditorFormattingChanged(MWContext *context) {GoldChrome *ui=chrome(context);if(ui) ui->Update();}

bool WFE_EditorCommand(MWContext *context,UINT command)
{
    if(!context || !context->is_editor) return false;
    if(WFE_PrintCommand(context,command) || WFE_EditorWorkflowCommand(context,command) || WFE_EditorPublishCommand(context,command)) return true;
    if(command==ID_WINDOW_BOOKMARKS) return WFE_WindowCommand(owner(context),command);
    if(unavailable(command)) return true;
    switch(command) {
    case ID_EDITOR_NEW:WFE_NewEditorWindow(nullptr);return true;
    case ID_EDITOR_OPEN: {
        CFileDialog dialog(TRUE,"htm",nullptr,OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR,"HTML Files (*.htm;*.html)|*.htm;*.html|All Files (*.*)|*.*||",owner(context));
        if(dialog.DoModal()==IDOK) WFE_NewEditorWindow(dialog.GetPathName());return true;
    }
    case ID_EDITOR_OPEN_LOCATION: {
        char *address=WFE_NativePrompt(context,"Open location in editor:","",0);
        if(address) {if(*address) WFE_NewEditorWindow(address);XP_FREE(address);}return true;
    }
    case ID_EDITOR_SAVE:case ID_EDITOR_SAVE_AS:WFE_EditorSave(context,nullptr,command==ID_EDITOR_SAVE_AS);return true;
    case ID_EDITOR_BROWSE:
        if(EDT_DirtyFlag(context) || context->is_new_document) {
            if(WFE_EditorSave(context,nullptr,0)>0) {
                GoldChrome *ui=chrome(context);
                if(ui) {ui->preview_after_save=true;ui->SetTimer(1,100,nullptr);}
            }
        } else WFE_NewWindow(WFE_CurrentDocumentURL(context));
        return true;
    case ID_EDITOR_FILEBAR:case ID_EDITOR_CHARBAR:case ID_EDITOR_PARABAR: {
        GoldChrome *ui=chrome(context);
        if(ui) {int row=command==ID_EDITOR_FILEBAR ? 0 : (command==ID_EDITOR_CHARBAR ? 1 : 2);ui->visible[row]=!ui->visible[row];
            CRect area;ui->GetParent()->GetClientRect(area);ui->GetParent()->SendMessage(WM_SIZE,SIZE_RESTORED,MAKELPARAM(area.Width(),area.Height()));}
        return true;
    }
    case ID_EDITOR_FIND:WFE_FindShow(context);return true;
    case ID_EDITOR_MAIL_PREFERENCES:WFE_ShowMailSettings(context);return true;
    case ID_EDITOR_FIND_AGAIN:WFE_FindAgain(context);return true;
    case ID_EDITOR_SOURCE:EDT_DisplaySource(context);return true;
    case ID_EDITOR_INFO: {
        CString message;message.Format("Document: %s\r\nEditor: Netscape Gold\r\nModified: %s",WFE_CurrentDocumentURL(context),EDT_DirtyFlag(context) ? "Yes" : "No");
        WFE_Message(context,message,0);return true;
    }
    case ID_EDITOR_RELOAD:
        if(WFE_EditorCheckSave(context)) {
            std::string address=WFE_CurrentDocumentURL(context);
            if(!address.empty()) WFE_Load(context,address.c_str());
        }
        return true;
    case ID_EDITOR_IMAGES:case ID_EDITOR_REFRESH:if(ready(context)) EDT_RefreshLayout(context);return true;
    }
    if(!ready(context)) return false;
    ED_TextFormat bit=style_bit(command);
    if(bit) {toggle_style(context,bit);changed(context);return true;}
    if(command>=ID_EDITOR_SIZE_FIRST && command<=ID_EDITOR_SIZE_LAST) {EDT_SetFontSize(context,command-ID_EDITOR_SIZE_FIRST+1);changed(context);return true;}
    for(const auto &style:paragraph_styles) if(style.command==command) {EDT_MorphContainer(context,style.tag);changed(context);return true;}
    if(properties(context,command)) {changed(context);return true;}
    switch(command) {
    case ID_EDIT_UNDO:EDT_Undo(context);break;
    case ID_EDIT_REDO:EDT_Redo(context);break;
    case ID_EDIT_COPY:copy_selection(context,false);break;
    case ID_EDIT_CUT:copy_selection(context,true);break;
    case ID_EDIT_PASTE:paste_selection(context);break;
    case ID_EDIT_SELECT_ALL:case ID_EDITOR_SELECT_ALL:EDT_SelectAll(context);break;
    case ID_EDITOR_DELETE:case ID_EDIT_CLEAR:EDT_DeleteChar(context);break;
    case ID_EDITOR_SMALLER:EDT_SetFontSize(context,max(1,EDT_GetFontSize(context)-1));break;
    case ID_EDITOR_LARGER:EDT_SetFontSize(context,min(7,EDT_GetFontSize(context)+1));break;
    case ID_EDITOR_CLEAR_STYLES: {
        EDT_CharacterData *data=EDT_NewCharacterData();if(data) {data->mask=TF_BOLD|TF_ITALIC|TF_FIXED|TF_SUPER|TF_SUB|TF_STRIKEOUT|TF_BLINK|TF_UNDERLINE;data->values=0;EDT_SetCharacterData(context,data);EDT_FreeCharacterData(data);}break;
    }
    case ID_EDITOR_COLOR: {
        LO_Color color;COLORREF initial=EDT_GetFontColor(context,&color) ? RGB(color.red,color.green,color.blue) : RGB(0,0,0);
        CColorDialog dialog(initial,CC_FULLOPEN,owner(context));if(dialog.DoModal()==IDOK) {COLORREF rgb=dialog.GetColor();color.red=GetRValue(rgb);color.green=GetGValue(rgb);color.blue=GetBValue(rgb);EDT_SetFontColor(context,&color);}break;
    }
    case ID_EDITOR_DEFAULT_COLOR:EDT_SetFontColor(context,nullptr);break;
    case ID_EDITOR_UNLINK:EDT_SetHREF(context,nullptr);break;
    case ID_EDITOR_INDENT:EDT_Indent(context);break;
    case ID_EDITOR_OUTDENT:EDT_Outdent(context);break;
    case ID_EDITOR_BULLETS:EDT_ToggleList(context,P_UNUM_LIST);break;
    case ID_EDITOR_NUMBERS:EDT_ToggleList(context,P_NUM_LIST);break;
    case ID_EDITOR_ALIGN_LEFT:EDT_SetParagraphAlign(context,ED_ALIGN_LEFT);break;
    case ID_EDITOR_ALIGN_CENTER:EDT_SetParagraphAlign(context,ED_ALIGN_ABSCENTER);break;
    case ID_EDITOR_ALIGN_RIGHT:EDT_SetParagraphAlign(context,ED_ALIGN_RIGHT);break;
    case ID_EDITOR_BREAK:EDT_InsertBreak(context,ED_BREAK_NORMAL);break;
    case ID_EDITOR_BREAK_CLEAR:EDT_InsertBreak(context,ED_BREAK_BOTH);break;
    case ID_EDITOR_NBSPACE:EDT_InsertNonbreakingSpace(context);break;
    case ID_EDITOR_HRULE: {EDT_HorizRuleData *data=EDT_NewHorizRuleData();if(data) {EDT_InsertHorizRule(context,data);EDT_FreeHorizRuleData(data);}break;}
    case ID_EDITOR_SELECT_TABLE:EDT_SelectTable(context);break;
    case ID_EDITOR_DELETE_TABLE:EDT_DeleteTable(context);break;
    case ID_EDITOR_DELETE_ROW:EDT_DeleteTableRows(context,1);break;
    case ID_EDITOR_DELETE_COLUMN:EDT_DeleteTableColumns(context,1);break;
    case ID_EDITOR_DELETE_CELL:EDT_DeleteTableCells(context,1);break;
    case ID_EDITOR_ROW: {EDT_TableRowData *data=EDT_NewTableRowData();if(data) {EDT_InsertTableRows(context,data,TRUE,1);EDT_FreeTableRowData(data);}break;}
    case ID_EDITOR_COLUMN:case ID_EDITOR_CELL: {EDT_TableCellData *data=EDT_NewTableCellData();if(data) {if(command==ID_EDITOR_COLUMN) EDT_InsertTableColumns(context,data,TRUE,1);else EDT_InsertTableCells(context,data,TRUE,1);EDT_FreeTableCellData(data);}break;}
    case ID_EDITOR_PARAGRAPH_MARKS:EDT_SetDisplayParagraphMarks(context,!EDT_GetDisplayParagraphMarks(context));break;
    case ID_EDITOR_DISPLAY_TABLES:EDT_SetDisplayTables(context,!EDT_GetDisplayTables(context));break;
    default:return false;
    }
    changed(context);return true;
}

bool WFE_EditorUpdate(MWContext *context,CCmdUI *command)
{
    if(WFE_PrintUpdate(context,command) || WFE_EditorWorkflowUpdate(context,command) || WFE_EditorPublishUpdate(context,command)) return true;
    if(command->m_nID==ID_WINDOW_BOOKMARKS) return WFE_WindowUpdateCommand(owner(context),command);
    UINT id=command->m_nID;bool active=ready(context),enable=active,check=false,handled=true;
    if(unavailable(id)) enable=false;
    else if(style_bit(id)) {enable=active && EDT_CanSetCharacterAttribute(context);check=active && style_checked(context,style_bit(id));}
    else if(id>=ID_EDITOR_SIZE_FIRST && id<=ID_EDITOR_SIZE_LAST) {enable=active && EDT_CanSetCharacterAttribute(context);check=active && EDT_GetFontSize(context)==(int)(id-ID_EDITOR_SIZE_FIRST+1);}
    else {
        bool paragraph=false;
        for(const auto &style:paragraph_styles) if(style.command==id) {paragraph=true;check=active && EDT_GetParagraphFormatting(context)==style.tag;break;}
        if(!paragraph) switch(id) {
        case ID_EDITOR_NEW:case ID_EDITOR_OPEN:case ID_EDITOR_OPEN_LOCATION:case ID_EDITOR_MAIL_PREFERENCES:enable=true;break;
        case ID_EDITOR_SAVE:case ID_EDITOR_SAVE_AS:enable=active && !WFE_EditorSaving(context);break;
        case ID_EDIT_UNDO:enable=active && EDT_GetUndoCommandID(context,0)!=CEDITCOMMAND_ID_NULL;break;
        case ID_EDIT_REDO:enable=active && EDT_GetRedoCommandID(context,0)!=CEDITCOMMAND_ID_NULL;break;
        case ID_EDIT_CUT:enable=active && EDT_CanCut(context,TRUE)==EDT_COP_OK;break;
        case ID_EDIT_COPY:enable=active && EDT_CanCopy(context,TRUE)==EDT_COP_OK;break;
        case ID_EDIT_PASTE:enable=active && EDT_CanPaste(context,TRUE)==EDT_COP_OK && (IsClipboardFormatAvailable(clipboard_format()) || IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(RegisterClipboardFormatA("HTML Format")));break;
        case ID_EDITOR_FILEBAR:case ID_EDITOR_CHARBAR:case ID_EDITOR_PARABAR: {GoldChrome *ui=chrome(context);enable=ui!=nullptr;check=ui && ui->visible[id==ID_EDITOR_FILEBAR ? 0 : (id==ID_EDITOR_CHARBAR ? 1 : 2)];break;}
        case ID_EDITOR_UNLINK:enable=active && EDT_SelectionContainsLink(context);break;
        case ID_EDITOR_LINK_PROPERTIES:enable=active && EDT_CanSetHREF(context);break;
        case ID_EDITOR_IMAGE_PROPERTIES:enable=active && EDT_GetCurrentElementType(context)==ED_ELEMENT_IMAGE;break;
        case ID_EDITOR_HRULE_PROPERTIES:enable=active && EDT_GetCurrentElementType(context)==ED_ELEMENT_HRULE;break;
        case ID_EDITOR_TARGET_PROPERTIES:enable=active && EDT_GetCurrentElementType(context)==ED_ELEMENT_TARGET;break;
        case ID_EDITOR_TAG_PROPERTIES:enable=active && EDT_GetCurrentElementType(context)==ED_ELEMENT_UNKNOWN_TAG;break;
        case ID_EDITOR_TABLE_PROPERTIES:case ID_EDITOR_SELECT_TABLE:case ID_EDITOR_DELETE_TABLE:case ID_EDITOR_DELETE_ROW:case ID_EDITOR_DELETE_COLUMN:case ID_EDITOR_DELETE_CELL:case ID_EDITOR_ROW:case ID_EDITOR_COLUMN:case ID_EDITOR_CELL:enable=active && EDT_IsInsertPointInTable(context);break;
        case ID_EDITOR_PARAGRAPH_MARKS:check=active && EDT_GetDisplayParagraphMarks(context);break;
        case ID_EDITOR_DISPLAY_TABLES:check=active && EDT_GetDisplayTables(context);break;
        case ID_EDITOR_BULLETS:check=active && EDT_GetToggleListState(context,P_UNUM_LIST);break;
        case ID_EDITOR_NUMBERS:check=active && EDT_GetToggleListState(context,P_NUM_LIST);break;
        case ID_EDITOR_ALIGN_LEFT:check=active && (EDT_GetParagraphAlign(context)==ED_ALIGN_LEFT || EDT_GetParagraphAlign(context)==ED_ALIGN_DEFAULT);break;
        case ID_EDITOR_ALIGN_CENTER:check=active && EDT_GetParagraphAlign(context)==ED_ALIGN_ABSCENTER;break;
        case ID_EDITOR_ALIGN_RIGHT:check=active && EDT_GetParagraphAlign(context)==ED_ALIGN_RIGHT;break;
        case ID_EDITOR_DELETE:case ID_EDIT_CLEAR:enable=active;break;
        case ID_EDIT_SELECT_ALL:case ID_EDITOR_SELECT_ALL:case ID_EDITOR_LINK:case ID_EDITOR_IMAGE:case ID_EDITOR_TARGET:case ID_EDITOR_TAG:case ID_EDITOR_TABLE:
        case ID_EDITOR_DOCUMENT_PROPERTIES:case ID_EDITOR_TEXT_PROPERTIES:case ID_EDITOR_HRULE:case ID_EDITOR_BREAK:case ID_EDITOR_BREAK_CLEAR:case ID_EDITOR_NBSPACE:
        case ID_EDITOR_BROWSE:case ID_EDITOR_RELOAD:case ID_EDITOR_REFRESH:case ID_EDITOR_IMAGES:case ID_EDITOR_SOURCE:case ID_EDITOR_INFO:
        case ID_EDITOR_FIND:case ID_EDITOR_FIND_AGAIN:case ID_EDITOR_INDENT:case ID_EDITOR_OUTDENT:case ID_EDITOR_SMALLER:case ID_EDITOR_LARGER:
        case ID_EDITOR_CLEAR_STYLES:case ID_EDITOR_COLOR:case ID_EDITOR_DEFAULT_COLOR:break;
        case 33115:check=true;enable=false;break;
        default:handled=false;break;
        }
    }
    if(!handled) return false;
    command->Enable(enable);command->SetCheck(check ? 1 : 0);return true;
}

bool WFE_EditorKey(MWContext *context,MSG *message)
{
    if(!context || !context->is_editor || !message) return false;
    if(message->message!=WM_KEYDOWN && message->message!=WM_CHAR) return false;
    if(!ready(context)) return true;
    bool shift=(GetKeyState(VK_SHIFT)&0x8000)!=0,control=(GetKeyState(VK_CONTROL)&0x8000)!=0;
    UINT key=(UINT)message->wParam;
    if(message->message==WM_CHAR) {
        if(key>=32 && key!=127 && !control) {
            if(key==' ' && shift) EDT_InsertNonbreakingSpace(context);
            else EDT_KeyDown(context,(uint16)key,(uint16)LOWORD(message->lParam),(uint16)HIWORD(message->lParam));
            changed(context);
        }
        return true;
    }
    if(control) {
        UINT command=0;
        switch(key) {
        case 'A':command=ID_EDITOR_SELECT_ALL;break;case 'B':command=ID_EDITOR_BOLD;break;
        case 'I':command=ID_EDITOR_ITALIC;break;case 'U':command=ID_EDITOR_UNDERLINE;break;
        case 'T':command=ID_EDITOR_FIXED;break;case 'K':command=ID_EDITOR_CLEAR_STYLES;break;
        case 'X':command=ID_EDIT_CUT;break;case 'C':command=ID_EDIT_COPY;break;case 'V':command=ID_EDIT_PASTE;break;
        case 'Z':command=shift ? ID_EDIT_REDO : ID_EDIT_UNDO;break;
        }
        if(command) return WFE_EditorCommand(context,command);
    }
    switch(key) {
    case VK_LEFT:if(control) EDT_PreviousWord(context,shift);else EDT_PreviousChar(context,shift);break;
    case VK_RIGHT:if(control) EDT_NextWord(context,shift);else EDT_NextChar(context,shift);break;
    case VK_UP:EDT_Up(context,shift);break;
    case VK_DOWN:EDT_Down(context,shift);break;
    case VK_HOME:if(control) EDT_BeginOfDocument(context,shift);else EDT_BeginOfLine(context,shift);break;
    case VK_END:if(control) EDT_EndOfDocument(context,shift);else EDT_EndOfLine(context,shift);break;
    case VK_PRIOR:EDT_PageUp(context,shift);break;
    case VK_NEXT:EDT_PageDown(context,shift);break;
    case VK_BACK:
        if(control && !EDT_IsSelected(context)) EDT_PreviousWord(context,TRUE);
        if(EDT_IsSelected(context)) EDT_DeleteChar(context);else EDT_DeletePreviousChar(context);
        break;
    case VK_DELETE:
        if(shift) copy_selection(context,true);
        else {if(control && !EDT_IsSelected(context)) EDT_NextWord(context,TRUE);EDT_DeleteChar(context);}
        break;
    case VK_INSERT:if(control) copy_selection(context,false);else if(shift) paste_selection(context);else return false;break;
    case VK_RETURN:if(shift) EDT_InsertBreak(context,ED_BREAK_NORMAL);else EDT_ReturnKey(context);break;
    case VK_TAB:if(shift) EDT_Outdent(context);else EDT_Indent(context);break;
    default:return false;
    }
    changed(context);return true;
}

/* The native smoke harness supplies a loaded editor document. This checks
 * native chrome/menu wiring without changing the document or undo history. */
bool WFE_EditorUISmoke(MWContext *context,const char *report)
{
    FILE *file=report ? fopen(report,"a") : nullptr;bool pass=ready(context) && chrome(context);
    if(pass) {
        GoldChrome *ui=chrome(context);
        pass=ui->buttons.size()==30 && ui->paragraph.GetCount()==12 && ui->size.GetCount()==7;
        CRect area;ui->GetParent()->GetClientRect(area);
        bool original[3]={ui->visible[0],ui->visible[1],ui->visible[2]};
        int expected=(original[0] ? ClassicUI::Scale(ui,33) : 0)+
                     (original[1] ? ClassicUI::Scale(ui,38) : 0)+
                     (original[2] ? ClassicUI::Scale(ui,33) : 0);
        int actual=ui->Arrange(area.Width());pass=pass && actual==expected;
        const UINT bars[]={ID_EDITOR_FILEBAR,ID_EDITOR_CHARBAR,ID_EDITOR_PARABAR};
        for(int i=0;i<3;i++) {
            WFE_EditorCommand(context,bars[i]);
            pass=pass && ui->visible[i]!=original[i];
            WFE_EditorCommand(context,bars[i]);
            pass=pass && ui->visible[i]==original[i] && ui->Arrange(area.Width())==expected;
        }
        CMenu menu;bool menu_ok=menu.LoadMenu(IDR_EDITOR)!=FALSE && menu.GetMenuItemCount()==8;
        const char *labels[]={"&File","&Edit","&View","&Insert","&Properties","&Options","&Window","&Help"};
        for(int i=0;menu_ok && i<8;i++) {CString label;menu.GetMenuString(i,label,MF_BYPOSITION);menu_ok=label==labels[i];}
        pass=pass && menu_ok;
        if(file) fprintf(file,"editor_toolbar_height=%d\neditor_menu=%s\n",actual,menu_ok ? "pass" : "fail");
    }
    if(file) {fprintf(file,"editor_ui=%s\n",pass ? "pass" : "fail");fclose(file);}return pass;
}
#endif
