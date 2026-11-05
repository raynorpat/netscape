static int WFE_RunAddressBookSmoke(const char *report,const char *snapshot,int reopen)
{
    MWContext *source=WFE_NewWindow("about:blank");if(!source) return 2;
    MWContext *addresses=reopen==2?WFE_AddressBookContext(FALSE):FE_GetAddressBook(source,FALSE);
    FILE *file=fopen(report,"wb");if(!file) {WFE_CloseWindow(source);return 3;}
    fprintf(file,"address_context=%d\n",addresses&&addresses->type==MWContextAddressBook);
    fclose(file);int result=1;
    if(addresses&&addresses->type==MWContextAddressBook) {
        result=WFE_AddressBookSmoke(report,snapshot,reopen);
        if(reopen==2) {WFE_CloseWindow(source);return result;}
        auto owner=frame_for(source);CMenu menu;menu.CreatePopupMenu();menu.AppendMenu(MF_STRING|MF_GRAYED,ID_MENU_ADDRESS_BOOK,"Address Book");
        CCmdUI update;update.m_nID=ID_MENU_ADDRESS_BOOK;update.m_nIndex=0;update.m_nIndexMax=1;update.m_pMenu=&menu;
        WFE_WindowUpdateCommand(owner,&update);
        bool enabled=!(menu.GetMenuState(ID_MENU_ADDRESS_BOOK,MF_BYCOMMAND)&(MF_DISABLED|MF_GRAYED));
        HWND book=WFE_AddressBookWindow();
        bool routed=enabled&&WFE_WindowCommand(owner,ID_MENU_ADDRESS_BOOK)&&book==WFE_AddressBookWindow();
        bool captured=book&&chrome_capture(CWnd::FromHandlePermanent(book),snapshot,true);
        if(book) ::SendMessage(book,WM_CLOSE,0,0);
        bool retained=FE_GetAddressBook(source,FALSE)==addresses&&!WFE_AddressBookWindow();
        file=fopen(report,"a");if(file) {fprintf(file,"window_command=%d\ncaptured=%d\nclosed_lookup=%d\n",routed,captured,retained);fclose(file);}
        if(!routed||!captured||!retained) result=1;
    }
    WFE_CloseWindow(source);return result;
}
