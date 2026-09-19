#pragma once
#include <string>
namespace sv {
enum class FileKind { Project, Level };
// Cancellation returns an empty path. Native dialogs never change working dir.
std::string chooseFile(FileKind kind, bool save, const std::string& initialDirectory);
std::string chooseFolder(const std::string& title);
}
