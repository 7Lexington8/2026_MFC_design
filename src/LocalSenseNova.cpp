#include "pch.h"
#include "LocalSenseNova.h"
#include "MainDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CLocalSenseNovaApp theApp;

BOOL CLocalSenseNovaApp::InitInstance()
{
    // Keep layout coordinates, fonts and the actual window bounds in the same
    // coordinate space on high-DPI displays.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    CWinApp::InitInstance();
    AfxEnableControlContainer();
    AfxInitRichEdit2();

    // The dialog resource uses the RichEdit50W window class, which is
    // registered by Msftedit.dll rather than the RichEdit 2.0 runtime.
    if (!::LoadLibraryW(L"Msftedit.dll")) {
        AfxMessageBox(L"无法加载系统组件 Msftedit.dll，程序不能创建主窗口。", MB_ICONERROR);
        return FALSE;
    }

    CMainDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();
    return FALSE;
}
