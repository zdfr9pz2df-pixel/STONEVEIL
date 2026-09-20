#include "CampaignState.hpp"
#include "Character.hpp"
#include "LevelIO.hpp"
#include "SaveSystem.hpp"
#include "StoryState.hpp"
#include "WorldEvents.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(1); } } while (false)

int main() {
    using namespace sv;
    std::string error;
    LevelDefinition gatehouseDefinition, underkeepDefinition;
    CHECK(LevelIO::load(std::string{STONEVEIL_SOURCE_DIR} + "/content/levels/gatehouse.svl",
                        gatehouseDefinition, error));
    CHECK(LevelIO::load(std::string{STONEVEIL_SOURCE_DIR} + "/content/levels/underkeep.svl",
                        underkeepDefinition, error));

    Dungeon gatehouse{gatehouseDefinition};
    EventRuntime gatehouseEvents;
    CHECK(configureWorldEvents(gatehouse, gatehouseEvents));
    int keys = 2;
    std::vector<std::string> lore;
    WorldEventPresentation presentation;
    presentation.message = [&](const auto& text) { lore.push_back(text); };
    CHECK(dispatchWorldEvent(gatehouseEvents,
        {EventTriggerType::InteractObject, 3, 2, "note.gatehouse.watch-order"},
        gatehouse, keys, presentation).eventsRun == 1);
    CHECK(!lore.empty());
    CHECK(dispatchWorldEvent(gatehouseEvents,
        {EventTriggerType::InteractObject, 7, 4, "door.gatehouse.main-lock"},
        gatehouse, keys, presentation).eventsRun >= 1);
    CHECK(gatehouse.tile(7, 4) == Tile::DoorOpen);
    gatehouse.pickups().front().taken = true;

    CampaignState campaign;
    CHECK(campaign.capture(gatehouse, gatehouseEvents));

    std::vector<CharacterDefinition> definitions;
    CHECK(CharacterCatalogIO::load(std::string{STONEVEIL_SOURCE_DIR} +
        "/content/characters/characters.svc", definitions, error));
    Roster roster;
    CHECK(roster.setDefinitions(definitions));
    CHECK(roster.beginNewGame({1001, 1002, 1003}));
    CHECK(roster.recruit(2001));
    CHECK(!roster.damage(1001, 9));
    CHECK(roster.damage(1003, 999));
    roster.find(1001)->xp = 17;
    roster.find(2001)->xp = 6;
    CHECK(roster.setActiveParty({1001, 1002, 2001}));
    Party party;
    CHECK(party.setMembers({1001, 1002, 2001}));

    auto warning = std::find_if(underkeepDefinition.triggers.begin(), underkeepDefinition.triggers.end(),
        [](const auto& trigger) { return trigger.id == "trigger.underkeep.note"; });
    CHECK(warning != underkeepDefinition.triggers.end());
    warning->setFlag = "underkeep.warning-read";
    StoryTrigger consequence{"trigger.underkeep.warning-echo", TriggerEvent::InteractObject,
        3, 2, "note.underkeep.masons", true, "The warning now has meaning."};
    consequence.requiredFlag = "underkeep.warning-read";
    consequence.setFlag = "underkeep.echo-understood";
    underkeepDefinition.triggers.push_back(consequence);
    Dungeon underkeep{underkeepDefinition};
    EventRuntime underkeepEvents;
    CHECK(configureWorldEvents(underkeep, underkeepEvents));
    PlayerState player{2, 2, 1};
    StoryState storyState;
    CHECK(storyState.set("gatehouse.watch-order-read"));
    CHECK(storyState.set("underkeep.elska-recruited"));
    CHECK(dispatchWorldEvent(underkeepEvents,
        {EventTriggerType::InteractObject, 3, 2, "note.underkeep.masons"},
        underkeep, keys, presentation, &storyState).eventsRun == 2);
    CHECK(storyState.value("underkeep.warning-read"));
    CHECK(storyState.value("underkeep.echo-understood"));
    const auto savePath = std::filesystem::current_path() /
        ("campaign-loop-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sav");
    CHECK(SaveSystem::save(savePath.string(), player, roster, party, underkeep,
                           keys, 4, 88, &underkeepEvents, &campaign, &storyState));

    std::string savedLevel;
    CHECK(SaveSystem::savedLevelId(savePath.string(), savedLevel));
    CHECK(savedLevel == "underkeep.level-1");
    Dungeon loadedDungeon{underkeepDefinition};
    EventRuntime loadedEvents;
    CampaignState loadedCampaign;
    StoryState loadedStoryState;
    Roster loadedRoster;
    CHECK(loadedRoster.setDefinitions(definitions));
    Party loadedParty;
    PlayerState loadedPlayer;
    int loadedKeys{}, loadedPotions{}, loadedXp{};
    CHECK(SaveSystem::load(savePath.string(), loadedPlayer, loadedRoster, loadedParty,
                           loadedDungeon, loadedKeys, loadedPotions, loadedXp,
                           &loadedEvents, &loadedCampaign, &loadedStoryState));
    CHECK(loadedDungeon.levelId() == "underkeep.level-1");
    CHECK(loadedParty.members() == std::vector<CharacterId>({1001, 1002, 2001}));
    CHECK(loadedRoster.find(1001)->hp == 33 && loadedRoster.find(1001)->xp == 17);
    CHECK(loadedRoster.find(2001)->status == CharacterStatus::Active && loadedRoster.find(2001)->xp == 6);
    CHECK(loadedRoster.find(1003)->status == CharacterStatus::Dead);
    CHECK(loadedKeys == 1 && loadedPotions == 4 && loadedXp == 88);
    CHECK(loadedStoryState.value("gatehouse.watch-order-read"));
    CHECK(loadedStoryState.value("underkeep.elska-recruited"));

    Dungeon restoredGatehouse{gatehouseDefinition};
    EventRuntime restoredGatehouseEvents;
    CHECK(configureWorldEvents(restoredGatehouse, restoredGatehouseEvents));
    CHECK(loadedCampaign.restore("gatehouse.level-1", restoredGatehouse, restoredGatehouseEvents));
    CHECK(restoredGatehouse.tile(7, 4) == Tile::DoorOpen);
    CHECK(restoredGatehouse.pickups().front().taken);
    CHECK(restoredGatehouseEvents.firedCount("trigger.gatehouse.watch-order") == 1);
    std::filesystem::remove(savePath);
    return 0;
}
