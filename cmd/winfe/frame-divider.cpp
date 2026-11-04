#include <afxwin.h>
#include <algorithm>
#include "frame-divider.h"

bool FrameDivider::Begin(MWContext *document,LO_EdgeStruct *element,CPoint point)
{
    End(false);
    if(!element || !element->visible || !element->movable || element->right_bottom_bound-element->left_top_bound<4) return false;
    context=document;edge=element;origin=position=CPoint(edge->x,edge->y);
    mouse_offset=edge->is_vertical?point.x-edge->x:point.y-edge->y;
    /* A disabled, nonactivating popup keeps the tracker visible above child
     * frames without stealing the captured pointer or document focus. */
    if(!tracker.CreateEx(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,
        AfxRegisterWndClass(0,nullptr,(HBRUSH)GetStockObject(GRAY_BRUSH)),"",
        WS_POPUP|WS_DISABLED,CRect(0,0,1,1),CWnd::FromHandle(WFE_DocumentWindow(context)),0)) {
        edge=nullptr;context=nullptr;return false;
    }
    Move(point);return true;
}
HCURSOR FrameDivider::Cursor() const
{return ::LoadCursor(nullptr,edge&&edge->is_vertical?IDC_SIZEWE:IDC_SIZENS);}
void FrameDivider::Move(CPoint point)
{
    if(!edge) return;
    int margin=(std::min)(20,(int)(edge->right_bottom_bound-edge->left_top_bound)/3);
    int lower=edge->left_top_bound+margin,upper=edge->right_bottom_bound-margin;
    int coordinate=(edge->is_vertical?point.x:point.y)-mouse_offset;
    coordinate=(std::max)(lower,(std::min)(coordinate,upper));
    if(edge->is_vertical) position.x=coordinate;else position.y=coordinate;
    HWND view=WFE_DocumentWindow(context);if(!::IsWindow(view)) {End(false);return;}
    int32 x=0,y=0;WFE_Position(context,&x,&y);
    CPoint screen(position.x+edge->x_offset-x,position.y+edge->y_offset-y);
    ::ClientToScreen(view,&screen);
    tracker.SetWindowPos(&CWnd::wndTop,screen.x,screen.y,edge->is_vertical?2:edge->width,
        edge->is_vertical?edge->height:2,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    ::SetCursor(Cursor());
}
void FrameDivider::End(bool commit)
{
    LO_EdgeStruct *element=edge;MWContext *document=context;
    edge=nullptr;context=nullptr;
    if(tracker.GetSafeHwnd()) tracker.DestroyWindow();
    /* Clear the captured element before layout callbacks or window resizing
     * can reenter the frontend. FE_FreeEdgeElement also cancels this state. */
    if(commit && element && position!=origin) {
        LO_MoveGridEdge(document,element,position.x,position.y);WFE_Invalidate(document);
    }
}
void WFE_PaintFrameEdge(HDC target,LO_EdgeStruct *edge)
{
    if(!target || !edge || !edge->visible) return;
    RECT area={edge->x+edge->x_offset,edge->y+edge->y_offset,
        edge->x+edge->x_offset+edge->width,edge->y+edge->y_offset+edge->height};
    COLORREF color=edge->bg_color?RGB(edge->bg_color->red,edge->bg_color->green,edge->bg_color->blue):GetSysColor(COLOR_3DFACE);
    HBRUSH fill=CreateSolidBrush(color);FillRect(target,&area,fill);DeleteObject(fill);
    DrawEdge(target,&area,EDGE_RAISED,BF_RECT);
}
