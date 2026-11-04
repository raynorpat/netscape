#ifndef NETSCAPE_NATIVE_APPLICATIONS_H
#define NETSCAPE_NATIVE_APPLICATIONS_H
#include "frontend.h"
#ifdef __cplusplus
#include <string>
#include <vector>
bool WFE_ValidateApplication(const std::string &,std::string *error);
bool WFE_ValidateTemporaryDirectory(const std::string &,std::string *error);
bool WFE_LaunchApplication(const std::string &,const std::vector<std::string> &,std::string *error);
extern "C" {
#endif
const char *WFE_DefaultTemporaryDirectory(void);
int WFE_TestTemporaryDirectory(const char *);
void WFE_ApplyTemporaryDirectory(const char *);
int WFE_ExternalSource(MWContext *,const char *);
int WFE_TerminalApplication(MWContext *,int,const char *,const char *,const char *);
#ifdef __cplusplus
}
#endif
#endif
