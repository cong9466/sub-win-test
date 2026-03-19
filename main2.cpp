#include <windows.h>

// 全局变量
HINSTANCE g_hInst = NULL;
HWND g_hMainWnd = NULL; // W1 句柄
HWND g_hExtWnd = NULL;  // 外部进程的 W2 句柄

HHOOK g_hMouseHook = NULL; // 全局鼠标钩子
bool g_isDragging = false;
POINT g_ptOffset = { 0, 0 }; // 鼠标相对于 W2 左上角的偏移

// =========================================================================
// 全局低级鼠标钩子（用于在外部捕获对 W2 的拖拽操作）
// =========================================================================
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && g_hExtWnd != NULL) {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;

        if (wParam == WM_LBUTTONDOWN) {
            // 获取当前鼠标点击位置的窗口
            HWND hHit = WindowFromPoint(pMouse->pt);
            
            // 检查点击的窗口是否是 W2，或者是 W2 内部的子控件
            HWND hCheck = hHit;
            while (hCheck != NULL && hCheck != g_hExtWnd) {
                hCheck = GetParent(hCheck);
            }

            // 如果点击确实发生在 W2 内部
            if (hCheck == g_hExtWnd) {
                g_isDragging = true;
                
                // 计算鼠标相对于 W2 左上角的偏移量（屏幕坐标）
                RECT rcW2;
                GetWindowRect(g_hExtWnd, &rcW2);
                g_ptOffset.x = pMouse->pt.x - rcW2.left;
                g_ptOffset.y = pMouse->pt.y - rcW2.top;
            }
        }
        else if (wParam == WM_MOUSEMOVE && g_isDragging) {
            // 将当前屏幕鼠标坐标转换为 W1 客户区坐标
            POINT ptClient = pMouse->pt;
            ScreenToClient(g_hMainWnd, &ptClient);

            // 获取 W1 客户区尺寸和 W2 尺寸
            RECT rcParent, rcW2;
            GetClientRect(g_hMainWnd, &rcParent);
            GetWindowRect(g_hExtWnd, &rcW2);
            int width = rcW2.right - rcW2.left;
            int height = rcW2.bottom - rcW2.top;

            // 计算 W2 的新坐标（相对于 W1 客户区）
            int newX = ptClient.x - g_ptOffset.x;
            int newY = ptClient.y - g_ptOffset.y;

            // 边界限制检测（Clamp）
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX + width > rcParent.right) newX = rcParent.right - width;
            if (newY + height > rcParent.bottom) newY = rcParent.bottom - height;

            // 跨进程移动 W2 窗口
            SetWindowPos(g_hExtWnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else if (wParam == WM_LBUTTONUP && g_isDragging) {
            g_isDragging = false;
        }
    }
    // 将消息传递给下一个钩子或目标窗口，不要吞掉消息，否则 W2 无法正常响应点击
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam); 
}

// =========================================================================
// 核心接口：将外部进程窗口 W2 挂载到 W1 内
// =========================================================================
void AttachExternalProcessWindow(HWND hMain, HWND hW2) {
    // 1. 去除弹出属性，增加子窗口属性 (跨进程设置 Style 可能会受限，但尽量做)
    LONG_PTR style = GetWindowLongPtr(hW2, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME);
    style |= WS_CHILD;
    SetWindowLongPtr(hW2, GWL_STYLE, style);

    // 2. 跨进程 SetParent (核心：实现区域裁剪)
    SetParent(hW2, hMain);

    // 3. 安装低级鼠标钩子来接管拖拽
    if (g_hMouseHook == NULL) {
        g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    }

    // 4. 设定初始位置
    SetWindowPos(hW2, NULL, 0, 0, 200, 150, SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow(hW2, SW_SHOW);
}

// =========================================================================
// W1（主窗口）的消息处理过程
// =========================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindow(L"BUTTON", L"挂载外部记事本(测试)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 180, 30, hWnd, (HMENU)1001, g_hInst, NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001) {
            if (g_hExtWnd == NULL) {
                // 【测试逻辑】尝试寻找当前打开的记事本窗口作为 W2
                // 测试前请先手动打开一个 Windows 记事本
                g_hExtWnd = FindWindow(L"Notepad", NULL); 
                
                if (g_hExtWnd) {
                    AttachExternalProcessWindow(hWnd, g_hExtWnd);
                } else {
                    MessageBox(hWnd, L"请先手动打开一个记事本(Notepad)作为外部进程窗口进行测试！", L"提示", MB_OK);
                }
            }
        }
        break;
    }
    case WM_SIZE: {
        // 应对 W1 缩小导致的 W2 超出边界，自动挤回可视区
        if (g_hExtWnd) {
            RECT rcParent, rcW2;
            GetClientRect(hWnd, &rcParent);
            GetWindowRect(g_hExtWnd, &rcW2);
            
            POINT pt = { rcW2.left, rcW2.top };
            ScreenToClient(hWnd, &pt);

            int width = rcW2.right - rcW2.left;
            int height = rcW2.bottom - rcW2.top;
            int newX = pt.x, newY = pt.y;
            bool needMove = false;

            if (newX + width > rcParent.right)  { newX = rcParent.right - width; needMove = true; }
            if (newY + height > rcParent.bottom){ newY = rcParent.bottom - height; needMove = true; }
            if (newX < 0) { newX = 0; needMove = true; }
            if (newY < 0) { newY = 0; needMove = true; }

            if (needMove) {
                SetWindowPos(g_hExtWnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        break;
    }
    case WM_DESTROY:
        // 退出前务必卸载钩子
        if (g_hMouseHook) {
            UnhookWindowsHookEx(g_hMouseHook);
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// =========================================================================
// 程序入口
// =========================================================================
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"W1_MainClass";
    RegisterClassExW(&wcex);

    g_hMainWnd = CreateWindowW(L"W1_MainClass", L"大窗口 W1 (跨进程版)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}