#include "resource.h"
#include <Windows.h>
#include <commctrl.h>
#include <iostream>

#define APP_NAME L"MouseGuard"
#define WM_USER_SHELLICON WM_USER + 1
#define WM_TASKBAR_CREATE RegisterWindowMessage(L"TaskbarCreated")

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFILE WIDE1(__FILE__)
#define WFUNCTION WIDE1(__FUNCTION__)

const int MONITOR_WIDTH = GetSystemMetrics(SM_CXSCREEN);
const float X_UNIT = 65536.0f / MONITOR_WIDTH;
const float Y_UNIT = 65536.0f / GetSystemMetrics(SM_CYSCREEN);

HHOOK gMouseHookHandle;
HWND gHwnd;
HINSTANCE gInstance;
NOTIFYICONDATA gTrayIconData;

void debugLog(const wchar_t *file, int line, const wchar_t *func, const wchar_t *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    wchar_t inner[4096];
    vswprintf_s(inner, sizeof(inner), fmt, args);
    va_end(args);

    wchar_t outer[4096 + 1024];
    swprintf_s(outer, sizeof(outer), L"[%s] %s:%d (%s) %s", APP_NAME, file, line, func, inner);

    OutputDebugStringW(outer);
}
#define DEBUG(fmt, ...) debugLog(WFILE, __LINE__, WFUNCTION, fmt, __VA_ARGS__)

void showWarning(const wchar_t *file, int line, const wchar_t *func, const wchar_t *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    wchar_t inner[4096];
    vswprintf_s(inner, sizeof(inner), fmt, args);
    va_end(args);

    wchar_t outer[4096 + 1024];
    swprintf_s(outer, sizeof(outer), L"[%s] %s:%d (%s) %s", APP_NAME, file, line, func, inner);

    MessageBoxW(0, outer, APP_NAME, MB_ICONWARNING | MB_OK);
}
#define WARNING(fmt, ...) showWarning(WFILE, __LINE__, WFUNCTION, fmt, __VA_ARGS__)

LRESULT __stdcall lowLevelMouseProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    static int prevX = 0;
    if (nCode == 0) {
        if (wParam == WM_MOUSEMOVE) {
            MSLLHOOKSTRUCT *lp = (MSLLHOOKSTRUCT *)lParam;
            const int x = lp->pt.x;
            if (prevX < MONITOR_WIDTH && x >= MONITOR_WIDTH && x - prevX < 5) {
                INPUT input;
                input.type = INPUT_MOUSE;
                input.mi.mouseData = 0;
                input.mi.dx = (int)(X_UNIT * (MONITOR_WIDTH - 1) - 1);
                input.mi.dy = (int)(Y_UNIT * lp->pt.y - 1);
                input.mi.time = 0;
                input.mi.dwExtraInfo = 0;
                input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

                if (SendInput(1, &input, sizeof(INPUT)) > 0) {
                    prevX = x;
                    return 1; // mouse handled
                }
                WARNING(L"SendInput: %s", GetLastError());
            }
            prevX = x;
            break;
        }
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
}

int cleanup()
{
    DEBUG(L"cleanup");
    if (gMouseHookHandle != NULL) {
        if (!UnhookWindowsHookEx(gMouseHookHandle)) {
            WARNING(L"UnHookWindowsHookEx failed");
            return -1;
        }
        gMouseHookHandle = NULL;
    }
    return 0;
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // cannot use in switch because WM_TASKBAR_CREATE is not a constant
    if (message == WM_TASKBAR_CREATE) {
        // taskbar has been recreated (Explorer crashed?)
        if (!Shell_NotifyIcon(NIM_ADD, &gTrayIconData)) {
            WARNING(L"Shell_NotifyIcon failed");
            DestroyWindow(gHwnd);
            return -1;
        }
    }

    switch (message) {
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &gTrayIconData);
        PostQuitMessage(0);
        break;
    case WM_USER_SHELLICON:
        switch (LOWORD(lParam)) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            HMENU menu = LoadMenu(gInstance, MAKEINTRESOURCE(IDR_POPUP_MENU));
            if (!hMenu) {
                WARNING("LoadMenu failed");
                return -1;
            }

            HMENU submenu = GetSubMenu(menu, 0);
            if (!submenu) {
                DestroyMenu(menu);
                WARNING("GetSubMenu failed");
                return -1;
            }

            // Display menu
            POINT pos;
            GetCursorPos(&pos);
            SetForegroundWindow(gHwnd);
            TrackPopupMenu(submenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, pos.x, pos.y, 0, gHwnd, NULL);
            SendMessage(gHwnd, WM_NULL, 0, 0);

            DestroyMenu(menu);
        } break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(gHwnd);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_POPUP_EXIT:
            DestroyWindow(gHwnd);
            break;
        }
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Check for running instance
    const HANDLE mutex = CreateMutex(NULL, FALSE, L"{516AE336-EFBB-4EE3-85FF-A7EF650C1D72}");
    if (mutex == NULL) {
        switch (GetLastError()) {
        case ERROR_ALREADY_EXISTS:
            WARNING(L"CreateMutex: ERROR_ALREADY_EXISTS");
            break;
        case ERROR_INVALID_HANDLE:
            WARNING(L"CreateMutex: ERROR_INVALID_HANDLE");
            break;
        case ERROR_ACCESS_DENIED:
            WARNING(L"CreateMutex: ERROR_ACCESS_DENIED");
            break;
        }
        return 0;
    }

    // Save handles
    gInstance = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_UPDOWN_CLASS | ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&iccex)) {
        WARNING(0, L"InitCommonControlsEx failed");
        return -1;
    }

    // Create window
    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_TRAYICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = APP_NAME;
    wc.hIconSm = LoadIcon(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_TRAYICON));
    if (!RegisterClassEx(&wc)) {
        WARNING(L"RegisterClassEx failed");
        return -1;
    }
    gHwnd = CreateWindowEx(WS_EX_CLIENTEDGE, APP_NAME, APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                           NULL, NULL, hInstance, NULL);
    if (gHwnd == NULL) {
        WARNING(L"CreateWindowEx failed");
        return -1;
    }

    // Setup icon data
    gTrayIconData.cbSize = sizeof(NOTIFYICONDATA);
    gTrayIconData.hWnd = (HWND)gHwnd;
    gTrayIconData.uID = IDI_TRAYICON;
    gTrayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    wcscpy(gTrayIconData.szTip, APP_NAME);
    gTrayIconData.hIcon = LoadIcon(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_TRAYICON));
    gTrayIconData.uCallbackMessage = WM_USER_SHELLICON;

    // Display tray icon
    if (!Shell_NotifyIcon(NIM_ADD, &gTrayIconData)) {
        WARNING(L"Failed creating tray icon");
        return -1;
    }

    // Enable mouse hook
    gMouseHookHandle = SetWindowsHookEx(WH_MOUSE_LL, &lowLevelMouseProc, NULL, 0);
    if (gMouseHookHandle == NULL) {
        WARNING(L"SetWindowsHookEx failed: %s", GetLastError());
        return -1;
    }

    // Start message loop
    BOOL ret;
    MSG msg;
    while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (ret == -1) {
            WARNING(L"GetMessage: ret == -1");
            cleanup();
            return -1;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return cleanup();
}
