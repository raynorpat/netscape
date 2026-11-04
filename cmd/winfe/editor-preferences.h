#ifndef NETSCAPE_GOLD_PREFERENCES_H
#define NETSCAPE_GOLD_PREFERENCES_H
#include "editor.h"
#include <string>
struct EditorPreferences {
    std::string author, document_template, html_editor, image_editor;
    std::string publish_location, browse_location, publish_username;
    int autosave_minutes = 0;
    bool maintain_links = true, keep_images = true;
};
EditorPreferences WFE_EditorPreferencesRead();
bool WFE_EditorPreferencesWrite(const EditorPreferences &value,std::string &error);
bool WFE_EditorPreferencesValid(const EditorPreferences &value,std::string &error);
bool WFE_EditorPreferencesDialog(MWContext *context);
void WFE_EditorApplyPreferences(MWContext *context,bool new_document);
std::string WFE_EditorQuoteArgument(const std::string &argument);
bool WFE_EditorPreferencesSmoke(MWContext *context,const char *report);
#endif
