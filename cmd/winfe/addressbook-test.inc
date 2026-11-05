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
        WFE_AddressBookShow();
        bool retry_menu=address_window&&address_window->GetMenu()->GetMenuState(IDC_AB_RETRY_LOAD,MF_BYCOMMAND)!=(UINT)-1;
        bool retained=!WFE_AddressBookRetryLoad(FALSE)&&BM_Modified(&address_context);
        char *native_path=WH_FileName("address.htm",xpBookmarks);std::string path=native_path?native_path:"";XP_FREE(native_path);
        std::string backup=path+".unreadable";
        const char data[]="<!DOCTYPE NETSCAPE-Addressbook-file-1>\n<DL><p>\n<DT><A HREF=\"mailto:seed@example.invalid\">Recovered contact</A>\n</DL><p>\n";
        bool restored=MoveFileExA(path.c_str(),backup.c_str(),0)!=FALSE;
        HANDLE handle=restored?CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr):INVALID_HANDLE_VALUE;
        DWORD written=0;restored=handle!=INVALID_HANDLE_VALUE&&WriteFile(handle,data,sizeof(data)-1,&written,nullptr)&&written==sizeof(data)-1;
        if(handle!=INVALID_HANDLE_VALUE) restored=CloseHandle(handle)&&restored;
        bool recovered=restored&&WFE_AddressBookRetryLoad(TRUE)&&!BM_ReadFailed(&address_context)&&!BM_Modified(&address_context)
            &&!BM_FindAddress(&address_context,"mailto:?to=memory@example.invalid");
        auto seed=recovered?BM_FindAddress(&address_context,"mailto:?to=seed@example.invalid"):nullptr;
        if(seed) BM_SetName(&address_context,seed,"Recovered and saved");
        recovered=recovered&&seed&&save_addresses()&&FE_GetAddressBook(nullptr,FALSE)==&address_context;
        fprintf(file,"read_save_blocked=%d\nretry_load_menu=%d\nreload_cancel_preserved=%d\nread_recovered=%d\n",blocked,retry_menu,retained,recovered);
        fclose(file);return blocked&&retry_menu&&retained&&recovered?0:1;
    }
    bool hidden=address_window==nullptr;
    auto root=BM_GetRoot(&address_context);
    auto seeded=BM_FindAddress(&address_context,"mailto:?to=seed@example.invalid");
    bool preserved=seeded&&strcmp(BM_GetName(seeded),"Existing & Contact")==0;
    bool edited=true,deleted=true,cancelled=true,invalid=true,list=true,retry=true,persisted=true,core_edit=true,undo_properties=true;
    auto alpha=BM_FindAddress(&address_context,"mailto:?to=alpha@example.invalid");CString error;
    if(!reopen) {
        fprintf(file,"phase=contacts\n");fflush(file);
        alpha=BM_NewAddress("","");auto beta=BM_NewAddress("","");auto group=BM_NewHeader("");
        edited=alpha&&beta&&apply_properties(alpha,true,"Alpha, A & <native>","alpha@example.invalid","alpha","Keep & <notes>","",error);
        BM_ObeyCommand(&address_context,BM_Cmd_Undo);
        undo_properties=!address_exists(root,alpha);
        BM_ObeyCommand(&address_context,BM_Cmd_Redo);
        undo_properties=undo_properties&&address_exists(root,alpha)&&strcmp(BM_GetAddress(alpha),"alpha@example.invalid")==0;
        edited=edited&&apply_properties(beta,true,"Beta","beta@example.invalid","beta","","",error);
        list=group&&apply_properties(group,true,"Native Team","","team","Two members",
            "Alpha <alpha@example.invalid>\r\nBeta <beta@example.invalid>",error);
        CString prior_members=list_members(group),many_members;
        int prior_count=BM_GetCount(&address_context);
        for(int i=0;i<14;++i) {CString member;member.Format("Member %d <member%d@example.invalid>\r\n",i,i);many_members+=member;}
        bool replaced=apply_properties(group,false,"Native Team","","team","Two members",many_members,error);
        BM_ObeyCommand(&address_context,BM_Cmd_Undo);
        undo_properties=undo_properties&&replaced&&list_members(group)==prior_members&&BM_GetCount(&address_context)==prior_count;
        BM_ObeyCommand(&address_context,BM_Cmd_Redo);
        undo_properties=undo_properties&&list_members(group).Find("member13@example.invalid")>=0;
        BM_ObeyCommand(&address_context,BM_Cmd_Undo);
        fprintf(file,"phase=validation\n");fflush(file);
        undo_properties=undo_properties&&BM_SetNickName(&address_context,alpha,"ALPHA")
            &&strcmp(BM_GetNickName(alpha),"alpha")==0;
        std::string before=full_address(alpha);
        invalid=!apply_properties(beta,false,"Changed","other@example.invalid","ALPHA","","",error)
            &&!apply_properties(alpha,false,"Bad\r\nBcc: injected@example.invalid","alpha@example.invalid","alpha","","",error)
            &&!apply_properties(alpha,false,"Bad","not-an-address","alpha","","",error)
            &&full_address(alpha)==before&&strcmp(BM_GetName(beta),"Beta")==0;
        fprintf(file,"phase=delete\n");fflush(file);
        auto discarded=BM_NewAddress("","");
        deleted=apply_properties(discarded,true,"Discard","discard@example.invalid","discard","","",error);
        select_address(discarded);BM_ObeyCommand(&address_context,BM_Cmd_Delete);
        deleted=deleted&&BM_FindAddress(&address_context,"mailto:?to=discard@example.invalid")==nullptr;
        BM_ObeyCommand(&address_context,BM_Cmd_Undo);
        char *restored_nickname=BM_ExpandHeaderString(&address_context,"discard",TRUE);
        deleted=deleted&&address_exists(root,discarded)&&restored_nickname&&strstr(restored_nickname,"discard@example.invalid");XP_FREE(restored_nickname);
        BM_ObeyCommand(&address_context,BM_Cmd_Redo);
        deleted=deleted&&BM_FindAddress(&address_context,"mailto:?to=discard@example.invalid")==nullptr;
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
    char *nickname=BM_ExpandHeaderString(&address_context,"alpha",TRUE);
    bool nickname_undo=nickname&&strstr(nickname,"alpha@example.invalid");XP_FREE(nickname);
    fprintf(file,"nickname_undo=%d\n",nickname_undo);
    char *expanded=BM_ExpandHeaderString(&address_context,"alpha, team",TRUE);
    std::string expansion=expanded?expanded:"";XP_FREE(expanded);
    list=list&&expansion.find("alpha@example.invalid")!=std::string::npos&&expansion.find("beta@example.invalid")!=std::string::npos;
    WFE_AddressBookShow();HWND window=WFE_AddressBookWindow();WFE_AddressBookShow();
    bool singleton=window&&window==WFE_AddressBookWindow();address_window->Refresh();
    bool native_tree=address_window->tree.GetCount()>=6;
    fprintf(file,"hidden_lookup=%d\nseed_preserved=%d\ncontact_edited=%d\ncontact_deleted=%d\nproperties_cancel=%d\ninvalid_rejected=%d\nlist_expanded=%d\nsave_retry=%d\npersisted=%d\nsingleton=%d\nnative_tree=%d\ncore_edit_cancel=%d\nundo_properties=%d\nexpanded=%s\n",hidden,preserved,edited,deleted,cancelled,invalid,list,retry,persisted,singleton,native_tree,core_edit,undo_properties,expansion.c_str());
    fclose(file);return hidden&&preserved&&edited&&deleted&&cancelled&&invalid&&list&&retry&&persisted&&singleton&&native_tree&&core_edit&&undo_properties&&nickname_undo?0:1;
}
