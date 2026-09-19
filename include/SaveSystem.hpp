#pragma once

#include "Dungeon.hpp"
#include "Party.hpp"
#include "Player.hpp"
#include "Roster.hpp"

#include <string>

namespace sv {

class SaveSystem {
public:
    static bool save(const std::string& path,
                     const PlayerState& player,
                     const Roster& roster,
                     const Party& party,
                     const Dungeon& dungeon,
                     int keys,
                     int potions,
                     int xp,
                     const EventRuntime* events = nullptr);

    static bool load(const std::string& path,
                     PlayerState& player,
                     Roster& roster,
                     Party& party,
                     Dungeon& dungeon,
                     int& keys,
                     int& potions,
                     int& xp,
                     EventRuntime* events = nullptr);
};

} // namespace sv
