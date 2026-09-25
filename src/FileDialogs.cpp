#include "FileDialogs.hpp"
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

namespace sv {
namespace {
std::wstring wide(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring value(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, value.data(), size);
    return value;
}
std::string narrow(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string value(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), size, nullptr, nullptr);
    if (!value.empty()) value.pop_back();
    return value;
}
}
std::string chooseFile(FileKind kind, bool save, const std::string& initialDirectory) {
    wchar_t file[32768]{};
    const auto initial = wide(initialDirectory);
    OPENFILENAMEW options{};
    options.lStructSize = sizeof(options);
    options.hwndOwner = GetActiveWindow();
    options.lpstrFile = file;
    options.nMaxFile = 32768;
    options.lpstrInitialDir = initial.c_str();
    options.lpstrFilter = kind == FileKind::Project ? L"STONEVEIL Project\0*.stoneveil\0\0" : L"STONEVEIL Level\0*.svl\0\0";
    options.lpstrDefExt = kind == FileKind::Project ? L"stoneveil" : L"svl";
    options.lpstrTitle = save ? L"Save a new file (choose a new empty folder for a project)" : L"Open STONEVEIL content";
    options.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    const bool chosen = save ? GetSaveFileNameW(&options) != 0 : GetOpenFileNameW(&options) != 0;
    return chosen ? narrow(file) : std::string{};
}
std::string chooseFolder(const std::string& title) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto caption = wide(title);
    BROWSEINFOW options{};
    options.hwndOwner = GetActiveWindow();
    options.lpszTitle = caption.c_str();
    options.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    auto selected = SHBrowseForFolderW(&options);
    wchar_t path[MAX_PATH]{};
    const bool found = selected && SHGetPathFromIDListW(selected, path);
    if (selected) CoTaskMemFree(selected);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return found ? narrow(path) : std::string{};
}
}
#else
namespace sv {
std::string chooseFile(FileKind, bool, const std::string&) { return {}; }
std::string chooseFolder(const std::string&) { return {}; }
}
#endif
