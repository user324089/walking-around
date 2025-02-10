#include <chrono>
#include <numbers>
#include <source_location>
#include <fstream>
#include <string>
#include <algorithm>
#include <array>
#include <limits>

#include "Game.hpp"

#include "Utility.hpp"


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {

    constexpr static TCHAR class_name[] = TEXT("my class");

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassEx(&wc)) {
        return 1;
    }

    HWND hwnd = CreateWindowEx(0,                            // window extra style
                               class_name,                   // window class
                               TEXT("Walking around"),       // window title
                               WS_OVERLAPPEDWINDOW,          // window style
                               CW_USEDEFAULT, CW_USEDEFAULT, // position
                               CW_USEDEFAULT, CW_USEDEFAULT, // size
                               nullptr,                      // parent window
                               nullptr,                      // menu
                               hInstance,                    // instance
                               nullptr                       // application data
    );

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};

    do {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message != WM_QUIT) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            msg.message = WM_USER;
            DispatchMessage(&msg);
            InvalidateRect(hwnd, nullptr, false);
        }
    } while (msg.message != WM_QUIT);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static Game pnt;

    try {
        switch (uMsg) {
            case WM_CREATE:
                pnt.init(hwnd);
                break;
            case WM_SIZE:
                pnt.resize(LOWORD(lParam), HIWORD(lParam));
                break;
            case WM_DESTROY:
                pnt.release();
                PostQuitMessage(0);
                return 0;
            case WM_KEYDOWN:
                pnt.key_down(wParam, lParam);
                return 0;
            case WM_KEYUP:
                pnt.key_up(wParam, lParam);
                return 0;
            case WM_PAINT:
                pnt.paint();
                ValidateRect(hwnd, nullptr);
                return 0;
            case WM_USER:
                pnt.update();
                return 0;
        }
    } catch (std::runtime_error &er) {
        OutputDebugStringA(er.what());
        PostQuitMessage(1);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}