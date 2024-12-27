#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>   // For ShellExecute
#include <string>
#include <vector>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// Forward declare or include headers for our helper modules:
#include "config.hpp"        // read/write config
#include "server_status.hpp" // get MC server status
#include "color_picker.hpp"  // color picker dialog

// Global references for simplicity (could be in a struct/class).
static HINSTANCE g_hInst;
static HWND      g_hWndMain;
static std::string g_javaPath;           // from argv
static int        g_currentColor = 16722355;
static std::string g_modpackVersion;
static std::vector<std::string> g_activePlayers;
static bool       g_serverOnline = false;
static Gdiplus::Bitmap* g_logoBmp = nullptr;

// Button IDs
enum {
    ID_BTN_STARTMC = 101,
    ID_BTN_CHANGECOLOR,
    ID_BTN_CHANGELOG,
    ID_BTN_EXIT
};

// Simple function to open a URL
void OpenUrl(const std::string& url) {
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// Launch Minecraft: <java_path> -jar packwiz-installer-bootstrap.jar http://chromatic.pink/pack.toml
void LaunchMinecraft() {
    std::string cmd = "\"" + g_javaPath + "\" -jar packwiz-installer-bootstrap.jar http://chromatic.pink/pack.toml";

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// Our main window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Create buttons
        CreateWindowA("button", "Start MC", WS_CHILD | WS_VISIBLE,
                      10, 200, 80, 30, hWnd, (HMENU)ID_BTN_STARTMC, g_hInst, NULL);
        CreateWindowA("button", "Change tempad color", WS_CHILD | WS_VISIBLE,
                      100, 200, 160, 30, hWnd, (HMENU)ID_BTN_CHANGECOLOR, g_hInst, NULL);
        CreateWindowA("button", "Changelog", WS_CHILD | WS_VISIBLE,
                      270, 200, 80, 30, hWnd, (HMENU)ID_BTN_CHANGELOG, g_hInst, NULL);
        CreateWindowA("button", "Exit", WS_CHILD | WS_VISIBLE,
                      360, 200, 50, 30, hWnd, (HMENU)ID_BTN_EXIT, g_hInst, NULL);
    } break;

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BTN_STARTMC:
            // Start MC and exit(0) => success
            LaunchMinecraft();
            PostQuitMessage(0);
            break;
        case ID_BTN_CHANGECOLOR:
            // Show color picker. On success, it updates g_currentColor and saves config.
            ShowColorPickerDialog(hWnd, g_currentColor);
            // On return, g_currentColor is updated if changed
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        case ID_BTN_CHANGELOG:
            OpenUrl("https://discord.com/channels/1315863508960804905/1318760261229744190");
            break;
        case ID_BTN_EXIT:
            // Quit with error code => the script won't continue
            PostQuitMessage(1);
            break;
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Create a GDI+ graphics object from HDC
        Gdiplus::Graphics graphics(hdc);

        // Fill background dark gray
        RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 40, 40));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        if (g_logoBmp) {
            // Let's draw it at (10, 10)
            graphics.DrawImage(g_logoBmp, 10, 10,
                            g_logoBmp->GetWidth(),
                            g_logoBmp->GetHeight());
        }

        // White text
        SetTextColor(hdc, RGB(255,255,255));
        SetBkMode(hdc, TRANSPARENT);

        // Display version
        std::string versionStr = "Modpack version = " + g_modpackVersion;
        TextOutA(hdc, 10, 10, versionStr.c_str(), (int)versionStr.size());

        // Server status
        std::string statusStr = "Server: 188.165.47.57:26955 is ";
        statusStr += (g_serverOnline ? "ONLINE" : "OFFLINE");
        if (g_serverOnline) {
            SetTextColor(hdc, RGB(0, 255, 0)); // green
        } else {
            SetTextColor(hdc, RGB(255, 0, 0)); // red
        }
        TextOutA(hdc, 10, 40, statusStr.c_str(), (int)statusStr.size());
        SetTextColor(hdc, RGB(255,255,255));

        // If online, show active players
        if (g_serverOnline) {
            int yPos = 60;
            for (auto& player : g_activePlayers) {
                TextOutA(hdc, 10, yPos, player.c_str(), (int)player.size());
                yPos += 20;
            }
        }

        // Draw color preview rectangle
        int previewX = 265;
        int previewY = 205;
        int previewSize = 20;

        COLORREF clr = RGB((g_currentColor >> 16) & 0xFF,
                        (g_currentColor >> 8) & 0xFF,
                        (g_currentColor & 0xFF));
        HBRUSH colorBrush = CreateSolidBrush(clr);
        RECT colorRect = { previewX, previewY, previewX + previewSize, previewY + previewSize };
        FillRect(hdc, &colorRect, colorBrush);
        DeleteObject(colorBrush);

        FrameRect(hdc, &colorRect, (HBRUSH)GetStockObject(WHITE_BRUSH));

        EndPaint(hWnd, &ps);
    } break;

    case WM_DESTROY:
        // If user closed window, return error code
        PostQuitMessage(1);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// WinMain entry
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // Command line => single argument: path to java
    g_javaPath = lpCmdLine;
    if (g_javaPath.empty()) {
        MessageBoxA(NULL, "Usage: this_exe <path_to_java>", "Error", MB_OK);
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "Failed to initialize Winsock!", "Error", MB_OK);
        return 1;
    }

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    g_hInst = hInstance;

    // 1) Read or create config
    g_currentColor = ReadOrCreateConfig();  // from config.hpp/ config.cpp

    // 2) Get modpack version from pack.toml (online)
    g_modpackVersion = GetModpackVersion(); // from server_status.cpp or wherever

    // 3) Check server status
    g_serverOnline = GetServerStatus("188.165.47.57", 26955, g_activePlayers);

    std::wstring widePath = L"config\\simplemenu\\logo\\edition.png";
    g_logoBmp = Gdiplus::Bitmap::FromFile(widePath.c_str());
    if (!g_logoBmp || g_logoBmp->GetLastStatus() != Gdiplus::Ok) {
        // failed to load, set to nullptr or handle error
        delete g_logoBmp;
        g_logoBmp = nullptr;
    }

    // Register window class
    WNDCLASSA wc = {0};
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "TempadLauncherClass";
    RegisterClassA(&wc);

    // Create main window
    g_hWndMain = CreateWindowA("TempadLauncherClass", "Tempad Launcher",
                               WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
                               nullptr, nullptr, hInstance, nullptr);
    if (!g_hWndMain) {
        return 1;
    }

    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);

    // Main loop
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    // Cleanup
    Gdiplus::GdiplusShutdown(gdiplusToken);
    WSACleanup();

    // Return code: 0 if "Start MC" pressed, else 1 for everything else
    return (int)msg.wParam;
}
