#ifndef NETSCAPE_NATIVE_ADDRESSBOOK_H
#define NETSCAPE_NATIVE_ADDRESSBOOK_H
#include "frontend.h"
#ifdef __cplusplus
#include <afxwin.h>
bool WFE_PickRecipients(CWnd *owner,CString &to,CString &cc,CString &bcc);
void WFE_CancelRecipientPicker(CWnd *owner);
extern "C" {
#endif
MWContext *WFE_AddressBookContext(int view);
void WFE_AddressBookShow(void);
HWND WFE_AddressBookWindow(void);
int WFE_AddressBookRetryLoad(int discard_changes);
void WFE_AddressBookChanged(MWContext *);
void WFE_AddressBookEdit(MWContext *,BM_Entry *);
void WFE_AddressBookShutdown(void);
int WFE_AddressBookSmoke(const char *report,const char *snapshot,int reopen);
#ifdef __cplusplus
}
#endif
#endif
