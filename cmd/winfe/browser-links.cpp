#include <afxwin.h>
#include "browser-links.h"
#include "resource.h"

BrowserLinkHit WFE_BrowserLinkAt(MWContext *context,int x,int y)
{
    BrowserLinkHit hit;
    LO_Element *element=LO_XYToElement(context,x,y);
    if(!element) return hit;
    LO_AnchorData *anchor=nullptr;
    if(element->type==LO_TEXT) {hit.text=true;anchor=element->lo_text.anchor_href;}
    else if(element->type==LO_IMAGE) {
        LO_ImageStruct *image=&element->lo_image;
        if(image->image_url) hit.image=(const char *)image->image_url;
        if(image->image_attr && (image->image_attr->attrmask&LO_ATTR_ISFORM)) return hit;
        x-=image->x+image->x_offset+image->border_width;
        y-=image->y+image->y_offset+image->border_width;
        anchor=image->anchor_href;
        if(image->image_attr && image->image_attr->usemap_name) anchor=LO_MapXYToAreaAnchor(context,image,x,y);
        if(anchor && anchor->anchor) {
            hit.link=(const char *)anchor->anchor;
            if(image->image_attr && (image->image_attr->attrmask&LO_ATTR_ISMAP)) {
                char position[48];sprintf(position,"?%d,%d",x,y);hit.link+=position;
            }
        }
    }
    if(anchor) {
        if(hit.link.empty() && anchor->anchor) hit.link=(const char *)anchor->anchor;
        if(anchor->target) hit.target=(const char *)anchor->target;
    }
    return hit;
}
void WFE_BrowserLinkMenu(CMenu &menu,const BrowserLinkHit &hit)
{
    if(!hit.link.empty()) {
        menu.AppendMenu(MF_STRING,ID_BROWSER_OPEN_LINK,"&Open this Link");
        menu.AppendMenu(MF_STRING,ID_BROWSER_LINK_NEW,"Open in &New Window");
        menu.AppendMenu(MF_STRING,ID_BROWSER_SAVE_LINK,"&Save Link As...");
        menu.AppendMenu(MF_STRING,ID_BROWSER_COPY_LINK,"Copy Li&nk Location");
        menu.AppendMenu(MF_SEPARATOR);
    }
    if(!hit.image.empty()) {
        menu.AppendMenu(MF_STRING,ID_BROWSER_VIEW_IMAGE,"View &Image");
        menu.AppendMenu(MF_STRING,ID_BROWSER_SAVE_IMAGE,"Save &Image As...");
        menu.AppendMenu(MF_STRING,ID_BROWSER_COPY_IMAGE,"Cop&y Image Location");
        menu.AppendMenu(MF_SEPARATOR);
    }
}
static void copy_location(MWContext *context,const std::string &text)
{
    if(text.empty()) return;
    /* Windows clipboard history may briefly lock it after a preceding copy.
     * Bound the retry without pumping messages that could destroy context. */
    bool opened=false;
    for(int i=0;i<20;++i) {
        if(::OpenClipboard(WFE_DocumentWindow(context))) {opened=true;break;}
        Sleep(10);
    }
    if(!opened) {WFE_Status(context,"Clipboard is busy. Try Copy again.");return;}
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,text.size()+1);
    void *memory=data?GlobalLock(data):nullptr;
    if(memory) {
        memcpy(memory,text.c_str(),text.size()+1);GlobalUnlock(data);
        if(!EmptyClipboard() || !SetClipboardData(CF_TEXT,data)) GlobalFree(data);
    } else if(data) GlobalFree(data);
    CloseClipboard();
}
bool WFE_BrowserLinkCommand(MWContext *context,const BrowserLinkHit &hit,UINT command)
{
    switch(command) {
    case ID_BROWSER_COPY_LINK:copy_location(context,hit.link);break;
    case ID_BROWSER_COPY_IMAGE:copy_location(context,hit.image);break;
    case ID_BROWSER_LINK_NEW:if(!hit.link.empty()) WFE_NewWindow(hit.link.c_str());break;
    case ID_BROWSER_OPEN_LINK:
        if(!hit.link.empty()) {
            if(context->type==MWContextBrowser) WFE_NavigateTarget(context,hit.link.c_str(),hit.target.c_str());
            else WFE_NewWindow(hit.link.c_str());
        }
        break;
    case ID_BROWSER_VIEW_IMAGE:
        if(!hit.image.empty()) {
            if(context->type==MWContextBrowser) WFE_NavigateTarget(context,hit.image.c_str(),nullptr);
            else WFE_NewWindow(hit.image.c_str());
        }
        break;
    default:return false;
    }
    return true;
}
