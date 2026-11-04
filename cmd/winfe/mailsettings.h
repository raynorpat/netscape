#ifndef NETSCAPE_NATIVE_MAILSETTINGS_H
#define NETSCAPE_NATIVE_MAILSETTINGS_H
#ifdef __cplusplus
extern "C" {
#endif

/* Values are borrowed from fixed storage and remain valid until shutdown.
 * A successful setter changes the corresponding value in that storage. */
const char *WFE_MailPreference(const char *key);
int WFE_SetMailPreference(const char *key, const char *value);
/* Returns NULL if the profile's mailbox directory could not be prepared. */
const char *WFE_MailDirectory(void);
/* Unescaped /D|/path form used by libmsg; escape only when making a URL. */
const char *WFE_MailURLDirectory(void);
void WFE_ApplyMailPreferences(void);

#ifdef __cplusplus
}
#endif
#endif
