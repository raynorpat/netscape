#ifndef NETSCAPE_NATIVE_COMPOSER_H
#define NETSCAPE_NATIVE_COMPOSER_H
#include "frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
void WFE_NewComposer(MWContext *source, int is_news);
void WFE_ComposerStatus(MWContext *context, const char *message);
void WFE_ComposerComplete(MWContext *context);
void WFE_ComposerCloseAll(void);
int WFE_ComposerWindowCount(void);
int WFE_ComposerClose(MWContext *context);
/* Appends observations to report; returns zero on success. Never sends. */
int WFE_ComposerSmoke(MWContext *source, const char *report, int is_news);
/* Checks and captures the populated composition window. Never sends. */
int WFE_ComposerUISmoke(MWContext *source, const char *report, const char *snapshot);
/* Quotes and interrupts only a loopback HTTP fixture. Never sends. */
int WFE_ComposerQuoteSmoke(const char *report, const char *url);
/* Sends a test message only if the configured endpoint is loopback:port. */
int WFE_ComposerDeliverSmoke(MWContext *source, const char *report, int is_news);
#ifdef __cplusplus
}
#endif
#endif
