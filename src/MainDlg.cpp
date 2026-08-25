#include "pch.h"
#include "MainDlg.h"

#include "resource.h"
#include "Utf8.h"

#include <array>
#include <iomanip>
#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr UINT WM_STREAM_TOKEN = WM_APP + 101;
    constexpr UINT WM_GENERATION_DONE = WM_APP + 102;
    constexpr UINT WM_MODEL_LOADED = WM_APP + 103;

    constexpr COLORREF UI_WINDOW = RGB(247, 247, 248);
    constexpr COLORREF UI_SIDEBAR = RGB(242, 247, 248);
    constexpr COLORREF UI_SIDEBAR_CARD = RGB(226, 234, 237);
    constexpr COLORREF UI_SIDEBAR_HOVER = RGB(234, 240, 242);
    constexpr COLORREF UI_CHAT = RGB(255, 255, 255);
    constexpr COLORREF UI_CONTROL = RGB(242, 242, 242);
    constexpr COLORREF UI_SETTINGS = RGB(250, 250, 250);
    constexpr COLORREF UI_TEXT = RGB(32, 33, 35);
    constexpr COLORREF UI_MUTED = RGB(112, 115, 122);
    constexpr COLORREF UI_SIDEBAR_TEXT = RGB(52, 61, 64);
    constexpr COLORREF UI_SIDEBAR_MUTED = RGB(132, 144, 148);
    constexpr COLORREF UI_ACCENT = RGB(16, 163, 127);
    constexpr COLORREF UI_DANGER = RGB(185, 55, 55);
    constexpr COLORREF UI_BORDER = RGB(225, 225, 225);

    constexpr std::array<int, 15> SETTINGS_CONTROL_IDS = {
        IDC_SETTINGS_TITLE,
        IDC_LABEL_PRESET,
        IDC_COMBO_PRESET,
        IDC_LABEL_SYSTEM,
        IDC_EDIT_SYSTEM,
        IDC_LABEL_TEMP,
        IDC_EDIT_TEMP,
        IDC_LABEL_TOPP,
        IDC_EDIT_TOPP,
        IDC_LABEL_TOPK,
        IDC_EDIT_TOPK,
        IDC_LABEL_REPEAT,
        IDC_EDIT_REPEAT,
        IDC_LABEL_MAXTOK,
        IDC_EDIT_MAXTOK,
    };

    struct GenerationResult
    {
        size_t conversationIndex = 0;
        bool ok = false;
        std::string response;
        std::string error;
        GenerationStats stats;
    };

    struct ModelLoadResult
    {
        bool ok = false;
        std::wstring path;
        std::string description;
        std::string error;
    };

    std::wstring ToWideLossy(const std::string& s)
    {
        bool ok = false;
        std::wstring w = Utf8ToWide(s, &ok);
        if (ok) return w;

        if (s.empty()) return {};
        int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) return L"[UTF-8 decode error]";
        w.assign(static_cast<size_t>(n), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    }
}

BEGIN_MESSAGE_MAP(CConversationListBox, CListBox)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

