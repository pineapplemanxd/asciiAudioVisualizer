#include "config.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include "config.h"
extern HWND hWnd;
float sensitivity = 200.0f;
int barWidthMultiplier = 1;
int barSpread = 1;
int barCount = 80;
COLORREF barColor = RGB(255, 255, 255);
COLORREF textColor = RGB(255, 255, 255);
int windowPosY = 0;
int windowPosX = 1500;
int windowWidth = 400;
int windowHeight = 200;
bool topMost = false;

std::wstring ColorRefToHex(unsigned long color) {
    std::wstringstream ss;
    ss << L"#"
        << std::hex << std::setw(6) << std::setfill(L'0')
        << (color & 0xFFFFFF);
    return ss.str();
}

unsigned long HexToColorRef(const wchar_t* hexStr) {
    unsigned long color = 0;
    if (hexStr[0] == L'#') {
        ++hexStr;
    }
    std::wistringstream iss(hexStr);
    iss >> std::hex >> color;
    return color;
}

void LoadSettingsFromFile() {
    std::wifstream file(L"settings.txt");
    if (!file.is_open()) return;

    std::wstring line;
    while (std::getline(file, line)) {
        std::wistringstream iss(line);
        std::wstring key, eq, value;
        if (!(iss >> key >> eq >> value) || eq != L"=") continue;

        if (key == L"sensitivity") sensitivity = std::stof(value);
        else if (key == L"barWidthMultiplier") barWidthMultiplier = std::stoi(value);
        else if (key == L"barSpread") barSpread = std::stoi(value);
        else if (key == L"barCount") barCount = std::stoi(value);
        else if (key == L"barColor") barColor = HexToColorRef(value.c_str());
        else if (key == L"textColor") textColor = HexToColorRef(value.c_str());
        else if (key == L"windowPosX") windowPosX = std::stoi(value);
        else if (key == L"windowPosY") windowPosY = std::stoi(value);
        else if (key == L"windowWidth") windowWidth = std::stoi(value);
        else if (key == L"windowHeight") windowHeight = std::stoi(value);
        else if (key == L"topMost") topMost = (value == L"1");
    }
}

void SaveSettingsToFile() {
    std::wofstream file(L"settings.txt");
    if (!file.is_open()) return;

    file << L"sensitivity = " << sensitivity << L"\n";
    file << L"barWidthMultiplier = " << barWidthMultiplier << L"\n";
    file << L"barSpread = " << barSpread << L"\n";
    file << L"barCount = " << barCount << L"\n";
    file << L"barColor = " << ColorRefToHex(barColor) << L"\n";
    file << L"textColor = " << ColorRefToHex(textColor) << L"\n";
    file << L"windowPosX = " << windowPosX << L"\n";
    file << L"windowPosY = " << windowPosY << L"\n";
    file << L"windowWidth = " << windowWidth << L"\n";
    file << L"windowHeight = " << windowHeight << L"\n";
    file << L"topMost = " << (topMost ? L"1" : L"0") << L"\n";
}