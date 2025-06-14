#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <iostream>

#pragma comment(lib, "Ole32.lib")

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 200;
constexpr int NUM_BARS = 64;

std::wstring currentTitle = L"";
std::wstring currentArtist = L"";

// Simulate bar height (replace this later with real WASAPI loopback analysis)
int mockBarHeight() {
    static std::default_random_engine e{ std::random_device{}() };
    static std::uniform_int_distribution<int> dist(0, 20);
    return dist(e);
}

// Convert wide string to narrow
std::string ws2s(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

void FetchSMTCMetadata() {
    init_apartment();

    GlobalSystemMediaTransportControlsSessionManager manager =
        GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

    auto sessions = manager.GetSessions();
    if (sessions.Size() > 0) {
        auto session = sessions.GetAt(0);
        if (session) {
            auto info = session.TryGetMediaPropertiesAsync().get();
            currentTitle = info.Title();
            currentArtist = info.Artist();
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fill black background
        FillRect(hdc, &ps.rcPaint, CreateSolidBrush(RGB(0, 0, 0)));

        // Draw ASCII visualizer bars
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 255, 0));
        HFONT hFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FF_DONTCARE, L"Consolas");
        SelectObject(hdc, hFont);

        int barWidth = WINDOW_WIDTH / NUM_BARS;
        for (int i = 0; i < NUM_BARS; ++i) {
            int height = mockBarHeight();
            for (int j = 0; j < height; ++j) {
                TextOutA(hdc, i * barWidth, WINDOW_HEIGHT / 2 - j * 10, "|", 1);
                TextOutA(hdc, i * barWidth, WINDOW_HEIGHT / 2 + j * 10, "|", 1);
            }
        }

        // Show SMTC metadata centered at bottom
        std::wstring meta = currentTitle + L" - " + currentArtist;
        SIZE textSize;
        GetTextExtentPoint32W(hdc, meta.c_str(), (int)meta.length(), &textSize);
        TextOutW(hdc, (WINDOW_WIDTH - textSize.cx) / 2, WINDOW_HEIGHT - 30, meta.c_str(), (int)meta.length());

        EndPaint(hwnd, &ps);
        DeleteObject(hFont);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    init_apartment();

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AudioVisClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"",
        WS_POPUP,
        100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );

    // Set always on bottom
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 800, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_SHOWWINDOW);

    // Make click-through
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);

    std::thread metadataThread([] {
        while (true) {
            FetchSMTCMetadata();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        });
    metadataThread.detach();

    MSG msg = {};
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
