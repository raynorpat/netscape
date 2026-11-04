#ifndef NETSCAPE_NATIVE_PRINTING_H
#define NETSCAPE_NATIVE_PRINTING_H
#include <afxwin.h>
#include "frontend.h"

/* The render adapter uses layout coordinates, with the requested slice's top
 * at the HDC's logical origin. It must preserve the live document and view. */
bool WFE_PrintDocumentSize(MWContext *, int *width, int *height);
/* Shorten a page before a crossing text line, image, rule or form control.
 * Return 1..maximumHeight; oversized objects must still make progress. */
int WFE_PrintPageHeight(MWContext *, int top, int maximumHeight);
bool WFE_PrintDocumentPage(MWContext *, HDC, int top, int width, int height);
bool WFE_PrintCommand(MWContext *, UINT command);
bool WFE_PrintUpdate(MWContext *, CCmdUI *);
/* No physical output: records real layout pages as EMFs and cancels Page Setup. */
int WFE_PrintSmoke(MWContext *, const char *report);
#endif
