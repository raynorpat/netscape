#ifndef NETSCAPE_CLASSIC_CHROME_H
#define NETSCAPE_CLASSIC_CHROME_H
#include "classic.h"
#include "legacy/genedit.h"
#include <afxext.h>
#include <afxcmn.h>

class ClassicLogo : public CStatic {
public:
    CBitmap bitmap;
protected:
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
};

class ClassicChrome : public CWnd {
public:
    bool toolbar_visible = true, location_visible = true, directory_visible = true;
    bool Create(CWnd *owner, CGenericEdit *edit, ClassicButton *go, int mode);
    int Arrange(int width);
    void UpdateCommands();
    void SetPresentation(int style);
    int Presentation() const { return presentation; }
    void Remember(const char *address);
    void FocusBar(bool backwards);
    size_t ButtonCount() const { return buttons.size(); }
    CString Labels() const;
    CWnd *Button(UINT command);
    CComboBox location;
private:
    int mode = 0, presentation = 2;
    CGenericEdit *edit = nullptr;
    ClassicButton *go = nullptr;
    CStatic label;
    ClassicLogo logo;
    CToolTipCtrl tips;
    std::vector<std::unique_ptr<ClassicButton>> buttons, shortcuts;
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *window, UINT type);
    BOOL OnCommand(WPARAM command, LPARAM control) override;
    BOOL PreTranslateMessage(MSG *message) override;
    DECLARE_MESSAGE_MAP()
};

class ClassicStatusBar : public CStatusBar {
public:
    bool busy = false;
    int progress = 0;
protected:
    afx_msg void OnPaint();
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()
};
#endif
