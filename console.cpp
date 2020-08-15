#include <Windows.h>
#include <iostream>
#include <signal.h>

using namespace std;

#define MONITOR_WIDTH 1920

HHOOK handle;
const float xUnit = 65536.0f / GetSystemMetrics(SM_CXSCREEN);
const float yUnit = 65536.0f / GetSystemMetrics(SM_CYSCREEN);

int prevX = 0;
// int prevY = 0;

bool mousePressed = false;

LRESULT __stdcall lowLevelMouseProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    if (nCode == 0) {
        switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            mousePressed = true;
            break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            mousePressed = false;
            break;
        case WM_MOUSEMOVE:
            MSLLHOOKSTRUCT *lp = (MSLLHOOKSTRUCT *)lParam;
            const int x = lp->pt.x;
            if (!mousePressed && prevX < MONITOR_WIDTH && x >= MONITOR_WIDTH) {
                if (x - prevX < 5) {
                    INPUT input;
                    input.type = INPUT_MOUSE;
                    input.mi.mouseData = 0;
                    input.mi.dx = (int)(xUnit * (MONITOR_WIDTH - 1) - 1);
                    input.mi.dy = (int)(yUnit * lp->pt.y - 1);
                    input.mi.time = 0;
                    input.mi.dwExtraInfo = 0;
                    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

                    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
                        cout << "SendInput error = " << GetLastError() << endl;
                    }

                    return 1;
                }
            }
            prevX = x;
            // prevY = y;
            break;
        }
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
}

int cleanup()
{
    cout << "cleanup" << endl;
    if (!UnhookWindowsHookEx(handle)) {
        cout << "Error: UnHookWindowsHookEx" << endl;
        return -1;
    }
    return 0;
}

void exitHandler(int signal)
{
    cleanup();
    exit(signal);
}

int main()
{
    handle = SetWindowsHookEx(WH_MOUSE_LL, &lowLevelMouseProc, NULL, 0);
    if (handle == NULL) {
        cout << "SetWindowsHookEx failed: " << GetLastError() << endl;
        return -1;
    }

    signal(SIGINT, exitHandler);

    BOOL ret;
    MSG msg;
    while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (ret == -1) {
            cout << "GetMessage: ret == -1" << endl;
            cleanup();
            return -1;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return cleanup();
}
