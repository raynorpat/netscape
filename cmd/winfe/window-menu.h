#ifndef NETSCAPE_NATIVE_WINDOW_MENU_H
#define NETSCAPE_NATIVE_WINDOW_MENU_H
#include <afxwin.h>
bool WFE_IsWindowMenu(CMenu *menu);
void WFE_PopulateWindowMenu(CMenu *menu,CWnd *owner);
bool WFE_WindowCommand(CWnd *owner,UINT command);
bool WFE_WindowUpdateCommand(CWnd *owner,CCmdUI *command);
#endif
