#ifndef NETSCAPE_NATIVE_FORMS_H
#define NETSCAPE_NATIVE_FORMS_H
#include <windows.h>
#include "frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
void WFE_FormGetInfo(MWContext *, LO_FormElementStruct *);
void WFE_FormGetValue(MWContext *, LO_FormElementStruct *, XP_Bool hide);
void WFE_FormReset(MWContext *, LO_FormElementStruct *);
void WFE_FormSetToggle(MWContext *, LO_FormElementStruct *, XP_Bool);
void WFE_FormFree(MWContext *, LO_FormElementData *);
void WFE_FormTextIsSubmit(MWContext *, LO_FormElementStruct *);
void WFE_FormDisplay(MWContext *, int location, LO_FormElementStruct *);
void WFE_FormImageSubmit(MWContext *, LO_ImageStruct *, int32 x, int32 y);
void WFE_FormPositionAll(MWContext *);
void WFE_FormForgetContext(MWContext *);
int WFE_FormCommand(MWContext *, WPARAM, LPARAM);
int WFE_FormKey(MWContext *, HWND target, UINT message, WPARAM, LPARAM);
HWND WFE_FormFindControl(MWContext *, const char *name, int index);
/* Counts form elements with live controls; hidden inputs have no HWND. */
int WFE_FormCount(MWContext *);
#ifdef __cplusplus
}
#endif
#endif
