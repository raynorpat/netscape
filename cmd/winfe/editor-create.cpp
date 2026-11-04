/* Local document creation workflows for the original Gold editing engine. */
#include <afxwin.h>
#include <afxdlgs.h>
#include <shlwapi.h>
#include <shellapi.h>
#include "editor-create.h"
#include "editor-preferences.h"
#include "editor_resource.h"
#include "classic.h"
#include "edt.h"
#include "proto.h"
#include "xp_list.h"
#include <string>
#include <vector>
#include <memory>
#include <stdio.h>

#ifdef WFE_EDITOR_STREAM_TEST
bool WFE_EditorInvokeAutoSave(MWContext *context);
#endif

namespace {
const char wizard_url[]="http://home.netscape.com/home/gold3.0_wizard.html";
bool ready(MWContext *context)
{
    return context && XP_IsContextInList(context) && context->is_editor && EDT_HaveEditBuffer(context)
        && !EDT_IsBlocked(context) && !WFE_EditorSaving(context);
}
bool creation_ready(MWContext *context)
{
    return context && XP_IsContextInList(context) && !XP_IsContextBusy(context)
        && (!context->is_editor || ready(context));
}
CWnd *owner(MWContext *context) {return CWnd::FromHandle(WFE_DocumentWindow(context));}
void pump()
{
    WFE_Pump();MSG message;
    while(PeekMessage(&message,nullptr,0,0,PM_REMOVE)) {
        if(message.message==WM_QUIT) {PostQuitMessage((int)message.wParam);return;}
        TranslateMessage(&message);DispatchMessage(&message);
    }
}
bool wait_ready(MWContext *context)
{
    ULONGLONG deadline=GetTickCount64()+15000;
    while(XP_IsContextInList(context) && !ready(context) && GetTickCount64()<deadline) {
        pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }
    return ready(context);
}
bool save_wait(MWContext *context,const char *path,bool save_as)
{
    if(WFE_EditorSave(context,path,save_as)!=1) return false;
    ULONGLONG deadline=GetTickCount64()+15000;
    while(XP_IsContextInList(context) && WFE_EditorSaving(context) && GetTickCount64()<deadline) {
        pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }
    return XP_IsContextInList(context) && !WFE_EditorSaving(context) && WFE_EditorSaveStatus(context)==ED_ERROR_NONE;
}
std::string local_path(const std::string &path)
{
    if(!_strnicmp(path.c_str(),"file:",5)) {
        char result[32768];DWORD count=sizeof(result);
        return SUCCEEDED(PathCreateFromUrlA(path.c_str(),result,&count,0))?result:"";
    }
    return path.find("://")==std::string::npos?path:"";
}
std::string escape(const std::string &value)
{
    std::string result;
    for(char c:value) {
        if(c=='&') result+="&amp;";else if(c=='<') result+="&lt;";
        else if(c=='>') result+="&gt;";else if(c=='\"') result+="&quot;";
        else if(c=='\n') result+="<BR>\n";else if(c!='\r') result+=c;
    }
    return result;
}
bool write_file(const std::string &path,const std::string &html,bool fresh=false)
{
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,fresh?CREATE_NEW:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    DWORD written=0;bool good=WriteFile(file,html.data(),(DWORD)html.size(),&written,nullptr)!=FALSE && written==html.size();
    if(good) good=FlushFileBuffers(file)!=FALSE;
    if(!CloseHandle(file)) good=false;
    if(!good) DeleteFileA(path.c_str());return good;
}
bool read_file(const std::string &path,std::string &bytes)
{
    FILE *file=fopen(path.c_str(),"rb");if(!file) return false;
    bytes.clear();char block[4096];size_t count;
    while((count=fread(block,1,sizeof(block),file))!=0) bytes.append(block,count);
    bool good=!ferror(file);fclose(file);return good;
}
std::string temp_html()
{
    char directory[MAX_PATH],name[MAX_PATH];
    if(!GetTempPathA(sizeof(directory),directory) || !GetTempFileNameA(directory,"gld",0,name)) return "";
    std::string path=std::string(name)+".html";DeleteFileA(name);return path;
}
MWContext *new_copy(const std::string &path)
{
    DWORD attributes=GetFileAttributesA(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_DIRECTORY)) return nullptr;
    MWContext *created=WFE_NewEditorDocument(path.c_str());
    return created && wait_ready(created)?created:nullptr;
}
MWContext *new_html(const std::string &html)
{
    std::string path=temp_html();if(path.empty() || !write_file(path,html,true)) return nullptr;
    MWContext *created=new_copy(path);
    /* Loading has completed before the temporary source is removed. The EDT
     * tree, rather than this file, owns the new independent document. */
    if(created) DeleteFileA(path.c_str());
    return created;
}
std::string wizard_html(const std::string &title,const std::string &body,COLORREF color,int layout)
{
    char background[16];sprintf(background,"#%02X%02X%02X",GetRValue(color),GetGValue(color),GetBValue(color));
    std::string heading=escape(title),content=escape(body);
    std::string html="<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n<HTML><HEAD><TITLE>"+heading+
        "</TITLE></HEAD><BODY BGCOLOR=\""+background+"\">\n";
    if(layout==1) html+="<TABLE BORDER=\"0\" WIDTH=\"100%\"><TR><TD WIDTH=\"25%\" VALIGN=\"TOP\"><B>Contents</B><P>Introduction<BR>Contact</TD><TD VALIGN=\"TOP\"><H1>"+heading+"</H1><P>"+content+"</TD></TR></TABLE>";
    else if(layout==2) html+="<CENTER><H1>"+heading+"</H1></CENTER><HR><P>"+content+"<HR><P>Contact information";
    else html+="<H1>"+heading+"</H1><P>"+content;
    return html+"\n</BODY></HTML>\n";
}
class PageWizard : public CDialog {
public:
    MWContext *context;
    CEdit title,body;
    CComboBox layout;
    COLORREF background=RGB(255,255,255);
    std::string result;
    std::vector<std::unique_ptr<CWnd>> controls;
    explicit PageWizard(MWContext *value):context(value) {}
protected:
    CRect rect(int x,int y,int width,int height) {
        return CRect(ClassicUI::Scale(this,x),ClassicUI::Scale(this,y),ClassicUI::Scale(this,x+width),ClassicUI::Scale(this,y+height));
    }
    void label(const char *text,int x,int y,int width,int height) {
        std::unique_ptr<CStatic> control(new CStatic);control->Create(text,WS_CHILD|WS_VISIBLE,rect(x,y,width,height),this);controls.push_back(std::move(control));
    }
    void button(const char *text,UINT id,int x,int y,int width) {
        std::unique_ptr<ClassicButton> control(new ClassicButton);control->Create(text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,rect(x,y,width,26),this,id);controls.push_back(std::move(control));
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();SetWindowText("PageWizard - Offline Page Builder");
        CRect outer=rect(0,0,490,380);AdjustWindowRectEx(&outer,GetStyle(),FALSE,GetExStyle());
        SetWindowPos(nullptr,0,0,outer.Width(),outer.Height(),SWP_NOMOVE|SWP_NOZORDER);
        label("Create an editable page locally. The original PageWizard was a Netscape web service.",12,12,465,35);
        label("Page &title:",12,54,90,20);
        title.Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,rect(105,50,370,24),this,5101);title.LimitText(240);title.SetWindowText("My Web Page");
        label("Page &text:",12,88,90,20);
        body.Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,rect(105,84,370,145),this,5102);
        body.LimitText(16000);body.SetWindowText("Welcome to my web page.");
        label("&Layout:",12,242,90,20);
        layout.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,rect(105,238,245,120),this,5103);
        layout.AddString("Heading and text");layout.AddString("Contents and main text");layout.AddString("Centered heading and contact");layout.SetCurSel(0);
        button("Background &Color...",5104,105,278,175);
        button("Original &PageWizard...",5105,12,338,180);
        button("Create",IDOK,300,338,80);button("Cancel",IDCANCEL,392,338,80);
        ClassicUI::Apply(this);CenterWindow();title.SetFocus();title.SetSel(0,-1);return FALSE;
    }
    BOOL OnCommand(WPARAM command,LPARAM control) override {
        if(LOWORD(command)==5104) {
            CColorDialog dialog(background,CC_FULLOPEN,this);if(dialog.DoModal()==IDOK) background=dialog.GetColor();return TRUE;
        }
        if(LOWORD(command)==5105) {WFE_NewWindow(wizard_url);EndDialog(IDCANCEL);return TRUE;}
        return CDialog::OnCommand(command,control);
    }
    void OnOK() override {
        CString heading,text;title.GetWindowText(heading);body.GetWindowText(text);
        if(heading.IsEmpty()) {MessageBox("Enter a title for the new page.","PageWizard",MB_OK|MB_ICONEXCLAMATION);title.SetFocus();return;}
        result=wizard_html((const char *)heading,(const char *)text,background,layout.GetCurSel());EndDialog(IDOK);
    }
    afx_msg BOOL OnEraseBkgnd(CDC *dc) {CRect r;GetClientRect(r);dc->FillSolidRect(r,ClassicUI::Face);return TRUE;}
    afx_msg HBRUSH OnCtlColor(CDC *dc,CWnd *window,UINT type) {return ClassicUI::ControlColor(dc,window,type);}
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(PageWizard,CDialog)
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
END_MESSAGE_MAP()
void show_wizard(MWContext *context)
{
    struct {DLGTEMPLATE dialog;WORD menu,window_class,title;} resource={};
    resource.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;resource.dialog.cx=330;resource.dialog.cy=250;
    PageWizard dialog(context);
    if(dialog.InitModalIndirect(&resource.dialog,owner(context)) && dialog.DoModal()==IDOK)
        if(!new_html(dialog.result)) WFE_Message(context,"Could not create the new page. Check temporary folder access.",0);
}
void from_template(MWContext *context)
{
    EditorPreferences preferences=WFE_EditorPreferencesRead();std::string path=local_path(preferences.document_template);
    CFileDialog dialog(TRUE,"html",path.empty()?nullptr:path.c_str(),OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR,
        "HTML Templates (*.html;*.htm)|*.html;*.htm|All Files (*.*)|*.*||",owner(context));
    dialog.m_ofn.lpstrTitle="New Document from Local Template";
    if(dialog.DoModal()!=IDOK) return;
    path=(const char *)dialog.GetPathName();
    if(!new_copy(path)) WFE_Message(context,"The local template could not be loaded into an editable document.",0);
}
void starter(MWContext *context)
{
    const char *html="<HTML><HEAD><TITLE>Web Page Starter - Offline Guide</TITLE></HEAD><BODY>"
        "<H1>Web Page Starter</H1><P>This offline guide describes the local page tools available in Netscape Gold."
        "<H2>Start with a blank page</H2><P>Choose File / New Document / Blank, then type and format your text."
        "<H2>Use an HTML template</H2><P>Choose File / New Document / From Template and select a local HTML file. "
        "The new document is independent. Save asks for a new filename."
        "<H2>Build a page with the offline PageWizard</H2><P>Choose File / New Document / From Wizard. "
        "Enter a title and text, choose a layout and background color, then press Create."
        "<H2>Publish your work</H2><P>Set the default publishing and browsing locations in Options / Editor Preferences / Publish. "
        "Choose File / Publish when ready."
        "<P>The historical Netscape PageWizard and template catalog were web services. This guide and the native offline builder "
        "are local replacements; they do not include the original hosted content.</BODY></HTML>";
    if(!new_html(html)) WFE_Message(context,"The offline starter guide could not be opened.",0);
}
void source_edit(MWContext *context)
{
    EditorPreferences preferences=WFE_EditorPreferencesRead();
    if(preferences.html_editor.empty()) {
        WFE_Message(context,"Choose the HTML source application in Editor Preferences / General first. Enter an executable path without command arguments.",0);return;
    }
    std::string error;EditorPreferences launch;
    launch.html_editor=preferences.html_editor;
    if(!WFE_EditorPreferencesValid(launch,error)) {WFE_Message(context,error.c_str(),0);return;}
    /* The core save supplies complete HTML, including HEAD and page colors.
     * A clipboard fragment is insufficient for external source editing. */
    if(!save_wait(context,nullptr,false)) return;
    std::string source=WFE_EditorSavedPath(context);
    if(source.empty()) {WFE_Message(context,"Save the document to a local HTML file before editing its source.",0);return;}
    std::string temporary;
    bool copied=false;
    for(unsigned i=0;i<100;i++) {
        char suffix[96];sprintf(suffix,".gold-source-%lu-%llu-%u.html",GetCurrentProcessId(),GetTickCount64(),i);
        temporary=source+suffix;
        if(CopyFileA(source.c_str(),temporary.c_str(),TRUE)) {copied=true;break;}
        if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS) break;
    }
    if(!copied) {WFE_Message(context,"Could not create a temporary source copy beside the saved document. Check folder access.",0);return;}
    std::string command=WFE_EditorQuoteArgument(preferences.html_editor)+" "+WFE_EditorQuoteArgument(temporary);
    std::vector<char> mutable_command(command.begin(),command.end());mutable_command.push_back(0);
    STARTUPINFOA startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION process={};
    std::string directory=source.substr(0,source.find_last_of("\\/"));
    if(!CreateProcessA(preferences.html_editor.c_str(),mutable_command.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&process)) {
        DeleteFileA(temporary.c_str());WFE_Message(context,"The HTML source application could not be started.",0);return;
    }
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    int answer=MessageBoxA(WFE_DocumentWindow(context),
        "Edit and save the temporary HTML file in the external editor. Close that file before continuing.\n\n"
        "Open the edited source as a new, independent document now?\n\n"
        "The saved original remains available in this window. Save the new document under a filename of your choice.",
        "Edit HTML Source",MB_YESNO|MB_ICONQUESTION);
    bool imported=false;
    if(answer==IDYES && ready(context)) {
        /* Nested UI may have changed the original while the external editor
         * was open. Respect Cancel in the regular dirty-document save guard. */
        if(!WFE_EditorCheckSave(context)) return;
        if(!new_copy(temporary)) {WFE_Message(context,("Could not import edited HTML. The source copy remains at:\n"+temporary).c_str(),0);return;}
        imported=true;
    }
    if(imported) DeleteFileA(temporary.c_str());
    else if(XP_IsContextInList(context)) WFE_Status(context,("External source copy retained: "+temporary).c_str());
}
} // namespace

