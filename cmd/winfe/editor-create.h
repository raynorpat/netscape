#ifndef NETSCAPE_GOLD_CREATE_H
#define NETSCAPE_GOLD_CREATE_H
#include "editor.h"
bool WFE_EditorWorkflowCommand(MWContext *context,UINT command);
bool WFE_EditorWorkflowUpdate(MWContext *context,CCmdUI *command);
bool WFE_EditorWorkflowSmoke(MWContext *context,const char *report);
#endif
