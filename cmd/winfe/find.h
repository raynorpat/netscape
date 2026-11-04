#ifndef NETSCAPE_NATIVE_FIND_H
#define NETSCAPE_NATIVE_FIND_H
#include "frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
void WFE_FindShow(MWContext *context);
int WFE_FindAgain(MWContext *context);
/* Search the supplied document, select and reveal a match. Returns 1 on success.
 * Repeating a query advances from the current selection; changing the query or
 * document starts over. A failed search leaves the previous selection intact.
 * Text uses the document's legacy byte encoding, as required by LO_FindText. */
int WFE_FindText(MWContext *context, const char *term,
                 int match_case, int backwards, int wrap);
/* Native fixture: enter a query, invoke Find Next, then close the dialog. */
int WFE_FindDialogSmoke(MWContext *context, const char *term);
#ifdef __cplusplus
}
#endif
#endif
