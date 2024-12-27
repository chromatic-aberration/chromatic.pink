#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <shellapi.h>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include "serverlist.hpp"

static HINSTANCE g_hInst = nullptr;
static HWND g_hWndMain = nullptr;
static std::wstring g_javaPath;
static std::string g_modpackVersion;
static bool g_serverOnline = false;
static std::string g_playerList;
static COLORREF g_chosenColor = RGB(255, 192, 203);
static bool g_firstRun = false;
static Gdiplus::Image* g_logo = nullptr;

struct PresetColor { const wchar_t* name; int decimal; };
static PresetColor g_presetColors[] = {
    { L"Pink", 16722355 }, { L"Red", 16714240 }, { L"Green", 65280 },
    { L"Blue", 25343 }, { L"Purple", 8000511 }, { L"Yellow", 16772117 },
    { L"White", 16777215 }
};

LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ColorChoiceWndProc(HWND, UINT, WPARAM, LPARAM);

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }
int strToInt(const std::string& s) { try { return std::stoi(s); } catch(...) { return 16722355; } }

Gdiplus::Image* LoadImageFromFile(const std::wstring& path) {
    std::ifstream fin(std::string(path.begin(), path.end()).c_str(), std::ios::binary);
    if(!fin.is_open()) return nullptr;
    fin.close();
    Gdiplus::Image* img = Gdiplus::Image::FromFile(path.c_str());
    if(img && img->GetLastStatus() == Gdiplus::Ok) return img;
    if(img) delete img;
    return nullptr;
}

