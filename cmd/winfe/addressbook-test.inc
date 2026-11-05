/* Native tests use only an isolated, test-owned profile. */
static std::string address_file_bytes(const std::string &path)
{
    FILE *file=fopen(path.c_str(),"rb");if(!file) return "";
    std::string result;char buffer[4096];size_t size;
    while((size=fread(buffer,1,sizeof(buffer),file))!=0) result.append(buffer,size);
    fclose(file);return result;
}
static bool address_edit_cancelled;
static UINT_PTR address_edit_timer;
static void CALLBACK cancel_core_address_edit(HWND,UINT,UINT_PTR,DWORD)
{
    HWND owner=WFE_AddressBookWindow();if(!owner) return;
    HWND popup=GetLastActivePopup(owner);
    if(popup!=owner&&::GetDlgItem(popup,IDC_AB_NAME)) {
        address_edit_cancelled=true;KillTimer(nullptr,address_edit_timer);
        ::PostMessage(popup,WM_COMMAND,IDCANCEL,0);
    }
}
extern "C" int WFE_AddressBookSmoke(const char *report,const char *,int reopen)
{
    FILE *file=fopen(report,"a");if(!file) return 3;
    if(reopen==2) {
        CString error;auto entry=BM_NewAddress("","");
        bool blocked=entry&&apply_properties(entry,true,"Retained change","memory@example.invalid","memory","","",error)
            &&!save_addresses()&&BM_Modified(&address_context);
        fprintf(file,"read_save_blocked=%d\n",blocked);fclose(file);return blocked?0:1;
    }
    bool hidden=address_window==nullptr;
    auto root=BM_GetRoot(&address_context);
    auto seeded=BM_FindAddress(&address_context,"mailto:?to=seed@example.invalid");
    bool preserved=seeded&&strcmp(BM_GetName(seeded),"Existing & Contact")==0;
    bool edited=true,deleted=true,cancelled=true,invalid=true,list=true,retry=true,persisted=true,core_edit=true;
    auto alpha=BM_FindAddress(&address_context,"mailto:?to=alpha@example.invalid");CString error;
    if(!reopen) {
        fprintf(file,"phase=contacts\n");fflush(file);
        alpha=BM_NewAddress("","");auto beta=BM_NewAddress("","");auto group=BM_NewHeader("");
        edited=alpha&&beta&&apply_properties(alpha,true,"Alpha, A & <native>","alpha@example.invalid","alpha","Keep & <notes>","",error)
            &&apply_properties(beta,true,"Beta","beta@example.invalid","beta","","",error);
        list=group&&apply_properties(group,true,"Native Team","","team","Two members",
            "Alpha <alpha@example.invalid>\r\nBeta <beta@example.invalid>",error);
        fprintf(file,"phase=validation\n");fflush(file);
        std::string before=full_address(alpha);
        invalid=!apply_properties(beta,false,"Changed","other@example.invalid","ALPHA","","",error)
            &&!apply_properties(alpha,false,"Bad\r\nBcc: injected@example.invalid","alpha@example.invalid","alpha","","",error)
            &&!apply_properties(alpha,false,"Bad","not-an-address","alpha","","",error)
            &&full_address(alpha)==before&&strcmp(BM_GetName(beta),"Beta")==0;
        fprintf(file,"phase=delete\n");fflush(file);
        auto discarded=BM_NewAddress("Discard","discard@example.invalid");BM_AppendToHeader(&address_context,root,discarded);
        select_address(discarded);BM_ObeyCommand(&address_context,BM_Cmd_Delete);
        deleted=BM_FindAddress(&address_context,"mailto:?to=discard@example.invalid")==nullptr;
        fprintf(file,"phase=show\n");fflush(file);
        WFE_AddressBookShow();address_window->Refresh();
        fprintf(file,"phase=properties_cancel\n");fflush(file);
        class CancelProperties : public AddressProperties {
        public:
            CancelProperties(CWnd *owner,BM_Entry *entry):AddressProperties(owner,entry,false) {}
            BOOL OnInitDialog() override {AddressProperties::OnInitDialog();SetDlgItemText(IDC_AB_NAME,"Cancelled name");PostMessage(WM_COMMAND,IDCANCEL);return TRUE;}
        } dialog(address_window,alpha);
        cancelled=dialog.DoModal()==IDCANCEL&&full_address(alpha)==before;
        address_edit_cancelled=false;address_edit_timer=::SetTimer(nullptr,0,30,cancel_core_address_edit);
        BM_EditAddress(&address_context,"mailto:?to=Sender%20%3Csender@example.invalid%3E");
        ULONGLONG deadline=GetTickCount64()+3000;
        while(!address_edit_cancelled&&GetTickCount64()<deadline) {
            MSG message;while(PeekMessage(&message,nullptr,0,0,PM_REMOVE)) {if(message.message!=WM_QUIT) {TranslateMessage(&message);DispatchMessage(&message);}}
            WFE_Pump();
        }
        KillTimer(nullptr,address_edit_timer);
        core_edit=address_edit_cancelled&&!BM_FindAddress(&address_context,"mailto:?to=sender@example.invalid");
        fprintf(file,"phase=save\n");fflush(file);
        persisted=save_addresses();
        std::string path=BM_GetFileName(&address_context),backup=path+".fixture",original=address_file_bytes(path);
        BM_SetName(&address_context,alpha,"Alpha, A & <native> retry");
        retry=MoveFileExA(path.c_str(),backup.c_str(),0)&&CreateDirectoryA(path.c_str(),nullptr);
        if(retry) {
            retry=!save_addresses()&&BM_Modified(&address_context)&&address_file_bytes(backup)==original;
            bool restored=RemoveDirectoryA(path.c_str())&&MoveFileExA(backup.c_str(),path.c_str(),0);
            retry=restored&&retry&&save_addresses();
        }
    }
    alpha=BM_FindAddress(&address_context,"mailto:?to=alpha@example.invalid");
    edited=edited&&alpha&&strcmp(BM_GetName(alpha),"Alpha, A & <native> retry")==0;
    fprintf(file,"alpha_name=%s\nalpha_nick=%s\n",alpha?BM_GetName(alpha):"missing",alpha?BM_GetNickName(alpha):"missing");
    char *expanded=BM_ExpandHeaderString(&address_context,"alpha, team",TRUE);
    std::string expansion=expanded?expanded:"";XP_FREE(expanded);
    list=list&&expansion.find("alpha@example.invalid")!=std::string::npos&&expansion.find("beta@example.invalid")!=std::string::npos;
    WFE_AddressBookShow();HWND window=WFE_AddressBookWindow();WFE_AddressBookShow();
    bool singleton=window&&window==WFE_AddressBookWindow();address_window->Refresh();
    bool native_tree=address_window->tree.GetCount()>=6;
    fprintf(file,"hidden_lookup=%d\nseed_preserved=%d\ncontact_edited=%d\ncontact_deleted=%d\nproperties_cancel=%d\ninvalid_rejected=%d\nlist_expanded=%d\nsave_retry=%d\npersisted=%d\nsingleton=%d\nnative_tree=%d\ncore_edit_cancel=%d\nexpanded=%s\n",hidden,preserved,edited,deleted,cancelled,invalid,list,retry,persisted,singleton,native_tree,core_edit,expansion.c_str());
    fclose(file);return hidden&&preserved&&edited&&deleted&&cancelled&&invalid&&list&&retry&&persisted&&singleton&&native_tree&&core_edit?0:1;
}
