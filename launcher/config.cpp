#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>     // For sscanf
#include <windows.h>  // Optional, if you want to use Win32 API calls

namespace fs = std::filesystem;

static const char* INIT_FLAG      = ".tempad_initialized";
static const char* CONFIG_FILE    = "config\\tempad-client.jsonc";

// Utility: Write default JSON to config file
static void WriteDefaultConfig() {
    fs::create_directories(fs::path(CONFIG_FILE).parent_path());
    std::ofstream ofs(CONFIG_FILE, std::ios::trunc);
    // Minimal JSONC example with default color 16722355 and blur true
    ofs << R"({
    // This is tempad-client config
    "color": 16722355,
    "renderBlur": true
})";
}

// Manually parse "color" from JSON (naive approach) - could be replaced with a real JSON library
static int ParseColorFromFile(const std::string& fileContents) {
    int colorValue = 16722355; // fallback
    // Find `"color": <someInteger>`
    std::string key = "\"color\"";
    size_t pos = fileContents.find(key);
    if (pos != std::string::npos) {
        pos = fileContents.find(':', pos);
        if (pos != std::string::npos) {
            // Attempt to parse an integer after the colon
            std::sscanf(fileContents.c_str() + pos + 1, "%d", &colorValue);
        }
    }
    return colorValue;
}

int ReadOrCreateConfig() {
    // 1) Check if ".tempad_initialized" exists
    if (!fs::exists(INIT_FLAG)) {
        // Mark initialization
        std::ofstream initFile(INIT_FLAG);
        initFile.close();

        // Write default config if not existing
        if (!fs::exists(CONFIG_FILE)) {
            WriteDefaultConfig();
        }
    }

    // 2) Read config file
    int colorValue = 16722355; // default
    if (fs::exists(CONFIG_FILE)) {
        std::ifstream ifs(CONFIG_FILE, std::ios::in | std::ios::binary);
        if (ifs) {
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            ifs.close();
            // parse color
            colorValue = ParseColorFromFile(buffer.str());
        }
    } else {
        // If somehow config disappeared, re-create default
        WriteDefaultConfig();
    }

    return colorValue;
}

void SaveColorToConfig(int newColor) {
    // We'll just rewrite the entire JSON. In a more advanced approach,
    // you'd parse the existing JSON and only update the "color" field.
    fs::create_directories(fs::path(CONFIG_FILE).parent_path());
    std::ofstream ofs(CONFIG_FILE, std::ios::trunc);
    ofs << R"({
    // This is tempad-client config
    "color": )" << newColor << R"(,
    "renderBlur": true
})";
    ofs.close();
}