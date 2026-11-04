#ifndef NETSCAPE_NATIVE_PREFERENCES_H
#define NETSCAPE_NATIVE_PREFERENCES_H
#ifdef __cplusplus
#include <string>
#include <vector>
class CWnd;
struct GeneralPreferences {
    int toolbar = 2; /* pictures, text, pictures and text */
    int startup = 1; /* browser=1, mail=2, news=4 */
    bool blank = false, underline = true, expire = true;
    int days = 9;
    std::string home = "about:";
    std::string proportional = "Times New Roman", fixed = "Courier New";
    int proportional_size = 12, fixed_size = 12;
    bool text_custom = false, link_custom = false, visited_custom = false;
    bool background_custom = false, override_colors = false;
    bool background_image = false;
    std::string background_file;
    std::string telnet_application,tn3270_application,source_application,temporary_directory;
    unsigned long colors[4] = {0xffffff, 0x000000, 0xff0000, 0x800080}; /* COLORREF: background,text,link,visited */
    int dither = 0; /* automatic, dither, closest */
    bool incremental = true;
    std::vector<std::string> languages;
};
const GeneralPreferences &WFE_GeneralPreferences();
bool WFE_ValidateGeneralPreferences(const GeneralPreferences &, std::string *error);
bool WFE_SaveGeneralPreferences(const GeneralPreferences &, std::string *error);
int WFE_PreferencesStoreSmoke(const char *report);
int WFE_ShowGeneralPreferences(CWnd *parent);
int WFE_PreferencesUISmoke(CWnd *parent, const char *report, const char *snapshot);
void WFE_GeneralPreferencesChanged();
extern "C" {
#endif
int WFE_PreferencesInit(void);
int WFE_PreferenceIncrementalImages(void);
int WFE_PreferenceImageDither(void);
const char *WFE_PreferenceLanguages(void);
#ifdef __cplusplus
}
#endif
#endif
