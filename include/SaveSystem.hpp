#pragma once

#include "Dungeon.hpp"
#include "CampaignState.hpp"
#include "Party.hpp"
#include "Player.hpp"
#include "Roster.hpp"
#include "StoryState.hpp"

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
                     const EventRuntime* events = nullptr,
                     const CampaignState* campaignState = nullptr,
                     const StoryState* storyState = nullptr);

    static bool load(const std::string& path,
                     PlayerState& player,
                     Roster& roster,
                     Party& party,
                     Dungeon& dungeon,
                     int& keys,
                     int& potions,
                     int& xp,
                     EventRuntime* events = nullptr,
                     CampaignState* campaignState = nullptr,
                     StoryState* storyState = nullptr);

    // Reads only the authored level identity needed to construct the correct
    // Dungeon before a transactional full load. Legacy/version-2 saves return
    // success with an empty level ID.
    static bool savedLevelId(const std::string& path, std::string& levelId);
};

} // namespace sv
