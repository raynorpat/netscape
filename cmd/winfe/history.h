#ifndef NETSCAPE_NATIVE_HISTORY_H
#define NETSCAPE_NATIVE_HISTORY_H
#include "frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
void WFE_HistorySavePosition(MWContext *);
int WFE_HistoryMove(MWContext *, int direction);
int WFE_HistoryCanMove(MWContext *, int direction);
void WFE_HistoryShow(MWContext *);
int WFE_HistoryDialogSmoke(MWContext *, int32 unique_id);
#ifdef __cplusplus
}
#endif
#endif
