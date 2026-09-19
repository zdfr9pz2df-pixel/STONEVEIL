#include "LevelIO.hpp"
#include "SaveSystem.hpp"
#include "WorldEvents.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(1); } } while (false)

using namespace sv;

namespace {
void writeFixture(const char* path, const std::string& text) {
    std::ofstream file(path);
    file << text;
    CHECK(static_cast<bool>(file));
}

std::string legacyLevel(int version) {
    std::ostringstream out;
    out << "STONEVEIL_LEVEL " << version << "\nID \"compat.level\"\nNAME \"Compatibility\"\n";
    if (version >= 5) out << "MUSIC \"\"\n";
    out << "SIZE 4 4\nSPAWN 1 1 1\nDEFAULT_WALL \"material.wall.cave.mossy-rock\"\n"
        "DEFAULT_FLOOR \"material.floor.cave.uneven-stone\"\nCEILING_MODE MATERIAL\n"
        "DEFAULT_CEILING \"material.ceiling.cave.rocky\"\nMAP\n####\n#.D#\n#.E#\n####\nEND_MAP\n"
        "PICKUPS 0\nENEMIES 0\nSURFACE_OVERRIDES 0\n";
    if (version >= 2) out << "LIGHTS 0\n";
    if (version >= 4) out << "WATER 0\n";
    if (version >= 6) out << "DOORS 1\n\"door.legacy.1\" 2 1 DOOR 1\nOBJECTS 0\nROOMS 0\nTRIGGERS 0\n";
    out << "END\n";
    return out.str();
}

// Independent old-format writer, not the current production serializer.
std::string legacySave(int version, const Dungeon& dungeon) {
    std::ostringstream out;
    if (version == 0) return "1 1 1 2 3 10\n42 32 0\n";
    out << "STONEVEIL_SAVE " << version << "\n1 1 1 2 3 10\n3\n"
        "1001 37 0 2\n1002 32 0 1\n1003 0 0 3\n";
    if (version >= 4) out << "PARTY 6 ";
    out << "1 1001\n";
    if (version >= 3) {
        out << "LEVEL compat.level\nTILES 4 4\n";
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) out << static_cast<int>(dungeon.tile(x, y)) << ' ';
            out << '\n';
        }
        out << "PICKUPS 0\nENEMIES 0\nEND\n";
    }
    return out.str();
}

void oldFormatsAndMalformedInput() {
    const char* path = "compat-level.svl";
    LevelDefinition level;
    std::string error;
    for (int version = 1; version <= 6; ++version) {
        writeFixture(path, legacyLevel(version));
        CHECK(LevelIO::load(path, level, error));
        CHECK(level.doors.size() == 1 && level.doors[0].id == "door.legacy.1");
        CHECK(LevelIO::save(path, level, error));
        LevelDefinition upgraded;
        CHECK(LevelIO::load(path, upgraded, error));
        CHECK(upgraded.map == level.map && upgraded.doors[0].id == level.doors[0].id);
    }
    auto malformed = legacyLevel(1);
    malformed.replace(malformed.find("#.D#"), 4, "#");
    writeFixture(path, malformed);
    CHECK(!LevelIO::load(path, level, error));
    CHECK(level.map[1] == "#.D#"); // failed load leaves caller's document intact
    std::remove(path);

    const char* save = "compat-save.sav";
    for (int version : {0, 2, 3, 4}) {
        Dungeon dungeon{level};
        writeFixture(save, legacySave(version, dungeon));
        PlayerState player;
        Roster roster;
        Party party;
        int keys{}, potions{}, xp{};
        EventRuntime events;
        CHECK(SaveSystem::load(save, player, roster, party, dungeon, keys, potions, xp, &events));
        CHECK(keys == 2 && potions == 3 && xp == 10);
        CHECK(party.capacity() == 3);
        CHECK(roster.find(1003)->status == CharacterStatus::Dead);
        CHECK(!roster.recruit(1003));
        CHECK(player.eyeX() == 1.5 && player.eyeY() == 1.5);
    }
    std::remove(save);
}

