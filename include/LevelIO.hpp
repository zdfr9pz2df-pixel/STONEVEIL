#pragma once

#include "Dungeon.hpp"

#include <string>
#include <vector>

namespace sv {

class LevelIO {
public:
    static bool load(const std::string& path, LevelDefinition& level, std::string& error);
    static bool save(const std::string& path, const LevelDefinition& level, std::string& error);
    static std::vector<std::string> validate(const LevelDefinition& level);
};

} // namespace sv