bool WFE_EditorWorkflowCommand(MWContext *context,UINT command)
{
    switch(command) {
    case ID_EDITOR_PREFERENCES:if(creation_ready(context)) WFE_EditorPreferencesDialog(context);return true;
    case 34401:if(creation_ready(context)) from_template(context);return true;
    case ID_EDITOR_WIZARD:if(creation_ready(context)) show_wizard(context);return true;
    case ID_EDITOR_STARTER:if(creation_ready(context)) starter(context);return true;
    case ID_EDITOR_SOURCE_EDIT:if(ready(context)) source_edit(context);return true;
    }
    return false;
}
bool WFE_EditorWorkflowUpdate(MWContext *context,CCmdUI *command)
{
    switch(command->m_nID) {
    case ID_EDITOR_PREFERENCES:case 34401:case ID_EDITOR_WIZARD:case ID_EDITOR_STARTER:
        command->Enable(creation_ready(context));return true;
    case ID_EDITOR_SOURCE_EDIT:command->Enable(ready(context));return true;
    }
    return false;
}
namespace {
bool autosave_prompt=false;
void CALLBACK cancel_autosave_prompt(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindWindowA("#32770","Netscape Gold");DWORD process=0;
    if(!dialog || !GetWindowThreadProcessId(dialog,&process) || process!=GetCurrentProcessId()) return;
    autosave_prompt=true;KillTimer(nullptr,timer);PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
}
class WorkflowCommandUI : public CCmdUI {
public:
    bool enabled=false;
    void Enable(BOOL value=TRUE) override {enabled=value!=FALSE;}
};
int new_editor_count()
{
    XP_List *list=XP_GetGlobalContextList();MWContext *context;int count=0;
    while((context=(MWContext *)XP_ListNextObject(list))!=nullptr)
        if(context->is_editor && context->is_new_document && EDT_HaveEditBuffer(context)) count++;
    return count;
}
}
bool WFE_EditorWorkflowSmoke(MWContext *context,const char *report)
{
    if(!ready(context)) return false;
    bool preferences=WFE_EditorPreferencesSmoke(context,report);
    MWContext *browser=WFE_NewWindow("about:blank");
    ULONGLONG browser_deadline=GetTickCount64()+15000;
    while(browser && XP_IsContextInList(browser) && XP_IsContextBusy(browser) && GetTickCount64()<browser_deadline) {
        pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }
    WorkflowCommandUI browser_command;browser_command.m_nID=ID_EDITOR_STARTER;
    bool browser_ok=browser && WFE_EditorWorkflowUpdate(browser,&browser_command) && browser_command.enabled;
    browser_command.m_nID=ID_EDITOR_SOURCE_EDIT;
    browser_ok=browser_ok && WFE_EditorWorkflowUpdate(browser,&browser_command) && !browser_command.enabled;
    int editor_count=new_editor_count();
    browser_ok=browser_ok && WFE_EditorWorkflowCommand(browser,ID_EDITOR_STARTER) && new_editor_count()==editor_count+1;
    std::string base=report,path=base+".template.html",copy_path=base+".template-copy.html";
    const std::string original="<html><head><title>Template original</title></head><body><h1>Template source untouched</h1></body></html>";
    bool template_written=write_file(path,original);
    MWContext *copy=template_written?new_copy(path):nullptr;
    bool independent=copy && copy!=context && copy->is_new_document && EDT_HaveEditBuffer(copy);
    MWContext *original_editor=template_written?WFE_NewEditorWindow(path.c_str()):nullptr;
    bool original_distinct=original_editor && original_editor!=copy && wait_ready(original_editor) && !original_editor->is_new_document;
    independent=independent && original_distinct;
    if(independent) {
        EDT_EndOfDocument(copy,FALSE);char text[]="Independent copy";
        EDT_PasteText(copy,text);independent=save_wait(copy,copy_path.c_str(),true);
    }
    bool autosave_write=false;
    if(independent && ready(copy)) {
        EDT_EndOfDocument(copy,FALSE);char text[]="Autosave fixture write";EDT_PasteText(copy,text);
        EDT_SetAutoSavePeriod(copy,1);autosave_prompt=false;
        UINT_PTR timer=SetTimer(nullptr,0,50,cancel_autosave_prompt);
        if(timer) {
#ifdef WFE_EDITOR_STREAM_TEST
            bool saved=WFE_EditorInvokeAutoSave(copy);
#else
            bool saved=false;
#endif
            KillTimer(nullptr,timer);
            ULONGLONG deadline=GetTickCount64()+15000;
            while(XP_IsContextInList(copy) && WFE_EditorSaving(copy) && GetTickCount64()<deadline) {
                pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
            }
            std::string saved_html;
            autosave_write=saved && !autosave_prompt && ready(copy) && !EDT_DirtyFlag(copy)
                && read_file(copy_path,saved_html) && saved_html.find("Autosave fixture write")!=std::string::npos;
        }
    }
    std::string unchanged;
    bool template_unchanged=read_file(path,unchanged) && unchanged==original;
    MWContext *wizard=new_html(wizard_html("Offline wizard fixture","Wizard body fixture",RGB(230,240,255),1));
    bool wizard_ok=wizard && wizard->is_new_document && EDT_HaveEditBuffer(wizard)
        && save_wait(wizard,(base+".wizard.html").c_str(),true);
    std::string probe="C:\\Gold folder\\quoted\" argument\\";
    /* CommandLineToArgvW checks actual Windows command-line parsing rather than
     * reproducing the quoting implementation in the expected value. */
    std::string quoted="editor.exe "+WFE_EditorQuoteArgument(probe);
    int length=MultiByteToWideChar(CP_ACP,0,quoted.c_str(),-1,nullptr,0);
    std::vector<wchar_t> wide(length);MultiByteToWideChar(CP_ACP,0,quoted.c_str(),-1,wide.data(),length);
    int argc=0;LPWSTR *argv=CommandLineToArgvW(wide.data(),&argc);bool quote_ok=false;
    if(argv && argc==2) {
        int size=WideCharToMultiByte(CP_ACP,0,argv[1],-1,nullptr,0,nullptr,nullptr);
        std::vector<char> parsed(size);WideCharToMultiByte(CP_ACP,0,argv[1],-1,parsed.data(),size,nullptr,nullptr);
        quote_ok=probe==parsed.data();
    }
    if(argv) LocalFree(argv);
    bool passed=preferences && browser_ok && independent && autosave_write && template_unchanged && wizard_ok && quote_ok;
    FILE *file=fopen(report,"ab");if(!file) return false;
    fprintf(file,"workflow_original_distinct=%d\n",original_distinct);
    fprintf(file,"workflow_browser=%d\nworkflow_template=%d\nworkflow_autosave_write=%d\nworkflow_template_unchanged=%d\nworkflow_wizard=%d\nworkflow_quote=%d\neditor_workflows=%s\n",
        browser_ok,independent,autosave_write,template_unchanged,wizard_ok,quote_ok,passed?"pass":"fail");fclose(file);
    return passed;
}
