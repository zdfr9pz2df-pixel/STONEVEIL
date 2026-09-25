#include "AtomicFile.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace sv {
bool writeFileAtomically(const std::string& path, const std::string& bytes, std::string& error, bool replace) {
    namespace fs = std::filesystem;
    if (path.empty()) { error = "Choose a destination file."; return false; }
    const fs::path destination{path};
    auto temporary = destination;
    temporary += ".writing-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code ec;
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = "Cannot create a temporary save file: " + std::system_category().message(GetLastError());
        return false;
    }
    bool good = true;
    std::size_t offset = 0;
    while (offset < bytes.size() && good) {
        const auto remaining = bytes.size() - offset;
        const DWORD chunk = static_cast<DWORD>(remaining > 1048576 ? 1048576 : remaining);
        DWORD written{};
        good = WriteFile(file, bytes.data() + offset, chunk, &written, nullptr) && written == chunk;
        offset += written;
    }
    good = good && FlushFileBuffers(file);
    if (!CloseHandle(file)) good = false;
    if (good) good = MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != 0;
    if (!good) {
        error = "Save could not be committed; the previous file was preserved: " +
            std::system_category().message(GetLastError());
        fs::remove(temporary, ec);
        return false;
    }
#else
    if (fs::exists(temporary, ec)) { error = "Temporary save name is already in use."; return false; }
    std::ofstream output(temporary, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output || (!replace && fs::exists(destination, ec))) {
        error = "Save could not be committed; the previous file was preserved.";
        fs::remove(temporary, ec);
        return false;
    }
    fs::rename(temporary, destination, ec);
    if (ec) { error = ec.message(); fs::remove(temporary, ec); return false; }
#endif
    error.clear();
    return true;
}
}
