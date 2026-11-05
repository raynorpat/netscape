#include <afxwin.h>
#include <map>
#include <string>
#include <vector>
#include "addressbook.h"
#include "addressbook-resource.h"
#include "classic.h"
extern "C" {
#include "bkmks.h"
#include "mime.h"
}

/* Accept RFC header folding, but never a new header or a raw control byte. */
static bool recipient_line(const char *value,CString &line)
{
    line.Empty();if(!value) return true;
    for(size_t i=0;value[i];++i) {
        unsigned char c=(unsigned char)value[i];
        if(c=='\r'||c=='\n') {
            if(c=='\r') {if(value[i+1]!='\n') return false;++i;}
            if(value[i+1]!=' '&&value[i+1]!='\t') return false;
            line+=' ';
        } else if(c<32&&c!='\t') return false;
        else line+=(char)c;
        if(line.GetLength()>32767) return false;
    }
    return true;
}
struct RecipientRow {CString label,addresses;};
class RecipientDialog : public ClassicDialog {
public:
    RecipientDialog(CWnd *owner,const CString &to,const CString &cc,const CString &bcc):ClassicDialog(IDD_ADDRESS_PICKER,owner),owner_handle(owner->GetSafeHwnd()) {values[0]=to;values[1]=cc;values[2]=bcc;}
    CString values[3];CListBox available;std::vector<RecipientRow> rows;
    ClassicButton to_button,cc_button,bcc_button;
    HWND owner_handle;bool cancelled=false;
    BOOL OnInitDialog() override {
        ClassicDialog::OnInitDialog();available.SubclassDlgItem(IDC_AB_PICK_LIST,this);
        ClassicButton *buttons[]={&to_button,&cc_button,&bcc_button};int ids[]={IDC_AB_TO,IDC_AB_CC,IDC_AB_BCC};
        for(int i=0;i<3;++i) {buttons[i]->SubclassDlgItem(ids[i],this);buttons[i]->SetButtonStyle(BS_OWNERDRAW);}
        if(available.AddString("Address Book")<0) {PostMessage(WM_COMMAND,IDCANCEL);return TRUE;}
        rows.push_back(RecipientRow{});
        auto context=WFE_AddressBookContext(FALSE);
        if(context) for(auto entry=BM_GetChildren(BM_GetRoot(context));entry;entry=BM_GetNext(entry)) {
            auto original=BM_IsAlias(entry)?BM_GetAliasOriginal(entry):entry;
            if(!original||(!BM_IsAddress(original)&&!BM_IsHeader(original))) continue;
            /* Keep snapshots: another window may edit/delete the original. */
            char *full=BM_GetFullAddress(context,original);
            char *expanded=full?BM_ExpandHeaderString(context,full,TRUE):nullptr;
            RecipientRow row;row.label=BM_GetName(original);
            /* NULL means no nickname needed expansion, not no address. */
            bool safe=recipient_line(expanded?expanded:full,row.addresses);
            if(BM_IsAddress(original)&&full) row.label=full;
            XP_FREE(full);XP_FREE(expanded);
            if(!safe||row.label.Find('\r')>=0||row.label.Find('\n')>=0) continue;
            if(available.AddString(row.label)>=0) rows.push_back(row);
        }
        available.SetSel(0,TRUE);
        if(cancelled) PostMessage(WM_COMMAND,IDCANCEL);
        return TRUE;
    }
    void OnOK() override {
        if(cancelled||!::IsWindow(owner_handle)) {EndDialog(IDCANCEL);return;}
        ClassicDialog::OnOK();
    }
    void Cancel() {cancelled=true;if(GetSafeHwnd()) SendMessage(WM_COMMAND,IDCANCEL);}
protected:
    afx_msg void OnAdd(UINT command) {
        int target=command==IDC_AB_TO?0:command==IDC_AB_CC?1:2;
        CString added;
        for(int i=1;i<available.GetCount()&&i<(int)rows.size();++i) if(available.GetSel(i)>0&&!rows[i].addresses.IsEmpty()) {
            if(!added.IsEmpty()) added+=", ";added+=rows[i].addresses;
        }
        if(added.IsEmpty()) return;
        CString base,others;
        if(!recipient_line(values[target],base)) {MessageBox("The recipient field contains an invalid line break.");return;}
        for(int i=0;i<3;++i) if(i!=target) {
            CString value;if(!recipient_line(values[i],value)) {MessageBox("A recipient field contains an invalid line break.");return;}
            if(value.IsEmpty()) continue;
            if(!others.IsEmpty()) others+=", ";others+=value;
        }
        CString combined=base;if(!combined.IsEmpty()) combined+=", ";combined+=added;
        char *unique=MSG_RemoveDuplicateAddresses(combined,others);CString value;
        bool valid=unique&&recipient_line(unique,value);XP_FREE(unique);
        if(!valid) {MessageBox("Could not add these recipients.");return;}
        values[target]=value;
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(RecipientDialog,ClassicDialog)
 ON_COMMAND_RANGE(IDC_AB_TO,IDC_AB_BCC,OnAdd)
END_MESSAGE_MAP()
static std::map<HWND,RecipientDialog *> recipient_dialogs;
bool WFE_PickRecipients(CWnd *owner,CString &to,CString &cc,CString &bcc)
{
    if(!owner||!::IsWindow(owner->GetSafeHwnd())) return false;
    auto context=WFE_AddressBookContext(FALSE);
    if(!context||BM_ReadFailed(context)) {
        FE_Alert(context,"The address book could not be loaded. Open Window > Address Book and choose File > Retry Load.");return false;
    }
    HWND handle=owner->GetSafeHwnd();if(recipient_dialogs.count(handle)) return false;
    RecipientDialog dialog(owner,to,cc,bcc);recipient_dialogs[handle]=&dialog;
    INT_PTR result=dialog.DoModal();recipient_dialogs.erase(handle);
    if(result!=IDOK) return false;
    to=dialog.values[0];cc=dialog.values[1];bcc=dialog.values[2];return true;
}
void WFE_CancelRecipientPicker(CWnd *owner)
{
    auto found=recipient_dialogs.find(owner?owner->GetSafeHwnd():nullptr);
    if(found!=recipient_dialogs.end()) found->second->Cancel();
}
