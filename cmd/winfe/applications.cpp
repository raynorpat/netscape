/* Supporting applications receive explicit argv, never a shell template. */
#include <afxwin.h>
#include <shlwapi.h>
#include "applications.h"
#include "preferences.h"
#include "downloads.h"

static bool application_error(std::string *error,const char *message)
{if(error) *error=message;return false;}
static bool absolute_path(const std::string &path)
{
    if(path.empty() || path.size()>=MAX_PATH) return false;
    for(unsigned char c:path) if(c<32 || c==127) return false;
    return (path.size()>2 && ((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z')) && path[1]==':' && (path[2]=='\\'||path[2]=='/')) || path.compare(0,2,"\\\\")==0;
}
bool WFE_ValidateApplication(const std::string &path,std::string *error)
{
    if(path.empty()) return true;
    DWORD type=0;
    if(!absolute_path(path) || _stricmp(PathFindExtensionA(path.c_str()),".exe") ||
        !GetBinaryTypeA(path.c_str(),&type) || (type!=SCS_32BIT_BINARY && type!=SCS_64BIT_BINARY))
        return application_error(error,"Choose an existing Windows .exe application using its full path. Enter the executable path without arguments.");
    return true;
}
bool WFE_ValidateTemporaryDirectory(const std::string &path,std::string *error)
{
    if(path.empty()) return true;
    if(!absolute_path(path) || !WFE_TestTemporaryDirectory(path.c_str()))
        return application_error(error,"Choose an existing writable temporary directory using its full path.");
    return true;
}
static std::string quote_argument(const std::string &value)
{
    std::string result="\"";size_t slashes=0;
    for(char c:value) {
        if(c=='\\') {++slashes;continue;}
        if(c=='\"') result.append(slashes*2+1,'\\');else result.append(slashes,'\\');
        result+=c;slashes=0;
    }
    result.append(slashes*2,'\\');result+='\"';return result;
}
bool WFE_LaunchApplication(const std::string &program,const std::vector<std::string> &arguments,std::string *error)
{
    if(program.empty() || !WFE_ValidateApplication(program,error)) return false;
    std::string command=quote_argument(program);
    for(const auto &argument:arguments) {
        if(argument.find('\0')!=std::string::npos || argument.size()>32700) return application_error(error,"An application argument is too long or contains a NUL character.");
        command+=' ';command+=quote_argument(argument);
    }
    if(command.size()>=32767) return application_error(error,"The application command line is too long.");
    std::vector<char> mutable_command(command.begin(),command.end());mutable_command.push_back(0);
    STARTUPINFOA startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION process={};
    if(!CreateProcessA(program.c_str(),mutable_command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))
        return application_error(error,"The supporting application could not be started. Check its path and runtime dependencies.");
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
extern "C" int WFE_ExternalSource(MWContext *context,const char *address)
{
    std::string program=WFE_GeneralPreferences().source_application;
    if(program.empty()) return 0;
    if(!WFE_SaveSourceForApplication(context,address,program.c_str()))
        WFE_Message(context,"Could not prepare an external source view. Check the temporary directory and application settings.",0);
    return 1;
}
extern "C" int WFE_TerminalApplication(MWContext *context,int type,const char *host,const char *port,const char *user)
{
    if(type!=FE_TELNET_URL_TYPE && type!=FE_TN3270_URL_TYPE) return 0;
    const GeneralPreferences &p=WFE_GeneralPreferences();
    std::string program=type==FE_TELNET_URL_TYPE?p.telnet_application:p.tn3270_application;
    std::string error;
    bool valid=host && *host && strlen(host)<=253 && isalnum((unsigned char)*host);
    if(host) for(const unsigned char *c=(const unsigned char *)host;*c;++c)
        if(!isalnum(*c) && *c!='.' && *c!='-' && *c!='_') valid=false;
    unsigned number=0;
    if(port && *port) {
        if(strlen(port)>5) valid=false;
        else for(const char *c=port;*c;++c) {if(*c<'0'||*c>'9') {valid=false;break;}number=number*10+*c-'0';}
        if(number<1 || number>65535) valid=false;
    }
    if(user && *user) {
        if(strlen(user)>64 || !isalnum((unsigned char)*user)) valid=false;
        for(const unsigned char *c=(const unsigned char *)user;*c;++c)
            if(!isalnum(*c) && *c!='.' && *c!='_' && *c!='-') valid=false;
    }
    if(!valid) error="The terminal link has an invalid host, port or username.";
    else if(program.empty()) error="Choose the Telnet or TN3270 application in General Preferences, Apps, before opening this link.";
    else {
        std::vector<std::string> arguments;
        if(user && *user) {arguments.push_back("-l");arguments.push_back(user);}
        arguments.push_back(host);if(port && *port) arguments.push_back(port);
        if(WFE_LaunchApplication(program,arguments,&error)) {WFE_Status(context,"Supporting terminal application started.");return 1;}
    }
    WFE_Message(context,error.c_str(),0);return -1;
}
