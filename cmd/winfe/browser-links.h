#ifndef NETSCAPE_NATIVE_BROWSER_LINKS_H
#define NETSCAPE_NATIVE_BROWSER_LINKS_H
#include <string>
#include "frontend.h"
class CMenu;
/* Own the strings: a popup menu can pump document loads and free LO elements. */
struct BrowserLinkHit {
    std::string link,target,image;
    bool text=false;
};
BrowserLinkHit WFE_BrowserLinkAt(MWContext *,int x,int y);
void WFE_BrowserLinkMenu(CMenu &,const BrowserLinkHit &);
bool WFE_BrowserLinkCommand(MWContext *,const BrowserLinkHit &,UINT command);
#endif