int CConversationListBox::ScaleForDpi(int value) const
{
    UINT dpi = GetSafeHwnd() ? ::GetDpiForWindow(GetSafeHwnd()) : 96;
    if (dpi == 0) dpi = 96;
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

void CConversationListBox::MeasureItem(LPMEASUREITEMSTRUCT measureItem)
{
    if (measureItem) measureItem->itemHeight = static_cast<UINT>(ScaleForDpi(44));
}

void CConversationListBox::DrawItem(LPDRAWITEMSTRUCT drawItem)
{
    if (!drawItem || drawItem->itemID == static_cast<UINT>(-1)) return;

    CDC dc;
    dc.Attach(drawItem->hDC);
    const CRect row(drawItem->rcItem);
    dc.FillSolidRect(row, UI_SIDEBAR);

    const bool selected = (drawItem->itemState & ODS_SELECTED) != 0;
    const bool hovered = static_cast<int>(drawItem->itemID) == hoveredItem_;
    CRect pill(row);
    pill.DeflateRect(ScaleForDpi(5), ScaleForDpi(3));

    if (selected || hovered) {
        const COLORREF fill = selected ? UI_SIDEBAR_CARD : UI_SIDEBAR_HOVER;
        CBrush brush(fill);
        CPen pen(PS_SOLID, 1, fill);
        CBrush* oldBrush = dc.SelectObject(&brush);
        CPen* oldPen = dc.SelectObject(&pen);
        dc.RoundRect(pill, CPoint(pill.Height(), pill.Height()));
        dc.SelectObject(oldPen);
        dc.SelectObject(oldBrush);
    }

    CString text;
    GetText(static_cast<int>(drawItem->itemID), text);
    CRect textRect(pill);
    textRect.left += ScaleForDpi(14);
    textRect.right -= ScaleForDpi(selected ? 38 : 14);

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(UI_SIDEBAR_TEXT);
    CFont* oldFont = dc.SelectObject(GetFont());
    dc.DrawText(text, textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    dc.SelectObject(oldFont);

    if (selected) {
        const int radius = ScaleForDpi(6);
        const CPoint center(pill.right - ScaleForDpi(18), pill.CenterPoint().y);
        CPen ringPen(PS_SOLID, std::max(1, ScaleForDpi(1)), UI_SIDEBAR_MUTED);
        CPen* oldPen = dc.SelectObject(&ringPen);
        CBrush* oldBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
        dc.Ellipse(center.x - radius, center.y - radius, center.x + radius, center.y + radius);
        dc.SelectObject(oldBrush);
        dc.SelectObject(oldPen);
    }

    dc.Detach();
}

void CConversationListBox::RedrawItem(int index)
{
    if (index < 0 || index >= GetCount()) return;
    CRect rect;
    if (GetItemRect(index, &rect) != LB_ERR) InvalidateRect(rect, FALSE);
}

void CConversationListBox::OnMouseMove(UINT flags, CPoint point)
{
    BOOL outside = FALSE;
    const int item = static_cast<int>(ItemFromPoint(point, outside));
    const int nextHovered = outside ? -1 : item;
    if (nextHovered != hoveredItem_) {
        const int previous = hoveredItem_;
        hoveredItem_ = nextHovered;
        RedrawItem(previous);
        RedrawItem(hoveredItem_);
    }

    if (!trackingMouse_) {
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, GetSafeHwnd(), 0 };
        trackingMouse_ = ::TrackMouseEvent(&tracking) != FALSE;
    }
    CListBox::OnMouseMove(flags, point);
}

void CConversationListBox::OnMouseLeave()
{
    const int previous = hoveredItem_;
    hoveredItem_ = -1;
    trackingMouse_ = false;
    RedrawItem(previous);
    CListBox::OnMouseLeave();
}

CMainDlg::CMainDlg(CWnd* pParent)
    : CDialogEx(IDD_LOCALSENSENOVA_DIALOG, pParent)
{
}

CMainDlg::~CMainDlg()
{
    closing_ = true;
    engine_.RequestStop();
    if (worker_.joinable()) worker_.join();
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CHAT, chat_);
    DDX_Control(pDX, IDC_INPUT, input_);
    DDX_Control(pDX, IDC_SESSION_LIST, sessions_);
    DDX_Control(pDX, IDC_COMBO_PRESET, presets_);
    DDX_Control(pDX, IDC_BTN_LOAD_MODEL, loadModelButton_);
    DDX_Control(pDX, IDC_BTN_SEND, sendButton_);
    DDX_Control(pDX, IDC_BTN_STOP, stopButton_);
    DDX_Control(pDX, IDC_BTN_NEW_SESSION, newSessionButton_);
    DDX_Control(pDX, IDC_BTN_DELETE_SESSION, deleteSessionButton_);
    DDX_Control(pDX, IDC_BTN_APPLY_SETTINGS, applySettingsButton_);
    DDX_Control(pDX, IDC_BTN_TOGGLE_SETTINGS, toggleSettingsButton_);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BTN_LOAD_MODEL, &CMainDlg::OnBnClickedLoadModel)
    ON_BN_CLICKED(IDC_BTN_SEND, &CMainDlg::OnBnClickedSend)
    ON_BN_CLICKED(IDC_BTN_STOP, &CMainDlg::OnBnClickedStop)
    ON_BN_CLICKED(IDC_BTN_NEW_SESSION, &CMainDlg::OnBnClickedNewSession)
    ON_BN_CLICKED(IDC_BTN_DELETE_SESSION, &CMainDlg::OnBnClickedDeleteSession)
    ON_BN_CLICKED(IDC_BTN_APPLY_SETTINGS, &CMainDlg::OnBnClickedApplySettings)
    ON_BN_CLICKED(IDC_BTN_TOGGLE_SETTINGS, &CMainDlg::OnBnClickedToggleSettings)
    ON_CBN_SELCHANGE(IDC_COMBO_PRESET, &CMainDlg::OnCbnSelchangePreset)
    ON_LBN_SELCHANGE(IDC_SESSION_LIST, &CMainDlg::OnLbnSelchangeSession)
    ON_MESSAGE(WM_STREAM_TOKEN, &CMainDlg::OnStreamToken)
    ON_MESSAGE(WM_GENERATION_DONE, &CMainDlg::OnGenerationDone)
    ON_MESSAGE(WM_MODEL_LOADED, &CMainDlg::OnModelLoaded)
END_MESSAGE_MAP()

