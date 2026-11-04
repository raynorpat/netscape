/* Included by main.cpp after the native UI fixture helpers.  Exercise the
   original menu command routes against real MSG and layout state. */
static void original_menu_pump(unsigned milliseconds = 200)
{
    ULONGLONG end = GetTickCount64() + milliseconds;
    do { mail_smoke_pump(); } while (GetTickCount64() < end);
}

static bool original_menu_marked(BrowserFrame *frame, bool marked)
{
    MSG_MessageLine line; int count = 0;
    for (; MSG_GetThreadLine(&frame->context, count, &line, nullptr); ++count)
        if (!!(line.flags & MSG_FLAG_MARKED) != marked) return false;
    return count == 2;
}

static void original_menu_select(BrowserFrame *frame, int row)
{
    frame->messages.SetFocus();
    frame->messages.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (row >= 0 && row < frame->messages.GetItemCount())
        frame->messages.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED,
                                    LVIS_SELECTED | LVIS_FOCUSED);
    if (row >= 0) browser_smoke_wait(frame);
    else original_menu_pump();
}

static int original_menu_folder_total(BrowserFrame *frame, unsigned long flag)
{
    MSG_FolderLine folder;
    for (int row = 0; MSG_GetFolderLine(&frame->context, row, &folder, nullptr); ++row)
        if (folder.flags & flag) return folder.total;
    return -1;
}

