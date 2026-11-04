#ifndef NETSCAPE_NATIVE_FRAME_DIVIDER_H
#define NETSCAPE_NATIVE_FRAME_DIVIDER_H
#include "frontend.h"
class FrameDivider {
public:
    ~FrameDivider() {End(false);}
    bool Begin(MWContext *,LO_EdgeStruct *,CPoint point);
    void Move(CPoint point);
    void End(bool commit);
    bool Active() const {return edge!=nullptr;}
    void Forget(LO_EdgeStruct *released) {if(edge==released) End(false);}
    HCURSOR Cursor() const;
private:
    MWContext *context=nullptr;
    LO_EdgeStruct *edge=nullptr;
    CPoint origin,position;
    int mouse_offset=0;
    CWnd tracker;
};
void WFE_PaintFrameEdge(HDC,LO_EdgeStruct *);
#endif
