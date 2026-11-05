#include <afxwin.h>
#include <afxcmn.h>
#include <afxext.h>
#include <algorithm>
#include <string>
#include <vector>
#include "addressbook.h"
#include "addressbook-resource.h"
#include "classic.h"
#include "resource.h"
extern "C" {
#include "bkmks.h"
#include "mime.h"
#include "msgcom.h"
#include "xp_file.h"
}

/* The original BM ContextList retains contexts for the process lifetime. */
static MWContext address_context;
static bool address_initialized;
class AddressBookFrame;
static AddressBookFrame *address_window;
static const UINT WM_ADDRESS_REFRESH=WM_APP+62;
static const UINT WM_ADDRESS_EDIT=WM_APP+63;
static BM_Entry *pending_address_edit;
class AddressEditBatch {
public:
    AddressEditBatch() {BM_StartBatch(&address_context);}
    ~AddressEditBatch() {BM_EndBatch(&address_context);}
};
static void select_address(BM_Entry *entry)
{
    BM_ClearAllSelection(&address_context,FALSE);
    BM_SelectItem(&address_context,entry,FALSE,TRUE,TRUE);
}

static bool address_exists(BM_Entry *parent,BM_Entry *wanted)
{
    if(parent==wanted) return true;
    for(auto child=BM_GetChildren(parent);child;child=BM_GetNext(child)) {
        if(child==wanted) return true;
        if(BM_IsHeader(child)&&address_exists(child,wanted)) return true;
    }
    return false;
}
static bool header_text(const CString &text)
{
    return text.GetLength()<=8192&&text.Find('\r')<0&&text.Find('\n')<0;
}
static bool mailbox(const CString &text,std::string &name,std::string &email)
{
    if(!header_text(text)) return false;
    char *names=nullptr,*addresses=nullptr;
    int count=MSG_ParseRFC822Addresses(text,&names,&addresses);
    bool valid=count==1&&addresses&&*addresses&&strchr(addresses,'@');
    if(valid) {name=names?names:"";email=addresses;}
    XP_FREE(names);XP_FREE(addresses);return valid;
}
static std::string full_address(BM_Entry *entry)
{
    char *value=BM_GetFullAddress(&address_context,entry);
    std::string result=value?value:"";XP_FREE(value);return result;
}
static CString list_members(BM_Entry *entry)
{
    CString result;
    for(auto child=BM_GetChildren(entry);child;child=BM_GetNext(child)) {
        if(!result.IsEmpty()) result+="\r\n";
        result+=full_address(child).c_str();
    }
    return result;
}
static BM_Entry *nickname_owner(BM_Entry *parent,const CString &nickname)
{
    for(auto child=BM_GetChildren(parent);child;child=BM_GetNext(child)) {
        if(BM_IsAlias(child)) continue;
        const char *value=BM_GetNickName(child);
        if(value&&!nickname.CompareNoCase(value)) return child;
        if(BM_IsHeader(child)) if(auto found=nickname_owner(child,nickname)) return found;
    }
    return nullptr;
}
static bool apply_properties(BM_Entry *entry,bool fresh,const CString &name,const CString &email,
                             const CString &nickname,const CString &description,const CString &members,CString &error)
{
    bool list=BM_IsHeader(entry);error.Empty();
    if(!header_text(name)||name.IsEmpty()) {error="Enter a name without line breaks.";return false;}
    if(!header_text(nickname)||nickname.GetLength()>128) {error="Enter a shorter nickname without line breaks.";return false;}
    for(int i=0;i<nickname.GetLength();++i) {
        unsigned char c=(unsigned char)nickname[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')) {
            error="Nicknames use letters, digits, hyphens and underscores.";return false;
        }
    }
    if(!nickname.IsEmpty()) {
        auto owner=nickname_owner(BM_GetRoot(&address_context),nickname);
        if(owner&&owner!=entry) {error="That nickname is already in the address book.";return false;}
    }
    std::string parsed_name,parsed_email;
    if(!list&&!mailbox(email,parsed_name,parsed_email)) {error="Enter one email address.";return false;}
    std::vector<BM_Entry *> new_members,new_contacts;
    auto release_members=[&]() {
        for(auto member:new_members) BM_FreeEntry(&address_context,member);
        for(auto contact:new_contacts) BM_FreeEntry(&address_context,contact);
    };
    bool replace_members=list&&(fresh||members!=list_members(entry));
    if(replace_members) {
        int position=0;
        while(position<members.GetLength()) {
            int end=members.Find('\n',position);if(end<0) end=members.GetLength();
            CString line=members.Mid(position,end-position);line.Trim();position=end+1;
            if(line.IsEmpty()) continue;
            if(!mailbox(line,parsed_name,parsed_email)) {error="Enter one email address per list-member line.";break;}
            BM_Entry *contact=nullptr;
            for(auto item=BM_GetChildren(BM_GetRoot(&address_context));item;item=BM_GetNext(item))
                if(BM_IsAddress(item)&&!_stricmp(BM_GetAddress(item),parsed_email.c_str())) {contact=item;break;}
            if(!contact) for(auto item:new_contacts)
                if(!_stricmp(BM_GetAddress(item),parsed_email.c_str())) {contact=item;break;}
            if(!contact) {
                contact=BM_NewAddress(parsed_name.c_str(),parsed_email.c_str());
                if(!contact) {error="Not enough memory to update this list.";break;}
                new_contacts.push_back(contact);
            }
            auto member=BM_NewAlias(contact);
            if(!member) {error="Not enough memory to update this list.";break;}
            new_members.push_back(member);
        }
        if(!error.IsEmpty()) {release_members();return false;}
    }
    AddressEditBatch batch;
    if(!BM_SetNickName(&address_context,entry,nickname)) {
        release_members();
        error="Could not set the nickname.";return false;
    }
    if(fresh) BM_AppendToHeader(&address_context,BM_GetRoot(&address_context),entry);
    BM_SetName(&address_context,entry,name);BM_SetDescription(&address_context,entry,description);
    if(!list) {mailbox(email,parsed_name,parsed_email);BM_SetAddress(&address_context,entry,parsed_email.c_str());}
    if(replace_members) {
        for(auto contact:new_contacts) BM_AppendToHeader(&address_context,BM_GetRoot(&address_context),contact);
        while(auto child=BM_GetChildren(entry)) {select_address(child);BM_ObeyCommand(&address_context,BM_Cmd_Delete);}
        for(auto member:new_members) BM_AppendToHeader(&address_context,entry,member);
    }
    return true;
}
static bool save_addresses() {return !address_initialized||BM_SaveBookmarks(&address_context,nullptr)>=0;}