BOOL CMainDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    dpi_ = ::GetDpiForWindow(m_hWnd);
    if (dpi_ == 0) dpi_ = 96;

    windowBrush_.CreateSolidBrush(UI_WINDOW);
    sidebarBrush_.CreateSolidBrush(UI_SIDEBAR);
    chatBrush_.CreateSolidBrush(UI_CHAT);
    controlBrush_.CreateSolidBrush(UI_CONTROL);
    settingsBrush_.CreateSolidBrush(UI_SETTINGS);

    presets_.AddString(L"普通助手");
    presets_.AddString(L"C++ 代码助手");
    presets_.AddString(L"中英翻译");
    presets_.AddString(L"公文写作");
    presets_.SetCurSel(0);

    ApplyTheme();
    ShowSettingsPanel(false);

    conversations_.Load(HistoryPath());
    RefreshSessionList();
    RenderCurrentConversation();
    UpdateSettingsControls();

    GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE);
    SetDlgItemText(IDC_STATUS, L"状态：请选择 SenseNova 8B GGUF 模型");
    input_.SendMessage(EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"给 LocalSenseNova 发送消息……"));
    input_.SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(Scale(12), Scale(12)));

    CRect workArea;
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int targetWidth = std::min(Scale(1180), workArea.Width() * 9 / 10);
    const int targetHeight = std::min(Scale(780), workArea.Height() * 88 / 100);
    SetWindowPos(nullptr, 0, 0, targetWidth, targetHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    CRect client;
    GetClientRect(&client);
    LayoutControls(client.Width(), client.Height());
    CenterWindow();
    input_.SetFocus();
    return FALSE;
}

int CMainDlg::Scale(int value) const
{
    return ::MulDiv(value, static_cast<int>(dpi_), 96);
}

void CMainDlg::StyleButton(CMFCButton& button, COLORREF face, COLORREF text)
{
    button.m_bDontUseWinXPTheme = TRUE;
    button.m_nFlatStyle = CMFCButton::BUTTONSTYLE_FLAT;
    button.m_bDrawFocus = FALSE;
    button.SetFaceColor(face, FALSE);
    button.SetTextColor(text);
    button.SetTextHotColor(text);
    button.RedrawWindow();
}

