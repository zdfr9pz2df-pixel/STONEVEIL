#include "Character.hpp"
#include "LevelDocument.hpp"
#include "LevelIO.hpp"
#include "Party.hpp"
#include "Roster.hpp"
#include "SaveSystem.hpp"
#include "WorldEvents.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(1); } } while (false)

int main() {
    using namespace sv;

    auto definitions = characterDefinitions();
    CharacterDefinition recruit;
    recruit.id = 2001;
    recruit.stableKey = "recruit.ember";
    recruit.name = "Ember";
    recruit.role = "Warden";
    recruit.summary = "A creator-authored companion.";
    recruit.maxHp = 38;
    recruit.power = 8;
    recruit.starter = false;
    recruit.abilityId.clear();
    recruit.recruitmentText = "I will guard the road behind us.";
    definitions.push_back(recruit);

    auto level = LevelDocument::newLevel();
    level.objects.push_back({"recruit.ember.object", WorldObjectKind::Recruit, 3, 3,
                             "Ember", recruit.recruitmentText, true, recruit.id});
    level.objects.push_back({"party.campfire", WorldObjectKind::PartyManagement, 4, 3,
                             "Company Fire", "Choose who travels onward.", false});
    CHECK(LevelIO::validate(level).empty());

    const auto temporary = std::filesystem::current_path() /
        ("recruitment-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto levelPath = temporary.string() + ".svl";
    const auto savePath = temporary.string() + ".sav";
    std::string error;
    CHECK(LevelIO::save(levelPath, level, error));
    LevelDefinition loadedLevel;
    CHECK(LevelIO::load(levelPath, loadedLevel, error));
    CHECK(loadedLevel.objects.size() == 2);
    CHECK(loadedLevel.objects[0].kind == WorldObjectKind::Recruit);
    CHECK(loadedLevel.objects[0].characterId == recruit.id);

    Dungeon dungeon{loadedLevel};
    EventRuntime events;
    CHECK(configureWorldEvents(dungeon, events));
    CHECK(events.definitions().size() == 2);

    Roster roster;
    CHECK(roster.setDefinitions(definitions));
    CHECK(roster.beginNewGame({character_ids::Vanguard}));
    Party party;
    CHECK(party.setMembers({character_ids::Vanguard}));
    CHECK(!roster.damage(character_ids::Vanguard, 5));

    std::vector<std::string> messages;
    int managementOpens = 0;
    WorldEventPresentation presentation;
    presentation.message = [&](const std::string& message) { messages.push_back(message); };
    presentation.recruit = [&](CharacterId id) { return roster.recruit(id); };
    presentation.openPartyManagement = [&] { ++managementOpens; };
    int keys = 0;
    CHECK(dispatchWorldEvent(events, {EventTriggerType::InteractObject, 3, 3, "recruit.ember.object"},
                                     dungeon, keys, presentation).eventsRun == 1);
    CHECK(roster.find(recruit.id)->status == CharacterStatus::Reserve);
    CHECK(roster.find(character_ids::Vanguard)->hp == 37);
    CHECK(messages.back() == recruit.recruitmentText);

    // Repeat interaction never duplicates or heals a recruit; the runtime gives
    // a truthful unavailable response rather than consuming persistent state.
    CHECK(!roster.damage(recruit.id, 7));
    CHECK(dispatchWorldEvent(events, {EventTriggerType::InteractObject, 3, 3, "recruit.ember.object"},
                                     dungeon, keys, presentation).eventsRun == 1);
    CHECK(roster.find(recruit.id)->hp == 31);
    CHECK(messages.back() == "This character is already recruited or unavailable.");

    CHECK(dispatchWorldEvent(events, {EventTriggerType::InteractObject, 4, 3, "party.campfire"},
                                     dungeon, keys, presentation).eventsRun == 1);
    CHECK(managementOpens == 1);
    CHECK(messages.back() == "Choose who travels onward.");

    PlayerState player{level.spawnX, level.spawnY, level.spawnDirection};
    CHECK(SaveSystem::save(savePath, player, roster, party, dungeon, keys, 2, 11, &events));
    Roster restoredRoster;
    CHECK(restoredRoster.setDefinitions(definitions));
    Party restoredParty;
    Dungeon restoredDungeon{loadedLevel};
    EventRuntime restoredEvents;
    PlayerState restoredPlayer;
    int restoredKeys{}, restoredPotions{}, restoredXp{};
    CHECK(SaveSystem::load(savePath, restoredPlayer, restoredRoster, restoredParty, restoredDungeon,
                           restoredKeys, restoredPotions, restoredXp, &restoredEvents));
    CHECK(restoredRoster.find(recruit.id)->status == CharacterStatus::Reserve);
    CHECK(restoredRoster.find(recruit.id)->hp == 31);
    CHECK(restoredRoster.find(character_ids::Vanguard)->hp == 37);
    CHECK(restoredParty.members() == std::vector<CharacterId>{character_ids::Vanguard});
    CHECK(restoredPotions == 2 && restoredXp == 11);

    std::filesystem::remove(levelPath);
    std::filesystem::remove(savePath);
    return 0;
}
