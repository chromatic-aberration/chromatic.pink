#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdint>

#include "serverlist.hpp"
// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// Adjust to match the Minecraft server version. 760 ~ MC 1.19.4, 761 ~ MC 1.20, etc.
static const int PROTOCOL_VERSION = 760;
static const int MINECRAFT_PORT   = 26955;

// Utility to write a VarInt into a buffer
void writeVarInt(std::vector<uint8_t>& buf, int value) {
    do {
        uint8_t temp = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0) {
            temp |= 0x80;
        }
        buf.push_back(temp);
    } while (value != 0);
}

// Utility to read a VarInt from the socket
int readVarInt(SOCKET sock) {
    int result  = 0;
    int numRead = 0;

    while (true) {
        uint8_t byte;
        int received = recv(sock, reinterpret_cast<char*>(&byte), 1, 0);
        if (received <= 0) {
            return -1;  // error or connection closed
        }
        int value = (byte & 0x7F);
        result |= (value << (7 * numRead));
        numRead++;
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    return result;
}

// Utility to read an exact number of bytes
bool readExact(SOCKET sock, char* buffer, int bytesToRead) {
    int totalRead = 0;
    while (totalRead < bytesToRead) {
        int received = recv(sock, buffer + totalRead, bytesToRead - totalRead, 0);
        if (received <= 0) {
            return false; // connection closed or error
        }
        totalRead += received;
    }
    return true;
}

int main() {
    // 1) Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // 2) Create socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
        WSACleanup();
        return 1;
    }

    // 3) Resolve hostname
    std::string hostname = "mc.chromatic.pink";
    struct addrinfo hints = {0};
    hints.ai_family       = AF_INET;
    hints.ai_socktype     = SOCK_STREAM;
    hints.ai_protocol     = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    if (getaddrinfo(hostname.c_str(), std::to_string(MINECRAFT_PORT).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 4) Connect
    if (connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect.\n";
        freeaddrinfo(result);
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    //
    // 5) Construct Handshake Packet (State=1 for status)
    //
    // Packet ID = 0x00, fields:
    //   - protocol version (varint)
    //   - server address (string)
    //   - server port (ushort)
    //   - next state (varint) = 1
    //
    // We'll build the packet payload in a vector first, then send length+payload
    //
    std::vector<uint8_t> handshakeData;
    // Packet ID
    writeVarInt(handshakeData, 0x00);
    // Protocol version
    writeVarInt(handshakeData, PROTOCOL_VERSION);
    // Write server address as: [varint len][bytes of hostname]
    writeVarInt(handshakeData, static_cast<int>(hostname.size()));
    for (char c : hostname) {
        handshakeData.push_back(static_cast<uint8_t>(c));
    }
    // Write server port as two bytes big-endian
    handshakeData.push_back(static_cast<uint8_t>((MINECRAFT_PORT >> 8) & 0xFF));
    handshakeData.push_back(static_cast<uint8_t>(MINECRAFT_PORT & 0xFF));
    // Next state = 1 (status)
    writeVarInt(handshakeData, 0x01);

    // Now send the packet: [varint length of handshakeData] + handshakeData
    {
        std::vector<uint8_t> packet;
        // total length = handshakeData.size()
        writeVarInt(packet, static_cast<int>(handshakeData.size()));
        // then the handshakeData
        packet.insert(packet.end(), handshakeData.begin(), handshakeData.end());

        // send them
        send(sock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
    }

    //
    // 6) Send Status Request Packet (Packet ID=0x00, no fields)
    //
    {
        std::vector<uint8_t> packet;
        // The packet itself is just 1 byte long (the packet ID),
        // so total length = 1
        writeVarInt(packet, 1);       // length
        writeVarInt(packet, 0x00);    // packet ID
        send(sock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
    }

    //
    // 7) Read the Status Response
    //    - varint totalLength
    //    - varint packetID (should be 0x00)
    //    - varint string length, then JSON string
    //
    int totalLen = readVarInt(sock);
    if (totalLen <= 0) {
        std::cerr << "Invalid response length.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    int packetID = readVarInt(sock);
    if (packetID != 0x00) {
        std::cerr << "Unexpected packet ID: " << packetID << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    int jsonLen = readVarInt(sock);
    if (jsonLen <= 0) {
        std::cerr << "Invalid JSON length.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::vector<char> jsonBuffer(jsonLen + 1, '\0');
    if (!readExact(sock, jsonBuffer.data(), jsonLen)) {
        std::cerr << "Failed to read JSON data.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Convert to std::string
    std::string jsonStr(jsonBuffer.data(), jsonLen);

    // 8) Naive substring parsing for "description", "players","online","max","sample", etc.
    //    In real code, consider a JSON parser library. This can easily break on tricky formatting.

    auto findValue = [&](const std::string& key) -> std::string {
        // Key is something like: "\"online\":"
        // We'll look for key, then read until we hit ',' or '}'
        size_t pos = jsonStr.find(key);
        if (pos == std::string::npos) return "";
        pos += key.size();

        // skip possible spaces or quotes
        while (pos < jsonStr.size() && (jsonStr[pos] == ' ' || jsonStr[pos] == '"' || jsonStr[pos] == ':')) {
            pos++;
        }

        // read until ',', '}', or a quote
        std::string val;
        bool inQuotes = false;
        bool started  = false;
        for (; pos < jsonStr.size(); ++pos) {
            char c = jsonStr[pos];
            if (c == '"') {
                // If we haven't started capturing, this might be the start of a quoted string
                if (!started) {
                    inQuotes = true;
                    started  = true;
                    continue;
                }
                // If we are in quotes and see a quote, that might be the end
                if (inQuotes) {
                    break;
                }
            }
            if (!inQuotes && (c == ',' || c == '}')) {
                break;
            }
            if (!started) started = true;
            val.push_back(c);
        }

        // Trim whitespace
        while (!val.empty() && (val.front() == ' ')) val.erase(val.begin());
        while (!val.empty() && (val.back() == ' ')) val.pop_back();
        return val;
    };

    std::string descKey  = "\"description\":";
    std::string textKey  = "\"text\":";
    std::string descTemp  = findValue(descKey);
    std::string descVal  = findValue(textKey);

    std::string onlineVal = findValue("\"online\":");
    std::string maxVal    = findValue("\"max\":");

    // The "sample" field is an array like:
    // "sample":[{"name":"Player1","id":"<UUID>"},{"name":"Player2", ... }]
    // We'll do a naive scan for player names.
    std::string sampleKey = "\"sample\":";
    size_t samplePos = jsonStr.find(sampleKey);
    std::vector<std::string> playerNames;

    if (samplePos != std::string::npos) {
        // find all occurrences of "\"name\":\"..."
        size_t start = samplePos;
        while (true) {
            auto namePos = jsonStr.find("\"name\":\"", start);
            if (namePos == std::string::npos) break;
            namePos += 8; // move past "\"name\":\""
            // read until next quote
            std::string playerName;
            while (namePos < jsonStr.size() && jsonStr[namePos] != '"') {
                playerName.push_back(jsonStr[namePos]);
                namePos++;
            }
            playerNames.push_back(playerName);
            start = namePos + 1;
        }
    }

    // Print them
    std::cout << "=== Server Status ===\n";
    std::cout << "Description: " << descVal << "\n";
    std::cout << "Players: " << onlineVal << " / " << maxVal << "\n";

    if (!playerNames.empty()) {
        std::cout << "Sample Players:\n";
        for (auto& nm : playerNames) {
            std::cout << " - " << nm << "\n";
        }
    } else {
        std::cout << "No sample players or sample hidden.\n";
    }

    // Clean up
    closesocket(sock);
    WSACleanup();
    return 0;
}
