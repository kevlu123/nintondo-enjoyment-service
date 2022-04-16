#pragma once

#if defined(__ANDROID__)
#include "filesystem.h"
namespace fs = ghc::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include <string>
#include <stdint.h>
#include <vector>
#include <fstream>
#include <exception>

namespace File {

    inline std::string GetFilenameWithoutExtension(std::string name) {
        // Extract filename from path
        size_t slashIndex = name.find_last_of('/');
        if (slashIndex != std::string::npos)
            name = name.substr(slashIndex + 1);

        // Remove extension
        size_t dotIndex = name.find_last_of('.');
        name = name.substr(0, dotIndex);
        return name;
    }

    inline bool ReadFromFile(const std::string& path, std::vector<uint8_t>& data) {
        data.clear();

        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open())
            return false;

        size_t len = f.tellg();
        f.seekg(0);
        data.resize(len);
        f.read((char*)data.data(), len);

        if (f.fail() || f.bad()) {
            data.clear();
            return false;
        } else {
            return true;
        }
    }

    inline bool WriteToFile(const std::string& path, const std::vector<uint8_t>& data) {
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open())
            return false;

        f.write((const char*)data.data(), data.size());
        return !f.fail() && !f.bad();
    }

    inline void CreateFolder(const std::string& path) {
        try {
            fs::create_directory(path);
        } catch (std::exception&) {}
    }


    inline bool FileExists(const std::string& path) {
        try {
            return fs::exists(path);
        } catch (std::exception&) {
            return false;
        }
    }

    template <class F>
    void IterateDirectory(const std::string& path, F f) {
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    f(entry.path().string());
                }
            }
        } catch (std::exception&) {}
    }
}
