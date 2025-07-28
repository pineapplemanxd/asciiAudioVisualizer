#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <thread>
#include <vector>
#include <iostream>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.Collections.h>
#include "resource.h"
#include "config.h"
HWND hWnd = nullptr;
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Winmm.lib")

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

#define BAR_COUNT_DEFAULT 80
#define REFRESH_MS 30
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SETTINGS 2001
#define ID_TRAY_EXIT 2002

#define IDC_EDIT_POSX 109
#define IDC_EDIT_WIDTH 110
#define IDC_EDIT_HEIGHT 111

void CreateSettingsWindow(HWND parent);
void ApplySettings(HWND hwndDlg);
INT_PTR CALLBACK SettingsWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hSettingsWnd = nullptr;

std::vector<int> barHeights;
std::wstring currentTitle = L"";
std::wstring currentArtist = L"";

std::mutex configMutex;

NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };

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
        else {
            currentTitle = L"";
            currentArtist = L"";
        }
    }
    catch (...) {
        currentTitle = L"";
        currentArtist = L"";
    }
}

void SetWindowStyles(HWND hwnd) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    SetWindowPos(hwnd, topMost ? HWND_TOPMOST : HWND_BOTTOM,
        windowPosX, windowPosY, windowWidth, windowHeight,
        SWP_NOACTIVATE);
}

void AudioLoop() {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* format = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) return;

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
    if (FAILED(hr)) return;

    hr = client->GetMixFormat(&format);
    if (FAILED(hr)) return;

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, format, nullptr);
    if (FAILED(hr)) return;

    hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
    if (FAILED(hr)) return;

    hr = client->Start();
    if (FAILED(hr)) return;

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

            int volume = 0;
            {
                std::lock_guard<std::mutex> lock(configMutex);
                volume = static_cast<int>(total * 100 * sensitivity);
            }
            int barLevel = min(volume / 2, 20);

            {
                std::lock_guard<std::mutex> lock(configMutex);
                for (int i = 0; i < barCount; ++i) {
                    int target = rand() % (barLevel + 1);
                    if (i >= (int)barHeights.size()) barHeights.push_back(1);
                    barHeights[i] = (barHeights[i] + target) / 2;
                    if (barLevel == 0)
                        barHeights[i] = max(barHeights[i] - 1, 1);
                }
            }

            capture->ReleaseBuffer(frames);
            capture->GetNextPacketSize(&packet);
        }

        InvalidateRect(hWnd, nullptr, FALSE);
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
    std::lock_guard<std::mutex> lock(configMutex);

    int totalSpacing = (barCount - 1) * barSpread;
    int barWidth = max(1, ((width - totalSpacing) / barCount) * barWidthMultiplier);

    HBRUSH brush = CreateSolidBrush(barColor);

    for (int i = 0; i < barCount; ++i) {
        int x = i * (barWidth + barSpread);
        int barHeight = barHeights.size() > i ? barHeights[i] * 4 : 0;
        RECT rect = { x, height / 2 - barHeight, x + barWidth - 2, height / 2 + barHeight };
        FillRect(hdc, &rect, brush);
    }

    DeleteObject(brush);

    std::wstring meta = currentTitle + L" - " + currentArtist;
    if (!meta.empty()) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        HFONT font = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            FF_DONTCARE, L"Consolas");
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        SIZE size;
        GetTextExtentPoint32W(hdc, meta.c_str(), (int)meta.length(), &size);
        TextOutW(hdc, (width - size.cx) / 2, height - 30, meta.c_str(), (int)meta.length());

        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);


        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memDC, &rc, brush);
        DeleteObject(brush);

        DrawBars(memDC, rc.right, rc.bottom);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);


        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    }
    break;

    break;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_SETTINGS:
            if (!hSettingsWnd)
                CreateSettingsWindow(hwnd);
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        case IDC_BUTTON_APPLY:
            ApplySettings(hwnd);
            break;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void CreateSettingsWindow(HWND parent) {
    if (hSettingsWnd)
        return;
    hSettingsWnd = CreateDialogParam(NULL, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), parent, SettingsWndProc, 0);
    ShowWindow(hSettingsWnd, SW_SHOW);
}

