#ifndef NETSCAPE_NATIVE_GOLD_EDITOR_H
#define NETSCAPE_NATIVE_GOLD_EDITOR_H
#include "frontend.h"
#ifdef EDITOR
#ifdef __cplusplus
extern "C" {
#endif
MWContext *WFE_NewEditorWindow(const char *address);
MWContext *WFE_NewEditorDocument(const char *source);
void WFE_EditorCopyLoaded(MWContext *context);
const char *WFE_CurrentDocumentURL(MWContext *context);
/* Save returns 1 when started, 0 for Cancel, -1 on failure. */
int WFE_EditorSave(MWContext *context,const char *path,int save_as);
int WFE_EditorSaving(MWContext *context);
int WFE_EditorSaveStatus(MWContext *context);
void WFE_EditorTestMode(int enabled);
const char *WFE_EditorSavedPath(MWContext *context);
int WFE_EditorCheckSave(MWContext *context);
void WFE_EditorForget(MWContext *context);
void WFE_EditorRefreshCaret(MWContext *context);
void WFE_EditorDocumentLoaded(MWContext *context);
void WFE_EditorFormattingChanged(MWContext *context);
#ifdef __cplusplus
}
#include <afxwin.h>
bool WFE_EditorCreateUI(MWContext *context,CWnd *owner);
int WFE_EditorArrange(MWContext *context,int width);
bool WFE_EditorCommand(MWContext *context,UINT command);
bool WFE_EditorUpdate(MWContext *context,CCmdUI *command);
bool WFE_EditorKey(MWContext *context,MSG *message);
void WFE_EditorDestroyUI(MWContext *context);
bool WFE_EditorUISmoke(MWContext *context,const char *report);
#endif
#endif
#endif
