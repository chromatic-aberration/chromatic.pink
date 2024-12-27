#include "server_status.hpp"

// For WinHTTP
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// For Winsock (Minecraft ping)
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// For JSON parsing of the MC server’s status response
// => Download nlohmann/json single-header from: https://github.com/nlohmann/json
// Place "json.hpp" in your include path
#include "json.hpp"

// STD includes
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdint>

static std::string FetchUrlWinHttp(const std::wstring& wHost, const std::wstring& wPath) {
    // Example: wHost = L"chromatic.pink", wPath = L"/pack.toml"
    // We'll do an HTTPS or HTTP GET using WinHTTP. 
    // NOTE: "chromatic.pink" is actually an HTTP site. If you need HTTPS, 
    // you'd do WinHttpOpenRequest with WINHTTP_FLAG_SECURE.

    std::string result;

    HINTERNET hSession = WinHttpOpen(L"TempadClient/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        return result; // empty on error
    }

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                        INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return result;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                            L"GET",
                                            wPath.c_str(),
                                            NULL,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    bool bResult = WinHttpSendRequest(hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS,
                                      0, WINHTTP_NO_REQUEST_DATA, 0,
                                      0, 0);

    if (bResult) {
        bResult = WinHttpReceiveResponse(hRequest, NULL);
    }
    if (bResult) {
        // Read data
        DWORD dwSize = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                // error
                break;
            }
            if (dwSize == 0) {
                // end of response
                break;
            }
            std::vector<char> buffer(dwSize);
            DWORD dwDownloaded = 0;
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                // error
                break;
            }
            if (dwDownloaded == 0) {
                // no more data
                break;
            }
            result.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

// Naive parse for: version = "<something>"
// For robust usage, consider toml++ or another real TOML parser.
static std::string ParseVersionFromToml(const std::string& tomlContent) {
    std::string version = "unknown";

    const std::string needle = "version = \"";
    auto pos = tomlContent.find(needle);
    if (pos != std::string::npos) {
        pos += needle.size();
        auto endPos = tomlContent.find('"', pos);
        if (endPos != std::string::npos) {
            version = tomlContent.substr(pos, endPos - pos);
        }
    }

    return version;
}

// Helper: write a "VarInt" to a buffer
static void WriteVarInt(std::vector<uint8_t>& buf, int value) {
    // Standard MC VarInt
    while (true) {
        if ((value & ~0x7F) == 0) {
            buf.push_back((uint8_t)value);
            return;
        } else {
            buf.push_back((uint8_t)((value & 0x7F) | 0x80));
            value >>= 7;
        }
    }
}

// Helper: read a VarInt from a buffer/offset
// This is a simplified approach, no error checks for invalid data
static int ReadVarInt(const uint8_t* data, size_t& offset, size_t maxLen) {
    int numRead = 0;
    int result = 0;
    uint8_t read;
    do {
        if (offset >= maxLen) break; 
        read = data[offset++];
        int value = (read & 0x7F);
        result |= (value << (7 * numRead));
        numRead++;
    } while ((read & 0x80) != 0 && numRead < 5);
    return result;
}

// Send a packet [length, packetID, data...]
static bool SendPacket(SOCKET s, const std::vector<uint8_t>& data) {
    // data includes the entire "length" + "packet id" + content
    int totalToSend = (int)data.size();
    int sent = 0;
    while (sent < totalToSend) {
        int ret = send(s, reinterpret_cast<const char*>(data.data() + sent), totalToSend - sent, 0);
        if (ret == SOCKET_ERROR) {
            return false;
        }
        sent += ret;
    }
    return true;
}

// Receive up to `maxLen` bytes into a buffer
static int RecvAll(SOCKET s, std::vector<uint8_t>& buffer, int maxLen, int timeoutMs = 2000) {
    // Basic blocking read with no fancy logic
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(s, &readSet);

    TIMEVAL tv;
    tv.tv_sec = (timeoutMs / 1000);
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = select(0, &readSet, nullptr, nullptr, &tv);
    if (ret <= 0) {
        // timed out or error
        return 0;
    }

    int recvCount = recv(s, reinterpret_cast<char*>(buffer.data()), maxLen, 0);
    if (recvCount == SOCKET_ERROR) {
        return 0;
    }
    return recvCount;
}

/**
 * Ping a 1.7+ MC server and read JSON status. 
 * We'll skip sending the "Ping" packet for the latency measure, 
 * but we'll send the handshake + status request.
 */
