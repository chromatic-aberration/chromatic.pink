#include "color_picker.hpp"
#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include "config.hpp"   // For SaveColorToConfig(int)

// Eight presets: default pink, a blue, plus six more.
static const int PRESET_COLORS[8] = {
    16722355, // Pink
    25343,    // Blue
    0x00FF00, // Green
    0xFF0000, // Red
    0xFFFF00, // Yellow
    0xFF00FF, // Magenta
    0x00FFFF, // Cyan
    0xFFFFFF  // White
};

// Control IDs
enum {
    ID_BTN_CUSTOM = 2000,
    ID_BTN_CLOSE,
};

// Static variables for the color picker state
static HWND      g_hWndPicker    = nullptr;  
static int*      g_pUserColorRef = nullptr;  
static HBRUSH    g_hBrushDarkBg  = nullptr;  
static HINSTANCE g_hInst         = nullptr;

// Forward declaration of the window procedure
LRESULT CALLBACK ColorPickerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Helper: Convert decimal color (0xRRGGBB) to COLORREF
static COLORREF DecToColorRef(int decColor) {
    BYTE r = (BYTE)((decColor >> 16) & 0xFF);
    BYTE g = (BYTE)((decColor >> 8) & 0xFF);
    BYTE b = (BYTE)(decColor & 0xFF);
    return RGB(r, g, b);
}

// Helper: Apply selected color, update, and save to config
static void ApplyColorSelection(int newColor) {
    if (!g_pUserColorRef) return;
    *g_pUserColorRef = newColor;
    SaveColorToConfig(newColor);
    // Repaint the color picker to reflect the new color
    if (g_hWndPicker) InvalidateRect(g_hWndPicker, nullptr, TRUE);
}

// Show the system ChooseColor dialog to pick a custom color
static void PickCustomColor(HWND) {
    if (!g_pUserColorRef) return;

    COLORREF current = DecToColorRef(*g_pUserColorRef);

    CHOOSECOLOR cc = { 0 };
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner   = g_hWndPicker; 
    static COLORREF customColors[16] = { 0 };
    cc.lpCustColors = customColors;
    cc.rgbResult    = current;
    cc.Flags        = CC_RGBINIT | CC_FULLOPEN;

    if (ChooseColor(&cc)) {
        int r = GetRValue(cc.rgbResult);
        int g = GetGValue(cc.rgbResult);
        int b = GetBValue(cc.rgbResult);
        int chosenDec = (r << 16) | (g << 8) | b;
        ApplyColorSelection(chosenDec);
    }
}

