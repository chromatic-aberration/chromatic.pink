#pragma once
#include <string>

/**
 * Reads or creates the config file "config/tempad-client.jsonc".
 * Also creates ".tempad_initialized" if it doesn't exist.
 * @return integer color read from JSON, or default if something fails.
 */
int ReadOrCreateConfig();

/**
 * Saves the given color to "config/tempad-client.jsonc".
 */
void SaveColorToConfig(int newColor);
