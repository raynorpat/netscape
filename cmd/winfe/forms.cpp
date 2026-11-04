#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <string>
#include <vector>
#include "forms.h"
#include "images.h"
extern "C" {
#include "proto.h"
#include "shist.h"
HWND WFE_DocumentWindow(MWContext *);
}

struct NativeForm {
    MWContext *context;
    LO_FormElementData *data;
    LO_FormElementStruct *element;
    HWND window, browse;
    int width, height;
    bool displayed, enter_submit, restored;
};
static std::vector<NativeForm *> native_forms;
static const int maximum_text = 1024 * 1024;

static int bounded(int value, int fallback, int limit)
{
    return value < 1 ? fallback : (std::min)(value, limit);
}

static void **data_slot(LO_FormElementData *data)
{
    if (!data) return nullptr;
    switch (data->type) {
    case FORM_TYPE_TEXT: case FORM_TYPE_PASSWORD: case FORM_TYPE_FILE:
        return &data->ele_text.FE_Data;
    case FORM_TYPE_TEXTAREA: return &data->ele_textarea.FE_Data;
    case FORM_TYPE_RADIO: case FORM_TYPE_CHECKBOX: return &data->ele_toggle.FE_Data;
    case FORM_TYPE_SELECT_ONE: case FORM_TYPE_SELECT_MULT: return &data->ele_select.FE_Data;
    case FORM_TYPE_HIDDEN: case FORM_TYPE_SUBMIT: case FORM_TYPE_RESET:
    case FORM_TYPE_BUTTON: case FORM_TYPE_READONLY: return &data->ele_minimal.FE_Data;
    default: return nullptr;
    }
}

static NativeForm *form_for(LO_FormElementData *data)
{
    void **slot = data_slot(data);
    return slot ? (NativeForm *)*slot : nullptr;
}

static std::string block_text(PA_Block block)
{
    if (!block) return "";
    char *text;
    PA_LOCK(text, char *, block);
    std::string result = text ? text : "";
    PA_UNLOCK(block);
    return result;
}

static PA_Block data_name(LO_FormElementData *data)
{
    switch (data->type) {
    case FORM_TYPE_TEXT: case FORM_TYPE_PASSWORD: case FORM_TYPE_FILE: return data->ele_text.name;
    case FORM_TYPE_TEXTAREA: return data->ele_textarea.name;
    case FORM_TYPE_RADIO: case FORM_TYPE_CHECKBOX: return data->ele_toggle.name;
    case FORM_TYPE_SELECT_ONE: case FORM_TYPE_SELECT_MULT: return data->ele_select.name;
    default: return data->ele_minimal.name;
    }
}

static bool replace_text(PA_Block *current, PA_Block original, const std::string &text)
{
    PA_Block replacement = (PA_Block)PA_ALLOC(text.size() + 1);
    if (!replacement) return false;
    char *destination;
    PA_LOCK(destination, char *, replacement);
    memcpy(destination, text.c_str(), text.size() + 1);
    PA_UNLOCK(replacement);
    if (*current && *current != original) PA_FREE(*current);
    *current = replacement;
    return true;
}

static std::string window_text(HWND window)
{
    int length = (std::min)(GetWindowTextLengthA(window), maximum_text);
    std::vector<char> buffer(length + 1);
    GetWindowTextA(window, buffer.data(), length + 1);
    return buffer.data();
}

static std::string edit_text(const std::string &text)
{
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (!i || text[i-1] != '\r')) result += '\r';
        result += text[i];
    }
    return result;
}

static void destroy_controls(NativeForm *form)
{
    HWND window = form->window, browse = form->browse;
    form->window = form->browse = nullptr;
    form->displayed = false;
    if (IsWindow(browse)) DestroyWindow(browse);
    if (IsWindow(window)) DestroyWindow(window);
}

