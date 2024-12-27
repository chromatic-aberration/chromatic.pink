#pragma once
#include <windows.h>

/**
 * Show a custom color picker in dark mode:
 *  - 8 preset color swatches
 *  - "Custom Color" button for arbitrary RGB
 *  - "Close" button
 * On picking a color (either from a swatch or via custom color dialog),
 * it immediately saves that color to config and updates `ioColor`.
 *
 * This call blocks until the user closes the color picker window.
 *
 * @param parent    Handle to the parent window (used as owner).
 * @param ioColor   Reference to the currently selected color (decimal int).
 */
void ShowColorPickerDialog(HWND parent, int& ioColor);
