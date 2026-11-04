/* Transactional profile-local General Preferences for the native front end. */
#include "frontend.h"
#include "preferences.h"
#include "glhist.h"
#include "applications.h"
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" int WFE_PlatformInit(void);

static GeneralPreferences wfe_general;
static bool wfe_general_loaded;
static std::string wfe_general_file, wfe_languages;
/* mkhttp.c formats this value in a 512-byte buffer: 17 header bytes,
 * two CR/LF bytes and the terminating NUL leave 492 bytes for the value. */
static const size_t wfe_language_header_limit = 492;

static bool wfe_preference_error(std::string *error, const char *message)
{
    if (error) *error = message;
    return false;
}

static bool wfe_safe_string(const std::string &value, size_t limit, bool empty)
{
    if ((!empty && value.empty()) || value.size() >= limit) return false;
    for (size_t i = 0; i < value.size(); ++i)
        if ((unsigned char)value[i] < 32 || value[i] == 127) return false;
    return true;
}

static bool wfe_language_tag(const std::string &value)
{
    if (value.empty() || value.size() > 63) return false;
    size_t component = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c == '-') {
            if (!component) return false;
            component = 0;
        } else {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9')) || ++component > 8) return false;
        }
    }
    return component != 0;
}

static bool wfe_valid_languages(const std::vector<std::string> &languages)
{
    if (languages.size() > 64) return false;
    size_t length = 0;
    for (size_t i = 0; i < languages.size(); ++i) {
        if (!wfe_language_tag(languages[i])) return false;
        length += languages[i].size() + (i ? 1 : 0);
        if (length > wfe_language_header_limit) return false;
        for (size_t j = 0; j < i; ++j)
            if (!_stricmp(languages[i].c_str(), languages[j].c_str())) return false;
    }
    return true;
}

bool WFE_ValidateGeneralPreferences(const GeneralPreferences &p, std::string *error)
{
    if (error) error->clear();
    if (p.toolbar < 0 || p.toolbar > 2)
        return wfe_preference_error(error, "Select a toolbar style.");
    if (p.startup < 1 || p.startup > 7)
        return wfe_preference_error(error, "Select at least one startup window.");
    if (p.days < 1 || p.days > 24855)
        return wfe_preference_error(error, "History expiration must be between 1 and 24855 days.");
    if (!wfe_safe_string(p.home, 32768, true))
        return wfe_preference_error(error, "The home location is too long or contains a control character.");
    if (!wfe_safe_string(p.proportional, LF_FACESIZE, false) ||
        !wfe_safe_string(p.fixed, LF_FACESIZE, false))
        return wfe_preference_error(error, "Choose font names shorter than 32 characters, without control characters.");
    if (p.proportional_size < 6 || p.proportional_size > 72 ||
        p.fixed_size < 6 || p.fixed_size > 72)
        return wfe_preference_error(error, "Font sizes must be between 6 and 72 points.");
    for (int i = 0; i < 4; ++i)
        if (p.colors[i] > 0xffffffUL)
            return wfe_preference_error(error, "Choose a valid 24-bit color.");
    if (p.dither < 0 || p.dither > 2)
        return wfe_preference_error(error, "Select an image color mode.");
    if (!wfe_safe_string(p.background_file, MAX_PATH, true))
        return wfe_preference_error(error, "Choose a background image path shorter than 260 characters, without control characters.");
    if (p.background_image) {
        const std::string &path=p.background_file;
        bool absolute=(path.size()>2 && ((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z')) && path[1]==':' && (path[2]=='\\'||path[2]=='/')) || path.compare(0,2,"\\\\")==0;
        DWORD attributes=GetFileAttributesA(path.c_str());
        if(!absolute || attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_DIRECTORY))
            return wfe_preference_error(error, "Choose an existing background image file using its full path.");
    }
    if (!wfe_valid_languages(p.languages))
        return wfe_preference_error(error, "Use unique language tags with letters, digits and single hyphens.");
    if(!WFE_ValidateApplication(p.telnet_application,error) || !WFE_ValidateApplication(p.tn3270_application,error) ||
        !WFE_ValidateApplication(p.source_application,error) || !WFE_ValidateTemporaryDirectory(p.temporary_directory,error)) return false;
    return true;
}

static std::string wfe_join_languages(const std::vector<std::string> &languages)
{
    std::string value;
    for (size_t i = 0; i < languages.size(); ++i) {
        if (i) value += ',';
        value += languages[i];
    }
    return value;
}

static void wfe_apply_general(void)
{
    const GeneralPreferences defaults;
    bool custom[] = { wfe_general.background_custom, wfe_general.text_custom,
                      wfe_general.link_custom, wfe_general.visited_custom };
    for (int i = 0; i < 4; ++i) {
        COLORREF color = custom[i] ? wfe_general.colors[i] : defaults.colors[i];
        LO_SetDefaultColor(i, GetRValue(color), GetGValue(color), GetBValue(color));
    }
    LO_SetUserOverride(wfe_general.override_colors ? TRUE : FALSE);
    char background_url[3*MAX_PATH]={};DWORD url_size=sizeof(background_url);
    if(wfe_general.background_image && SUCCEEDED(UrlCreateFromPathA(wfe_general.background_file.c_str(),background_url,&url_size,0)))
        LO_SetDefaultBackdrop(background_url);
    else LO_SetDefaultBackdrop(NULL);
    WFE_ApplyTemporaryDirectory(wfe_general.temporary_directory.c_str());
    GH_SetGlobalHistoryTimeout(wfe_general.expire ? (int32)wfe_general.days * 86400 : -1);
    wfe_languages = wfe_join_languages(wfe_general.languages);
}

/* Missing keys, truncated values and malformed numbers all retain defaults. */
static bool wfe_read_string(const char *section, const char *key, std::string &value,
                            bool *present = NULL)
{
    char buffer[32770];
    static const char missing[] = "\001";
    DWORD length = GetPrivateProfileStringA(section, key, missing, buffer,
                                           sizeof(buffer), wfe_general_file.c_str());
    if (present) *present = strcmp(buffer, missing) != 0;
    if (length >= sizeof(buffer) - 1 || !strcmp(buffer, missing)) return false;
    value.assign(buffer, length);
    return true;
}

static bool wfe_decimal(const std::string &value, unsigned long maximum, unsigned long &number)
{
    if (value.empty()) return false;
    number = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] < '0' || value[i] > '9') return false;
        unsigned long digit = value[i] - '0';
        if (number > maximum / 10 || (number == maximum / 10 && digit > maximum % 10))
            return false;
        number = number * 10 + digit;
    }
    return true;
}