static bool PingMinecraftServer(const std::string& host, int port, std::vector<std::string>& playersOut) {
    playersOut.clear();

    // Convert port to string
    char portStr[32];
    sprintf_s(portStr, "%d", port);

    // Create a socket
    ADDRINFOA hints = {0};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ADDRINFOA* result = nullptr;
    int rv = getaddrinfo(host.c_str(), portStr, &hints, &result);
    if (rv != 0 || !result) {
        return false;
    }

    SOCKET s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }

    if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        closesocket(s);
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);

    // 1) Handshake packet
    // Packet ID = 0x00, next state = 1 (status)
    // We’ll build the packet body, then prefix with the VarInt length.

    std::vector<uint8_t> handshakeData;
    {
        // We will build the packet body first
        // protocol version (as a VarInt) -> 761 for MC 1.19.3, for example
        // but some servers are flexible with lower version. We'll just use 47 for 1.8 as a fallback
        int protocolVersion = 763;

        // Packet ID (VarInt)
        handshakeData.push_back(0x00); // will fill in length later
        // Now we add the body:
        std::vector<uint8_t> body;
        // Packet ID: 0x00
        WriteVarInt(body, protocolVersion);

        // Host string (as MC short-prefixed?)
        // Actually for 1.7+ it's VarInt length + string (UTF-8)
        WriteVarInt(body, (int)host.size());
        for (char c : host) body.push_back((uint8_t)c);

        // Next: port as unsigned short big-endian
        body.push_back((uint8_t)((port >> 8) & 0xFF));
        body.push_back((uint8_t)(port & 0xFF));

        // Next state = 1 (status)
        WriteVarInt(body, 1);

        // Now the length prefix for the entire packet
        // The entire packet: [ packetLengthVarInt, packetID(0x00 VarInt?), body... ]
        // Actually we do: first VarInt = size of (packetID + body)
        // Then packetID, then body.

        // Build final
        // (packetID = 0x00) => VarInt(0)
        std::vector<uint8_t> fullPacket;
        // length = size of (packetID VarInt) + body => 1 byte for packetID=0 + body.size() as VarInts
        // But in 1.7, packetID also is a VarInt => let's do it properly.

        std::vector<uint8_t> temp;
        // Packet ID
        WriteVarInt(temp, 0x00);
        // Body
        temp.insert(temp.end(), body.begin(), body.end());

        // Now prefix with length
        std::vector<uint8_t> prefix;
        WriteVarInt(prefix, (int)temp.size());

        // Combine
        fullPacket.insert(fullPacket.end(), prefix.begin(), prefix.end());
        fullPacket.insert(fullPacket.end(), temp.begin(), temp.end());

        // Send
        if (!SendPacket(s, fullPacket)) {
            closesocket(s);
            return false;
        }
    }

    // 2) Send status request packet (packet ID = 0x00, no payload)
    {
        std::vector<uint8_t> temp;
        WriteVarInt(temp, 0x00); // packet ID
        // prefix with length
        std::vector<uint8_t> prefix;
        WriteVarInt(prefix, (int)temp.size());

        std::vector<uint8_t> full;
        full.insert(full.end(), prefix.begin(), prefix.end());
        full.insert(full.end(), temp.begin(), temp.end());
        
        if (!SendPacket(s, full)) {
            closesocket(s);
            return false;
        }
    }

    // 3) Receive the response
    std::vector<uint8_t> recvBuf(4096);
    int received = RecvAll(s, recvBuf, (int)recvBuf.size());
    if (received <= 0) {
        closesocket(s);
        return false;
    }
    closesocket(s);

    // We have some data: parse the VarInt length, packet ID, then the JSON string
    size_t offset = 0;
    int packetLength = ReadVarInt(recvBuf.data(), offset, received);
    if (packetLength <= 0) {
        return false;
    }
    // next: packetID
    int packetId = ReadVarInt(recvBuf.data(), offset, received);
    if (packetId != 0x00) {
        // We expected 0x00 as the response for Status
        return false;
    }

    // The rest is the JSON string as a VarInt length, then UTF-8 data
    int jsonLen = ReadVarInt(recvBuf.data(), offset, received);
    if (jsonLen <= 0 || offset + jsonLen > (size_t)received) {
        return false;
    }

    std::string jsonStr((char*)&recvBuf[offset], jsonLen);
    offset += jsonLen;

    // Parse the JSON to get players
    // The format is something like:
    // {
    //   "version": {...},
    //   "players": {
    //     "max": 20,
    //     "online": 2,
    //     "sample": [
    //       {"id": "...", "name": "Steve"},
    //       {"id": "...", "name": "Alex"}
    //     ]
    //   },
    //   "description": {...}
    // }
    try {
        auto j = nlohmann::json::parse(jsonStr);
        auto players = j["players"];
        bool hasSample = players.find("sample") != players.end();
        if (hasSample && !players["sample"].is_null()) {
            for (auto& p : players["sample"]) {
                if (p.contains("name")) {
                    playersOut.push_back(p["name"].get<std::string>());
                }
            }
        }
    } catch (...) {
        return false;
    }

    return true; // success, presumably
}

// -----------------------------------------------------
// Public API

std::string GetModpackVersion() {
    // "http://chromatic.pink/pack.toml"
    // Split host/path for WinHTTP
    // host:   chromatic.pink
    // path:   /pack.toml
    const std::wstring wHost = L"chromatic.pink";
    const std::wstring wPath = L"/pack.toml";

    // Fetch
    std::string tomlContent = FetchUrlWinHttp(wHost, wPath);
    if (tomlContent.empty()) {
        return "unknown";
    }

    // Parse
    return ParseVersionFromToml(tomlContent);
}

bool GetServerStatus(const std::string& host, int port, std::vector<std::string>& playersOut) {
    return PingMinecraftServer(host, port, playersOut);
}
