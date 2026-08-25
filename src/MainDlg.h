#pragma once

#include "resource.h"
#include "AppSettings.h"
#include "ConversationManager.h"
#include "LLMEngine.h"

#include <atomic>
#include <string>
#include <thread>

class CConversationListBox : public CListBox
{
protected:
    void DrawItem(LPDRAWITEMSTRUCT drawItem) override;
    void MeasureItem(LPMEASUREITEMSTRUCT measureItem) override;
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnMouseLeave();

    DECLARE_MESSAGE_MAP()

private:
    int ScaleForDpi(int value) const;
    void RedrawItem(int index);

    int hoveredItem_ = -1;
    bool trackingMouse_ = false;
};

class CMainDlg : public CDialogEx
{
public:
    explicit CMainDlg(CWnd* pParent = nullptr);
    ~CMainDlg() override;

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_LOCALSENSENOVA_DIALOG };
#endif

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedLoadModel();
    afx_msg void OnBnClickedSend();
    afx_msg void OnBnClickedStop();
    afx_msg void OnBnClickedNewSession();
    afx_msg void OnBnClickedDeleteSession();
    afx_msg void OnBnClickedApplySettings();
    afx_msg void OnBnClickedToggleSettings();
    afx_msg void OnCbnSelchangePreset();
    afx_msg void OnLbnSelchangeSession();

    afx_msg LRESULT OnStreamToken(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGenerationDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnModelLoaded(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    int Scale(int value) const;
    void ApplyTheme();
    void LayoutControls(int cx, int cy);
    void ShowSettingsPanel(bool show);
    void MoveControl(int id, int x, int y, int width, int height);
    void StyleButton(CMFCButton& button, COLORREF face, COLORREF text);
    void AppendRichText(const std::wstring& text, COLORREF color, bool bold, int heightTwips = 215);
    void AppendMessage(const ChatMessage& message);
    void AppendAssistantHeader();

    void RefreshSessionList();
    void RenderCurrentConversation();
    void AppendChat(const std::wstring& text);
    void AppendUtf8Piece(const std::string& piece);
    void FlushPendingUtf8();

    void UpdateSettingsControls();
    bool ReadSettingsControls(bool showErrors);
    void ApplyPreset(int index);
    void SetBusyUi(bool busy, const wchar_t* status = nullptr);
    std::wstring HistoryPath() const;

    CRichEditCtrl chat_;
    CEdit input_;
    CConversationListBox sessions_;
    CComboBox presets_;
    CMFCButton loadModelButton_;
    CMFCButton sendButton_;
    CMFCButton stopButton_;
    CMFCButton newSessionButton_;
    CMFCButton deleteSessionButton_;
    CMFCButton applySettingsButton_;
    CMFCButton toggleSettingsButton_;

    CFont uiFont_;
    CFont titleFont_;
    CFont sectionFont_;
    CFont messageFont_;
    CBrush windowBrush_;
    CBrush sidebarBrush_;
    CBrush chatBrush_;
    CBrush controlBrush_;
    CBrush settingsBrush_;

    UINT dpi_ = 96;
    bool settingsVisible_ = false;

    AppSettings settings_;
    ConversationManager conversations_;
    LLMEngine engine_;

    std::thread worker_;
    std::atomic_bool closing_{ false };
    std::string pendingUtf8_;
};
