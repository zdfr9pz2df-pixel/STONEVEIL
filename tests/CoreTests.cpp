#include "CampaignInventory.hpp"
#include "Campaign.hpp"
#include "Character.hpp"
#include "CombatSystem.hpp"
#include "CombatTuning.hpp"
#include "Formation.hpp"
#include "ActionGate.hpp"
#include "Dungeon.hpp"
#include "EnemyType.hpp"
#include "LevelBlueprint.hpp"
#include "LevelIO.hpp"
#include "Lighting.hpp"
#include "Material.hpp"
#include "Party.hpp"
#include "Player.hpp"
#include "Portrait.hpp"
#include "Roster.hpp"
#include "SaveSystem.hpp"
#include "StoryState.hpp"
#include "WorldAuthoring.hpp"
#include "WorldEvents.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::cerr << "Check failed at line " << __LINE__ << ": " #expression << '\n'; \
            return 1; \
        } \
    } while (false)

namespace {

bool canReach(const sv::Dungeon& dungeon, int startX, int startY, int targetX, int targetY) {
    std::vector<unsigned char> visited(static_cast<std::size_t>(dungeon.width()) *
                                       static_cast<std::size_t>(dungeon.height()));
    const auto indexOf = [&dungeon](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(dungeon.width()) + static_cast<std::size_t>(x);
    };
    std::queue<std::pair<int, int>> frontier;
    frontier.push({startX, startY});
    visited[indexOf(startX, startY)] = 1;
    constexpr int dx[4] = {0, 1, 0, -1};
    constexpr int dy[4] = {-1, 0, 1, 0};

    while (!frontier.empty()) {
        const auto [x, y] = frontier.front();
        frontier.pop();
        if (x == targetX && y == targetY) return true;
        for (int direction = 0; direction < 4; ++direction) {
            const int nextX = x + dx[direction];
            const int nextY = y + dy[direction];
            if (!dungeon.inBounds(nextX, nextY) || visited[indexOf(nextX, nextY)] || dungeon.blocksMovement(nextX, nextY)) continue;
            visited[indexOf(nextX, nextY)] = 1;
            frontier.push({nextX, nextY});
        }
    }
    return false;
}

} // namespace