class AddressProperties : public ClassicDialog {
public:
    AddressProperties(CWnd *parent,BM_Entry *entry,bool fresh):ClassicDialog(IDD_ADDRESS_PROPERTIES,parent),entry(entry),fresh(fresh) {}
    BM_Entry *entry;bool fresh;
    BOOL OnInitDialog() override {
        ClassicDialog::OnInitDialog();SetWindowText(BM_IsHeader(entry)?"Mailing List Properties":"User Properties");
        SetDlgItemText(IDC_AB_NAME,BM_GetName(entry));SetDlgItemText(IDC_AB_EMAIL,BM_IsHeader(entry)?"":BM_GetAddress(entry));
        SetDlgItemText(IDC_AB_NICKNAME,fresh?"":BM_GetNickName(entry));SetDlgItemText(IDC_AB_DESCRIPTION,BM_GetDescription(entry));
        SetDlgItemText(IDC_AB_MEMBERS,BM_IsHeader(entry)?list_members(entry):CString(""));
        GetDlgItem(IDC_AB_EMAIL)->EnableWindow(!BM_IsHeader(entry));GetDlgItem(IDC_AB_MEMBERS)->EnableWindow(BM_IsHeader(entry));
        for(int id=IDC_AB_NAME;id<=IDC_AB_MEMBERS;++id) ((CEdit*)GetDlgItem(id))->LimitText(id==IDC_AB_MEMBERS?32767:8192);
        return TRUE;
    }
    void OnOK() override {
        if(!fresh&&!address_exists(BM_GetRoot(&address_context),entry)) {EndDialog(IDCANCEL);return;}
        CString name,email,nick,description,members,error;
        GetDlgItemText(IDC_AB_NAME,name);GetDlgItemText(IDC_AB_EMAIL,email);GetDlgItemText(IDC_AB_NICKNAME,nick);
        GetDlgItemText(IDC_AB_DESCRIPTION,description);GetDlgItemText(IDC_AB_MEMBERS,members);
        if(!apply_properties(entry,fresh,name,email,nick,description,members,error)) {MessageBox(error);return;}
        fresh=false;ClassicDialog::OnOK();
    }
};

