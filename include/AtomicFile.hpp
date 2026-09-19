#pragma once
#include <string>
namespace sv {
// Commit a complete sibling temporary file, never truncate the destination.
bool writeFileAtomically(const std::string& path, const std::string& bytes,
                         std::string& error, bool replace = true);
}
