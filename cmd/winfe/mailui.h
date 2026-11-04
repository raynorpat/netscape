#ifndef NETSCAPE_NATIVE_MAIL_UI_H
#define NETSCAPE_NATIVE_MAIL_UI_H
#include <afxwin.h>
#include <afxcmn.h>

class MailPaneUI;

class MailPaneDivider : public CWnd {
public:
    bool Create(MailPaneUI *panes, CWnd *owner, bool vertical, UINT id);
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    afx_msg BOOL OnSetCursor(CWnd *window, UINT hit, UINT message);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd *window);
    afx_msg void OnCancelMode();
    DECLARE_MESSAGE_MAP()
private:
    MailPaneUI *panes = nullptr;
    bool vertical = false, dragging = false;
    int grab_offset = 0;
};

/* Geometry, native list images and dividers only; MSG owns the list contents. */
class MailPaneUI {
public:
    enum { FolderDividerID = 124, ReaderDividerID = 125, FlagImage = 2 };
    ~MailPaneUI();
    bool Attach(CWnd *owner, CListCtrl *folders, CListCtrl *messages,
                CWnd *reader, bool news);
    /* top is the toolbar bottom; footer is the reserved status bar height. */
    void Arrange(int top, int footer);
    void FitColumns();
    int FolderImage(unsigned long flags) const;
    int MessageImage(unsigned long flags) const;
    void DrawNewsFolder(NMLVCUSTOMDRAW *draw, LRESULT *result);
    bool NewsExpansionHit(int row, CPoint point) const;
    int SubscriptionImage(unsigned long flags) const;
    void SetDividers(int folder_width, int top_height);
    CRect FolderBounds() const { return folder_bounds; }
    CRect MessageBounds() const { return message_bounds; }
    CRect ReaderBounds() const { return reader_bounds; }
private:
    friend class MailPaneDivider;
    void DragDivider(bool vertical, CPoint screen, int offset);
    CWnd *owner = nullptr, *reader = nullptr;
    CListCtrl *folders = nullptr, *messages = nullptr;
    CImageList folder_images, message_images;
    MailPaneDivider folder_divider, reader_divider;
    CRect folder_bounds, message_bounds, reader_bounds;
    int folder_width = 170, top_height = -1, top = 0, footer = 0;
    bool arranged = false, news_mode = false;
    CFont unread_font;
};
#endif
