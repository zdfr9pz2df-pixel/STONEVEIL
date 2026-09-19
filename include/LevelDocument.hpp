#pragma once

#include "Dungeon.hpp"

#include <cstdint>
#include <map>

namespace sv {

// Editor-only, raylib-free document ownership. History includes file identity;
// undo across New/Save As must never silently retarget a different document.
class LevelDocument {
public:
    LevelDefinition& draft() { return current_.level; }
    const LevelDefinition& draft() const { return current_.level; }
    const std::string& path() const { return current_.path; }
    bool dirty() const;
    bool open(const std::string& path, std::string& error);
    bool save(std::string& error);
    bool saveCopy(const std::string& directory, std::string& error);
    void replaceUntitled(LevelDefinition level);
    void beginEdit();
    bool undo();
    bool redo();
    static LevelDefinition newLevel();

private:
    struct Snapshot {
        LevelDefinition level{levelOneDefinition()};
        std::string path;
        std::uint64_t revision{};
    };
    Snapshot current_;
    std::uint64_t nextRevision_{1};
    std::map<std::string, std::uint64_t> savedRevisions_;
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
};

} // namespace sv
