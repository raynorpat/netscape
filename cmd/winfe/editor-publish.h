#ifndef NETSCAPE_NATIVE_GOLD_EDITOR_PUBLISH_H
#define NETSCAPE_NATIVE_GOLD_EDITOR_PUBLISH_H
#include "editor.h"
#ifdef EDITOR
bool WFE_EditorPublishCommand(MWContext *context, UINT command);
bool WFE_EditorPublishUpdate(MWContext *context, CCmdUI *command);
void WFE_EditorPublishForget(MWContext *context);
int WFE_EditorPublishSmoke(MWContext *context, const char *report, const char *destination);
#endif
#endif