void runtimeDoorStoryAndIdentityRoundtrip() {
    auto level = levelOneDefinition();
    level.triggers.push_back({"test.first", TriggerEvent::EnterCell, 2, 2, {}, true, "First line"});
    level.triggers.push_back({"test.second", TriggerEvent::EnterCell, 2, 2, {}, true, "Second line"});
    Dungeon dungeon{level};
    EventRuntime events;
    CHECK(configureWorldEvents(dungeon, events));
    std::vector<std::string> messages;
    WorldEventPresentation presentation;
    presentation.message = [&](const auto& message) { messages.push_back(message); };
    int keys = 0;
    CHECK(dispatchWorldEvent(events, {EventTriggerType::PlayerEnterTile, 2, 2}, dungeon, keys, presentation).eventsRun == 2);
    CHECK(messages == std::vector<std::string>({"First line", "Second line"}));
    const auto door = level.doors.front();
    CHECK(dispatchWorldEvent(events, {EventTriggerType::InteractObject, door.x, door.y, door.id},
        dungeon, keys, presentation).consumed);
    CHECK(dungeon.tile(door.x, door.y) == Tile::DoorClosed && keys == 0);
    keys = 2;
    CHECK(dispatchWorldEvent(events, {EventTriggerType::InteractObject, door.x, door.y, door.id},
        dungeon, keys, presentation).consumed);
    CHECK(dungeon.tile(door.x, door.y) == Tile::DoorOpen && keys == 1);
    dispatchWorldEvent(events, {EventTriggerType::InteractObject, door.x, door.y, door.id}, dungeon, keys, presentation);
    CHECK(keys == 1);

    PlayerState player{2, 2, 1};
    Party party;
    Roster roster;
    CHECK(roster.beginNewGame({1001}) && party.setMembers({1001}));
    roster.damage(1001, 5);
    CHECK(!roster.recruit(1001));
    CHECK(roster.find(1001)->hp == 37);
    CHECK(roster.setActiveParty({1002}) && party.setMembers({1002}));
    CHECK(roster.find(1001)->status == CharacterStatus::Reserve && roster.find(1001)->hp == 37);
    CHECK(roster.setActiveParty({1001}) && party.setMembers({1001}));
    CHECK(roster.find(1001)->hp == 37);
    CHECK(!roster.setActiveParty({1001, 1002, 1003, 9000}));
    CHECK(roster.damage(1003, 999));
    CHECK(!roster.recruit(1003));
    CHECK(!party.setCapacity(4));
    dungeon.enemies()[0].hp = 7;
    dungeon.enemies()[0].typeId = "enemy.brute";
    dungeon.pickups()[0].taken = true;
    const char* save = "event-save.sav";
    CHECK(SaveSystem::save(save, player, roster, party, dungeon, keys, 1, 0, &events));

    // Reordering authored lists must not reassign state/archetype to another ID.
    level.enemies[0].typeId = "enemy.brute";
    std::reverse(level.enemies.begin(), level.enemies.end());
    std::reverse(level.pickups.begin(), level.pickups.end());
    Dungeon loaded{level};
    EventRuntime loadedEvents;
    int potions{}, xp{};
    CHECK(SaveSystem::load(save, player, roster, party, loaded, keys, potions, xp, &loadedEvents));
    CHECK(loaded.enemies()[0].id == dungeon.enemies()[0].id);
    CHECK(loaded.enemies()[0].typeId == "enemy.brute" && loaded.enemies()[0].hp == 7);
    CHECK(loaded.pickups()[0].id == dungeon.pickups()[0].id && loaded.pickups()[0].taken);
    CHECK(roster.find(1001)->hp == 37 && roster.find(1003)->status == CharacterStatus::Dead);
    messages.clear();
    CHECK(dispatchWorldEvent(loadedEvents, {EventTriggerType::PlayerEnterTile, 2, 2}, loaded, keys, presentation).eventsRun == 0);
    CHECK(messages.empty());

    writeFixture(save, "STONEVEIL_SAVE 5\n2 2 1 0 0 0\n1\n1001 42 0 2\nPARTY 4 1 1001\n");
    CHECK(!SaveSystem::load(save, player, roster, party, loaded, keys, potions, xp, &loadedEvents));
    CHECK(roster.find(1001)->hp == 37 && keys == 1);
    CHECK(loadedEvents.firedCount("test.first") == 1);
    std::remove(save);

    level.triggers.back().id = level.triggers.front().id;
    CHECK(!LevelIO::validate(level).empty());
    level.triggers.back().id = door.id + ".interact.success";
    CHECK(!LevelIO::validate(level).empty());
}
} // namespace

int main() {
    oldFormatsAndMalformedInput();
    runtimeDoorStoryAndIdentityRoundtrip();
    return 0;
}