static void position_control(NativeForm *form)
{
    if (!form->window || !form->element) return;
    int32 scroll_x = 0, scroll_y = 0;
    WFE_Position(form->context, &scroll_x, &scroll_y);
    LO_FormElementStruct *element = form->element;
    LONGLONG left = (LONGLONG)element->x + element->x_offset - scroll_x;
    LONGLONG top = (LONGLONG)element->y + element->y_offset - scroll_y;
    RECT area;
    GetClientRect(GetParent(form->window), &area);
    bool visible = form->displayed && left < area.right && top < area.bottom &&
        left + form->width > 0 && top + form->height > 0;
    if (visible) {
        int edit_width = form->width - (form->browse ? 84 : 0);
        int window_height = form->data->type == FORM_TYPE_SELECT_ONE ? 240 : form->height;
        SetWindowPos(form->window, nullptr, (int)left, (int)top, edit_width, window_height,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        if (form->browse) SetWindowPos(form->browse, nullptr, (int)left + edit_width + 4,
            (int)top, 80, form->height, SWP_NOACTIVATE | SWP_NOZORDER);
    }
    ShowWindow(form->window, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (form->browse) ShowWindow(form->browse, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

static void load_control(NativeForm *form, bool reset)
{
    LO_FormElementData *data = form->data;
    switch (data->type) {
    case FORM_TYPE_TEXT: case FORM_TYPE_PASSWORD: case FORM_TYPE_FILE: {
        lo_FormElementTextData *text = &data->ele_text;
        std::string value = block_text(!reset && text->current_text ? text->current_text : text->default_text);
        /* A page's VALUE attribute must never select a local upload file. */
        if (data->type == FORM_TYPE_FILE && (reset || !text->current_text)) value.clear();
        if ((int)value.size() > maximum_text) value.resize(maximum_text);
        SetWindowTextA(form->window, value.c_str());
        break;
    }
    case FORM_TYPE_TEXTAREA: {
        lo_FormElementTextareaData *text = &data->ele_textarea;
        std::string value = block_text(!reset && text->current_text ? text->current_text : text->default_text);
        if ((int)value.size() > maximum_text) value.resize(maximum_text);
        SetWindowTextA(form->window, edit_text(value).c_str());
        break;
    }
    case FORM_TYPE_CHECKBOX: case FORM_TYPE_RADIO:
        if (reset) data->ele_toggle.toggled = data->ele_toggle.default_toggle;
        SendMessage(form->window, BM_SETCHECK, data->ele_toggle.toggled ? BST_CHECKED : BST_UNCHECKED, 0);
        break;
    case FORM_TYPE_SELECT_ONE: case FORM_TYPE_SELECT_MULT: {
        lo_FormElementSelectData *select = &data->ele_select;
        bool combo = data->type == FORM_TYPE_SELECT_ONE;
        SendMessage(form->window, combo ? CB_RESETCONTENT : LB_RESETCONTENT, 0, 0);
        if (!select->options) break;
        lo_FormElementOptionData *options;
        PA_LOCK(options, lo_FormElementOptionData *, select->options);
        int count = (std::min)((std::max)((int)select->option_cnt, 0), 32767);
        int selected = -1;
        for (int i = 0; i < count; ++i) {
            if (reset) options[i].selected = options[i].def_selected;
            std::string text = block_text(options[i].text_value);
            SendMessageA(form->window, combo ? CB_ADDSTRING : LB_ADDSTRING, 0, (LPARAM)text.c_str());
            if (options[i].selected) {
                selected = i;
                if (!combo && select->multiple) SendMessage(form->window, LB_SETSEL, TRUE, i);
            }
        }
        if (combo) SendMessage(form->window, CB_SETCURSEL, selected, 0);
        else if (!select->multiple) SendMessage(form->window, LB_SETCURSEL, selected, 0);
        PA_UNLOCK(select->options);
        break;
    }
    default: break;
    }
}

extern "C" void WFE_FormGetInfo(MWContext *context, LO_FormElementStruct *element)
{
    if (!element || !element->element_data) return;
    LO_FormElementData *data = element->element_data;
    void **slot = data_slot(data);
    if (!slot || data->type == FORM_TYPE_HIDDEN) {
        element->width = element->height = element->baseline = 0;
        return;
    }
    NativeForm *form = form_for(data);
    if (!form) {
        int count = 0;
        for (auto item : native_forms) if (item->context == context && item->element) ++count;
        if (count >= 2048) { WFE_Status(context, "Too many form controls."); return; }
        form = new NativeForm{};
        form->context = context;
        form->data = data;
        *slot = form;
        native_forms.push_back(form);
    }
    form->element = element;
    if (!IsWindow(form->window)) {
        HWND parent = WFE_DocumentWindow(context);
        if (!parent) return;
        const char *class_name = "EDIT";
        DWORD style = WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL;
        DWORD extended = WS_EX_CLIENTEDGE;
        std::string caption;
        int width = 180, height = 24;
        switch (data->type) {
        case FORM_TYPE_TEXT: case FORM_TYPE_PASSWORD: case FORM_TYPE_FILE:
            width = bounded(data->ele_text.size, 20, 256) * 8 + 12;
            if (data->type == FORM_TYPE_PASSWORD) style |= ES_PASSWORD;
            if (data->type == FORM_TYPE_FILE) { style |= ES_READONLY; width += 84; }
            break;
        case FORM_TYPE_TEXTAREA:
            width = bounded(data->ele_textarea.cols, 40, 256) * 8 + 24;
            height = bounded(data->ele_textarea.rows, 4, 128) * 16 + 8;
            style = WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
            if (data->ele_textarea.auto_wrap == TEXTAREA_WRAP_OFF) style |= WS_HSCROLL | ES_AUTOHSCROLL;
            break;
        case FORM_TYPE_RADIO: case FORM_TYPE_CHECKBOX:
            class_name = "BUTTON"; extended = 0; width = height = 20;
            style = WS_CHILD | WS_TABSTOP | (data->type == FORM_TYPE_RADIO ? BS_RADIOBUTTON : BS_AUTOCHECKBOX);
            break;
        case FORM_TYPE_SELECT_ONE: case FORM_TYPE_SELECT_MULT: {
            bool combo = data->type == FORM_TYPE_SELECT_ONE;
            class_name = combo ? "COMBOBOX" : "LISTBOX";
            style = WS_CHILD | WS_TABSTOP | WS_VSCROLL;
            if (combo) style |= CBS_DROPDOWNLIST;
            else {
                style |= LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
                if (data->ele_select.multiple) style |= LBS_EXTENDEDSEL;
                height = bounded(data->ele_select.size, 4, 128) * 18 + 4;
            }
            break;
        }
        case FORM_TYPE_SUBMIT: case FORM_TYPE_RESET: case FORM_TYPE_BUTTON:
            class_name = "BUTTON"; extended = 0;
            style = WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON;
            caption = block_text(data->ele_minimal.value);
            if (caption.empty()) caption = data->type == FORM_TYPE_RESET ? "Reset" : "Submit";
            width = (std::min)((std::max)((int)caption.size() * 8 + 24, 64), 2048);
            break;
        case FORM_TYPE_READONLY:
            style |= ES_READONLY;
            caption = block_text(data->ele_minimal.value);
            break;
        default: return;
        }
        static unsigned next_id = 20000;
        if (next_id >= 60000) next_id = 20000;
        form->window = CreateWindowExA(extended, class_name, caption.c_str(), style,
            -20000, -20000, width, data->type == FORM_TYPE_SELECT_ONE ? 240 : height,
            parent, (HMENU)(UINT_PTR)next_id++, GetModuleHandle(nullptr), nullptr);
        if (!form->window) { WFE_Status(context, "Could not create form control."); return; }
        SendMessage(form->window, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
        if (data->type == FORM_TYPE_TEXTAREA)
            SendMessage(form->window, EM_LIMITTEXT, maximum_text, 0);
        else if (data->type == FORM_TYPE_TEXT || data->type == FORM_TYPE_PASSWORD || data->type == FORM_TYPE_FILE)
            SendMessage(form->window, EM_LIMITTEXT, bounded(data->ele_text.max_size, maximum_text, maximum_text), 0);
        if (data->type == FORM_TYPE_FILE) {
            form->browse = CreateWindowExA(0, "BUTTON", "Browse...", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                -20000, -20000, 80, height, parent, (HMENU)(UINT_PTR)next_id++, GetModuleHandle(nullptr), nullptr);
            if (form->browse) SendMessage(form->browse, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
        }
        form->width = width; form->height = height;
        load_control(form, false);
        form->restored = true;
    }
    element->width = form->width;
    element->height = form->height;
    element->baseline = form->height - 5;
}

extern "C" void WFE_FormGetValue(MWContext *context, LO_FormElementStruct *element, XP_Bool hide)
{
    if (!element || !element->element_data) return;
    LO_FormElementData *data = element->element_data;
    NativeForm *form = form_for(data);
    if (!form || !IsWindow(form->window)) return;
    switch (data->type) {
    case FORM_TYPE_TEXT: case FORM_TYPE_PASSWORD: case FORM_TYPE_FILE:
        if (!replace_text(&data->ele_text.current_text, data->ele_text.default_text, window_text(form->window)))
            WFE_Status(context, "Could not store form text.");
        break;
    case FORM_TYPE_TEXTAREA: {
        std::string value = window_text(form->window);
        if (data->ele_textarea.auto_wrap == TEXTAREA_WRAP_HARD) {
            unsigned char *wrapped = XP_WordWrap(context->win_csid, (unsigned char *)value.c_str(),
                bounded(data->ele_textarea.cols, 40, 256), FALSE);
            if (wrapped) { value = (char *)wrapped; XP_FREE(wrapped); }
        }
        if (!replace_text(&data->ele_textarea.current_text, data->ele_textarea.default_text, value))
            WFE_Status(context, "Could not store form text.");
        break;
    }
    case FORM_TYPE_CHECKBOX: case FORM_TYPE_RADIO:
        data->ele_toggle.toggled = SendMessage(form->window, BM_GETCHECK, 0, 0) == BST_CHECKED;
        break;
    case FORM_TYPE_SELECT_ONE: case FORM_TYPE_SELECT_MULT: {
        lo_FormElementSelectData *select = &data->ele_select;
        if (!select->options) break;
        lo_FormElementOptionData *options;
        PA_LOCK(options, lo_FormElementOptionData *, select->options);
        bool combo = data->type == FORM_TYPE_SELECT_ONE;
        int selected = (int)SendMessage(form->window, combo ? CB_GETCURSEL : LB_GETCURSEL, 0, 0);
        for (int i = 0; i < select->option_cnt; ++i)
            options[i].selected = !combo && select->multiple ?
                SendMessage(form->window, LB_GETSEL, i, 0) > 0 : i == selected;
        PA_UNLOCK(select->options);
        break;
    }
    default: break;
    }
    if (hide) {
        destroy_controls(form);
        form->element = nullptr;
    }
}

extern "C" void WFE_FormReset(MWContext *context, LO_FormElementStruct *element)
{
    if (!element || !element->element_data) return;
    NativeForm *form = form_for(element->element_data);
    if (!form || !form->window) return;
    load_control(form, true);
    WFE_FormGetValue(context, element, FALSE);
}

extern "C" void WFE_FormSetToggle(MWContext *, LO_FormElementStruct *element, XP_Bool state)
{
    if (!element || !element->element_data) return;
    LO_FormElementData *data = element->element_data;
    if (data->type != FORM_TYPE_RADIO && data->type != FORM_TYPE_CHECKBOX) return;
    data->ele_toggle.toggled = state;
    NativeForm *form = form_for(data);
    if (form && form->window) SendMessage(form->window, BM_SETCHECK, state ? BST_CHECKED : BST_UNCHECKED, 0);
}

extern "C" void WFE_FormFree(MWContext *, LO_FormElementData *data)
{
    NativeForm *form = form_for(data);
    if (!form) return;
    *data_slot(data) = nullptr;
    native_forms.erase(std::remove(native_forms.begin(), native_forms.end(), form), native_forms.end());
    destroy_controls(form);
    delete form;
}

extern "C" void WFE_FormTextIsSubmit(MWContext *, LO_FormElementStruct *element)
{
    NativeForm *form = element ? form_for(element->element_data) : nullptr;
    if (form) form->enter_submit = true;
}

extern "C" void WFE_FormDisplay(MWContext *context, int, LO_FormElementStruct *element)
{
    if (!element || !element->element_data) return;
    NativeForm *form = form_for(element->element_data);
    if (!form || !form->window) {
        WFE_FormGetInfo(context, element);
        form = form_for(element->element_data);
    }
    if (!form) return;
    form->element = element;
    form->displayed = true;
    position_control(form);
}

extern "C" void WFE_FormPrint(MWContext *context,LO_FormElementStruct *element)
{
    if(!element || !element->element_data) return;
    int x=element->x+element->x_offset,y=element->y+element->y_offset;
    WFE_RecordPrintSpan(context,y,element->height);
    HDC dc=WFE_DrawingContext(context);if(!dc) return;
    NativeForm *form=form_for(element->element_data);if(!form || !IsWindow(form->window)) return;
    RECT area={x,y,x+form->width,y+form->height};
    int saved=SaveDC(dc);if(!saved) return;
    HGDIOBJ font=(HGDIOBJ)SendMessage(form->window,WM_GETFONT,0,0);if(font) SelectObject(dc,font);
    SetTextColor(dc,RGB(0,0,0));SetBkColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);
    int type=form->data->type;
    char kind[32]={};GetClassNameA(form->window,kind,sizeof(kind));
    if(!_stricmp(kind,"ListBox")) {
        FillRect(dc,&area,(HBRUSH)GetStockObject(WHITE_BRUSH));FrameRect(dc,&area,(HBRUSH)GetStockObject(BLACK_BRUSH));
        RECT inside=area;InflateRect(&inside,-2,-2);IntersectClipRect(dc,inside.left,inside.top,inside.right,inside.bottom);
        int count=(int)SendMessage(form->window,LB_GETCOUNT,0,0);
        int first=(std::max)(0,(int)SendMessage(form->window,LB_GETTOPINDEX,0,0));
        int line=(std::max)(1,(int)SendMessage(form->window,LB_GETITEMHEIGHT,first,0));
        bool multiple=(GetWindowLong(form->window,GWL_STYLE)&(LBS_MULTIPLESEL|LBS_EXTENDEDSEL))!=0;
        int current=(int)SendMessage(form->window,LB_GETCURSEL,0,0);
        for(int row=first,yrow=inside.top;row<count && yrow<inside.bottom;++row,yrow+=line) {
            int length=(int)SendMessage(form->window,LB_GETTEXTLEN,row,0);
            if(length<0 || length>maximum_text) continue;
            std::vector<char> text(length+1);SendMessage(form->window,LB_GETTEXT,row,(LPARAM)text.data());
            RECT item={inside.left,yrow,inside.right,yrow+line};
            bool selected=multiple?SendMessage(form->window,LB_GETSEL,row,0)>0:row==current;
            if(selected) {HBRUSH brush=CreateSolidBrush(RGB(210,210,210));FillRect(dc,&item,brush);DeleteObject(brush);}
            item.left+=2;DrawTextA(dc,text.data(),length,&item,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        }
    } else if(type==FORM_TYPE_CHECKBOX || type==FORM_TYPE_RADIO) {
        UINT state=type==FORM_TYPE_RADIO?DFCS_BUTTONRADIO:DFCS_BUTTONCHECK;
        if(SendMessage(form->window,BM_GETCHECK,0,0)==BST_CHECKED) state|=DFCS_CHECKED;
        DrawFrameControl(dc,&area,DFC_BUTTON,state);
    } else {
        int length=(std::min)(GetWindowTextLengthA(form->window),maximum_text);
        std::vector<char> text(length+1);GetWindowTextA(form->window,text.data(),(int)text.size());
        if(type==FORM_TYPE_PASSWORD) std::fill(text.begin(),text.end()-1,'*');
        FillRect(dc,&area,(HBRUSH)GetStockObject(WHITE_BRUSH));FrameRect(dc,&area,(HBRUSH)GetStockObject(BLACK_BRUSH));
        InflateRect(&area,-3,-2);
        DrawTextA(dc,text.data(),-1,&area,DT_NOPREFIX|(type==FORM_TYPE_TEXTAREA?DT_WORDBREAK:DT_SINGLELINE|DT_VCENTER));
    }
    RestoreDC(dc,saved);
}

extern "C" void WFE_FormPositionAll(MWContext *context)
{
    for (auto form : native_forms) if (form->context == context) position_control(form);
}

extern "C" void WFE_FormForgetContext(MWContext *context)
{
    for (size_t i = native_forms.size(); i > 0; --i) {
        NativeForm *form = native_forms[i-1];
        if (form->context == context) WFE_FormFree(context, form->data);
    }
}

static NativeForm *control_for(MWContext *context, HWND window)
{
    if (!window) return nullptr;
    for (auto form : native_forms) if (form->context == context &&
        (form->window == window || form->browse == window)) return form;
    return nullptr;
}

static void submit_data(MWContext *context, LO_FormSubmitData *data)
{
    if (!data) { WFE_Status(context, "Could not collect form values."); return; }
    std::string action = block_text(data->action);
    URL_Struct *url = NET_CreateURLStruct(action.c_str(), NET_DONT_RELOAD);
    if (!url) { LO_FreeSubmitData(data); WFE_Status(context, "Could not allocate form request."); return; }
    std::string target = block_text(data->window_target);
    if (!target.empty()) url->window_target = XP_STRDUP(target.c_str());
    History_entry *history = SHIST_GetCurrent(&context->hist);
    if (history && history->address) url->referer = XP_STRDUP(history->address);
    int result = NET_AddLOSubmitDataToURLStruct(data, url);
    LO_FreeSubmitData(data);
    if (result <= 0) {
        NET_FreeURLStruct(url);
        WFE_Status(context, "Could not encode form request.");
        return;
    }
    FE_GetURL(context, url);
}

static void submit_form(MWContext *context, LO_FormElementStruct *element)
{
    submit_data(context, LO_SubmitForm(context, element));
}

extern "C" void WFE_FormImageSubmit(MWContext *context, LO_ImageStruct *image, int32 x, int32 y)
{
    if (!image || !image->image_attr || !(image->image_attr->attrmask & LO_ATTR_ISFORM)) return;
    submit_data(context, LO_SubmitImageForm(context, image, x, y));
}

extern "C" int WFE_FormCommand(MWContext *context, WPARAM command, LPARAM parameter)
{
    NativeForm *form = control_for(context, (HWND)parameter);
    if (!form || !form->element) return 0;
    LO_FormElementStruct *element = form->element;
    if ((HWND)parameter == form->browse && HIWORD(command) == BN_CLICKED) {
        char path[MAX_PATH] = "";
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetAncestor(form->window, GA_ROOT);
        dialog.lpstrFilter = "All files\0*.*\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = sizeof(path);
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (GetOpenFileNameA(&dialog)) {
            SetWindowTextA(form->window, path);
            WFE_FormGetValue(context, element, FALSE);
        }
        return 1;
    }
    if (HIWORD(command) == BN_CLICKED) switch (form->data->type) {
    case FORM_TYPE_SUBMIT: submit_form(context, element); return 1;
    case FORM_TYPE_RESET: LO_ResetForm(context, element); return 1;
    case FORM_TYPE_RADIO:
        SendMessage(form->window, BM_SETCHECK, BST_CHECKED, 0);
        LO_FormRadioSet(context, element);
        WFE_FormGetValue(context, element, FALSE);
        return 1;
    case FORM_TYPE_CHECKBOX: WFE_FormGetValue(context, element, FALSE); return 1;
    default: break;
    }
    return 1;
}

extern "C" int WFE_FormKey(MWContext *context, HWND target, UINT message, WPARAM key, LPARAM)
{
    if (message != WM_KEYDOWN) return 0;
    NativeForm *form = control_for(context, target);
    if (!form || !form->element) return 0;
    if (key == VK_TAB && !(GetKeyState(VK_CONTROL) & 0x8000)) {
        HWND next = GetNextDlgTabItem(WFE_DocumentWindow(context), target, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        if (next) { SetFocus(next); return 1; }
    }
    if (key != VK_RETURN || (form->data->type != FORM_TYPE_TEXT &&
        form->data->type != FORM_TYPE_PASSWORD && form->data->type != FORM_TYPE_FILE)) return 0;
    NativeForm *submit = nullptr;
    for (auto candidate : native_forms) if (candidate->context == context && candidate->element &&
        candidate->element->form_id == form->element->form_id && candidate->data->type == FORM_TYPE_SUBMIT &&
        (!submit || candidate->element->ele_id < submit->element->ele_id)) submit = candidate;
    if (submit) submit_form(context, submit->element);
    else if (form->enter_submit) submit_form(context, form->element);
    else return 0;
    return 1;
}

extern "C" HWND WFE_FormFindControl(MWContext *context, const char *name, int index)
{
    if (!name || index < 0) return nullptr;
    for (auto form : native_forms) if (form->context == context && form->window &&
        form->element && block_text(data_name(form->data)) == name && index-- == 0) return form->window;
    return nullptr;
}

extern "C" int WFE_FormCount(MWContext *context)
{
    int count = 0;
    for (auto form : native_forms) if (form->context == context && IsWindow(form->window)) ++count;
    return count;
}
