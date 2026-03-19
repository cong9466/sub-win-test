#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

// 链接通用控件库（用于安全的子类化函数）
#pragma comment(lib, "comctl32.lib")

// 全局变量
HINSTANCE g_hInst = NULL;
HWND g_hMainWnd = NULL; // W1 句柄
HWND g_hExtWnd = NULL;  // W2 句柄

// 用于记录拖拽状态的结构体
struct DragState {
    bool isDragging = false;
    POINT ptOffset = { 0, 0 };
};

// =========================================================================
// W2 的子类化过程（拦截外部 W2 的消息，实现拖拽和边界限制）
// =========================================================================
LRESULT CALLBACK W2SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    DragState* state = reinterpret_cast<DragState*>(dwRefData);

    switch (uMsg) {
    case WM_LBUTTONDOWN: {
        // 记录鼠标按下时，相对于 W2 左上角的偏移量
        state->isDragging = true;
        state->ptOffset.x = GET_X_LPARAM(lParam);
        state->ptOffset.y = GET_Y_LPARAM(lParam);
        SetCapture(hWnd); // 捕获鼠标，防止拖拽过快鼠标移出窗口丢失事件
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (state->isDragging) {
            POINT ptMouse;
            GetCursorPos(&ptMouse); // 获取全局鼠标坐标

            HWND hParent = GetParent(hWnd); // 获取 W1 句柄
            ScreenToClient(hParent, &ptMouse); // 将鼠标坐标转换为 W1 客户区坐标

            RECT rcParent;
            GetClientRect(hParent, &rcParent); // 获取 W1 客户区大小

            RECT rcWnd;
            GetWindowRect(hWnd, &rcWnd); // 获取 W2 当前大小
            int width = rcWnd.right - rcWnd.left;
            int height = rcWnd.bottom - rcWnd.top;

            // 计算 W2 的新坐标
            int newX = ptMouse.x - state->ptOffset.x;
            int newY = ptMouse.y - state->ptOffset.y;

            // 核心要求：限制 W2 不能超出 W1 的客户区（边界碰撞检测）
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX + width > rcParent.right) newX = rcParent.right - width;
            if (newY + height > rcParent.bottom) newY = rcParent.bottom - height;

            // 移动 W2
            SetWindowPos(hWnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (state->isDragging) {
            state->isDragging = false;
            ReleaseCapture(); // 释放鼠标捕获
        }
        return 0;
    }
    case WM_NCDESTROY: {
        // 窗口销毁时清理分配的内存并移除子类化
        delete state;
        RemoveWindowSubclass(hWnd, W2SubclassProc, uIdSubclass);
        break;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// =========================================================================
// 核心接口：将外部窗口 W2 挂载到 W1 内
// =========================================================================
void AttachExternalWindow(HWND hMain, HWND hW2) {
    // 1. 去除弹出/标题栏属性，强制转换为子窗口样式
    LONG_PTR style = GetWindowLongPtr(hW2, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME);
    style |= WS_CHILD;
    SetWindowLongPtr(hW2, GWL_STYLE, style);

    // 2. 设置 W1 为其父窗口
    SetParent(hW2, hMain);

    // 3. 注入子类化（实现拖拽功能）
    DragState* dragState = new DragState();
    SetWindowSubclass(hW2, W2SubclassProc, 1, reinterpret_cast<DWORD_PTR>(dragState));

    // 4. 调整初始位置并显示
    SetWindowPos(hW2, NULL, 20, 50, 200, 150, SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow(hW2, SW_SHOW);
}

// =========================================================================
// 模拟外部创建 W2 窗口的函数
// =========================================================================
HWND SimulateCreateExternalWindow() {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc; // 外部窗口有自己的处理逻辑，这里模拟默认
    wc.hInstance = g_hInst;
    wc.hbrBackground = CreateSolidBrush(RGB(100, 149, 237)); // 浅蓝色背景以示区分
    wc.lpszClassName = L"ExternalSimulatedClass";
    RegisterClass(&wc);

    // 外部创建时，往往是独立窗口（无父窗口）
    HWND hWnd = CreateWindow(L"ExternalSimulatedClass", L"", 
        WS_POPUP | WS_VISIBLE, 0, 0, 200, 150, NULL, NULL, g_hInst, NULL);
    return hWnd;
}

// =========================================================================
// W1（主窗口）的消息处理过程
// =========================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // 创建一个按钮，用于触发加载 W2
        CreateWindow(L"BUTTON", L"模拟外部加载 W2",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 10, 150, 30, hWnd, (HMENU)1001, g_hInst, NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001) { // 按钮点击事件
            if (g_hExtWnd == NULL) {
                // 1. 模拟外部创建了 W2，此时我们只拿到了句柄
                g_hExtWnd = SimulateCreateExternalWindow();
                
                // 2. 调用我们的核心接口，传入句柄进行挂载和限制
                AttachExternalWindow(hWnd, g_hExtWnd);
            }
        }
        break;
    }
    case WM_SIZE: {
        // 健壮性处理：如果 W1（主窗口）被缩小，导致 W2 超出边界，需要把 W2 挤回来
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

    g_hMainWnd = CreateWindowW(L"W1_MainClass", L"大窗口 W1", WS_OVERLAPPEDWINDOW,
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