static int wfe_read_int(const char *key, int fallback, int minimum, int maximum)
{
    std::string value;
    unsigned long number;
    if (!wfe_read_string("General", key, value) ||
        !wfe_decimal(value, (unsigned long)maximum, number) || number < (unsigned long)minimum)
        return fallback;
    return (int)number;
}

extern "C" int WFE_PreferencesInit(void)
{
    if (wfe_general_loaded) return 1;
    if (!WFE_PlatformInit()) return 0;
    const char *profile = WFE_ProfileDirectory();
    if (!profile || !*profile || strlen(profile) + sizeof("\\browser.ini") > MAX_PATH)
        return 0;
    wfe_general_file = std::string(profile) + "\\browser.ini";
    GeneralPreferences p;
    p.toolbar = wfe_read_int("Toolbar", p.toolbar, 0, 2);
    p.startup = wfe_read_int("Startup", p.startup, 1, 7);
    p.blank = wfe_read_int("Blank", p.blank, 0, 1) != 0;
    p.underline = wfe_read_int("Underline", p.underline, 0, 1) != 0;
    p.expire = wfe_read_int("Expire", p.expire, 0, 1) != 0;
    p.days = wfe_read_int("Days", p.days, 1, 24855);
    p.proportional_size = wfe_read_int("ProportionalSize", p.proportional_size, 6, 72);
    p.fixed_size = wfe_read_int("FixedSize", p.fixed_size, 6, 72);
    p.text_custom = wfe_read_int("TextCustom", p.text_custom, 0, 1) != 0;
    p.link_custom = wfe_read_int("LinkCustom", p.link_custom, 0, 1) != 0;
    p.visited_custom = wfe_read_int("VisitedCustom", p.visited_custom, 0, 1) != 0;
    p.background_custom = wfe_read_int("BackgroundCustom", p.background_custom, 0, 1) != 0;
    p.override_colors = wfe_read_int("OverrideColors", p.override_colors, 0, 1) != 0;
    p.background_image = wfe_read_int("BackgroundImage", p.background_image, 0, 1) != 0;
    p.dither = wfe_read_int("Dither", p.dither, 0, 2);
    p.incremental = wfe_read_int("Incremental", p.incremental, 0, 1) != 0;
    std::string value;
    if(wfe_read_string("General","TelnetApplication",value) && WFE_ValidateApplication(value,NULL)) p.telnet_application=value;
    if(wfe_read_string("General","TN3270Application",value) && WFE_ValidateApplication(value,NULL)) p.tn3270_application=value;
    if(wfe_read_string("General","SourceApplication",value) && WFE_ValidateApplication(value,NULL)) p.source_application=value;
    if(wfe_read_string("General","TemporaryDirectory",value) && WFE_ValidateTemporaryDirectory(value,NULL)) p.temporary_directory=value;
    if(wfe_read_string("General","BackgroundFile",value) && wfe_safe_string(value,MAX_PATH,true)) p.background_file=value;
    if(p.background_image) {
        std::string problem;
        if(!WFE_ValidateGeneralPreferences(p,&problem)) p.background_image=false;
    }
    bool present = false;
    bool home = wfe_read_string("General", "Home", value, &present);
    if (!present) home = wfe_read_string("Browser", "Home", value);
    if (home && wfe_safe_string(value, 32768, true)) p.home = value;
    if (wfe_read_string("General", "Proportional", value) && wfe_safe_string(value, LF_FACESIZE, false))
        p.proportional = value;
    if (wfe_read_string("General", "Fixed", value) && wfe_safe_string(value, LF_FACESIZE, false))
        p.fixed = value;
    for (int i = 0; i < 4; ++i) {
        char key[16]; unsigned long color;
        sprintf(key, "Color%d", i);
        if (wfe_read_string("General", key, value) && wfe_decimal(value, 0xffffffUL, color))
            p.colors[i] = color;
    }
    if (wfe_read_string("General", "Languages", value) && !value.empty()) {
        std::vector<std::string> languages;
        size_t start = 0, comma;
        do {
            comma = value.find(',', start);
            languages.push_back(value.substr(start, comma == std::string::npos ? comma : comma - start));
            start = comma == std::string::npos ? value.size() : comma + 1;
        } while (comma != std::string::npos && languages.size() <= 64);
        if (wfe_valid_languages(languages)) p.languages = languages;
    }
    wfe_general = p;
    wfe_general_loaded = true;
    wfe_apply_general();
    return 1;
}

