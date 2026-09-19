#pragma once
#include "Dungeon.hpp"

namespace sv {
enum class SelectionKind { Spawn, Object, Enemy, Pickup, Door, Light };
struct LevelSelection {
    SelectionKind kind{SelectionKind::Spawn};
    std::string id;
    int x{}, y{}; // Lights have cell identity in the existing v6 format.
};
class LevelEditing {
public:
    static std::vector<LevelSelection> at(const LevelDefinition& level, int x, int y);
    static bool locate(const LevelDefinition& level, const LevelSelection& selection, int& x, int& y);
    // Transactional: failures leave the caller's draft untouched.
    static bool move(LevelDefinition& level, LevelSelection& selection, int x, int y, std::string& error);
    static bool erase(LevelDefinition& level, const LevelSelection& selection, std::string& error);
};
}
