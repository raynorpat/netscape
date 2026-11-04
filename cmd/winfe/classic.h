#ifndef NETSCAPE_CLASSIC_UI_H
#define NETSCAPE_CLASSIC_UI_H
#include <afxwin.h>
#include <vector>
#include <memory>

namespace ClassicUI {
    const COLORREF Face = RGB(192,192,192);
    int Scale(CWnd *window, int value);
    CFont *Font(CWnd *window);
    void Apply(CWnd *window);
    HBRUSH ControlColor(CDC *dc, CWnd *window, UINT type);
}

class ClassicButton : public CButton {
public:
    void SetBitmapResource(UINT resource);
    void SetPresentation(int style) { presentation=style; if(GetSafeHwnd()) Invalidate(); }
    void DrawItem(LPDRAWITEMSTRUCT item) override;
private:
    CBitmap artwork, disabled_artwork;
    int presentation = 2;
};

class ClassicDialog : public CDialog {
public:
    using CDialog::CDialog;
protected:
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG *message) override;
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *window, UINT type);
    ClassicButton accept, cancel;
    DECLARE_MESSAGE_MAP()
};
#endif
