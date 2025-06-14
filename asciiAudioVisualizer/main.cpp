// ascii_audio_visualizer.cpp
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <thread>
#include <vector>
#include <iostream>
#include <cmath>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <string>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Winmm.lib")

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

#define BAR_COUNT 100
#define REFRESH_MS 30

int barWidthMultiplier = 1;  // Width multiplier for each bar
int barSpread = 1;           // Bar spacing
float sensitivity = 5.0f;    // Adjust this for responsiveness

HWND hWnd;
std::wstring currentTitle = L"";
std::wstring currentArtist = L"";
int barHeights[BAR_COUNT] = { 0 };

// Fetch media title & artist from SMTC
void FetchSMTCMetadata() {
    init_apartment();
    try {
        auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
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
    catch (...) {
        currentTitle = L"";
        currentArtist = L"";
    }
}

// Set layered + transparent window style
void SetWindowStyles(HWND hwnd) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Audio loop using WASAPI loopback
void AudioLoop() {
    CoInitialize(nullptr);

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* format;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
    client->GetMixFormat(&format);
    client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, format, nullptr);
    client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
    client->Start();

    UINT32 frames;
    BYTE* data;
    UINT32 packet;
    DWORD flags;

    while (true) {
        capture->GetNextPacketSize(&packet);
        while (packet != 0) {
            capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            float* samples = (float*)data;
            int count = frames * format->nChannels;

            float total = 0;
            for (int i = 0; i < count; i++)
                total += fabs(samples[i]);
            total /= count;

            int volume = static_cast<int>(total * 100 * sensitivity);
            int barLevel = min(volume / 2, 20);

            for (int i = 0; i < BAR_COUNT; ++i) {
                int target = rand() % (barLevel + 1);
                barHeights[i] = (barHeights[i] + target) / 2;
                if (barLevel == 0)
                    barHeights[i] = max(barHeights[i] - 1, 1); // smooth minimum
            }

            capture->ReleaseBuffer(frames);
            capture->GetNextPacketSize(&packet);
        }

        InvalidateRect(hWnd, nullptr, FALSE);  // Avoid background erase
        std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_MS));
    }

    client->Stop();
    capture->Release();
    client->Release();
    device->Release();
    enumerator->Release();
    CoUninitialize();
}

void DrawBars(HDC hdc, int width, int height) {
    int barWidth = max(1, (width / BAR_COUNT) * barWidthMultiplier);

    for (int i = 0; i < BAR_COUNT; ++i) {
        int x = i * (barWidth + barSpread);
        int barHeight = barHeights[i] * 4;
        RECT rect = { x, height / 2 - barHeight, x + barWidth - 2, height / 2 + barHeight };
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }

    std::wstring meta = currentTitle + L" - " + currentArtist;
    if (!meta.empty()) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        HFONT font = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            FF_DONTCARE, L"Consolas");
        SelectObject(hdc, font);

        SIZE size;
        GetTextExtentPoint32W(hdc, meta.c_str(), (int)meta.length(), &size);
        TextOutW(hdc, (width - size.cx) / 2, height - 30, meta.c_str(), (int)meta.length());

        DeleteObject(font);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        // Create a compatible memory DC
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // Fill background black in memDC
        HBRUSH blackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
        FillRect(memDC, &rect, blackBrush);

        // Draw your bars and metadata text on memDC instead of hdc
        DrawBars(memDC, width, height);

        // Copy the off-screen buffer to the window DC
        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

        // Cleanup
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }


    case WM_ERASEBKGND:
        return TRUE;  // Prevent background flicker
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    init_apartment();

    const wchar_t CLASS_NAME[] = L"ASCIIVisualizerWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    int width = 400;
    int height = 200;
    int x = GetSystemMetrics(SM_CXSCREEN) - width;
    int y = 0;

    hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        CLASS_NAME, L"", WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, hInstance, nullptr);

    SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetWindowStyles(hWnd);

    std::thread audioThread(AudioLoop);
    audioThread.detach();

    std::thread metaThread([] {
        while (true) {
            FetchSMTCMetadata();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        });
    metaThread.detach();

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
