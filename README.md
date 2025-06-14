# ASCII Audio Visualizer (Win32)

A simple Win32 C++ audio visualizer that displays a white ASCII-style vertical bar graph reflecting system audio output in real-time. It uses WASAPI loopback to capture audio and the Windows SMTC API to show current playing media metadata.

---

## Features

- **Real-time audio visualization** using WASAPI loopback capture
- **ASCII-style vertical bars** representing audio intensity
- **Displays current media title and artist** from Windows System Media Transport Controls (SMTC)
- **Transparent, always-on-top window** that does not interfere with mouse clicks
- Configurable **sensitivity** to adjust responsiveness
- Flicker-free rendering using **double buffering**

---


## Requirements

- Windows 10 or later
- Visual Studio 2019 or newer (with C++ Desktop Development workload)
- C++17 compatible compiler
- Windows SDK with support for WASAPI and WinRT APIs

---