INT_PTR CALLBACK SettingsWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
    {
        std::lock_guard<std::mutex> lock(configMutex);
        wchar_t buf[64];

        swprintf_s(buf, L"%.1f", sensitivity); SetDlgItemText(hwndDlg, IDC_EDIT_SENSITIVITY, buf);
        swprintf_s(buf, L"%d", barWidthMultiplier); SetDlgItemText(hwndDlg, IDC_EDIT_BARWIDTH, buf);
        swprintf_s(buf, L"%d", barSpread); SetDlgItemText(hwndDlg, IDC_EDIT_SPACING, buf);
        swprintf_s(buf, L"%d", barCount); SetDlgItemText(hwndDlg, IDC_EDIT_BARCOUNT, buf);
        SetDlgItemText(hwndDlg, IDC_EDIT_BARCOLOR, ColorRefToHex(barColor).c_str());
        swprintf_s(buf, L"%d", windowPosY); SetDlgItemText(hwndDlg, IDC_EDIT_POSY, buf);
        swprintf_s(buf, L"%d", windowPosX); SetDlgItemText(hwndDlg, IDC_EDIT_POSX, buf);
        swprintf_s(buf, L"%d", windowWidth); SetDlgItemText(hwndDlg, IDC_EDIT_WIDTH, buf);
        swprintf_s(buf, L"%d", windowHeight); SetDlgItemText(hwndDlg, IDC_EDIT_HEIGHT, buf);
        CheckDlgButton(hwndDlg, IDC_BUTTON_TOPMOST, topMost ? BST_CHECKED : BST_UNCHECKED);
    }
    return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BUTTON_APPLY) {
            ApplySettings(hwndDlg);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hwndDlg);
            hSettingsWnd = nullptr;
            return TRUE;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        hSettingsWnd = nullptr;
        return TRUE;
    }
    return FALSE;
}
void ApplySettings(HWND hwndDlg) {
    wchar_t buf[64];

    GetDlgItemText(hwndDlg, IDC_EDIT_SENSITIVITY, buf, 64); float newSensitivity = (float)_wtof(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_BARWIDTH, buf, 64); int newBarWidth = _wtoi(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_SPACING, buf, 64); int newSpacing = _wtoi(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_BARCOUNT, buf, 64); int newBarCount = _wtoi(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_BARCOLOR, buf, 64); COLORREF newBarColor = HexToColorRef(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_POSY, buf, 64); int newPosY = _wtoi(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_POSX, buf, 64); int newPosX = _wtoi(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_WIDTH, buf, 64); int newWidth = _wtoi(buf);
    GetDlgItemText(hwndDlg, IDC_EDIT_HEIGHT, buf, 64); int newHeight = _wtoi(buf);
    bool newTopMost = (IsDlgButtonChecked(hwndDlg, IDC_BUTTON_TOPMOST) == BST_CHECKED);

    {
        std::lock_guard<std::mutex> lock(configMutex);
        if (newSensitivity > 0) sensitivity = newSensitivity;
        if (newBarWidth > 0) barWidthMultiplier = newBarWidth;
        if (newSpacing >= 0) barSpread = newSpacing;
        if (newBarCount > 0 && newBarCount <= 300) barCount = newBarCount;
        if (newWidth >= 100) windowWidth = newWidth;
        if (newHeight >= 100) windowHeight = newHeight;
        if (newPosX >= 0) windowPosX = newPosX;
        if (newPosY >= 0) windowPosY = newPosY;
        barColor = newBarColor;
        topMost = newTopMost;

        SaveSettingsToFile();
    }

    SetWindowStyles(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);

    MessageBox(hwndDlg, L"Settings applied!", L"Info", MB_OK);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    LoadSettingsFromFile();

    init_apartment();

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"AsciiAudioVisualizer";
    wc.hbrBackground = NULL;
    RegisterClass(&wc);

    hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"Audio Visualizer", WS_POPUP,
        windowPosX, windowPosY, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    SetWindowStyles(hWnd);

    nid.hWnd = hWnd;
    nid.uID = 100;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"ASCII Audio Visualizer");
    Shell_NotifyIcon(NIM_ADD, &nid);

    std::thread audioThread(AudioLoop);
    audioThread.detach();


    SetTimer(hWnd, 1, 1000, NULL);

    ShowWindow(hWnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == 1) {
            FetchSMTCMetadata();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