void CMainDlg::ApplyTheme()
{
    uiFont_.CreateFont(
        Scale(-16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    titleFont_.CreateFont(
        Scale(-22), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    sectionFont_.CreateFont(
        Scale(-15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    messageFont_.CreateFont(
        Scale(-17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    for (CWnd* child = GetWindow(GW_CHILD); child; child = child->GetNextWindow()) {
        child->SetFont(&uiFont_);
    }
    GetDlgItem(IDC_APP_TITLE)->SetFont(&titleFont_);
    GetDlgItem(IDC_HEADER_TITLE)->SetFont(&sectionFont_);
    GetDlgItem(IDC_SIDEBAR_SECTION)->SetFont(&sectionFont_);
    GetDlgItem(IDC_SETTINGS_TITLE)->SetFont(&titleFont_);
    chat_.SetFont(&messageFont_);
    input_.SetFont(&messageFont_);

    chat_.SetBackgroundColor(FALSE, UI_CHAT);
    chat_.ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
    input_.ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
    sessions_.ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);
    sessions_.SetItemHeight(0, Scale(44));

    StyleButton(newSessionButton_, UI_SIDEBAR, UI_SIDEBAR_TEXT);
    StyleButton(deleteSessionButton_, UI_SIDEBAR, UI_SIDEBAR_MUTED);
    newSessionButton_.m_nAlignStyle = CMFCButton::ALIGN_LEFT;
    deleteSessionButton_.m_nAlignStyle = CMFCButton::ALIGN_LEFT;
    StyleButton(loadModelButton_, UI_CONTROL, UI_TEXT);
    StyleButton(toggleSettingsButton_, UI_CONTROL, UI_TEXT);
    StyleButton(sendButton_, UI_ACCENT, RGB(255, 255, 255));
    StyleButton(stopButton_, UI_DANGER, RGB(255, 255, 255));
    StyleButton(applySettingsButton_, UI_ACCENT, RGB(255, 255, 255));
}

void CMainDlg::MoveControl(int id, int x, int y, int width, int height)
{
    if (CWnd* control = GetDlgItem(id)) {
        control->MoveWindow(x, y, std::max(1, width), std::max(1, height), FALSE);
    }
}

void CMainDlg::ShowSettingsPanel(bool show)
{
    settingsVisible_ = show;
    for (int id : SETTINGS_CONTROL_IDS) {
        if (CWnd* control = GetDlgItem(id)) control->ShowWindow(show ? SW_SHOW : SW_HIDE);
    }
    applySettingsButton_.ShowWindow(show ? SW_SHOW : SW_HIDE);
    toggleSettingsButton_.SetWindowText(show ? L"关闭参数" : L"参数");
}

void CMainDlg::LayoutControls(int cx, int cy)
{
    if (!::IsWindow(chat_.GetSafeHwnd()) || cx <= 0 || cy <= 0) return;

    const int sidebar = Scale(224);
    const int margin = Scale(24);
    const int top = Scale(76);
    const int drawer = settingsVisible_ ? Scale(300) : 0;
    const int mainLeft = sidebar + margin;
    const int mainRight = cx - margin - drawer;
    const int composerTop = cy - Scale(132);
    const int statusTop = cy - Scale(34);

    MoveControl(IDC_APP_TITLE, Scale(20), Scale(18), sidebar - Scale(40), Scale(32));
    MoveControl(IDC_APP_SUBTITLE, Scale(20), Scale(49), sidebar - Scale(40), Scale(20));
    MoveControl(IDC_BTN_NEW_SESSION, Scale(16), Scale(82), sidebar - Scale(32), Scale(42));
    MoveControl(IDC_SIDEBAR_SECTION, Scale(20), Scale(145), sidebar - Scale(40), Scale(22));
    MoveControl(IDC_SESSION_LIST, Scale(16), Scale(172), sidebar - Scale(32), cy - Scale(250));
    MoveControl(IDC_BTN_DELETE_SESSION, Scale(16), cy - Scale(58), sidebar - Scale(32), Scale(38));

    MoveControl(IDC_HEADER_TITLE, mainLeft, Scale(18), Scale(190), Scale(28));
    const int toggleWidth = settingsVisible_ ? Scale(104) : Scale(76);
    const int toggleX = mainRight - toggleWidth;
    const int loadX = toggleX - Scale(112);
    MoveControl(IDC_BTN_LOAD_MODEL, loadX, Scale(16), Scale(100), Scale(38));
    MoveControl(IDC_BTN_TOGGLE_SETTINGS, toggleX, Scale(16), toggleWidth, Scale(38));
    MoveControl(IDC_MODEL_PATH, mainLeft, Scale(48), std::max(Scale(180), loadX - mainLeft - Scale(16)), Scale(20));

    MoveControl(IDC_CHAT, mainLeft, top, mainRight - mainLeft, composerTop - top - Scale(14));

    const int actionWidth = Scale(98);
    const int inputLeft = mainLeft + Scale(14);
    const int actionX = mainRight - actionWidth;
    MoveControl(IDC_INPUT, inputLeft, composerTop + Scale(12), actionX - inputLeft - Scale(12), Scale(78));
    MoveControl(IDC_BTN_SEND, actionX, composerTop + Scale(12), actionWidth, Scale(38));
    MoveControl(IDC_BTN_STOP, actionX, composerTop + Scale(54), actionWidth, Scale(36));
    MoveControl(IDC_STATUS, mainLeft + Scale(14), statusTop, mainRight - mainLeft - Scale(14), Scale(22));

    if (settingsVisible_) {
        const int drawerX = cx - Scale(276);
        const int fieldWidth = Scale(236);
        MoveControl(IDC_SETTINGS_TITLE, drawerX, Scale(82), fieldWidth, Scale(32));
        MoveControl(IDC_LABEL_PRESET, drawerX, Scale(130), fieldWidth, Scale(20));
        MoveControl(IDC_COMBO_PRESET, drawerX, Scale(152), fieldWidth, Scale(120));
        MoveControl(IDC_LABEL_SYSTEM, drawerX, Scale(204), fieldWidth, Scale(20));
        MoveControl(IDC_EDIT_SYSTEM, drawerX, Scale(226), fieldWidth, Scale(142));

        const int labelWidth = Scale(128);
        const int editX = drawerX + Scale(146);
        const int editWidth = fieldWidth - Scale(146);
        const int firstY = Scale(388);
        const int row = Scale(42);
        MoveControl(IDC_LABEL_TEMP, drawerX, firstY, labelWidth, Scale(24));
        MoveControl(IDC_EDIT_TEMP, editX, firstY - Scale(3), editWidth, Scale(30));
        MoveControl(IDC_LABEL_TOPP, drawerX, firstY + row, labelWidth, Scale(24));
        MoveControl(IDC_EDIT_TOPP, editX, firstY + row - Scale(3), editWidth, Scale(30));
        MoveControl(IDC_LABEL_TOPK, drawerX, firstY + row * 2, labelWidth, Scale(24));
        MoveControl(IDC_EDIT_TOPK, editX, firstY + row * 2 - Scale(3), editWidth, Scale(30));
        MoveControl(IDC_LABEL_REPEAT, drawerX, firstY + row * 3, labelWidth, Scale(24));
        MoveControl(IDC_EDIT_REPEAT, editX, firstY + row * 3 - Scale(3), editWidth, Scale(30));
        MoveControl(IDC_LABEL_MAXTOK, drawerX, firstY + row * 4, labelWidth, Scale(24));
        MoveControl(IDC_EDIT_MAXTOK, editX, firstY + row * 4 - Scale(3), editWidth, Scale(30));
        MoveControl(IDC_BTN_APPLY_SETTINGS, drawerX, cy - Scale(66), fieldWidth, Scale(42));
    }

    Invalidate(FALSE);
}

BOOL CMainDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect client;
    GetClientRect(&client);
    const int sidebar = Scale(224);

    pDC->FillSolidRect(client, UI_WINDOW);
    pDC->FillSolidRect(CRect(0, 0, sidebar, client.bottom), UI_SIDEBAR);
    pDC->FillSolidRect(CRect(sidebar, 0, client.right, Scale(70)), UI_CHAT);

    CPen borderPen(PS_SOLID, 1, UI_BORDER);
    CPen* oldPen = pDC->SelectObject(&borderPen);
    pDC->MoveTo(sidebar, 0);
    pDC->LineTo(sidebar, client.bottom);
    pDC->MoveTo(sidebar, Scale(70));
    pDC->LineTo(client.right, Scale(70));

    if (settingsVisible_) {
        const int drawerLeft = client.right - Scale(300);
        pDC->FillSolidRect(CRect(drawerLeft, Scale(70), client.right, client.bottom), UI_SETTINGS);
        pDC->MoveTo(drawerLeft, Scale(70));
        pDC->LineTo(drawerLeft, client.bottom);
    }
    pDC->SelectObject(oldPen);
    return TRUE;
}

HBRUSH CMainDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const int id = pWnd ? pWnd->GetDlgCtrlID() : 0;
    pDC->SetBkMode(TRANSPARENT);

    if (id == IDC_APP_TITLE || id == IDC_APP_SUBTITLE || id == IDC_SIDEBAR_SECTION || id == IDC_SESSION_LIST) {
        pDC->SetTextColor(id == IDC_APP_SUBTITLE || id == IDC_SIDEBAR_SECTION ? UI_SIDEBAR_MUTED : UI_SIDEBAR_TEXT);
        pDC->SetBkColor(UI_SIDEBAR);
        return static_cast<HBRUSH>(sidebarBrush_.GetSafeHandle());
    }

    if (id == IDC_CHAT) {
        pDC->SetTextColor(UI_TEXT);
        pDC->SetBkColor(UI_CHAT);
        return static_cast<HBRUSH>(chatBrush_.GetSafeHandle());
    }

    if (id == IDC_INPUT) {
        pDC->SetTextColor(UI_TEXT);
        pDC->SetBkColor(UI_CONTROL);
        return static_cast<HBRUSH>(controlBrush_.GetSafeHandle());
    }

    if (settingsVisible_ && id >= IDC_SETTINGS_TITLE && id <= IDC_LABEL_MAXTOK) {
        pDC->SetTextColor(id == IDC_SETTINGS_TITLE ? UI_TEXT : UI_MUTED);
        pDC->SetBkColor(UI_SETTINGS);
        return static_cast<HBRUSH>(settingsBrush_.GetSafeHandle());
    }

    if (settingsVisible_ && (nCtlColor == CTLCOLOR_EDIT || nCtlColor == CTLCOLOR_LISTBOX)) {
        pDC->SetTextColor(UI_TEXT);
        pDC->SetBkColor(UI_CONTROL);
        return static_cast<HBRUSH>(controlBrush_.GetSafeHandle());
    }

    pDC->SetTextColor(id == IDC_STATUS || id == IDC_MODEL_PATH ? UI_MUTED : UI_TEXT);
    pDC->SetBkColor(id == IDC_HEADER_TITLE || id == IDC_MODEL_PATH ? UI_CHAT : UI_WINDOW);
    return static_cast<HBRUSH>((id == IDC_HEADER_TITLE || id == IDC_MODEL_PATH) ? chatBrush_.GetSafeHandle() : windowBrush_.GetSafeHandle());
}

void CMainDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    LayoutControls(cx, cy);
}

void CMainDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    CDialogEx::OnGetMinMaxInfo(lpMMI);
    lpMMI->ptMinTrackSize.x = Scale(760);
    lpMMI->ptMinTrackSize.y = Scale(520);
}

void CMainDlg::AppendRichText(const std::wstring& text, COLORREF color, bool bold, int heightTwips)
{
    const long end = chat_.GetTextLength();
    chat_.SetSel(end, end);

    CHARFORMAT format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE | CFM_FACE;
    format.crTextColor = color;
    format.dwEffects = bold ? CFE_BOLD : 0;
    format.yHeight = heightTwips;
    wcscpy_s(format.szFaceName, L"Segoe UI");
    chat_.SetSelectionCharFormat(format);
    chat_.ReplaceSel(text.c_str(), FALSE);
}

void CMainDlg::AppendMessage(const ChatMessage& message)
{
    const bool user = message.role == ChatRole::User;
    AppendRichText(user ? L"你\r\n" : L"LocalSenseNova\r\n", user ? RGB(86, 88, 105) : UI_ACCENT, true, 205);
    AppendRichText(ToWideLossy(message.content) + L"\r\n\r\n", UI_TEXT, false, 220);
}

void CMainDlg::AppendAssistantHeader()
{
    AppendRichText(L"LocalSenseNova\r\n", UI_ACCENT, true, 205);
}

void CMainDlg::OnOK()
{
    // Treat the dialog default action as Send instead of closing the application.
    OnBnClickedSend();
}

void CMainDlg::OnDestroy()
{
    closing_ = true;
    engine_.RequestStop();
    if (worker_.joinable()) worker_.join();
    conversations_.Save(HistoryPath());
    CDialogEx::OnDestroy();
}

void CMainDlg::RefreshSessionList()
{
    sessions_.ResetContent();
    for (size_t i = 0; i < conversations_.Count(); ++i) {
        sessions_.AddString(conversations_.At(i).title.c_str());
    }
    sessions_.SetCurSel(static_cast<int>(conversations_.CurrentIndex()));
}

void CMainDlg::RenderCurrentConversation()
{
    chat_.SetWindowText(L"");
    const auto& messages = conversations_.Current().messages;
    if (messages.empty()) {
        AppendRichText(L"\r\n有什么可以帮忙的？\r\n", UI_TEXT, true, 390);
        AppendRichText(L"加载一个 GGUF 模型，然后开始属于你的本地 AI 对话。\r\n", UI_MUTED, false, 210);
    } else {
        for (const auto& message : messages) AppendMessage(message);
    }
    chat_.SetSel(-1, -1);
    chat_.LineScroll(chat_.GetLineCount());
}

void CMainDlg::AppendChat(const std::wstring& text)
{
    if (closing_) return;
    AppendRichText(text, UI_TEXT, false, 220);
    chat_.SetSel(-1, -1);
    chat_.LineScroll(chat_.GetLineCount());
}

void CMainDlg::AppendUtf8Piece(const std::string& piece)
{
    pendingUtf8_ += piece;
    bool ok = false;
    std::wstring w = Utf8ToWide(pendingUtf8_, &ok);
    if (ok) {
        AppendChat(w);
        pendingUtf8_.clear();
    }
}

void CMainDlg::FlushPendingUtf8()
{
    if (pendingUtf8_.empty()) return;
    AppendChat(ToWideLossy(pendingUtf8_));
    pendingUtf8_.clear();
}

void CMainDlg::UpdateSettingsControls()
{
    SetDlgItemText(IDC_EDIT_SYSTEM, ToWideLossy(settings_.systemPrompt).c_str());

    CString s;
    s.Format(L"%.2f", settings_.temperature);
    SetDlgItemText(IDC_EDIT_TEMP, s);
    s.Format(L"%.2f", settings_.topP);
    SetDlgItemText(IDC_EDIT_TOPP, s);
    s.Format(L"%d", settings_.topK);
    SetDlgItemText(IDC_EDIT_TOPK, s);
    s.Format(L"%.2f", settings_.repeatPenalty);
    SetDlgItemText(IDC_EDIT_REPEAT, s);
    s.Format(L"%d", settings_.maxTokens);
    SetDlgItemText(IDC_EDIT_MAXTOK, s);
}

bool CMainDlg::ReadSettingsControls(bool showErrors)
{
    CString system, temp, topP, topK, repeat, maxTok;
    GetDlgItemText(IDC_EDIT_SYSTEM, system);
    GetDlgItemText(IDC_EDIT_TEMP, temp);
    GetDlgItemText(IDC_EDIT_TOPP, topP);
    GetDlgItemText(IDC_EDIT_TOPK, topK);
    GetDlgItemText(IDC_EDIT_REPEAT, repeat);
    GetDlgItemText(IDC_EDIT_MAXTOK, maxTok);

    AppSettings next = settings_;
    next.systemPrompt = WideToUtf8(std::wstring(system.GetString()));
    next.temperature = static_cast<float>(_wtof(temp));
    next.topP = static_cast<float>(_wtof(topP));
    next.topK = _wtoi(topK);
    next.repeatPenalty = static_cast<float>(_wtof(repeat));
    next.maxTokens = _wtoi(maxTok);

    const bool valid =
        next.temperature >= 0.0f && next.temperature <= 2.0f &&
        next.topP >= 0.0f && next.topP <= 1.0f &&
        next.topK >= 0 && next.topK <= 500 &&
        next.repeatPenalty >= 0.5f && next.repeatPenalty <= 2.0f &&
        next.maxTokens >= 1 && next.maxTokens <= 4096;

    if (!valid) {
        if (showErrors) {
            AfxMessageBox(L"参数范围：Temperature 0~2，Top-P 0~1，Top-K 0~500，Repeat Penalty 0.5~2，Max Tokens 1~4096。", MB_ICONWARNING);
        }
        return false;
    }

    settings_ = std::move(next);
    return true;
}

void CMainDlg::ApplyPreset(int index)
{
    switch (index) {
    case 1:
        settings_.systemPrompt = "你是一名严谨的C++编程助手。优先给出可编译代码，并解释关键设计、边界条件和复杂度。";
        settings_.temperature = 0.20f;
        settings_.topP = 0.90f;
        settings_.topK = 40;
        settings_.repeatPenalty = 1.05f;
        break;
    case 2:
        settings_.systemPrompt = "你是一名专业中英翻译助手。准确翻译用户输入，保留术语、数字、格式和语气，不添加无关内容。";
        settings_.temperature = 0.20f;
        settings_.topP = 0.90f;
        settings_.topK = 40;
        settings_.repeatPenalty = 1.05f;
        break;
    case 3:
        settings_.systemPrompt = "你是一名中文公文写作助手。语言规范、简洁、正式，结构清楚，避免空泛套话。";
        settings_.temperature = 0.40f;
        settings_.topP = 0.90f;
        settings_.topK = 40;
        settings_.repeatPenalty = 1.10f;
        break;
    default:
        settings_.systemPrompt = "你是一名严谨、友好的中文助手。回答要准确、清楚，必要时给出步骤。";
        settings_.temperature = 0.80f;
        settings_.topP = 0.95f;
        settings_.topK = 40;
        settings_.repeatPenalty = 1.10f;
        break;
    }
    UpdateSettingsControls();
}

void CMainDlg::SetBusyUi(bool busy, const wchar_t* status)
{
    GetDlgItem(IDC_BTN_SEND)->EnableWindow(!busy);
    GetDlgItem(IDC_BTN_STOP)->EnableWindow(busy);
    GetDlgItem(IDC_BTN_LOAD_MODEL)->EnableWindow(!busy);
    GetDlgItem(IDC_BTN_NEW_SESSION)->EnableWindow(!busy);
    GetDlgItem(IDC_BTN_DELETE_SESSION)->EnableWindow(!busy);
    GetDlgItem(IDC_SESSION_LIST)->EnableWindow(!busy);
    GetDlgItem(IDC_BTN_APPLY_SETTINGS)->EnableWindow(!busy);
    GetDlgItem(IDC_COMBO_PRESET)->EnableWindow(!busy);
    if (status) SetDlgItemText(IDC_STATUS, status);
}

std::wstring CMainDlg::HistoryPath() const
{
    wchar_t buf[MAX_PATH]{};
    const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p = n > 0 ? std::filesystem::path(std::wstring(buf, n)) : std::filesystem::current_path();
    p = p.parent_path() / L"LocalSenseNovaHistory.dat";
    return p.wstring();
}

void CMainDlg::OnBnClickedLoadModel()
{
    if (worker_.joinable()) worker_.join();

    CFileDialog dlg(TRUE, L"gguf", nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        L"GGUF 模型 (*.gguf)|*.gguf|所有文件 (*.*)|*.*||", this);
    if (dlg.DoModal() != IDOK) return;

    const std::wstring path = dlg.GetPathName().GetString();
    const std::string path8 = WideToUtf8(path);

    SetBusyUi(true, L"状态：正在加载模型，请稍候……");
    GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE); // model loading itself is not cancellable
    worker_ = std::thread([this, path, path8]() {
        auto* result = new ModelLoadResult();
        result->path = path;
        result->ok = engine_.LoadModel(path8, settings_.contextSize, result->error);
        if (result->ok) result->description = engine_.ModelDescription();
        if (!closing_) {
            PostMessage(WM_MODEL_LOADED, 0, reinterpret_cast<LPARAM>(result));
        } else {
            delete result;
        }
    });
}

void CMainDlg::OnBnClickedSend()
{
    if (!engine_.IsLoaded()) {
        AfxMessageBox(L"请先点击“选择并加载 GGUF”。", MB_ICONINFORMATION);
        return;
    }
    if (engine_.IsBusy()) return;
    if (!ReadSettingsControls(true)) return;
    if (worker_.joinable()) worker_.join();

    CString input;
    input_.GetWindowText(input);
    input.Trim();
    if (input.IsEmpty()) return;

    const std::string userText = WideToUtf8(std::wstring(input.GetString()));
    const size_t convIndex = conversations_.CurrentIndex();
    conversations_.AddMessage(convIndex, ChatRole::User, userText);
    conversations_.AutoTitleFromFirstUser(convIndex);
    RefreshSessionList();
    RenderCurrentConversation();
    AppendAssistantHeader();
    pendingUtf8_.clear();
    input_.SetWindowText(L"");

    const auto history = conversations_.At(convIndex).messages;
    const AppSettings settingsCopy = settings_;

    SetBusyUi(true, L"状态：生成中……");

    worker_ = std::thread([this, convIndex, history, settingsCopy]() {
        auto* result = new GenerationResult();
        result->conversationIndex = convIndex;
        result->ok = engine_.Generate(
            history,
            settingsCopy,
            [this](const std::string& piece) {
                if (closing_) return;
                auto* heapPiece = new std::string(piece);
                if (!PostMessage(WM_STREAM_TOKEN, 0, reinterpret_cast<LPARAM>(heapPiece))) {
                    delete heapPiece;
                }
            },
            result->response,
            result->stats,
            result->error);

        if (!closing_) {
            PostMessage(WM_GENERATION_DONE, 0, reinterpret_cast<LPARAM>(result));
        } else {
            delete result;
        }
    });
}

void CMainDlg::OnBnClickedStop()
{
    engine_.RequestStop();
    SetDlgItemText(IDC_STATUS, L"状态：正在停止……");
}

void CMainDlg::OnBnClickedNewSession()
{
    conversations_.NewConversation();
    RefreshSessionList();
    RenderCurrentConversation();
    input_.SetFocus();
}

void CMainDlg::OnBnClickedDeleteSession()
{
    const int sel = sessions_.GetCurSel();
    if (sel == LB_ERR) return;
    conversations_.DeleteConversation(static_cast<size_t>(sel));
    RefreshSessionList();
    RenderCurrentConversation();
}

void CMainDlg::OnBnClickedApplySettings()
{
    if (ReadSettingsControls(true)) {
        SetDlgItemText(IDC_STATUS, L"状态：参数已应用；下一次生成生效");
    }
}

void CMainDlg::OnBnClickedToggleSettings()
{
    ShowSettingsPanel(!settingsVisible_);
    CRect client;
    GetClientRect(&client);
    LayoutControls(client.Width(), client.Height());
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void CMainDlg::OnCbnSelchangePreset()
{
    ApplyPreset(presets_.GetCurSel());
    SetDlgItemText(IDC_STATUS, L"状态：已套用预设");
}

void CMainDlg::OnLbnSelchangeSession()
{
    const int sel = sessions_.GetCurSel();
    if (sel == LB_ERR) return;
    if (conversations_.SetCurrent(static_cast<size_t>(sel))) {
        RenderCurrentConversation();
    }
}

LRESULT CMainDlg::OnStreamToken(WPARAM, LPARAM lParam)
{
    std::unique_ptr<std::string> piece(reinterpret_cast<std::string*>(lParam));
    if (!closing_ && piece) AppendUtf8Piece(*piece);
    return 0;
}

LRESULT CMainDlg::OnGenerationDone(WPARAM, LPARAM lParam)
{
    std::unique_ptr<GenerationResult> result(reinterpret_cast<GenerationResult*>(lParam));
    if (worker_.joinable()) worker_.join();
    if (!result) return 0;

    FlushPendingUtf8();
    AppendChat(L"\r\n\r\n");

    if (!result->response.empty() && result->conversationIndex < conversations_.Count()) {
        conversations_.AddMessage(result->conversationIndex, ChatRole::Assistant, result->response);
    }

    std::wostringstream ss;
    if (result->ok) {
        ss << L"状态：完成 | Prompt " << result->stats.promptTokens
           << L" tokens | 生成 " << result->stats.generatedTokens
           << L" tokens | " << std::fixed << std::setprecision(2)
           << result->stats.tokensPerSecond << L" tok/s";
    } else if (result->error == "已停止。") {
        ss << L"状态：已停止 | 已生成 " << result->stats.generatedTokens << L" tokens";
    } else {
        ss << L"状态：失败 - " << ToWideLossy(result->error);
        if (!result->error.empty()) {
            AppendChat(L"[错误] " + ToWideLossy(result->error) + L"\r\n\r\n");
        }
    }

    SetBusyUi(false, ss.str().c_str());
    conversations_.Save(HistoryPath());
    return 0;
}

LRESULT CMainDlg::OnModelLoaded(WPARAM, LPARAM lParam)
{
    std::unique_ptr<ModelLoadResult> result(reinterpret_cast<ModelLoadResult*>(lParam));
    if (worker_.joinable()) worker_.join();
    if (!result) return 0;

    if (result->ok) {
        SetDlgItemText(IDC_MODEL_PATH, result->path.c_str());
        std::wstring desc = ToWideLossy(result->description);
        if (desc.size() > 90) desc = desc.substr(0, 90) + L"…";
        SetBusyUi(false, (L"状态：模型加载成功\r\n" + desc).c_str());
    } else {
        SetBusyUi(false, (L"状态：模型加载失败 - " + ToWideLossy(result->error)).c_str());
        AfxMessageBox((L"模型加载失败：\n" + ToWideLossy(result->error)).c_str(), MB_ICONERROR);
    }
    return 0;
}
