#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <vector>
#include <cmath>
#include <algorithm>

#pragma comment(lib,"dwmapi.lib")

struct V2 {
    float x;
    float y;
};

struct Pocket {
    V2 mid;
    float halfWidth;
    bool side;
};

static HWND hwnd = nullptr;
static bool edit = true;

static V2 cuePos;
static float ghostR = 16.0f;

static bool dragCue = false;
static int dragPocket = -1;

static std::vector<Pocket> pockets;

static float Dist2(const V2& a, const V2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static void DrawCircle(HDC h, V2 p, float r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HBRUSH o = (HBRUSH)SelectObject(h, b);
    Ellipse(h,
        (int)(p.x - r),
        (int)(p.y - r),
        (int)(p.x + r),
        (int)(p.y + r));
    SelectObject(h, o);
    DeleteObject(b);
}

static void DrawLine(HDC h, V2 a, V2 b, int w, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, w, c);
    HPEN o = (HPEN)SelectObject(h, p);
    MoveToEx(h, (int)a.x, (int)a.y, nullptr);
    LineTo(h, (int)b.x, (int)b.y);
    SelectObject(h, o);
    DeleteObject(p);
}

static void ApplyEditStyle() {
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_TOPMOST | WS_EX_LAYERED;
    if (edit) ex &= ~WS_EX_TRANSPARENT;
    else      ex |= WS_EX_TRANSPARENT;
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex);
}

static void Paint(HWND w) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(w, &ps);

    RECT r;
    GetClientRect(w, &r);

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, r.right, r.bottom);
    HGDIOBJ old = SelectObject(mem, bmp);

    HBRUSH clear = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(mem, &r, clear);
    DeleteObject(clear);

    for (const auto& p : pockets) {
        V2 a, b;

        if (p.side) {
            a = { p.mid.x - p.halfWidth, p.mid.y };
            b = { p.mid.x + p.halfWidth, p.mid.y };
        }
        else {
            float dir = (p.mid.x < r.right * 0.5f) ? 1.0f : -1.0f;
            a = p.mid;
            b = { p.mid.x + dir * p.halfWidth, p.mid.y };
        }

        DrawLine(mem, cuePos, p.mid, 2, RGB(0, 255, 0));
        DrawLine(mem, a, b, 2, RGB(0, 140, 255));
        DrawCircle(mem, p.mid, 5.0f, RGB(255, 255, 255));
    }

    DrawCircle(mem, cuePos, ghostR, RGB(255, 255, 255));

    BitBlt(hdc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);

    EndPaint(w, &ps);
}

LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) PostQuitMessage(0);

        if (wp == VK_F5) {
            edit = !edit;
            ApplyEditStyle();
        }

        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
            if (wp == VK_OEM_PLUS || wp == VK_ADD) ghostR += 1.0f;
            if (wp == VK_OEM_MINUS || wp == VK_SUBTRACT)
                ghostR = std::max(4.0f, ghostR - 1.0f);
        }

        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            float delta = (wp == VK_OEM_PLUS || wp == VK_ADD) ? 2.0f : -2.0f;
            for (auto& p : pockets)
                p.halfWidth = std::clamp(p.halfWidth + delta, 10.0f, 300.0f);
        }

        InvalidateRect(w, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        if (!edit) return 0;

        V2 mpos{
            (float)GET_X_LPARAM(lp),
            (float)GET_Y_LPARAM(lp)
        };

        if (Dist2(mpos, cuePos) <= ghostR * ghostR) {
            dragCue = true;
            SetCapture(w);
            return 0;
        }

        for (int i = 0; i < (int)pockets.size(); i++) {
            if (Dist2(mpos, pockets[i].mid) <= 100.0f) {
                dragPocket = i;
                SetCapture(w);
                return 0;
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!edit) return 0;

        V2 mpos{
            (float)GET_X_LPARAM(lp),
            (float)GET_Y_LPARAM(lp)
        };

        if (dragCue) cuePos = mpos;
        if (dragPocket != -1) pockets[dragPocket].mid = mpos;

        InvalidateRect(w, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP:
        ReleaseCapture();
        dragCue = false;
        dragPocket = -1;
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        Paint(w);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"overlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    int W = GetSystemMetrics(SM_CXSCREEN);
    int H = GetSystemMetrics(SM_CYSCREEN);

    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"overlay", L"",
        WS_POPUP,
        0, 0, W, H,
        nullptr, nullptr, hInst, nullptr
    );

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    MARGINS m = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &m);

    cuePos = {
        (float)W * 0.5f,
        (float)H * 0.5f
    };

    pockets = {
        {{120.0f, 120.0f}, 60.0f, false},
        {{(float)W * 0.5f, 100.0f}, 55.0f, true},
        {{(float)W - 120.0f, 120.0f}, 60.0f, false},
        {{120.0f, (float)H - 120.0f}, 60.0f, false},
        {{(float)W * 0.5f, (float)H - 100.0f}, 55.0f, true},
        {{(float)W - 120.0f, (float)H - 120.0f}, 60.0f, false},
    };

    ShowWindow(hwnd, SW_SHOW);
    ApplyEditStyle();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