class AddressBookFrame : public CFrameWnd {
public:
    CTreeCtrl tree;CStatusBar status;bool refreshing=false,properties_open=false;
    void PostNcDestroy() override {delete this;}
    void GetMessageString(UINT,CString &text) const override {text=BM_ReadFailed(&address_context)?"Unable to load address book. Choose File > Retry Load.":"Netscape";}
    BM_Entry *Selected() {auto item=tree.GetSelectedItem();return item?(BM_Entry*)tree.GetItemData(item):nullptr;}
    void Append(BM_Entry *parent,HTREEITEM item) {
        for(auto entry=BM_GetChildren(parent);entry;entry=BM_GetNext(entry)) {
            CString label=BM_GetName(entry);if(BM_IsAddress(entry)) {label+=" <";label+=BM_GetAddress(entry);label+='>';}
            auto child=tree.InsertItem(label,item);tree.SetItemData(child,(DWORD_PTR)entry);if(BM_IsHeader(entry)) Append(entry,child);
        }
    }
    void Select(BM_Entry *entry) {
        auto item=tree.GetRootItem();
        while(item) {
            if((BM_Entry*)tree.GetItemData(item)==entry) {tree.SelectItem(item);return;}
            if(tree.GetChildItem(item)) item=tree.GetChildItem(item);
            else {while(item&&!tree.GetNextSiblingItem(item)) item=tree.GetParentItem(item);if(item) item=tree.GetNextSiblingItem(item);}
        }
    }
    void Refresh() {
        if(!tree.GetSafeHwnd()||refreshing) return;
        auto selected=Selected();refreshing=true;tree.SetRedraw(FALSE);tree.DeleteAllItems();
        auto root=tree.InsertItem("Address Book");tree.SetItemData(root,(DWORD_PTR)BM_GetRoot(&address_context));
        Append(BM_GetRoot(&address_context),root);tree.Expand(root,TVE_EXPAND);Select(selected);
        tree.SetRedraw(TRUE);tree.Invalidate();refreshing=false;
    }
    void Properties(BM_Entry *entry,bool fresh) {
        if(BM_ReadFailed(&address_context)) {if(fresh&&entry) BM_FreeEntry(&address_context,entry);return;}
        if(entry&&BM_IsAlias(entry)) entry=BM_GetAliasOriginal(entry);
        if(!entry||entry==BM_GetRoot(&address_context)) return;
        if(properties_open) {if(fresh) BM_FreeEntry(&address_context,entry);return;}
        properties_open=true;AddressProperties dialog(this,entry,fresh);INT_PTR result=dialog.DoModal();properties_open=false;
        if(result!=IDOK) {if(fresh) BM_FreeEntry(&address_context,entry);else BM_CancelEdit(&address_context,entry);}
        Refresh();save_addresses();
    }
    BOOL PreTranslateMessage(MSG *message) override {
        if(message->message==WM_KEYDOWN&&message->hwnd==tree.GetSafeHwnd()) {
            if(message->wParam==VK_DELETE) {SendMessage(WM_COMMAND,IDC_AB_DELETE);return TRUE;}
            if(message->wParam==VK_RETURN) {SendMessage(WM_COMMAND,IDC_AB_PROPERTIES);return TRUE;}
        }
        return CFrameWnd::PreTranslateMessage(message);
    }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create) {
        if(CFrameWnd::OnCreate(create)==-1) return -1;
        if(!tree.Create(WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS,CRect(),this,IDC_AB_TREE)) return -1;
        UINT panes[]={ID_SEPARATOR};if(!status.Create(this)||!status.SetIndicators(panes,1)) return -1;
        status.SetPaneText(0,"Netscape");ClassicUI::Apply(this);tree.SetBkColor(GetSysColor(COLOR_WINDOW));tree.SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
        WFE_RegisterNativeWindow(&address_context,GetSafeHwnd());Refresh();return 0;
    }
    afx_msg void OnSize(UINT type,int width,int height) {
        CFrameWnd::OnSize(type,width,height);if(!tree.GetSafeHwnd()) return;
        int footer=ClassicUI::Scale(this,21);tree.MoveWindow(0,0,width,(std::max)(0,height-footer));status.MoveWindow(0,(std::max)(0,height-footer),width,footer);
    }
    afx_msg BOOL OnEraseBkgnd(CDC *dc) {CRect r;GetClientRect(r);dc->FillSolidRect(r,ClassicUI::Face());return TRUE;}
    afx_msg HBRUSH OnCtlColor(CDC *dc,CWnd *window,UINT type) {return ClassicUI::ControlColor(dc,window,type);}
    afx_msg LRESULT OnRefresh(WPARAM,LPARAM) {Refresh();return 0;}
    afx_msg LRESULT OnEditRequest(WPARAM,LPARAM) {
        auto entry=pending_address_edit;pending_address_edit=nullptr;
        if(entry&&address_exists(BM_GetRoot(&address_context),entry)) {Select(entry);Properties(entry,false);}
        return 0;
    }
    afx_msg void OnClose() {if(!properties_open&&((BM_ReadFailed(&address_context)&&!BM_Modified(&address_context))||save_addresses())) CFrameWnd::OnClose();}
    afx_msg void OnDestroy() {WFE_UnregisterNativeWindow(&address_context);address_window=nullptr;pending_address_edit=nullptr;CFrameWnd::OnDestroy();}
    afx_msg void OnDoubleClick(NMHDR*,LRESULT *result) {Properties(Selected(),false);*result=0;}
    afx_msg void OnCommandRange(UINT command) {
        auto selected=Selected();
        if(command==IDC_AB_CLOSE) {SendMessage(WM_CLOSE);return;}
        if(command==IDC_AB_RETRY_LOAD) {
            bool discard=!BM_Modified(&address_context)||FE_Confirm((&address_context),"Discard unsaved address book changes and retry loading the file?");
            if(discard) WFE_AddressBookRetryLoad(TRUE);
            return;
        }
        if(BM_ReadFailed(&address_context)) return;
        if(command==IDC_AB_SAVE) {save_addresses();return;}
        if(command==IDC_AB_ADD_USER||command==IDC_AB_ADD_LIST) {Properties(command==IDC_AB_ADD_LIST?BM_NewHeader(""):BM_NewAddress("",""),true);return;}
        if(command==IDC_AB_PROPERTIES) {Properties(selected,false);return;}
        if(command==IDC_AB_COPY||command==IDC_AB_CUT||command==IDC_AB_PASTE||command==IDC_AB_SELECT_ALL) return;
        if(command==IDC_AB_COMPOSE) {
            if(selected&&selected!=BM_GetRoot(&address_context)) {
                std::string to=full_address(selected);
                if(!to.empty()) MSG_ComposeMessage(nullptr,nullptr,nullptr,to.c_str(),nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,FALSE,FALSE);
            }
            return;
        }
        BM_CommandType action=command==IDC_AB_DELETE?BM_Cmd_Delete:command==IDC_AB_UNDO?BM_Cmd_Undo:command==IDC_AB_REDO?BM_Cmd_Redo:BM_Cmd_SelectAllBookmarks;
        if(selected==BM_GetRoot(&address_context)&&action==BM_Cmd_Delete) return;
        if(selected&&action!=BM_Cmd_Undo&&action!=BM_Cmd_Redo) select_address(selected);
        BM_ObeyCommand(&address_context,action);Refresh();save_addresses();
    }
    afx_msg void OnUpdate(CCmdUI *command) {
        auto selected=Selected();bool editable=selected&&selected!=BM_GetRoot(&address_context);bool enabled=true;
        if(command->m_nID==IDC_AB_PROPERTIES||command->m_nID==IDC_AB_DELETE||command->m_nID==IDC_AB_COMPOSE) enabled=editable;
        if(command->m_nID==IDC_AB_COPY||command->m_nID==IDC_AB_CUT||command->m_nID==IDC_AB_PASTE||command->m_nID==IDC_AB_SELECT_ALL) enabled=false;
        if(command->m_nID==IDC_AB_UNDO||command->m_nID==IDC_AB_REDO) enabled=BM_FindCommandStatus(&address_context,command->m_nID==IDC_AB_UNDO?BM_Cmd_Undo:BM_Cmd_Redo);
        if(command->m_nID==IDC_AB_RETRY_LOAD) enabled=BM_ReadFailed(&address_context)&&!properties_open;
        else if(BM_ReadFailed(&address_context)&&command->m_nID!=IDC_AB_CLOSE) enabled=false;
        command->Enable(enabled);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(AddressBookFrame,CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_MESSAGE(WM_ADDRESS_REFRESH,OnRefresh)
 ON_MESSAGE(WM_ADDRESS_EDIT,OnEditRequest)
 ON_NOTIFY(NM_DBLCLK,IDC_AB_TREE,OnDoubleClick)
 ON_COMMAND_RANGE(IDC_AB_ADD_USER,IDC_AB_RETRY_LOAD,OnCommandRange)
 ON_UPDATE_COMMAND_UI_RANGE(IDC_AB_ADD_USER,IDC_AB_RETRY_LOAD,OnUpdate)
END_MESSAGE_MAP()

extern "C" MWContext *WFE_AddressBookContext(int view)
{
    if(!address_initialized) {
        char *path=WH_FileName("address.htm",xpBookmarks);if(!path) return nullptr;
        WFE_InitContext(&address_context,nullptr);address_context.type=MWContextAddressBook;
        if(BM_InitializeBookmarksContext(&address_context)<0) {WFE_DestroyContext(&address_context);XP_FREE(path);return nullptr;}
        address_initialized=true;BM_ReadBookmarksFromDisk(&address_context,path,nullptr);XP_FREE(path);
    }
    if(view) WFE_AddressBookShow();return &address_context;
}
extern "C" void WFE_AddressBookShow(void)
{
    if(!WFE_AddressBookContext(FALSE)) return;
    if(!address_window) {
        auto window=new AddressBookFrame;
        if(!window->Create(nullptr,"Address Book - address.htm",WS_OVERLAPPEDWINDOW,CRect(100,100,760,580),nullptr,MAKEINTRESOURCE(IDR_ADDRESSBOOK))) return;
        address_window=window;ClassicUI::SetWindowIcon(window,IDI_BOOKMARKS);
    }
    address_window->ShowWindow(SW_SHOWNORMAL);address_window->SetForegroundWindow();
}
extern "C" HWND WFE_AddressBookWindow(void) {return address_window?address_window->GetSafeHwnd():nullptr;}
extern "C" int WFE_AddressBookRetryLoad(int discard_changes)
{
    if(!address_initialized&&!WFE_AddressBookContext(FALSE)) return 0;
    if(!BM_ReadFailed(&address_context)) return 1;
    if((address_window&&address_window->properties_open)||(BM_Modified(&address_context)&&!discard_changes)) return 0;
    char *path=WH_FileName("address.htm",xpBookmarks);if(!path) return 0;
    pending_address_edit=nullptr;BM_ReadBookmarksFromDisk(&address_context,path,nullptr);XP_FREE(path);
    if(address_window) address_window->Refresh();
    return !BM_ReadFailed(&address_context);
}
extern "C" void WFE_AddressBookChanged(MWContext *context)
{
    if(context==&address_context&&address_window) address_window->PostMessage(WM_ADDRESS_REFRESH);
}
extern "C" void WFE_AddressBookEdit(MWContext *context,BM_Entry *entry)
{
    if(context!=&address_context||!address_exists(BM_GetRoot(context),entry)) return;
    WFE_AddressBookShow();if(address_window) {pending_address_edit=entry;address_window->PostMessage(WM_ADDRESS_EDIT);}
}
extern "C" void WFE_AddressBookShutdown(void)
{
    save_addresses();if(address_window) address_window->DestroyWindow();
}
#include "addressbook-test.inc"