const GeneralPreferences &WFE_GeneralPreferences()
{
    WFE_PreferencesInit();
    return wfe_general;
}

extern "C" int WFE_PreferenceIncrementalImages(void)
{
    return WFE_GeneralPreferences().incremental ? 1 : 0;
}

extern "C" int WFE_PreferenceImageDither(void)
{
    return WFE_GeneralPreferences().dither;
}

extern "C" const char *WFE_PreferenceLanguages(void)
{
    WFE_PreferencesInit();
    return wfe_languages.c_str();
}

static bool wfe_write_int(const char *file, const char *key, unsigned long value)
{
    char number[32];
    sprintf(number, "%lu", value);
    return WritePrivateProfileStringA("General", key, number, file) != FALSE;
}

static bool wfe_write_string(const char *file, const char *section, const char *key,
                             const std::string &value)
{
    /* Quotes keep leading/trailing spaces and literal quote pairs on reload. */
    std::string quoted = "\"" + value + "\"";
    return WritePrivateProfileStringA(section, key, quoted.c_str(), file) != FALSE;
}

bool WFE_SaveGeneralPreferences(const GeneralPreferences &p, std::string *error)
{
    if (!WFE_ValidateGeneralPreferences(p, error)) return false;
    if (!WFE_PreferencesInit()) return wfe_preference_error(error, "The profile directory is unavailable.");
    char temporary[MAX_PATH];
    if (!GetTempFileNameA(WFE_ProfileDirectory(), "prf", 0, temporary))
        return wfe_preference_error(error, "Cannot create a temporary preferences file.");
    DWORD attributes = GetFileAttributesA(wfe_general_file.c_str());
    bool ok = true;
    if (attributes != INVALID_FILE_ATTRIBUTES)
        ok = !(attributes & FILE_ATTRIBUTE_DIRECTORY) && CopyFileA(wfe_general_file.c_str(), temporary, FALSE);
    else if (GetLastError() != ERROR_FILE_NOT_FOUND) ok = false;
    if (ok) ok = SetFileAttributesA(temporary, FILE_ATTRIBUTE_NORMAL) != FALSE;
    if (ok) {
#define WRITE_INT(key, field) if (ok) ok = wfe_write_int(temporary, key, p.field)
        WRITE_INT("Toolbar", toolbar); WRITE_INT("Startup", startup);
        WRITE_INT("Blank", blank); WRITE_INT("Underline", underline);
        WRITE_INT("Expire", expire); WRITE_INT("Days", days);
        WRITE_INT("ProportionalSize", proportional_size); WRITE_INT("FixedSize", fixed_size);
        WRITE_INT("TextCustom", text_custom); WRITE_INT("LinkCustom", link_custom);
        WRITE_INT("VisitedCustom", visited_custom); WRITE_INT("BackgroundCustom", background_custom);
        WRITE_INT("OverrideColors", override_colors); WRITE_INT("Dither", dither);
        WRITE_INT("BackgroundImage", background_image);
        WRITE_INT("Incremental", incremental);
#undef WRITE_INT
        for (int i = 0; ok && i < 4; ++i) {
            char key[16]; sprintf(key, "Color%d", i);
            ok = wfe_write_int(temporary, key, p.colors[i]);
        }
        if (ok) ok = wfe_write_string(temporary, "General", "Home", p.home) &&
            wfe_write_string(temporary, "Browser", "Home", p.home) &&
            wfe_write_string(temporary, "General", "Proportional", p.proportional) &&
            wfe_write_string(temporary, "General", "Fixed", p.fixed) &&
            wfe_write_string(temporary, "General", "Languages", wfe_join_languages(p.languages)) &&
            wfe_write_string(temporary, "General", "BackgroundFile", p.background_file) &&
            wfe_write_string(temporary, "General", "TelnetApplication", p.telnet_application) &&
            wfe_write_string(temporary, "General", "TN3270Application", p.tn3270_application) &&
            wfe_write_string(temporary, "General", "SourceApplication", p.source_application) &&
            wfe_write_string(temporary, "General", "TemporaryDirectory", p.temporary_directory);
        /* The cache-flush form can return zero even after a successful write.
         * Check the actual file flush below before replacing the destination. */
        if (ok) WritePrivateProfileStringA(NULL, NULL, NULL, temporary);
        if (ok) {
            HANDLE file = CreateFileA(temporary, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            ok = file != INVALID_HANDLE_VALUE;
            if (ok) {
                ok = FlushFileBuffers(file) != FALSE;
                if (!CloseHandle(file)) ok = false;
            }
        }
        if (ok) ok = MoveFileExA(temporary, wfe_general_file.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!ok) {
        WritePrivateProfileStringA(NULL, NULL, NULL, temporary);
        SetFileAttributesA(temporary, FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(temporary);
        return wfe_preference_error(error, "Cannot save preferences. The existing settings have been retained.");
    }
    WritePrivateProfileStringA(NULL, NULL, NULL, wfe_general_file.c_str());
    wfe_general = p;
    wfe_apply_general();
    WFE_GeneralPreferencesChanged();
    return true;
}

static bool wfe_smoke_read(const char *path, std::string &bytes)
{
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    bytes.clear();
    char buffer[4096]; size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) bytes.append(buffer, count);
    bool ok = ferror(file) == 0;
    if (fclose(file)) ok = false;
    return ok;
}

static bool wfe_smoke_write(const char *path, const std::string &bytes)
{
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool ok = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    if (fclose(file)) ok = false;
    WritePrivateProfileStringA(NULL, NULL, NULL, path);
    return ok;
}

static int wfe_smoke_temporaries(void)
{
    WIN32_FIND_DATAA entry;
    std::string pattern = std::string(WFE_ProfileDirectory()) + "\\prf*.tmp";
    HANDLE search = FindFirstFileA(pattern.c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return 0;
    int count = 0;
    do { ++count; } while (FindNextFileA(search, &entry));
    FindClose(search);
    return count;
}

int WFE_PreferencesStoreSmoke(const char *report)
{
    FILE *output = report ? fopen(report, "wb") : NULL;
    if (!output) return 1;
    char environment[MAX_PATH], full[MAX_PATH];
    DWORD length = GetEnvironmentVariableA("NETSCAPE_PROFILE_DIR", environment, sizeof(environment));
    bool isolated = length && length < sizeof(environment) &&
        GetFullPathNameA(environment, sizeof(full), full, NULL) &&
        full[0] && !(full[0] == '\\' && full[1] == '\\') &&
        WFE_PreferencesInit() && !_stricmp(full, WFE_ProfileDirectory());
    fprintf(output, "isolated_profile=%d\r\n", isolated ? 1 : 0);
    if (!isolated) { fclose(output); return 1; }
    const GeneralPreferences original = wfe_general;
    std::string original_bytes;
    bool existed = GetFileAttributesA(wfe_general_file.c_str()) != INVALID_FILE_ATTRIBUTES;
    bool captured = !existed || wfe_smoke_read(wfe_general_file.c_str(), original_bytes);
    if (!captured) { fprintf(output, "original_captured=0\r\n"); fclose(output); return 1; }
    int failures = 0;
#define STORE_CHECK(name, expression) do { bool passed = (expression); \
    fprintf(output, name "=%d\r\n", passed ? 1 : 0); if (!passed) ++failures; } while (0)
    std::string fixture = "[Browser]\r\nHome=about:migrated\r\n[Unrelated]\r\nKeep=untouched\r\n";
    bool prepared = wfe_smoke_write(wfe_general_file.c_str(), fixture);
    wfe_general_loaded = false;
    STORE_CHECK("migration", prepared && WFE_PreferencesInit() && wfe_general.home == "about:migrated");
    GeneralPreferences p = wfe_general;
    std::string error;
    p.toolbar = 3; bool ranges = !WFE_ValidateGeneralPreferences(p, &error); p.toolbar = 2;
    p.startup = 0; ranges = ranges && !WFE_ValidateGeneralPreferences(p, &error); p.startup = 1;
    p.days = 24856; ranges = ranges && !WFE_ValidateGeneralPreferences(p, &error); p.days = 24855;
    ranges = ranges && WFE_ValidateGeneralPreferences(p, &error);
    p.proportional_size = 73; ranges = ranges && !WFE_ValidateGeneralPreferences(p, &error); p.proportional_size = 12;
    p.fixed = std::string(LF_FACESIZE, 'x'); ranges = ranges && !WFE_ValidateGeneralPreferences(p, &error); p.fixed = "Courier New";
    p.colors[0] = 0x1000000; ranges = ranges && !WFE_ValidateGeneralPreferences(p, &error); p.colors[0] = 0xffffff;
    p.home = "about:\r\nInjected=yes"; ranges = ranges && !WFE_ValidateGeneralPreferences(p, &error); p.home = "about:saved";
    STORE_CHECK("range_rejection", ranges);
    p.languages = { "en", "EN" }; bool tokens = !WFE_ValidateGeneralPreferences(p, &error);
    p.languages = { "en\r\nX-Test: bad" }; tokens = tokens && !WFE_ValidateGeneralPreferences(p, &error);
    p.languages = { "en--US" }; tokens = tokens && !WFE_ValidateGeneralPreferences(p, &error);
    p.languages = { "fr", "en-US", "es-419" }; tokens = tokens && WFE_ValidateGeneralPreferences(p, &error);
    STORE_CHECK("language_rejection", tokens);
    std::vector<std::string> boundary_languages;
    for (int i = 0; i < 54; ++i) {
        char tag[16]; sprintf(tag, "x%07d", i);
        boundary_languages.push_back(tag);
    }
    boundary_languages.push_back("z00000");
    p.languages = boundary_languages;
    bool boundary = wfe_join_languages(p.languages).size() == 492 &&
        WFE_ValidateGeneralPreferences(p, &error) && WFE_SaveGeneralPreferences(p, &error);
    wfe_general_loaded = false;
    boundary = boundary && WFE_PreferencesInit() && wfe_general.languages == boundary_languages;
    boundary_languages.back() = "z000000";
    p.languages = boundary_languages;
    boundary = boundary && wfe_join_languages(p.languages).size() == 493 &&
        !WFE_ValidateGeneralPreferences(p, &error) && !WFE_SaveGeneralPreferences(p, &error) &&
        strlen(WFE_PreferenceLanguages()) == 492;
    STORE_CHECK("language_header_boundary", boundary);
    p.languages = { "fr", "en-US", "es-419" };
    p.toolbar = 1; p.startup = 5; p.blank = true; p.underline = false; p.expire = false;
    p.days = 17; p.proportional = "Arial"; p.proportional_size = 18; p.fixed_size = 14;
    p.text_custom = p.link_custom = p.visited_custom = p.background_custom = p.override_colors = true;
    p.colors[0] = 0x123456; p.colors[1] = 0x654321; p.colors[2] = 0x102030; p.colors[3] = 0x908070;
    p.incremental = false; p.dither = 2;
    bool saved = WFE_SaveGeneralPreferences(p, &error);
    wfe_general_loaded = false;
    bool loaded = WFE_PreferencesInit() != 0;
    const GeneralPreferences &stored = wfe_general;
    STORE_CHECK("persisted_values", saved && loaded && stored.home == "about:saved" && stored.toolbar == 1 &&
        stored.startup == 5 && stored.blank && !stored.underline && !stored.expire && stored.days == 17 &&
        stored.proportional == "Arial" && stored.fixed == "Courier New" && stored.proportional_size == 18 && stored.fixed_size == 14 &&
        stored.text_custom && stored.link_custom && stored.visited_custom && stored.background_custom && stored.override_colors &&
        stored.colors[0] == 0x123456 && stored.colors[1] == 0x654321 && stored.colors[2] == 0x102030 && stored.colors[3] == 0x908070 &&
        !WFE_PreferenceIncrementalImages() && WFE_PreferenceImageDither() == 2 &&
        !strcmp(WFE_PreferenceLanguages(), "fr,en-US,es-419"));
    char check[64];
    GetPrivateProfileStringA("Unrelated", "Keep", "", check, sizeof(check), wfe_general_file.c_str());
    STORE_CHECK("unknown_section_preserved", !strcmp(check, "untouched"));
    GetPrivateProfileStringA("Browser", "Home", "", check, sizeof(check), wfe_general_file.c_str());
    STORE_CHECK("legacy_home_preserved", !strcmp(check, "about:saved"));
    std::string before, after;
    bool before_read = wfe_smoke_read(wfe_general_file.c_str(), before);
    int temporary_count = wfe_smoke_temporaries();
    HANDLE locked = CreateFileA(wfe_general_file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    p.home = "about:must-not-apply";
    bool rejected = locked != INVALID_HANDLE_VALUE && !WFE_SaveGeneralPreferences(p, &error);
    if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
    STORE_CHECK("failed_save_memory_preserved", rejected && wfe_general.home == "about:saved" &&
        !strcmp(WFE_PreferenceLanguages(), "fr,en-US,es-419"));
    STORE_CHECK("failed_save_file_preserved", rejected && before_read &&
        wfe_smoke_read(wfe_general_file.c_str(), after) && before == after);
    STORE_CHECK("temporary_files_cleaned", wfe_smoke_temporaries() == temporary_count);
    fixture += "[General]\r\nToolbar=99\r\nStartup=0\r\nDays=9999999999999\r\nFixedSize=73\r\n"
               "Proportional=abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\r\nColor0=16777216\r\nLanguages=";
    fixture += wfe_join_languages(boundary_languages) + "\r\n";
    prepared = wfe_smoke_write(wfe_general_file.c_str(), fixture);
    wfe_general_loaded = false;
    STORE_CHECK("invalid_persisted_defaults", prepared && WFE_PreferencesInit() && wfe_general.toolbar == 2 &&
        wfe_general.startup == 1 && wfe_general.days == 9 && wfe_general.fixed_size == 12 &&
        wfe_general.proportional == "Times New Roman" && wfe_general.languages.empty() && wfe_general.colors[0] == 0xffffff);
    bool restored = existed ? wfe_smoke_write(wfe_general_file.c_str(), original_bytes) :
                              DeleteFileA(wfe_general_file.c_str()) != FALSE;
    wfe_general = original; wfe_general_loaded = true; wfe_apply_general();
    WFE_GeneralPreferencesChanged();
    STORE_CHECK("original_restored", restored && (!existed ||
        (wfe_smoke_read(wfe_general_file.c_str(), after) && original_bytes == after)));
    fprintf(output, "store_smoke=%s\r\n", failures ? "FAIL" : "PASS");
#undef STORE_CHECK
    if (fclose(output)) ++failures;
    return failures ? 1 : 0;
}