std::string FetchPackToml() {
    std::wstring domain = L"chromatic.pink";
    std::wstring path = L"/pack.toml";
    std::string result;
    HINTERNET hSession = WinHttpOpen(L"SimpleMenuClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if(!hSession) return "";
    HINTERNET hConnect = WinHttpConnect(hSession, domain.c_str(), INTERNET_DEFAULT_HTTP_PORT, 0);
    if(!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if(!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
    if(bResult) bResult = WinHttpReceiveResponse(hRequest, NULL);
    if(bResult) {
        DWORD dwSize = 0;
        do {
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if(dwSize == 0) break;
            std::vector<char> buffer(dwSize+1);
            ZeroMemory(buffer.data(), buffer.size());
            DWORD dwRead = 0;
            WinHttpReadData(hRequest, buffer.data(), dwSize, &dwRead);
            result.append(buffer.data(), dwRead);
        } while(dwSize > 0);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

std::string ParseVersionFromTOML(const std::string& toml) {
    std::istringstream iss(toml);
    std::string line;
    while(std::getline(iss, line)) {
        if(line.find("version") != std::string::npos && line.find("=") != std::string::npos) {
            auto firstQ = line.find('"');
            if(firstQ == std::string::npos) continue;
            auto secondQ = line.find('"', firstQ+1);
            if(secondQ == std::string::npos) continue;
            return line.substr(firstQ+1, secondQ - firstQ - 1);
        }
    }
    return "???";
}

void CheckMinecraftServer() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(s == INVALID_SOCKET) { g_serverOnline = false; WSACleanup(); return; }
    addrinfo hints = {0}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo("188.165.47.57", "26955", &hints, &res);
    if(!res) { closesocket(s); g_serverOnline = false; WSACleanup(); return; }
    int c = connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    if(c != 0) { closesocket(s); g_serverOnline = false; WSACleanup(); return; }
    g_serverOnline = true;
    g_playerList = "server empty :(";
    closesocket(s);
    WSACleanup();
}

int LoadTempadColorFromFile() {
    CreateDirectoryA(".\\config", NULL);
    CreateDirectoryA(".\\config\\simplemenu", NULL);
    CreateDirectoryA(".\\config\\simplemenu\\logo", NULL);
    std::ifstream fin(".\\config\\tempad-client.jsonc");
    if(!fin.is_open()) return 16722355;
    std::string fileContent, line;
    while(std::getline(fin, line)) fileContent += line + "\n";
    fin.close();
    auto pos = fileContent.find("\"color\"");
    if(pos == std::string::npos) return 16722355;
    pos = fileContent.find(":", pos);
    if(pos == std::string::npos) return 16722355;
    while(pos < fileContent.size() && (fileContent[pos] == ':' || isspace((unsigned char)fileContent[pos]))) pos++;
    std::string digits;
    while(pos < fileContent.size() && isdigit((unsigned char)fileContent[pos])) { digits.push_back(fileContent[pos]); pos++; }
    return strToInt(digits);
}

void SaveTempadColorToFile(int colorDecimal) {
    CreateDirectoryA(".\\config", NULL);
    std::ofstream fout(".\\config\\tempad-client.jsonc", std::ios::trunc);
    fout << "{\n";
    fout << "    \"color\": " << colorDecimal << ",\n";
    fout << "    \"renderBlur\": true\n";
    fout << "}\n";
    fout.close();
}

int ShowCustomColorPicker(HWND parent, int currentDecimal) {
    CHOOSECOLORW cc = {0};
    static COLORREF custColors[16] = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = parent;
    cc.lpCustColors = custColors;
    cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
    cc.rgbResult = RGB((currentDecimal >> 16) & 0xFF, (currentDecimal >> 8 ) & 0xFF, (currentDecimal) & 0xFF);
    if(ChooseColorW(&cc)) {
        COLORREF c = cc.rgbResult;
        int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
        return ((r << 16) | (g << 8) | b);
    }
    return currentDecimal;
}

void StartMinecraft() {
    std::wstring cmd = L"\"" + g_javaPath + L"\" -jar packwiz-installer-bootstrap.jar http://chromatic.pink/pack.toml";
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if(CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    PostQuitMessage(1);
}

void ShowColorChoiceWindow(HWND parent);

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_CREATE:
        if(g_firstRun) {
            std::ofstream dummy(".tempad_initialized");
            dummy.close();
            const int pinkDec = 16722355;
            g_chosenColor = RGB((pinkDec >> 16)&0xFF, (pinkDec >> 8 )&0xFF, (pinkDec)&0xFF);
            SaveTempadColorToFile(pinkDec);
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH bgBrush = CreateSolidBrush(RGB(48,48,48));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SelectObject(hdc, hFont);
        int x = 10, y = 10;
        if(g_logo) {
            Gdiplus::Graphics g2(hdc);
            Gdiplus::REAL w = (Gdiplus::REAL)g_logo->GetWidth();
            Gdiplus::REAL h = (Gdiplus::REAL)g_logo->GetHeight();
            g2.DrawImage(g_logo, (Gdiplus::REAL)x, (Gdiplus::REAL)y, w, h);
            y += (int)h + 10;
        }
        SetTextColor(hdc, RGB(220,220,220));
        std::wstring verMsg = widen("Modpack version = " + g_modpackVersion);
        TextOutW(hdc, x, y, verMsg.c_str(), (int)verMsg.size());
        y += 20;
        {
            std::wstring statusLabel = L"Server Status: ";
            SetTextColor(hdc, RGB(220,220,220));
            TextOutW(hdc, x, y, statusLabel.c_str(), (int)statusLabel.size());
            SIZE sz;
            GetTextExtentPoint32W(hdc, statusLabel.c_str(), (int)statusLabel.size(), &sz);
            int offsetX = x + sz.cx;
            if(g_serverOnline) {
                SetTextColor(hdc, RGB(0,255,0));
                std::wstring onlineTxt = L"Online";
                TextOutW(hdc, offsetX, y, onlineTxt.c_str(), (int)onlineTxt.size());
            } else {
                SetTextColor(hdc, RGB(255,0,0));
                std::wstring offlineTxt = L"Offline";
                TextOutW(hdc, offsetX, y, offlineTxt.c_str(), (int)offlineTxt.size());
            }
            y += 20;
        }
        if(g_serverOnline) {
            SetTextColor(hdc, RGB(220,220,220));
            std::wstring wpl = widen(std::string("Players: ") + g_playerList);
            TextOutW(hdc, x, y, wpl.c_str(), (int)wpl.size());
            y += 20;
        }
        HBRUSH colorBrush = CreateSolidBrush(g_chosenColor);
        RECT colorRect = {170, 180, 190, 200};
        FillRect(hdc, &colorRect, colorBrush);
        DeleteObject(colorBrush);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case 101: StartMinecraft(); break;
        case 102: ShowColorChoiceWindow(hWnd); InvalidateRect(hWnd, NULL, TRUE); break;
        case 103: ShellExecuteW(NULL, L"open", L"https://discord.com/channels/1315863508960804905/1318760261229744190", NULL, NULL, SW_SHOWNORMAL); break;
        case 104: PostQuitMessage(1); break;
        }
        break;
    case WM_CLOSE: PostQuitMessage(1); break;
    case WM_DESTROY: PostQuitMessage(1); break;
    default: return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

static HBRUSH g_colorBrushes[7];

LRESULT CALLBACK ColorChoiceWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH hBrush = CreateSolidBrush(RGB(48,48,48));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return 0;
    }
    case WM_CREATE:
    {
        int x = 20, y = 20, bw = 150, bh = 25;
        for(int i=0; i<7; i++) {
            CreateWindowW(L"BUTTON", g_presetColors[i].name, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, bw, bh, hWnd, (HMENU)(200+i), g_hInst, nullptr);
            // CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_SIMPLE, x + bw + 5, y, 20, bh, hWnd, (HMENU)(300 + i), g_hInst, nullptr);
            int dec = g_presetColors[i].decimal;
            COLORREF colorRef = RGB((dec>>16)&0xFF, (dec>>8)&0xFF, dec&0xFF);
            g_colorBrushes[i] = CreateSolidBrush(colorRef);
            y += 35;
        }
        CreateWindowW(L"BUTTON", L"Custom", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, bw, bh, hWnd, (HMENU)207, g_hInst, nullptr);
        y += 40;
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, bw, bh, hWnd, (HMENU)IDCANCEL, g_hInst, nullptr);
        return 0;
    }
    case WM_CTLCOLORDLG:
    {
        static HBRUSH hBrushDark = CreateSolidBrush(RGB(48,48,48));
        return (INT_PTR)hBrushDark;
    }
    // case WM_CTLCOLORSTATIC:
    // {
    //     HDC hdcStatic = (HDC)wParam;
    //     SetBkMode(hdcStatic, TRANSPARENT);
    //     SetTextColor(hdcStatic, RGB(220,220,220));
    //     HWND hCtrl = (HWND)lParam;
    //     int ctrlID = GetDlgCtrlID(hCtrl);
    //     if(ctrlID >= 300 && ctrlID < 307) return (LRESULT)g_colorBrushes[ctrlID - 300];
    //     static HBRUSH hBrushDark = CreateSolidBrush(RGB(48,48,48));
    //     return (LRESULT)hBrushDark;
    // }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if(id >= 200 && id <= 206) {
            int idx = id - 200;
            int dec = g_presetColors[idx].decimal;
            g_chosenColor = RGB((dec >> 16)&0xFF, (dec >> 8 )&0xFF, (dec)&0xFF);
            SaveTempadColorToFile(dec);
            DestroyWindow(hWnd);
        }
        else if(id == 207) {
            int dec = ((GetRValue(g_chosenColor) << 16) | (GetGValue(g_chosenColor) << 8) | GetBValue(g_chosenColor));
            int newDec = ShowCustomColorPicker(hWnd, dec);
            g_chosenColor = RGB((newDec >> 16)&0xFF, (newDec >> 8 )&0xFF, (newDec)&0xFF);
            SaveTempadColorToFile(newDec);
            DestroyWindow(hWnd);
        }
        else if(id == IDCANCEL) DestroyWindow(hWnd);
    }
    break;
    case WM_DESTROY:
    {
        for(int i=0; i<7; i++) {
            if(g_colorBrushes[i]) { DeleteObject(g_colorBrushes[i]); g_colorBrushes[i] = nullptr; }
        }
        PostQuitMessage(0);
    }
    break;
    default: return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowColorChoiceWindow(HWND parent) {
    EnableWindow(parent, FALSE);
    const wchar_t* CLASS_NAME = L"ColorChoiceClass";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = ColorChoiceWndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassW(&wc);
    HWND hWndColor = CreateWindowExW(0, CLASS_NAME, L"Pick Tempad Color", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 220, 380, parent, NULL, g_hInst, NULL);
    RECT rcParent, rcSelf;
    GetWindowRect(parent, &rcParent);
    GetWindowRect(hWndColor, &rcSelf);
    int w = rcSelf.right - rcSelf.left;
    int h = rcSelf.bottom - rcSelf.top;
    int px = rcParent.left + ((rcParent.right - rcParent.left) - w)/2;
    int py = rcParent.top + ((rcParent.bottom - rcParent.top) - h)/2;
    MoveWindow(hWndColor, px, py, w, h, TRUE);
    MSG msg;
    while(GetMessageW(&msg, NULL, 0, 0) > 0) {
        if(!IsWindow(hWndColor)) break;
        if(!IsDialogMessageW(hWndColor, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
}

int RunGUI(HINSTANCE hInstance) {
    WNDCLASSW wc = {0};
    wc.hInstance = hInstance;
    wc.lpfnWndProc = MainWndProc;
    wc.lpszClassName = L"MainMenuClass";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"chromatic.pink launcher", style, CW_USEDEFAULT, CW_USEDEFAULT, 640, 320, NULL, NULL, hInstance, NULL);
    g_hWndMain = hWnd;
    ShowWindow(hWnd, SW_SHOW);
    CreateWindowW(L"BUTTON", L"Start MC", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 150, 100, 25, hWnd, (HMENU)101, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Change tempad color", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 180, 150, 25, hWnd, (HMENU)102, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Changelog", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 210, 100, 25, hWnd, (HMENU)103, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 240, 100, 25, hWnd, (HMENU)104, hInstance, NULL);
    MSG msg;
    while(GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInst = hInstance;
    {
        std::ifstream fin(".tempad_initialized");
        if(!fin.is_open()) g_firstRun = true;
    }
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if(argc < 2) {
        MessageBoxW(NULL, L"Usage: simplemenu.exe <path_to_java>", L"Error", MB_ICONERROR);
        return 1;
    }
    g_javaPath = argv[1];
    LocalFree(argv);
    int colorDecimal = LoadTempadColorFromFile();
    g_chosenColor = RGB((colorDecimal >> 16)&0xFF, (colorDecimal >> 8 )&0xFF, (colorDecimal)&0xFF);
    std::string toml = FetchPackToml();
    g_modpackVersion = ParseVersionFromTOML(toml);
    CheckMinecraftServer();
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    g_logo = LoadImageFromFile(L".\\config\\simplemenu\\logo\\edition.png");
    int ret = RunGUI(hInstance);
    if(g_logo) { delete g_logo; g_logo = nullptr; }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return ret;
}