bool original_mail_menu_smoke(BrowserFrame *frame, const char *report)
{
    if (!frame || frame->mode != 1 || frame->messages.GetItemCount() != 2) return false;
    int selected = frame->messages.GetNextItem(-1, LVNI_SELECTED);
    CString original_subject = selected >= 0
        ? frame->messages.GetItemText(selected, MailPaneUI::SubjectColumn) : CString("");
    frame->messages.SetFocus();
    frame->SendMessage(WM_COMMAND, ID_EDIT_SELECT_ALL); original_menu_pump();
    bool select_all = frame->messages.GetSelectedCount() == 2
        && MSG_GetNumSelectedMessages(&frame->context) == 2;
    frame->SendMessage(WM_COMMAND, ID_MENU_FLAG); original_menu_pump();
    bool flag_unflag = original_menu_marked(frame, true);
    frame->SendMessage(WM_COMMAND, ID_MENU_UNFLAG); original_menu_pump();
    flag_unflag = flag_unflag && original_menu_marked(frame, false);

    /* Ctrl is deliberately up: LVN_ITEMCHANGED must report the entire native
       selection, including a deselection that leaves another row selected. */
    frame->messages.SetItemState(0, 0, LVIS_SELECTED);
    browser_smoke_wait(frame);
    XP_Bool can_reply = FALSE;
    MSG_CommandStatus(&frame->context, MSG_ReplyToSender, &can_reply, nullptr, nullptr, nullptr);
    bool native_deselect = frame->messages.GetSelectedCount() == 1
        && frame->messages.GetNextItem(-1, LVNI_SELECTED) == 1
        && MSG_GetNumSelectedMessages(&frame->context) == 1 && can_reply
        && !frame_text(frame).empty() && frame->completed && frame->load_status >= 0;
    original_menu_select(frame, -1);
    bool native_clear = frame->messages.GetSelectedCount() == 0
        && MSG_GetNumSelectedMessages(&frame->context) == 0;
    MSG_CommandStatus(&frame->context,MSG_ReplyToSender,&can_reply,nullptr,nullptr,nullptr);
    native_clear=native_clear && !can_reply;
    original_menu_select(frame, 0);

    frame->SendMessage(WM_COMMAND, ID_MENU_SORT_SUBJECT); original_menu_pump();
    bool sort_subject = MSG_GetToggleStatus(&frame->context, MSG_SortBySubject) == MSG_Checked;
    bool was_reverse = MSG_GetToggleStatus(&frame->context, MSG_SortBackward) == MSG_Checked;
    frame->SendMessage(WM_COMMAND, ID_MENU_SORT_ASCENDING); original_menu_pump();
    bool sort_ascending = (MSG_GetToggleStatus(&frame->context, MSG_SortBackward) == MSG_Checked) != was_reverse;
    if (MSG_GetToggleStatus(&frame->context, MSG_SortBackward) == MSG_Checked) {
        frame->SendMessage(WM_COMMAND, ID_MENU_SORT_ASCENDING); original_menu_pump();
    }
    frame->SendMessage(WM_COMMAND, ID_MENU_SORT_DATE); original_menu_pump();
    MSG_MessageLine first = {}, second = {};
    bool sort_date = MSG_GetToggleStatus(&frame->context, MSG_SortByDate) == MSG_Checked
        && MSG_GetThreadLine(&frame->context, 0, &first, nullptr)
        && MSG_GetThreadLine(&frame->context, 1, &second, nullptr) && first.date <= second.date;
    original_menu_select(frame, 0);

    frame->SendMessage(WM_COMMAND, ID_MENU_ATTACHMENTS_LINKS); browser_smoke_wait(frame);
    bool attachments = MSG_GetToggleStatus(&frame->context, MSG_AttachmentsAsLinks) == MSG_Checked;
    frame->SendMessage(WM_COMMAND, ID_MENU_ATTACHMENTS_INLINE); browser_smoke_wait(frame);
    attachments = attachments && MSG_GetToggleStatus(&frame->context, MSG_AttachmentsInline) == MSG_Checked;
    frame->SendMessage(WM_COMMAND, ID_MENU_HEADERS_BRIEF); browser_smoke_wait(frame);
    bool headers = MSG_GetToggleStatus(&frame->context, MSG_ShowMicroHeaders) == MSG_Checked;
    frame->SendMessage(WM_COMMAND, ID_MAIL_HEADERS_ALL); browser_smoke_wait(frame);
    std::string all_headers = frame_text(frame);
    headers = headers && MSG_GetToggleStatus(&frame->context, MSG_ShowAllHeaders) == MSG_Checked
        && all_headers.find("Message-ID") != std::string::npos;
    std::string before_images = frame->current_url;
    frame->SendMessage(WM_COMMAND, ID_BROWSER_IMAGES); browser_smoke_wait(frame);
    bool images_headers = MSG_GetToggleStatus(&frame->context, MSG_ShowAllHeaders) == MSG_Checked
        && !before_images.empty() && before_images == frame->current_url
        && frame_text(frame).find("Message-ID") != std::string::npos
        && frame->completed && frame->load_status >= 0;
    frame->SendMessage(WM_COMMAND, ID_MAIL_HEADERS_SOME); browser_smoke_wait(frame);
    headers = headers && MSG_GetToggleStatus(&frame->context, MSG_ShowSomeHeaders) == MSG_Checked;

    bool find_again = WFE_FindText(&frame->context, "User", 0, 0, 1) != 0;
    browser_smoke_wait(frame); frame->RefreshMailLists();
    std::string first_match = frame->current_url;
    frame->SendMessage(WM_COMMAND, ID_MENU_FIND_AGAIN); browser_smoke_wait(frame);
    frame->RefreshMailLists();
    find_again = find_again && !first_match.empty() && first_match != frame->current_url
        && frame->completed && frame->load_status >= 0;

    /* Copy exercises filing without deleting or moving either Inbox fixture. */
    original_menu_select(frame, 0);
    int inbox_before = original_menu_folder_total(frame, MSG_FOLDER_FLAG_INBOX);
    int trash_before = original_menu_folder_total(frame, MSG_FOLDER_FLAG_TRASH);
    frame->SendMessage(WM_COMMAND, ID_MENU_COPY_TRASH); original_menu_pump(500);
    frame->RefreshMailLists();
    bool copy_trash = inbox_before == 2 && trash_before >= 0
        && original_menu_folder_total(frame, MSG_FOLDER_FLAG_INBOX) == inbox_before
        && original_menu_folder_total(frame, MSG_FOLDER_FLAG_TRASH) == trash_before + 1
        && frame->messages.GetItemCount() == 2;

    frame->messages.SetFocus(); frame->SendMessage(WM_COMMAND, ID_EDIT_SELECT_ALL);
    frame->SendMessage(WM_COMMAND, ID_MENU_UNFLAG); original_menu_pump();
    frame->SendMessage(WM_COMMAND, ID_MAIL_HEADERS_SOME); browser_smoke_wait(frame);
    int restore_row = 0;
    for (int row = 0; row < frame->messages.GetItemCount(); ++row)
        if (frame->messages.GetItemText(row, MailPaneUI::SubjectColumn) == original_subject) restore_row = row;
    original_menu_select(frame, restore_row);
    bool restore = original_menu_marked(frame, false)
        && MSG_GetNumSelectedMessages(&frame->context) == 1
        && MSG_GetToggleStatus(&frame->context, MSG_ShowSomeHeaders) == MSG_Checked
        && frame->completed && frame->load_status >= 0;
    FILE *file = fopen(report, "ab");
    if (!file) return false;
    fprintf(file, "menu_flag_unflag=%d\nmenu_select_all=%d\nmenu_native_deselect=%d\nmenu_native_clear=%d\nmenu_sort_subject=%d\nmenu_sort_date=%d\nmenu_sort_ascending=%d\nmenu_attachments=%d\nmenu_headers=%d\nmenu_mail_images_preserve_headers=%d\nmenu_find_again=%d\nmenu_copy_trash=%d\nmenu_restore=%d\n",
        flag_unflag, select_all, native_deselect, native_clear, sort_subject, sort_date,
        sort_ascending, attachments, headers, images_headers, find_again, copy_trash, restore);
    fclose(file);
    return flag_unflag && select_all && native_deselect && native_clear && sort_subject
        && sort_date && sort_ascending && attachments && headers && images_headers
        && find_again && copy_trash && restore;
}

