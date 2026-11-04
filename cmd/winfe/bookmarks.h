#ifndef NETSCAPE_NATIVE_BOOKMARKS_H
#define NETSCAPE_NATIVE_BOOKMARKS_H
#include "frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
int WFE_BookmarksInit(void);
void WFE_BookmarksShutdown(void);
void WFE_BookmarksAddPage(MWContext *source);
void WFE_BookmarksShow(MWContext *source);
void WFE_BookmarksEdit(MWContext *source, BM_Entry *entry);
/* BMFE callbacks may occur inside a database mutation. Queue the redisplay. */
void WFE_BookmarksChanged(MWContext *context);
void WFE_BookmarkNavigate(MWContext *context, const char *address);
/* Appends observations; seed and verify must run in separate processes. */
int WFE_BookmarksSmoke(MWContext *source, const char *report, const char *phase);
#ifdef __cplusplus
}
#endif
#endif
