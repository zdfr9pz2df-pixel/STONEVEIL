#include "Character.hpp"
#include "Roster.hpp"
#include "Party.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>

#define CHECK(e) do { if (!(e)) { std::cerr << "Failed line " << __LINE__ << ": " << #e << '\n'; std::exit(1); } } while(false)
int main() {
    using namespace sv;
    auto definitions = characterDefinitions();
    CharacterDefinition recruit;
    recruit.id = 2001;
    recruit.stableKey = "recruit.ember";
    recruit.name = "Ember";
    recruit.role = "Warden";
    recruit.summary = "A creator-authored reserve recruit.";
    recruit.maxHp = 38;
    recruit.power = 8;
    recruit.starter = false;
    recruit.traits = "stoic,fire";
    recruit.abilityId.clear();
    recruit.startingEquipment = "item.spear";
    recruit.recruitmentText = "I will guard the road behind us.";
    definitions.push_back(recruit);
    CHECK(CharacterCatalogIO::validate(definitions).empty());
    auto invalid = definitions;
    invalid.back().id = invalid.front().id;
    CHECK(!CharacterCatalogIO::validate(invalid).empty());
    invalid = definitions;
    invalid.back().id = static_cast<CharacterId>(std::numeric_limits<int>::max()) + 1U;
    CHECK(!CharacterCatalogIO::validate(invalid).empty());
    invalid = definitions;
    invalid.back().starter = true;
    invalid.push_back(recruit);
    invalid.back().id = 2002; invalid.back().stableKey = "recruit.second"; invalid.back().starter = true;
    CHECK(!CharacterCatalogIO::validate(invalid).empty()); // Four starting candidates.

    const auto path = std::filesystem::current_path() /
        ("characters-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".svc");
    std::string error;
    CHECK(CharacterCatalogIO::save(path.string(), definitions, error));
    std::vector<CharacterDefinition> loaded;
    CHECK(CharacterCatalogIO::load(path.string(), loaded, error));
    CHECK(loaded.size() == 4 && loaded.back().id == 2001);
    CHECK(loaded.back().startingEquipment == "item.spear" && !loaded.back().starter);

    Roster roster;
    CHECK(roster.setDefinitions(loaded));
    CHECK(roster.records().size() == loaded.size());
    const auto starters = roster.starterIds();
    CHECK(starters.size() == 3);
    CHECK(roster.beginNewGame({starters.front()}));
    CHECK(roster.recruit(2001));
    CHECK(!roster.recruit(2001)); // No duplication or refresh.
    auto* record = roster.find(2001);
    CHECK(record && record->status == CharacterStatus::Reserve && record->hp == 38);
    CHECK(!roster.setActiveParty({starters.front(), 2001, starters[1], starters[2]}));
    CHECK(roster.setActiveParty({starters.front(), 2001}));
    CHECK(!roster.damage(2001, 7)); // Nonlethal damage must not report permanent death.
    CHECK(record->hp == 31 && record->status == CharacterStatus::Active);
    std::filesystem::remove(path);
    return 0;
}