bool original_browser_menu_smoke(BrowserFrame *frame, const char *report, const char *base_url)
{
    if (!frame || frame->mode || !base_url || !*base_url) return false;
    if (frame->current_url != base_url) { frame->Navigate(base_url); browser_smoke_wait(frame); }
    frame_text(frame);
    bool find_again = WFE_FindText(&frame->context, "Classic toolbar", 0, 0, 1) != 0;
    LO_Element *first = nullptr, *last = nullptr; int32 start = 0, end = 0;
    LO_GetSelectionEndpoints(&frame->context, &first, &last, &start, &end);
    frame->SendMessage(WM_COMMAND, ID_MENU_FIND_AGAIN); original_menu_pump();
    LO_Element *next = nullptr, *next_last = nullptr; int32 next_start = 0, next_end = 0;
    LO_GetSelectionEndpoints(&frame->context, &next, &next_last, &next_start, &next_end);
    find_again = find_again && first && last && next && next_last
        && (first != next || start != next_start || last != next_last || end != next_end);

    std::vector<BrowserFrame *> before = browser_windows;
    frame->SendMessage(WM_COMMAND, ID_MENU_DOCUMENT_SOURCE);
    browser_smoke_wait(frame);
    std::vector<BrowserFrame *> sources;
    for (auto candidate : browser_windows)
        if (std::find(before.begin(), before.end(), candidate) == before.end()) sources.push_back(candidate);
    bool source = sources.size() == 1;
    BrowserFrame *source_frame = sources.empty() ? nullptr : sources[0];
    if (source_frame) {
        browser_smoke_wait(source_frame);
        std::string text = frame_text(source_frame);
        std::string markup=text;markup.erase(std::remove_if(markup.begin(),markup.end(),[](unsigned char c){return c==' '||c=='\r'||c=='\n'||c=='\t';}),markup.end());
        source = source && source_frame->completed && source_frame->load_status >= 0
            && !source_frame->GetMenu() && !source_frame->chrome.toolbar_visible && !source_frame->chrome.location_visible
            && source_frame->current_url == std::string("view-source:") + base_url
            && (markup.find("<HTML>") != std::string::npos || markup.find("<html>") != std::string::npos)
            && text.find("Classic toolbar needle") != std::string::npos;
    }

    CMenu *windows = nullptr; UINT window_index = 0; CMenu *menu = frame->GetMenu();
    for (int index = 0; menu && index < menu->GetMenuItemCount(); ++index) {
        CMenu *popup = menu->GetSubMenu(index);
        if (WFE_IsWindowMenu(popup)) { windows = popup; window_index = index; break; }
    }
    if (windows) frame->SendMessage(WM_INITMENUPOPUP, (WPARAM)windows->GetSafeHmenu(), MAKELPARAM(window_index, FALSE));
    auto entries = window_menu_entries.find(frame->GetSafeHwnd());
    bool window_list = windows && entries != window_menu_entries.end();
    UINT activate = 0; size_t live_count = 0;
    for (auto entry : native_windows) {
        if(entry.first->type!=MWContextBrowser && entry.first->type!=MWContextMessageComposition) continue;
        HWND handle = entry.second;
        if (!::IsWindow(handle) || (::GetWindowLong(handle, GWL_STYLE) & WS_CHILD)) continue;
        char caption[512] = {0}; ::GetWindowTextA(handle, caption, sizeof(caption));
        if (!*caption) continue;
        ++live_count; bool found = false;
        if (entries != window_menu_entries.end()) {
            for (size_t index = 0; index < entries->second.size(); ++index) {
                if (entries->second[index].first == entry.first && entries->second[index].second == handle) {
                    found = windows && windows->GetMenuState(36000 + (UINT)index, MF_BYCOMMAND) != (UINT)-1;
                    if (source_frame && handle == source_frame->GetSafeHwnd()) activate = 36000 + (UINT)index;
                }
            }
        }
        window_list = window_list && found;
    }
    window_list = window_list && live_count >= 2 && entries->second.size() == live_count;
    bool window_activate = false;
    if (activate && source_frame) {
        frame->ShowWindow(SW_SHOW); ::SetActiveWindow(frame->GetSafeHwnd());original_menu_pump();
        frame->SendMessage(WM_COMMAND, activate); original_menu_pump();
        window_activate = ::GetActiveWindow() == source_frame->GetSafeHwnd() && ::IsWindowVisible(source_frame->GetSafeHwnd());
    }
    for (auto created : sources) created->SendMessage(WM_CLOSE);
    original_menu_pump(); frame->ShowWindow(SW_SHOW); ::SetForegroundWindow(frame->GetSafeHwnd());
    CMenu *go=frame->GetMenu()->GetSubMenu(3);
    frame->SendMessage(WM_INITMENUPOPUP,(WPARAM)go->GetSafeHmenu(),MAKELPARAM(3,FALSE));
    auto go_entries=go_menu_entries.find(frame->GetSafeHwnd());
    bool go_history=go_entries!=go_menu_entries.end() && go_entries->second.size()>=2;
    History_entry *current=SHIST_GetCurrent(&frame->context.hist);std::string destination;UINT go_command=0;
    if(go_history) {
        XP_List *list=SHIST_GetList(&frame->context);
        for(size_t i=0;i<go_entries->second.size() && !go_command;++i) {
            for(int j=1;j<=XP_ListCount(list);++j) {
                History_entry *entry=(History_entry *)XP_ListGetObjectNum(list,j);
                if(entry && entry!=current && entry->unique_id==go_entries->second[i] && entry->method==URL_GET_METHOD) {
                    destination=entry->address;go_command=36500+(UINT)i;break;
                }
            }
        }
        if(go_command) {frame->SendMessage(WM_COMMAND,go_command);browser_smoke_wait(frame);}
        go_history=go_command && frame->completed && frame->load_status>=0 && frame->current_url==destination;
    }
    FILE *file = fopen(report, "ab");
    if (!file) return false;
    fprintf(file, "menu_document_source=%d\nmenu_browser_find_again=%d\nmenu_window_list=%d\nmenu_window_activate=%d\nmenu_go_history=%d\n",
        source, find_again, window_list, window_activate,go_history);
    fclose(file);
    return source && find_again && window_list && window_activate && go_history;
}