// Draw the 8 color swatches and the current color preview
static void PaintSwatches(HDC hdc, RECT clientRect) {
    // Fill background with dark color
    FillRect(hdc, &clientRect, g_hBrushDarkBg);

    // Set text properties
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    // Title text
    std::string title = "Pick a Tempad color:";
    TextOutA(hdc, 10, 10, title.c_str(), (int)title.size());

    // Draw 8 swatches in two rows of four
    int startX = 10;
    int startY = 40;
    int boxSize = 40;
    int spacing = 10;
    int perRow = 4;

    for (int i = 0; i < 8; i++) {
        int col = i % perRow;
        int row = i / perRow;
        int x = startX + col * (boxSize + spacing);
        int y = startY + row * (boxSize + spacing);

        RECT r = { x, y, x + boxSize, y + boxSize };
        HBRUSH swatchBrush = CreateSolidBrush(DecToColorRef(PRESET_COLORS[i]));
        FillRect(hdc, &r, swatchBrush);
        DeleteObject(swatchBrush);

        // Draw white border around each swatch
        FrameRect(hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }

    // Current color label
    std::string curLabel = "Current Color:";
    TextOutA(hdc, 10, 130, curLabel.c_str(), (int)curLabel.size());

    // Current color preview rectangle
    if (g_pUserColorRef) {
        HBRUSH prevBrush = CreateSolidBrush(DecToColorRef(*g_pUserColorRef));
        RECT prevRect = { 120, 125, 160, 165 };
        FillRect(hdc, &prevRect, prevBrush);
        DeleteObject(prevBrush);

        // White frame around the preview
        FrameRect(hdc, &prevRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }
}

// Detect if a swatch was clicked and apply the corresponding color
static void OnLButtonDown(int x, int y) {
    int startX = 10;
    int startY = 40;
    int boxSize = 40;
    int spacing = 10;
    int perRow = 4;

    for (int i = 0; i < 8; i++) {
        int col = i % perRow;
        int row = i / perRow;
        int sx = startX + col * (boxSize + spacing);
        int sy = startY + row * (boxSize + spacing);
        RECT r = { sx, sy, sx + boxSize, sy + boxSize };
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            // User clicked swatch i
            ApplyColorSelection(PRESET_COLORS[i]);
            return;
        }
    }
}

// Window procedure for the color picker window
LRESULT CALLBACK ColorPickerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // Create "Custom Color" button
        CreateWindowA("button", "Custom Color",
                      WS_CHILD | WS_VISIBLE,
                      10, 180, 100, 30,
                      hWnd, (HMENU)ID_BTN_CUSTOM, g_hInst, nullptr);

        // Create "Close" button
        CreateWindowA("button", "Close",
                      WS_CHILD | WS_VISIBLE,
                      120, 180, 60, 30,
                      hWnd, (HMENU)ID_BTN_CLOSE, g_hInst, nullptr);
        break;

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BTN_CUSTOM:
            PickCustomColor(hWnd);
            break;
        case ID_BTN_CLOSE:
            DestroyWindow(hWnd);
            break;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; 
        GetClientRect(hWnd, &rc);
        PaintSwatches(hdc, rc);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_LBUTTONDOWN: {
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);
        OnLButtonDown(xPos, yPos);
        break;
    }

    // Provide a dark background for child controls
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        return (LRESULT)g_hBrushDarkBg;
    }

    case WM_DESTROY:
        // Do NOT call PostQuitMessage here
        // Just break
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Function to display the color picker dialog
void ShowColorPickerDialog(HWND parent, int& ioColor) {
    // Store reference to the color variable
    g_pUserColorRef = &ioColor;

    // Register window class once
    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        g_hInst = (HINSTANCE)GetModuleHandle(NULL);
        WNDCLASSA wc = {0};
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ColorPickerWndProc;
        wc.hInstance = g_hInst;
        wc.hbrBackground = NULL; // Custom painted
        wc.lpszClassName = "FancyColorPickerClass";
        RegisterClassA(&wc);

        // Create a solid brush for dark background
        g_hBrushDarkBg = CreateSolidBrush(RGB(40, 40, 40));
        s_classRegistered = true;
    }

    // Create the color picker window
    g_hWndPicker = CreateWindowA(
        "FancyColorPickerClass",
        "Fancy Tempad Color Picker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        320, 260,
        parent, NULL, g_hInst, NULL
    );

    if (!g_hWndPicker) {
        return; // Failed to create window
    }

    // Center the picker relative to parent window
    if (parent) {
        RECT rcParent;
        GetWindowRect(parent, &rcParent);
        int pWidth  = rcParent.right - rcParent.left;
        int pHeight = rcParent.bottom - rcParent.top;

        RECT rcPicker;
        GetWindowRect(g_hWndPicker, &rcPicker);
        int wWidth  = rcPicker.right - rcPicker.left;
        int wHeight = rcPicker.bottom - rcPicker.top;

        int x = rcParent.left + (pWidth - wWidth) / 2;
        int y = rcParent.top  + (pHeight - wHeight) / 2;
        SetWindowPos(g_hWndPicker, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    ShowWindow(g_hWndPicker, SW_SHOW);
    UpdateWindow(g_hWndPicker);
}
