#pragma once
#include <string>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>



/**
 * Fetches remote "http://chromatic.pink/pack.toml" file via WinHTTP,
 * parses it, and returns the version string from the "version" field.
 *
 * @return The version read from pack.toml, or "unknown" if any error.
 */
std::string GetModpackVersion();

/**
 * Pings the specified Minecraft server (1.7+ protocol).
 * - If online, fills `playersOut` with active player names and returns true.
 * - Otherwise returns false.
 *
 * @param host       e.g. "188.165.47.57"
 * @param port       e.g. 26955
 * @param playersOut Filled with current player names if server is online.
 * @return           true if server is online, false otherwise.
 */
bool GetServerStatus(const std::string& host, int port, std::vector<std::string>& playersOut);