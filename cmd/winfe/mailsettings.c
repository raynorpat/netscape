/* Profile-local settings for the original 3.02 mail and news back ends. */
#include "frontend.h"
#include "mailsettings.h"
#include "msgcom.h"
#include "xp_file.h"

extern int WFE_PlatformInit(void);

typedef struct WFE_MailSetting {
    const char *key;
    const char *environment;
    unsigned int capacity;
    char value[2048];
} WFE_MailSetting;

static WFE_MailSetting wfe_settings[] = {
    { "email", "EMAIL", 2048, "" },
    { "name", "NAME", 2048, "" },
    { "organization", "ORGANIZATION", 2048, "" },
    { "smtp", NULL, 1024, "" },
    { "pop", NULL, 1024, "" },
    { "news", NULL, 1024, "" }
};
static char wfe_settings_file[MAX_PATH];
static char wfe_mail_directory[MAX_PATH];
static char wfe_mail_url_directory[MAX_PATH + 1];
static int wfe_settings_loaded;
static int wfe_mail_ready;
static int wfe_news_map_loaded;

static int wfe_valid_setting(const char *value, unsigned int capacity)
{
    return value && strlen(value) < capacity &&
        !strchr(value, '\r') && !strchr(value, '\n');
}

static WFE_MailSetting *wfe_find_setting(const char *key)
{
    unsigned int i;
    if (key) for (i = 0; i < sizeof(wfe_settings) / sizeof(wfe_settings[0]); ++i)
        if (!strcmp(key, wfe_settings[i].key)) return &wfe_settings[i];
    return NULL;
}

static int wfe_load_settings(void)
{
    const char *profile;
    unsigned int i;
    if (wfe_settings_loaded) return 1;
    if (!WFE_PlatformInit()) return 0;
    profile = WFE_ProfileDirectory();
    if (strlen(profile) + sizeof("\\mailnews.ini") > sizeof(wfe_settings_file))
        return 0;
    sprintf(wfe_settings_file, "%s\\mailnews.ini", profile);
    for (i = 0; i < sizeof(wfe_settings) / sizeof(wfe_settings[0]); ++i) {
        WFE_MailSetting *setting = &wfe_settings[i];
        char fallback[2048];
        char stored[2049];
        DWORD count = 0;
        fallback[0] = 0;
        if (setting->environment)
            count = GetEnvironmentVariableA(setting->environment, fallback, sizeof(fallback));
        if (!count && !strcmp(setting->key, "name"))
            count = GetEnvironmentVariableA("USERNAME", fallback, sizeof(fallback));
        if (count >= sizeof(fallback) || !wfe_valid_setting(fallback, setting->capacity))
            fallback[0] = 0;
        count = GetPrivateProfileStringA("MailNews", setting->key, fallback,
            stored, setting->capacity + 1, wfe_settings_file);
        if (count >= setting->capacity || !wfe_valid_setting(stored, setting->capacity))
            setting->value[0] = 0;
        else strcpy(setting->value, stored);
    }
    wfe_settings_loaded = 1;
    return 1;
}

const char *WFE_MailPreference(const char *key)
{
    WFE_MailSetting *setting = wfe_find_setting(key);
    if (!setting || !wfe_load_settings()) return "";
    return setting->value;
}

int WFE_SetMailPreference(const char *key, const char *value)
{
    WFE_MailSetting *setting = wfe_find_setting(key);
    if (!setting || !wfe_valid_setting(value, setting->capacity) || !wfe_load_settings())
        return 0;
    if (!WritePrivateProfileStringA("MailNews", setting->key, value, wfe_settings_file))
        return 0;
    memmove(setting->value, value, strlen(value) + 1);
    return 1;
}

const char *WFE_MailDirectory(void)
{
    /* QUEUE_FOLDER_NAME in libmsg/msg.h is Outbox. */
    static const char *names[] = { "Inbox", "Drafts", "Sent", "Outbox", "Trash" };
    char path[MAX_PATH];
    const char *profile;
    DWORD attributes;
    unsigned int i;
    if (wfe_mail_ready) return wfe_mail_directory;
    if (!WFE_PlatformInit()) return NULL;
    profile = WFE_ProfileDirectory();
    if (strlen(profile) + sizeof("\\Mail\\Outbox") > sizeof(path)) return NULL;
    sprintf(wfe_mail_directory, "%s\\Mail", profile);
    if (!CreateDirectoryA(wfe_mail_directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return NULL;
    attributes = GetFileAttributesA(wfe_mail_directory);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return NULL;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        HANDLE file;
        sprintf(path, "%s\\%s", wfe_mail_directory, names[i]);
        file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) return NULL;
            attributes = GetFileAttributesA(path);
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
                return NULL;
        } else if (!CloseHandle(file)) return NULL;
    }
    wfe_mail_ready = 1;
    return wfe_mail_directory;
}

const char *WFE_MailURLDirectory(void)
{
    const char *directory = WFE_MailDirectory();
    char *p;
    if (!directory) return NULL;
    sprintf(wfe_mail_url_directory, "/%s", directory);
    for (p = wfe_mail_url_directory; *p; ++p) {
        if (*p == '\\') *p = '/';
        else if (*p == ':') *p = '|';
    }
    return wfe_mail_url_directory;
}

void WFE_ApplyMailPreferences(void)
{
    if (!wfe_load_settings()) return;
    if (!wfe_news_map_loaded) {
        NET_ReadNewsrcFileMappings();
        wfe_news_map_loaded = 1;
    }
    MSG_SetPopHost(WFE_MailPreference("pop"));
    NET_SetMailRelayHost((char *)WFE_MailPreference("smtp"));
    FE_UserNewsHost = (char *)WFE_MailPreference("news");
    NET_SetNewsHost(FE_UserNewsHost);
}