int main() {
    using namespace sv;

    const auto starters = starterCharacterIds();
    CHECK(starters.size() == 3);
    CHECK(starters[0] != starters[1]);
    CHECK(findCharacterDefinition(character_ids::Vanguard)->maxHp == 42);

    StoryState story;
    CHECK(story.set("gatehouse.watch-order-read"));
    CHECK(story.value("gatehouse.watch-order-read"));
    CHECK(story.set("gatehouse.watch-order-read", false));
    CHECK(story.contains("gatehouse.watch-order-read") && !story.value("gatehouse.watch-order-read"));
    CHECK(!story.set("invalid flag name"));
    CHECK(!story.replace({{"also invalid", true}}));
    CHECK(story.value("gatehouse.watch-order-read") == false);

    for (const auto& material : materialCatalog()) {
        if (!material.texturePath.empty()) {
            std::ifstream texture(std::string(STONEVEIL_SOURCE_DIR) + "/" + material.texturePath, std::ios::binary);
            CHECK(texture.good());
        }
    }
    for (const auto& doorTexture : {
             "content/textures/doors/iron-banded-wooden-door.png",
             "content/textures/doors/secret-stone-door.png",
             "content/textures/doors/rusted-iron-gate.png",
         }) {
        std::ifstream texture(std::string(STONEVEIL_SOURCE_DIR) + "/" + doorTexture, std::ios::binary);
        CHECK(texture.good());
    }

    Party party;
    CHECK(party.setMembers({starters[0]}));
    CHECK(party.add(starters[1]));
    CHECK(party.add(starters[2]));
    CHECK(!party.add(9001));
    CHECK(!party.setMembers({starters[0], starters[0]}));
    CHECK(party.capacity() == Party::InitialCapacity);
    CHECK(!party.increaseCapacity(2));
    CHECK(party.capacity() == 3);
    CHECK(!party.add(2001));
    CHECK(!party.add(2002));
    CHECK(!party.increaseCapacity(2));
    CHECK(!party.setCapacity(4));

    Roster roster;
    Roster recruitmentRoster;
    CHECK(recruitmentRoster.recruit(starters[0]));
    CHECK(recruitmentRoster.find(starters[0])->status == CharacterStatus::Reserve);
    CHECK(roster.beginNewGame({starters[0], starters[2]}));
    CHECK(roster.find(starters[0])->status == CharacterStatus::Active);
    CHECK(roster.find(starters[1])->status == CharacterStatus::Reserve);
    CHECK(roster.damage(starters[2], 999));
    CHECK(roster.find(starters[2])->status == CharacterStatus::Dead);
    CHECK(!roster.heal(starters[2], 10));

    PlayerState player{2, 2, 1};
    CHECK(std::abs(player.eyeX() - 2.5) < 0.0001);
    CHECK(std::abs(player.eyeY() - 2.5) < 0.0001);
    CHECK(player.movementTarget(1, 0) == std::make_pair(3, 2));
    CHECK(player.movementTarget(0, 1) == std::make_pair(2, 3));

    Dungeon dungeon;
    CHECK(dungeon.inBounds(player.x(), player.y()));
    CHECK(!dungeon.blocksMovement(player.x(), player.y()));
    CHECK(dungeon.width() == 16 && dungeon.height() == 16);
    CHECK(dungeon.tile(13, 13) == Tile::Exit);
    CHECK(dungeon.tile(7, 9) == Tile::SecretDoorClosed);
    CHECK(canReach(dungeon, dungeon.spawnX(), dungeon.spawnY(), 13, 13));
    for (const auto& pickup : dungeon.pickups()) {
        CHECK(dungeon.inBounds(pickup.x, pickup.y));
        CHECK(!dungeon.blocksMovement(pickup.x, pickup.y));
    }
    CHECK(dungeon.openDoor(7, 4, true));
    CHECK(dungeon.revealSecret(7, 9));
    CHECK(dungeon.tile(7, 9) == Tile::DoorOpen);

    const char* savePath = "stoneveil_core_test.sav";
    Party savedParty;
    CHECK(savedParty.setCapacity(3));
    CHECK(savedParty.setMembers({starters[0], starters[1]}));
    Roster savedRoster;
    CHECK(savedRoster.beginNewGame(savedParty.members()));
    CHECK(!savedRoster.damage(starters[1], 5));
    CHECK(savedRoster.damage(starters[2], 999));
    dungeon.pickups()[0].taken = true;
    CHECK(SaveSystem::save(savePath, player, savedRoster, savedParty, dungeon, 1, 2, 75));

    PlayerState loadedPlayer;
    Roster loadedRoster;
    Party loadedParty;
    Dungeon loadedDungeon;
    int loadedKeys{};
    int loadedPotions{};
    int loadedXp{};
    CHECK(SaveSystem::load(savePath, loadedPlayer, loadedRoster, loadedParty, loadedDungeon,
                           loadedKeys, loadedPotions, loadedXp));
    CHECK(loadedPlayer.x() == player.x() && loadedPlayer.eyeX() == 2.5);
    CHECK(loadedParty.capacity() == 3);
    CHECK(loadedParty.members() == savedParty.members());
    CHECK(loadedRoster.find(starters[1])->hp == findCharacterDefinition(starters[1])->maxHp - 5);
    CHECK(loadedRoster.find(starters[2])->status == CharacterStatus::Dead);
    CHECK(loadedDungeon.tile(7, 4) == Tile::DoorOpen);
    CHECK(loadedDungeon.tile(7, 9) == Tile::DoorOpen);
    CHECK(loadedDungeon.pickups()[0].taken);
    CHECK(loadedDungeon.pickups()[0].id == dungeon.pickups()[0].id);
    CHECK(loadedDungeon.enemies()[0].id == dungeon.enemies()[0].id);
    CHECK(loadedKeys == 1 && loadedPotions == 2 && loadedXp == 75);
    std::remove(savePath);

    const std::string levelPath = std::string(STONEVEIL_SOURCE_DIR) + "/content/levels/gatehouse.svl";
    LevelDefinition loadedLevel;
    std::string levelError;
    CHECK(LevelIO::load(levelPath, loadedLevel, levelError));
    CHECK(LevelIO::validate(loadedLevel).empty());
    CHECK(loadedLevel.id == "gatehouse.level-1");
    CHECK(loadedLevel.musicPath == "content/audio/music/gatehouse-drone.wav");
    const auto* wallMaterial = findMaterial(loadedLevel.surfaces.wallMaterial);
    CHECK(wallMaterial != nullptr);
    CHECK(wallMaterial->surface == SurfaceKind::Wall);
    const auto* floorMaterial = findMaterial(loadedLevel.surfaces.floorMaterial);
    CHECK(floorMaterial != nullptr);
    CHECK(floorMaterial->surface == SurfaceKind::Floor);
    CHECK(loadedLevel.map[9][7] == 'S');
    const std::size_t originalSurfaceOverrideCount = loadedLevel.surfaceOverrides.size();
    CHECK(originalSurfaceOverrideCount >= 6);
    CHECK(findMaterial(loadedLevel.surfaceOverrides[0].materialId) != nullptr);

    const std::string meadHallBlueprint =
        "STONEVEIL_BLUEPRINT 1\n"
        "NAME \"THE LOW MEAD HALL\"\n"
        "SIZE 26 18\n"
        "DEFAULTS WALL timber.hewn FLOOR timber.planks CEILING timber.rafters\n"
        "FILL wall\n"
        "ROOM 2 2 22 12 floor\n"
        "ROOM 10 14 6 2 floor\n"
        "DOOR 12 13\n"
        "SPAWN 13 15 NORTH\n"
        "EXIT 13 2\n"
        "LIGHT brazier 7 7\n"
        "LIGHT candle 22 4\n"
        "PICKUP key 5 11\n"
        "PICKUP potion 20 11\n"
        "ENEMY 8 6 18\n"
        "ENEMY 18 6 18\n"
        "SURFACE floor 5 5 16 1 mud\n"
        "WATER 5 8 3 1 2 EAST 4\n"
        "END\n";
    LevelDefinition blueprintLevel;
    CHECK(LevelBlueprint::parse(meadHallBlueprint, blueprintLevel, levelError));
    CHECK(blueprintLevel.name == "THE LOW MEAD HALL");
    CHECK(blueprintLevel.width == 26 && blueprintLevel.height == 18);
    CHECK(blueprintLevel.spawnX == 13 && blueprintLevel.spawnY == 15 && blueprintLevel.spawnDirection == 0);
    CHECK(blueprintLevel.map[2][13] == 'E');
    CHECK(blueprintLevel.map[13][12] == 'D');
    CHECK(blueprintLevel.lights.size() == 2);
    CHECK(blueprintLevel.lights.front().lightId == "light.torch.pitch-brazier");
    CHECK(blueprintLevel.pickups.size() == 2);
    CHECK(blueprintLevel.enemies.size() == 2);
    CHECK(blueprintLevel.water.size() == 3);
    CHECK(blueprintLevel.water.front().depth == 2);
    CHECK(blueprintLevel.water.front().flow == WaterFlow::East);
    CHECK(blueprintLevel.water.front().volumeId == 4);
    CHECK(LevelIO::validate(blueprintLevel).empty());
    const Dungeon blueprintDungeon{blueprintLevel};
    CHECK(blueprintDungeon.waterDepthAt(5, 8) == 2);
    CHECK(blueprintDungeon.waterFlowAt(5, 8) == WaterFlow::East);
    CHECK(blueprintDungeon.waterVolumeAt(5, 8) == 4);
    CHECK(!blueprintDungeon.hasWaterAt(4, 8));
    CHECK(!LevelBlueprint::parse("STONEVEIL_BLUEPRINT 1\nSIZE 6 6\nEND\n", blueprintLevel, levelError));

    const char* waterRoundTripPath = "stoneveil_water_round_trip.svl";
    CHECK(LevelIO::save(waterRoundTripPath, blueprintLevel, levelError));
    LevelDefinition waterRoundTrip;
    CHECK(LevelIO::load(waterRoundTripPath, waterRoundTrip, levelError));
    CHECK(waterRoundTrip.water.size() == blueprintLevel.water.size());
    CHECK(waterRoundTrip.water.front().flow == WaterFlow::East);
    CHECK(waterRoundTrip.water.front().volumeId == 4);
    std::remove(waterRoundTripPath);

    LevelDefinition badWater = blueprintLevel;
    badWater.water.push_back({1, 1, 4, WaterFlow::Still, 0});
    CHECK(!LevelIO::validate(badWater).empty());

    loadedLevel.surfaceOverrides.push_back({1, 1, "material.floor.cave.mud", SurfaceKind::Floor, CeilingMode::Material});
    const char* levelRoundTripPath = "stoneveil_level_round_trip.svl";
    CHECK(LevelIO::save(levelRoundTripPath, loadedLevel, levelError));
    LevelDefinition roundTrippedLevel;
    CHECK(LevelIO::load(levelRoundTripPath, roundTrippedLevel, levelError));
    CHECK(roundTrippedLevel.surfaceOverrides.size() == originalSurfaceOverrideCount + 1);
    CHECK(roundTrippedLevel.surfaceOverrides.back().materialId == "material.floor.cave.mud");
    CHECK(roundTrippedLevel.musicPath == loadedLevel.musicPath);
    const Dungeon surfacedDungeon{roundTrippedLevel};
    CHECK(surfacedDungeon.musicPath() == loadedLevel.musicPath);
    CHECK(surfacedDungeon.materialAt(1, 1, SurfaceKind::Floor) == "material.floor.cave.mud");
    CHECK(surfacedDungeon.ceilingModeAt(1, 1) == CeilingMode::Material);
    std::remove(levelRoundTripPath);

    LevelDefinition variableLevel;
    variableLevel.id = "test.variable-size";
    variableLevel.name = "VARIABLE SIZE TEST";
    variableLevel.width = 24;
    variableLevel.height = 10;
    variableLevel.map.assign(static_cast<std::size_t>(variableLevel.height),
                             std::string(static_cast<std::size_t>(variableLevel.width), '.'));
    for (int y = 0; y < variableLevel.height; ++y) {
        variableLevel.map[static_cast<std::size_t>(y)].front() = '#';
        variableLevel.map[static_cast<std::size_t>(y)].back() = '#';
    }
    std::fill(variableLevel.map.front().begin(), variableLevel.map.front().end(), '#');
    std::fill(variableLevel.map.back().begin(), variableLevel.map.back().end(), '#');
    variableLevel.spawnX = 1;
    variableLevel.spawnY = 1;
    variableLevel.map[8][22] = 'E';
    CHECK(LevelIO::validate(variableLevel).empty());
    const char* variableLevelPath = "stoneveil_variable_level.svl";
    CHECK(LevelIO::save(variableLevelPath, variableLevel, levelError));
    LevelDefinition loadedVariableLevel;
    CHECK(LevelIO::load(variableLevelPath, loadedVariableLevel, levelError));
    CHECK(loadedVariableLevel.width == 24 && loadedVariableLevel.height == 10);
    const Dungeon variableDungeon{loadedVariableLevel};
    CHECK(variableDungeon.width() == 24 && variableDungeon.height() == 10);
    CHECK(canReach(variableDungeon, variableDungeon.spawnX(), variableDungeon.spawnY(), 22, 8));

    const char* variableSavePath = "stoneveil_variable_level.sav";
    PlayerState variablePlayer{1, 1, 1};
    Roster variableRoster;
    CHECK(variableRoster.beginNewGame({starters[0]}));
    Party variableParty;
    CHECK(variableParty.setMembers({starters[0]}));
    CHECK(SaveSystem::save(variableSavePath, variablePlayer, variableRoster, variableParty,
                           variableDungeon, 0, 1, 0));
    PlayerState loadedVariablePlayer;
    Roster loadedVariableRoster;
    Party loadedVariableParty;
    Dungeon loadedVariableDungeon{loadedVariableLevel};
    int variableKeys{};
    int variablePotions{};
    int variableXp{};
    CHECK(SaveSystem::load(variableSavePath, loadedVariablePlayer, loadedVariableRoster,
                           loadedVariableParty, loadedVariableDungeon,
                           variableKeys, variablePotions, variableXp));
    CHECK(loadedVariableDungeon.width() == 24 && loadedVariableDungeon.height() == 10);
    std::remove(variableSavePath);
    std::remove(variableLevelPath);

    LevelDefinition invalidLevel = loadedLevel;
    invalidLevel.map[0] = "short";
    CHECK(!LevelIO::validate(invalidLevel).empty());
    invalidLevel = loadedLevel;
    invalidLevel.surfaceOverrides.push_back({1, 1, "material.wall.cave.mossy-rock", SurfaceKind::Wall, CeilingMode::Material});
    CHECK(!LevelIO::validate(invalidLevel).empty());
    invalidLevel = loadedLevel;
    invalidLevel.musicPath = "../outside.wav";
    CHECK(!LevelIO::validate(invalidLevel).empty());
    invalidLevel.musicPath = "content/audio/music/not-a-track.txt";
    CHECK(!LevelIO::validate(invalidLevel).empty());

    // ---- editor-placed lights ------------------------------------------------

    CHECK(!lightCatalog().empty());
    CHECK(findLight(defaultLightId()) != nullptr);
    CHECK(findLight("light.torch.iron-sconce") != nullptr);
    CHECK(findLight("light.torch.not-a-real-light") == nullptr);

    CHECK(std::abs(Lighting::falloff(0.0, 5.0f) - 1.0f) < 0.0001f);
    CHECK(Lighting::falloff(1.0, 5.0f) > Lighting::falloff(3.0, 5.0f));
    CHECK(Lighting::falloff(3.0, 5.0f) > 0.0f);
    CHECK(Lighting::falloff(5.0, 5.0f) == 0.0f);
    CHECK(Lighting::falloff(9.0, 5.0f) == 0.0f);
    CHECK(Lighting::falloff(1.0, 0.0f) == 0.0f);

    // Two chambers split by a solid wall at x == 4.
    LevelDefinition lightingFixture;
    lightingFixture.id = "test.lighting";
    lightingFixture.name = "LIGHTING TEST";
    lightingFixture.width = 9;
    lightingFixture.height = 5;
    lightingFixture.map = {
        "#########",
        "#...#...#",
        "#...#...#",
        "#...#...#",
        "#########",
    };
    lightingFixture.spawnX = 1;
    lightingFixture.spawnY = 1;
    lightingFixture.lights = {{2, 2, "light.torch.pitch-brazier"}};

    const Dungeon lightingDungeon{lightingFixture};
    CHECK(lightingDungeon.lights().size() == 1);

    const LightSample atSource = Lighting::sampleAt(lightingDungeon, 2.5, 2.5, 2, 2);
    CHECK(atSource.lit());
    CHECK(atSource.red >= atSource.green && atSource.green >= atSource.blue);

    const LightSample oneCellAway = Lighting::sampleAt(lightingDungeon, 1.5, 1.5, 1, 1);
    CHECK(oneCellAway.lit());
    CHECK(oneCellAway.red < atSource.red);

    // Behind the dividing wall nothing arrives, even well inside the radius.
    CHECK(!Lighting::sampleAt(lightingDungeon, 6.5, 2.5, 6, 2).lit());
    CHECK(!Lighting::reaches(lightingDungeon, 2, 2, 6, 2));
    CHECK(Lighting::reaches(lightingDungeon, 2, 2, 1, 1));

    // A closed door blocks light and opening it immediately restores the path.
    LevelDefinition doorFixture = lightingFixture;
    doorFixture.map[2][4] = 'D';
    Dungeon doorDungeon{doorFixture};
    CHECK(!Lighting::reaches(doorDungeon, 2, 2, 6, 2));
    CHECK(doorDungeon.openDoor(4, 2, true));
    CHECK(Lighting::reaches(doorDungeon, 2, 2, 6, 2));

    // A diagonal ray may not leak through the corner of even one wall cell.
    LevelDefinition cornerFixture;
    cornerFixture.width = 4;
    cornerFixture.height = 4;
    cornerFixture.map = {
        "####",
        "#.##",
        "#..#",
        "####",
    };
    CHECK(!Lighting::reaches(Dungeon{cornerFixture}, 1, 1, 2, 2));
    cornerFixture.map = {
        "####",
        "#..#",
        "##.#",
        "####",
    };
    CHECK(!Lighting::reaches(Dungeon{cornerFixture}, 1, 1, 2, 2));

    // A sconce sitting inside a wall cell still lights both faces of that wall.
    LevelDefinition sconceFixture = lightingFixture;
    sconceFixture.lights = {{4, 2, "light.torch.iron-sconce"}};
    const Dungeon sconceDungeon{sconceFixture};
    CHECK(Lighting::sampleAt(sconceDungeon, 3.5, 2.5, 3, 2).lit());
    CHECK(Lighting::sampleAt(sconceDungeon, 5.5, 2.5, 5, 2).lit());

    // Lights outside the map never reach the runtime dungeon.
    LevelDefinition strayFixture = lightingFixture;
    strayFixture.lights.push_back({99, 99, "light.torch.iron-sconce"});
    const Dungeon strayDungeon{strayFixture};
    CHECK(strayDungeon.lights().size() == 1);

    // The authored Gatehouse carries lights through to the runtime dungeon.
    CHECK(!loadedLevel.lights.empty());
    const Dungeon gatehouseDungeon{loadedLevel};
    CHECK(gatehouseDungeon.lights().size() == loadedLevel.lights.size());

    LevelDefinition lightValidation = loadedLevel;
    lightValidation.lights.push_back({1, 1, "light.torch.iron-sconce"});
    CHECK(LevelIO::validate(lightValidation).empty());
    lightValidation.lights.push_back({1, 1, "light.torch.tallow-candle"});
    CHECK(!LevelIO::validate(lightValidation).empty());

    lightValidation = loadedLevel;
    lightValidation.lights.push_back({1, 1, "light.torch.not-a-real-light"});
    CHECK(!LevelIO::validate(lightValidation).empty());

    lightValidation = loadedLevel;
    lightValidation.lights.push_back({99, 1, "light.torch.iron-sconce"});
    CHECK(!LevelIO::validate(lightValidation).empty());

    // Lights survive a level save/load round trip.
    const char* lightRoundTripPath = "stoneveil_light_round_trip.svl";
    LevelDefinition lightLevel = loadedLevel;
    lightLevel.lights.push_back({1, 1, "light.torch.tallow-candle"});
    CHECK(LevelIO::save(lightRoundTripPath, lightLevel, levelError));
    LevelDefinition reloadedLightLevel;
    CHECK(LevelIO::load(lightRoundTripPath, reloadedLightLevel, levelError));
    CHECK(reloadedLightLevel.lights.size() == lightLevel.lights.size());
    CHECK(reloadedLightLevel.lights.back().lightId == "light.torch.tallow-candle");
    CHECK(reloadedLightLevel.lights.back().x == 1 && reloadedLightLevel.lights.back().y == 1);
    std::remove(lightRoundTripPath);

    // Format version 1 files predate lights and must still load, then upgrade.
    const char* legacyLevelPath = "stoneveil_legacy_level.svl";
    {
        std::ofstream legacy(legacyLevelPath, std::ios::trunc);
        legacy << "STONEVEIL_LEVEL 1\n"
                  "ID \"test.legacy\"\n"
                  "NAME \"LEGACY LEVEL\"\n"
                  "SIZE 6 5\n"
                  "SPAWN 1 1 1\n"
                  "DEFAULT_WALL \"material.wall.cave.mossy-rock\"\n"
                  "DEFAULT_FLOOR \"material.floor.cave.uneven-stone\"\n"
                  "CEILING_MODE MATERIAL\n"
                  "DEFAULT_CEILING \"material.ceiling.cave.rocky\"\n"
                  "MAP\n"
                  "######\n"
                  "#....#\n"
                  "#...E#\n"
                  "#....#\n"
                  "######\n"
                  "END_MAP\n"
                  "PICKUPS 0\n"
                  "ENEMIES 0\n"
                  "SURFACE_OVERRIDES 0\n"
                  "END\n";
        CHECK(static_cast<bool>(legacy));
    }
    LevelDefinition legacyLevel;
    CHECK(LevelIO::load(legacyLevelPath, legacyLevel, levelError));
    CHECK(legacyLevel.id == "test.legacy");
    CHECK(legacyLevel.musicPath.empty());
    CHECK(legacyLevel.lights.empty());
    legacyLevel.musicPath = "content/audio/music/gatehouse-drone.wav";
    legacyLevel.lights.push_back({2, 2, "light.torch.iron-sconce"});
    CHECK(LevelIO::save(legacyLevelPath, legacyLevel, levelError));
    LevelDefinition upgradedLevel;
    CHECK(LevelIO::load(legacyLevelPath, upgradedLevel, levelError));
    CHECK(upgradedLevel.map == legacyLevel.map);
    CHECK(upgradedLevel.musicPath == "content/audio/music/gatehouse-drone.wav");
    CHECK(upgradedLevel.lights.size() == 1);
    CHECK(upgradedLevel.lights.front().lightId == "light.torch.iron-sconce");
    std::remove(legacyLevelPath);

    // Lights are authored data, so they also survive a gameplay save/load.
    const char* lightSavePath = "stoneveil_light.sav";
    Dungeon savedLightDungeon{loadedLevel};
    Party lightParty;
    CHECK(lightParty.setMembers({starters[0]}));
    Roster lightRoster;
    CHECK(lightRoster.beginNewGame(lightParty.members()));
    PlayerState lightPlayer{loadedLevel.spawnX, loadedLevel.spawnY, 1};
    CHECK(SaveSystem::save(lightSavePath, lightPlayer, lightRoster, lightParty, savedLightDungeon, 0, 1, 0));
    Dungeon reloadedLightDungeon{loadedLevel};
    PlayerState reloadedLightPlayer;
    Roster reloadedLightRoster;
    Party reloadedLightParty;
    int lightKeys{};
    int lightPotions{};
    int lightXp{};
    CHECK(SaveSystem::load(lightSavePath, reloadedLightPlayer, reloadedLightRoster, reloadedLightParty,
                           reloadedLightDungeon, lightKeys, lightPotions, lightXp));
    CHECK(reloadedLightDungeon.lights().size() == savedLightDungeon.lights().size());
    CHECK(reloadedLightDungeon.lights().front().lightId == savedLightDungeon.lights().front().lightId);
    std::remove(lightSavePath);

    Dungeon combatDungeon;
    combatDungeon.enemies() = {{3, 2, 1, true, 0.0f}};
    Party combatParty;
    CHECK(combatParty.setMembers({starters[0]}));
    Roster combatRoster;
    CHECK(combatRoster.beginNewGame(combatParty.members()));
    CombatSystem combat{1234};
    CHECK(combat.enemyAt(combatDungeon, 3, 2));
    CHECK(combat.frontEnemyIndex(combatDungeon, player, 1) == 0);
    const auto attackEvent = combat.partyAttack(combatRoster, combatParty, combatDungeon, player);
    CHECK(!combatDungeon.enemies()[0].alive);
    CHECK(attackEvent.xpGained == 25);
    CHECK(combatRoster.find(starters[0])->xp == 25);

    Dungeon lethalDungeon;
    lethalDungeon.enemies() = {{3, 2, 18, true, 0.0f}};
    Party lethalParty;
    CHECK(lethalParty.setMembers({starters[0]}));
    Roster lethalRoster;
    CHECK(lethalRoster.beginNewGame(lethalParty.members()));
    lethalRoster.find(starters[0])->hp = 1;
    CombatSystem lethalCombat{5678};
    // 0.2 change: enemy attacks are telegraphed, so the blow lands only after
    // the windup elapses. This used to resolve in a single zero-length tick.
    CombatEvent deathEvent;
    for (int step = 0; step < 4 && !deathEvent.occurred(); ++step) {
        deathEvent = lethalCombat.updateEnemies(2.0f, lethalRoster, lethalParty, lethalDungeon, player);
    }
    CHECK(deathEvent.occurred());
    CHECK(lethalCombat.meleeResolutions() == 1);
    CHECK(lethalParty.empty());
    CHECK(lethalRoster.find(starters[0])->status == CharacterStatus::Dead);

    // ---- portrait reaction states -------------------------------------------

    CHECK(!portraitStateCatalog().empty());
    CHECK(findPortraitState("portrait.hit") != nullptr);
    CHECK(findPortraitState("portrait.not-real") == nullptr);
    for (const auto& portraitState : portraitStateCatalog()) {
        if (portraitState.fallbackId.empty()) continue;
        CHECK(findPortraitState(portraitState.fallbackId) != nullptr);
    }

    PortraitTrack portrait;
    const PortraitConditions portraitHealthy{false, 40, 40, false, false};
    const PortraitConditions portraitDark{false, 40, 40, false, true};
    const PortraitConditions portraitDarkEnemy{false, 40, 40, true, true};
    const PortraitConditions portraitWounded{false, 20, 40, false, false};
    const PortraitConditions portraitWoundedFighting{false, 20, 40, true, false};
    const PortraitConditions portraitCriticalFighting{false, 4, 40, true, false};
    const PortraitConditions portraitDead{true, 0, 40, true, false};

    CHECK(portrait.resolve(portraitHealthy) == "portrait.idle");
    CHECK(portrait.resolve(portraitDark) == "portrait.peering");
    CHECK(portrait.resolve(portraitDarkEnemy) == "portrait.alert");
    CHECK(portrait.resolve(portraitWounded) == "portrait.wounded");
    CHECK(portrait.resolve(portraitWoundedFighting) == "portrait.alert");
    CHECK(portrait.resolve(portraitCriticalFighting) == "portrait.critical");
    CHECK(portrait.resolve(portraitDead) == "portrait.dead");

    CHECK(portrait.trigger("portrait.hit"));
    CHECK(portrait.resolve(portraitHealthy) == "portrait.hit");
    portrait.tick(0.3f);
    CHECK(portrait.resolve(portraitHealthy) == "portrait.hit");
    portrait.tick(0.4f);
    CHECK(portrait.resolve(portraitHealthy) == "portrait.idle");

    CHECK(portrait.trigger("portrait.hit"));
    CHECK(portrait.resolve(portraitDead) == "portrait.dead");
    portrait.clear();

    CHECK(portrait.trigger("portrait.hit"));
    CHECK(!portrait.trigger("portrait.talking"));
    CHECK(portrait.resolve(portraitHealthy) == "portrait.hit");
    CHECK(portrait.trigger("portrait.ally-down"));
    CHECK(portrait.resolve(portraitHealthy) == "portrait.ally-down");
    CHECK(!portrait.trigger("portrait.idle"));

    const std::vector<std::string> portraitTierZero = {
        "portrait.dead", "portrait.idle", "portrait.alert",
        "portrait.wounded", "portrait.critical", "portrait.hit",
    };
    CHECK(resolvePortraitFallback("portrait.hurt-badly", portraitTierZero) == "portrait.hit");
    CHECK(resolvePortraitFallback("portrait.spell-fizzle", portraitTierZero) == "portrait.alert");
    CHECK(resolvePortraitFallback("portrait.peering", portraitTierZero) == "portrait.idle");
    CHECK(resolvePortraitFallback("portrait.alert", portraitTierZero) == "portrait.alert");
    CHECK(resolvePortraitFallback("portrait.not-real", portraitTierZero).empty());
    CHECK(resolvePortraitFallback("portrait.idle", {}).empty());

    PortraitTrack portraitBarks;
    CHECK(portraitBarks.requestBark("portrait.alert"));
    CHECK(!portraitBarks.requestBark("portrait.alert"));
    portraitBarks.tick(3.0f);
    CHECK(!portraitBarks.requestBark("portrait.alert"));
    portraitBarks.tick(3.5f);
    CHECK(portraitBarks.requestBark("portrait.alert"));
    CHECK(!portraitBarks.requestBark("portrait.not-real"));

    // ---- 0.2 formation targeting --------------------------------------------

    resetCombatTuning();
    const auto& tuning = combatTuning();

    CHECK(formationSlotForIndex(0) == FormationSlot::Front);
    CHECK(formationSlotForIndex(1) == FormationSlot::Middle);
    CHECK(formationSlotForIndex(2) == FormationSlot::Rear);
    CHECK(formationSlotForIndex(9) == FormationSlot::Rear);

    // Party of three, nobody defending: the foremost living member is hit.
    std::vector<FormationMember> line = {
        {starters[0], true, false},
        {starters[1], true, false},
        {starters[2], true, false},
    };
    auto pick = selectMeleeTarget(line, tuning);
    CHECK(pick.valid());
    CHECK(pick.target == starters[0]);
    CHECK(!pick.intercepted);
    CHECK(pick.damageScale == 1.0f);

    // Front falls: targeting walks back deterministically, never randomly.
    line[0].alive = false;
    pick = selectMeleeTarget(line, tuning);
    CHECK(pick.target == starters[1]);
    line[1].alive = false;
    pick = selectMeleeTarget(line, tuning);
    CHECK(pick.target == starters[2]);

    // Nobody alive: no target at all, so the caller can enter a defeat state
    // instead of inventing a victim.
    line[2].alive = false;
    pick = selectMeleeTarget(line, tuning);
    CHECK(!pick.valid());
    CHECK(pick.target == InvalidCharacterId);

    // Vanguard defends from the front: it is already the target, so no
    // interception is reported, but damage is still reduced.
    line = {{starters[0], true, true}, {starters[1], true, false}, {starters[2], true, false}};
    pick = selectMeleeTarget(line, tuning);
    CHECK(pick.target == starters[0]);
    CHECK(!pick.intercepted);
    CHECK(pick.damageScale == tuning.vanguardDefendDamageScale);

    // Vanguard defends while the front member is down: it intercepts the blow
    // meant for the member behind it. This is TEST A in the milestone spec.
    line = {{starters[0], false, false}, {starters[1], true, true}, {starters[2], true, false}};
    pick = selectMeleeTarget(line, tuning);
    CHECK(pick.intendedTarget == starters[1]);
    CHECK(pick.target == starters[1]);

    line = {{starters[0], true, false}, {starters[1], true, true}, {starters[2], true, false}};
    pick = selectMeleeTarget(line, tuning);
    CHECK(pick.intendedTarget == starters[0]);
    CHECK(pick.target == starters[0]);
    CHECK(!pick.intercepted);

    // A rear defender cannot reach past the front, so the front still takes it.
    line = {{starters[0], true, false}, {starters[1], false, false}, {starters[2], true, true}};
    pick = selectMeleeTarget(line, tuning);
    CHECK(pick.target == starters[0]);
    CHECK(!pick.intercepted);

    // Party sizes 1 and 2 resolve without special-casing.
    std::vector<FormationMember> solo = {{starters[0], true, false}};
    CHECK(selectMeleeTarget(solo, tuning).target == starters[0]);
    std::vector<FormationMember> duo = {{starters[0], false, false}, {starters[1], true, false}};
    CHECK(selectMeleeTarget(duo, tuning).target == starters[1]);
    CHECK(!selectMeleeTarget({}, tuning).valid());

    // ---- campaign inventory survives its carrier -----------------------------

    CampaignInventory campaign;
    campaign.reset();
    CHECK(campaign.keys() == 0);
    campaign.addKeys(1);
    CHECK(campaign.addItem("item.progression.gatehouse-seal"));
    CHECK(!campaign.addItem("item.progression.gatehouse-seal"));
    CHECK(campaign.hasItem("item.progression.gatehouse-seal"));

    // Kill every member of the party. Campaign state is party state, so it is
    // untouched: there is no carrier to lose it with.
    Roster wipeRoster;
    Party wipeParty;
    CHECK(wipeParty.setMembers({starters[0], starters[1]}));
    CHECK(wipeRoster.beginNewGame(wipeParty.members()));
    for (const auto id : wipeParty.members()) wipeRoster.damage(id, 9999);
    CHECK(wipeRoster.find(starters[0])->status == CharacterStatus::Dead);
    CHECK(campaign.keys() == 1);
    CHECK(campaign.hasItem("item.progression.gatehouse-seal"));
    CHECK(campaign.consumeKey());
    CHECK(!campaign.consumeKey());

    CHECK(!campaign.restore(-1, 0, {}));
    CHECK(!campaign.restore(0, -3, {}));
    CHECK(campaign.restore(2, 1, {"item.a", "item.a", "item.b"}));
    CHECK(campaign.keys() == 2);
    CHECK(campaign.items().size() == 2);

    // ---- tuning is centralised and sane --------------------------------------

    CHECK(tuning.strikerAttackCooldown < tuning.vanguardAttackCooldown);
    CHECK(tuning.vanguardAttackCooldown < tuning.casterAttackCooldown);
    CHECK(tuning.playerTurnRecovery < tuning.playerMoveRecovery);
    CHECK(tuning.enemyAttackWindup > 0.0f);
    CHECK(tuning.projectileSpeed > 0.0f);
    CHECK(tuning.downedBleedOutSeconds > 0.0f);
    CHECK(tuning.vanguardDefendDamageScale < 1.0f);
    combatTuning().projectileSpeed = 99.0f;
    CHECK(combatTuning().projectileSpeed == 99.0f);
    resetCombatTuning();
    CHECK(combatTuning().projectileSpeed < 99.0f);

    // ---- 0.2 integration: enemy archetypes -----------------------------------

    CHECK(findEnemyType("enemy.brute") != nullptr);
    CHECK(findEnemyType("enemy.nope") == nullptr);
    CHECK(findEnemyType(defaultEnemyTypeId()) != nullptr);
    CHECK(enemyTypeOrDefault("enemy.nope").id == defaultEnemyTypeId());
    CHECK(enemyTypeOrDefault("").id == defaultEnemyTypeId());
    const auto& bruteType = enemyTypeOrDefault("enemy.brute");
    CHECK(bruteType.maxHp > enemyTypeOrDefault("enemy.prowler").maxHp);
    CHECK(bruteType.moveRecoveryScale > 1.0f);   // Brute is slow on purpose

    // Level format 3 carries the archetype; format 2 still loads without one.
    const std::string pressurePath = std::string(STONEVEIL_SOURCE_DIR) + "/content/levels/pressure-cooker.svl";
    LevelDefinition pressure;
    std::string pressureError;
    CHECK(LevelIO::load(pressurePath, pressure, pressureError));
    CHECK(pressure.enemies.size() == 2);
    CHECK(pressure.enemies[0].typeId == "enemy.brute");
    CHECK(LevelIO::validate(pressure).empty());

    const char* legacyEnemyPath = "stoneveil_legacy_enemy.svl";
    {
        std::ofstream legacy(legacyEnemyPath, std::ios::trunc);
        legacy << "STONEVEIL_LEVEL 2\n"
                  "ID \"test.legacy-enemy\"\n"
                  "NAME \"LEGACY ENEMY\"\n"
                  "SIZE 6 5\n"
                  "SPAWN 1 1 1\n"
                  "DEFAULT_WALL \"material.wall.cave.mossy-rock\"\n"
                  "DEFAULT_FLOOR \"material.floor.cave.uneven-stone\"\n"
                  "CEILING_MODE MATERIAL\n"
                  "DEFAULT_CEILING \"material.ceiling.cave.rocky\"\n"
                  "MAP\n######\n#....#\n#...E#\n#....#\n######\n"
                  "END_MAP\n"
                  "PICKUPS 0\n"
                  "ENEMIES 1\n2 3 12\n"
                  "SURFACE_OVERRIDES 0\n"
                  "LIGHTS 0\n"
                  "END\n";
        CHECK(static_cast<bool>(legacy));
    }
    LevelDefinition legacyEnemyLevel;
    CHECK(LevelIO::load(legacyEnemyPath, legacyEnemyLevel, pressureError));
    CHECK(legacyEnemyLevel.enemies.size() == 1);
    CHECK(legacyEnemyLevel.enemies[0].typeId.empty());
    CHECK(enemyTypeOrDefault(legacyEnemyLevel.enemies[0].typeId).id == defaultEnemyTypeId());
    // Re-saving upgrades it to v3 and stamps the default archetype explicitly.
    CHECK(LevelIO::save(legacyEnemyPath, legacyEnemyLevel, pressureError));
    LevelDefinition upgradedEnemyLevel;
    CHECK(LevelIO::load(legacyEnemyPath, upgradedEnemyLevel, pressureError));
    CHECK(upgradedEnemyLevel.enemies[0].typeId == defaultEnemyTypeId());
    std::remove(legacyEnemyPath);

    // ---- player action gating ------------------------------------------------

    resetCombatTuning();
    ActionGate gate;
    CHECK(gate.moveReady());
    CHECK(gate.tryMove(combatTuning()));
    CHECK(!gate.moveReady());
    CHECK(!gate.tryMove(combatTuning()));           // committed: movement is not free
    CHECK(gate.movesTaken() == 1 && gate.movesBlocked() == 1);

    // Turning is NOT gated by the attack timer and vice versa.
    CHECK(gate.tryTurn(combatTuning()));
    CHECK(gate.tryAttack(combatTuning()));
    CHECK(!gate.tryAttack(combatTuning()));
    CHECK(!gate.tryTurn(combatTuning()));
    gate.tick(combatTuning().playerTurnRecovery + 0.01f);
    CHECK(gate.tryTurn(combatTuning()));            // turn recovered first
    CHECK(!gate.attackReady());                     // attack still recovering
    gate.tick(combatTuning().playerAttackRecovery + 0.01f);
    CHECK(gate.attackReady() && gate.moveReady());
    gate.reset();
    CHECK(gate.movesTaken() == 0);

    // ---- deterministic melee targeting at runtime -----------------------------

    Dungeon bruteDungeon;
    bruteDungeon.enemies() = {{3, 2, 400, true, 0.0f, "enemy.brute"}};
    PlayerState brutePlayer{2, 2, 1};
    Party brutedParty;
    CHECK(brutedParty.setMembers({starters[0], starters[1], starters[2]}));
    Roster brutedRoster;
    CHECK(brutedRoster.beginNewGame(brutedParty.members()));

    const auto bruteLine = CombatSystem::formationFor(brutedRoster, brutedParty);
    CHECK(bruteLine.size() == 3);
    CHECK(bruteLine[0].id == starters[0]);   // party order is the formation

    CombatSystem bruteCombat{4242};
    bruteCombat.updateEnemies(0.016f, brutedRoster, brutedParty, bruteDungeon, brutePlayer);
    CHECK(bruteDungeon.enemies()[0].winding);            // telegraph, not an instant hit
    CHECK(CombatSystem::dangerActive(bruteDungeon));     // save blocking hooks onto this

    // Resolve three separate swings: the FRONT member is hit every time. Under
    // the old uniform-random rule this would almost certainly have varied.
    for (int swing = 0; swing < 3; ++swing) {
        for (int step = 0; step < 3; ++step) {
            bruteCombat.updateEnemies(2.0f, brutedRoster, brutedParty, bruteDungeon, brutePlayer);
        }
        CHECK(bruteCombat.lastMeleeTarget().target == starters[0]);
    }
    CHECK(bruteCombat.meleeResolutions() >= 3);
    CHECK(brutedRoster.find(starters[1])->hp == findCharacterDefinition(starters[1])->maxHp);
    CHECK(brutedRoster.find(starters[2])->hp == findCharacterDefinition(starters[2])->maxHp);
    CHECK(!CombatSystem::dangerActive(Dungeon{}));

    // Front falls: targeting walks back, still deterministically.
    brutedRoster.damage(starters[0], 9999);
    brutedParty.remove(starters[0]);
    for (int step = 0; step < 3; ++step) {
        bruteCombat.updateEnemies(2.0f, brutedRoster, brutedParty, bruteDungeon, brutePlayer);
    }
    CHECK(bruteCombat.lastMeleeTarget().target == starters[1]);

    // Party sizes 1 and 2 resolve through the same path.
    for (int size = 1; size <= 2; ++size) {
        Dungeon sizedDungeon;
        sizedDungeon.enemies() = {{3, 2, 400, true, 0.0f, "enemy.brute"}};
        Party sizedParty;
        std::vector<CharacterId> chosen(starters.begin(), starters.begin() + size);
        CHECK(sizedParty.setMembers(chosen));
        Roster sizedRoster;
        CHECK(sizedRoster.beginNewGame(chosen));
        CombatSystem sizedCombat{7 + static_cast<std::uint32_t>(size)};
        for (int step = 0; step < 3; ++step) {
            sizedCombat.updateEnemies(2.0f, sizedRoster, sizedParty, sizedDungeon, brutePlayer);
        }
        CHECK(sizedCombat.meleeResolutions() == 1);
        CHECK(sizedCombat.lastMeleeTarget().target == chosen.front());
    }

    // Enemy archetype survives a save/load: a Brute must not silently become
    // the default archetype when a save is restored.
    const char* archetypeSavePath = "stoneveil_archetype.sav";
    Dungeon archetypeDungeon;
    archetypeDungeon.enemies() = {{3, 2, 40, true, 0.0f, "enemy.brute"}};
    archetypeDungeon.enemies()[0].id = "enemy.test.brute";
    Party archetypeParty;
    CHECK(archetypeParty.setMembers({starters[0]}));
    Roster archetypeRoster;
    CHECK(archetypeRoster.beginNewGame(archetypeParty.members()));
    PlayerState archetypePlayer{2, 2, 1};
    CHECK(SaveSystem::save(archetypeSavePath, archetypePlayer, archetypeRoster, archetypeParty,
                           archetypeDungeon, 0, 1, 0));
    Dungeon reloadedArchetype;
    reloadedArchetype.enemies() = {{3, 2, 40, true, 0.0f, "enemy.brute"}};
    reloadedArchetype.enemies()[0].id = "enemy.test.brute";
    PlayerState reloadedArchetypePlayer;
    Roster reloadedArchetypeRoster;
    Party reloadedArchetypeParty;
    int archKeys{}, archPotions{}, archXp{};
    CHECK(SaveSystem::load(archetypeSavePath, reloadedArchetypePlayer, reloadedArchetypeRoster,
                           reloadedArchetypeParty, reloadedArchetype, archKeys, archPotions, archXp));
    CHECK(reloadedArchetype.enemies()[0].typeId == "enemy.brute");
    std::remove(archetypeSavePath);

    // ---- authored objects, story rooms, triggers, and campaign registry -------

    const std::string authoredPath = std::string(STONEVEIL_SOURCE_DIR) + "/content/levels/gatehouse.svl";
    LevelDefinition authoredLevel;
    std::string authoredError;
    CHECK(LevelIO::load(authoredPath, authoredLevel, authoredError));
    CHECK(!authoredLevel.doors.empty());
    CHECK(!authoredLevel.objects.empty());
    CHECK(!authoredLevel.rooms.empty());
    CHECK(!authoredLevel.triggers.empty());
    CHECK(!authoredLevel.pickups.front().id.empty());
    CHECK(!authoredLevel.enemies.front().id.empty());

    Dungeon authoredDungeon{authoredLevel};
    CHECK(authoredDungeon.roomAt(2, 2) != nullptr);
    CHECK(authoredDungeon.roomAt(2, 2)->id == "room.gatehouse.entry");
    CHECK(authoredDungeon.objectAt(3, 2) != nullptr);
    CHECK(authoredDungeon.doorRequiresKeyAt(7, 4));
    LevelDefinition unlockedLevel = authoredLevel;
    unlockedLevel.doors.front().locked = false;
    Dungeon unlockedDungeon{unlockedLevel};
    CHECK(unlockedDungeon.openDoor(7, 4, false));

    TriggerSystem triggerSystem;
    const auto firstStoryBeat = triggerSystem.fire(authoredLevel.triggers, TriggerEvent::EnterRoom, 2, 2,
                                                    "room.gatehouse.entry");
    CHECK(firstStoryBeat.size() == 1);
    CHECK(triggerSystem.fire(authoredLevel.triggers, TriggerEvent::EnterRoom, 2, 2,
                             "room.gatehouse.entry").empty());
    triggerSystem.reset();
    CHECK(!triggerSystem.fire(authoredLevel.triggers, TriggerEvent::EnterRoom, 2, 2,
                              "room.gatehouse.entry").empty());

    const char* authoredRoundTripPath = "stoneveil_story_roundtrip.svl";
    CHECK(LevelIO::save(authoredRoundTripPath, authoredLevel, authoredError));
    LevelDefinition authoredRoundTrip;
    CHECK(LevelIO::load(authoredRoundTripPath, authoredRoundTrip, authoredError));
    CHECK(authoredRoundTrip.objects.size() == authoredLevel.objects.size());
    CHECK(authoredRoundTrip.rooms[0].intendedFeeling == authoredLevel.rooms[0].intendedFeeling);
    CHECK(authoredRoundTrip.triggers[0].subjectId == authoredLevel.triggers[0].subjectId);
    std::remove(authoredRoundTripPath);

    CampaignDefinition campaignRegistry;
    const std::string campaignPath = std::string(STONEVEIL_SOURCE_DIR) + "/content/campaigns/stoneveil.campaign";
    CHECK(CampaignIO::load(campaignPath, campaignRegistry, authoredError));
    CHECK(campaignRegistry.startingLevelId == "gatehouse.level-1");
    CHECK(campaignRegistry.levels.size() >= 2);

    return 0;
}
