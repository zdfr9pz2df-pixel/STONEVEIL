#pragma once

#include "Dungeon.hpp"

#include <string>

namespace sv {

class LevelBlueprint {
public:
    static bool parse(const std::string& text, LevelDefinition& level, std::string& error);
};

} // namespace sv
