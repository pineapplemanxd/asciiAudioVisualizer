// config.h

#pragma once
#include <Windows.h>
extern HWND hWnd;
#include <string>
extern float sensitivity;
extern int barWidthMultiplier;
extern int barSpread;
extern int barCount;
extern COLORREF barColor;
extern int windowPosY;
extern int windowPosX;
extern int windowWidth;
extern int windowHeight;
extern bool topMost;
std::wstring ColorRefToHex(unsigned long color);
unsigned long HexToColorRef(const wchar_t* hexStr);
void LoadSettingsFromFile();
void SaveSettingsToFile();
COLORREF HexToColorRef(const wchar_t* hex);
std::wstring ColorRefToHex(COLORREF color);
