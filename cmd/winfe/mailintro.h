#ifndef NETSCAPE_WINDOWS_MAILINTRO_H
#define NETSCAPE_WINDOWS_MAILINTRO_H
#include "xp_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Publish a complete first-run Inbox without replacing an existing mailbox. */
int WFE_CreateMailIntro(const char *name,const char *envelope,const char *message,int32 length);
#ifdef __cplusplus
}
#endif
#endif
