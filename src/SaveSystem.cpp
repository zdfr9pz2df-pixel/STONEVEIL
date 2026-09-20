#include "SaveSystem.hpp"
#include "AtomicFile.hpp"
#include "WorldEvents.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <set>
#include <sstream>
#include <vector>

namespace sv {
namespace {
constexpr int CurrentSaveVersion = 7;
constexpr std::size_t MaxSavedEntities = 1024;

bool readRosterAndParty(std::istream& input, Roster& roster, Party& party, int version) {
    std::size_t rosterCount{};
    input >> rosterCount;
    if (!input || rosterCount > MaxSavedEntities) return false;

    std::vector<CharacterRecord> records(rosterCount);
    for (auto& record : records) {
        int status{};
        input >> record.id >> record.hp >> record.xp >> status;
        if (!input || status < static_cast<int>(CharacterStatus::Unrecruited) || status > static_cast<int>(CharacterStatus::Dead)) {
            return false;
        }
        record.status = static_cast<CharacterStatus>(status);
        // Legacy saves may have up to six active slots. Membership below is
        // authoritative; excess living members migrate to Reserve, never die.
        if (version <= 4 && record.status == CharacterStatus::Active) record.status = CharacterStatus::Reserve;
    }
    if (!roster.restore(records)) return false;

    std::size_t partyCount{};
    if (version >= 4) {
        std::string label;
        std::size_t capacity{};
        input >> label >> capacity >> partyCount;
        const std::size_t maximum = version <= 4 ? 6 : Party::MaximumCapacity;
        if (!input || label != "PARTY" || capacity == 0 || capacity > maximum || partyCount > capacity ||
            !party.setCapacity(std::min(capacity, Party::MaximumCapacity))) return false;
    } else {
        input >> partyCount;
    }
    if (!input || partyCount > (version <= 4 ? 6 : Party::MaximumCapacity)) return false;
    std::vector<CharacterId> ids(partyCount);
    for (auto& id : ids) input >> id;
    std::set<CharacterId> seen;
    for (const auto id : ids) {
        const auto* record = roster.find(id);
        if (!record || !record->alive() || record->status == CharacterStatus::Unrecruited || !seen.insert(id).second) return false;
    }
    if (version >= 5) {
        for (const auto& record : roster.records()) {
            if ((record.status == CharacterStatus::Active) != (seen.count(record.id) != 0)) return false;
        }
    }
    if (ids.size() > party.capacity()) ids.resize(party.capacity());
    return input && party.setMembers(ids) && roster.setActiveParty(ids);
}

bool readCampaignSnapshots(std::istream& input, CampaignState& campaignState, const std::string& activeLevelId) {
    std::string label;
    std::size_t count{};
    input >> label >> count;
    if (!input || label != "CAMPAIGN_STATES" || count > 256) return false;
    std::vector<LevelRuntimeSnapshot> snapshots;
    snapshots.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        LevelRuntimeSnapshot snapshot;
        input >> label >> std::quoted(snapshot.levelId) >> snapshot.width >> snapshot.height;
        if (!input || label != "LEVEL_STATE" || snapshot.levelId == activeLevelId || snapshot.width < 1 ||
            snapshot.height < 1 || snapshot.width > LevelDefinition::MaximumDimension ||
            snapshot.height > LevelDefinition::MaximumDimension) return false;
        input >> label;
        if (!input || label != "TILES") return false;
        const auto tileCount = static_cast<std::size_t>(snapshot.width) * static_cast<std::size_t>(snapshot.height);
        snapshot.tiles.reserve(tileCount);
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            int value{};
            input >> value;
            if (!input || value < static_cast<int>(Tile::Floor) || value > static_cast<int>(Tile::SecretDoorClosed)) return false;
            snapshot.tiles.push_back(static_cast<Tile>(value));
        }
        std::size_t pickupCount{};
        input >> label >> pickupCount;
        if (!input || label != "PICKUPS" || pickupCount > MaxSavedEntities) return false;
        for (std::size_t pickupIndex = 0; pickupIndex < pickupCount; ++pickupIndex) {
            Pickup pickup;
            int type{};
            input >> std::quoted(pickup.id) >> type >> pickup.x >> pickup.y >> pickup.taken;
            if (!input || type < static_cast<int>(Pickup::Type::Key) || type > static_cast<int>(Pickup::Type::Potion)) return false;
            pickup.type = static_cast<Pickup::Type>(type);
            snapshot.pickups.push_back(std::move(pickup));
        }
        std::size_t enemyCount{};
        input >> label >> enemyCount;
        if (!input || label != "ENEMIES" || enemyCount > MaxSavedEntities) return false;
        for (std::size_t enemyIndex = 0; enemyIndex < enemyCount; ++enemyIndex) {
            Enemy enemy;
            input >> std::quoted(enemy.id) >> enemy.x >> enemy.y >> enemy.hp >> enemy.alive >> enemy.attackCooldown;
            if (!input) return false;
            snapshot.enemies.push_back(std::move(enemy));
        }
        std::size_t eventCount{};
        input >> label >> eventCount;
        if (!input || label != "EVENTS" || eventCount > 4096) return false;
        for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
            std::string id;
            std::uint32_t fired{};
            input >> std::quoted(id) >> fired;
            if (!input || !snapshot.firedEventCounts.emplace(std::move(id), fired).second) return false;
        }
        input >> label;
        if (!input || label != "END_LEVEL_STATE") return false;
        snapshots.push_back(std::move(snapshot));
    }
    return campaignState.replaceSnapshots(std::move(snapshots));
}

bool readStoryState(std::istream& input, StoryState& storyState) {
    std::string label;
    std::size_t count{};
    input >> label >> count;
    if (!input || label != "STORY_FLAGS" || count > 4096) return false;
    std::map<std::string, bool> facts;
    for (std::size_t i = 0; i < count; ++i) {
        std::string key;
        int value{};
        input >> std::quoted(key) >> value;
        if (!input || (value != 0 && value != 1) || !facts.emplace(std::move(key), value != 0).second) return false;
    }
    return storyState.replace(std::move(facts));
}

void writeCampaignSnapshots(std::ostream& output, const CampaignState* campaignState) {
    const std::size_t count = campaignState == nullptr ? 0 : campaignState->snapshots().size();
    output << "CAMPAIGN_STATES " << count << '\n';
    if (campaignState == nullptr) return;
    for (const auto& entry : campaignState->snapshots()) {
        const auto& snapshot = entry.second;
        output << "LEVEL_STATE " << std::quoted(snapshot.levelId) << ' ' << snapshot.width << ' ' << snapshot.height << '\n';
        output << "TILES";
        for (const auto tile : snapshot.tiles) output << ' ' << static_cast<int>(tile);
        output << '\n';
        output << "PICKUPS " << snapshot.pickups.size() << '\n';
        for (const auto& pickup : snapshot.pickups)
            output << std::quoted(pickup.id) << ' ' << static_cast<int>(pickup.type) << ' ' << pickup.x << ' '
                   << pickup.y << ' ' << pickup.taken << '\n';
        output << "ENEMIES " << snapshot.enemies.size() << '\n';
        for (const auto& enemy : snapshot.enemies)
            output << std::quoted(enemy.id) << ' ' << enemy.x << ' ' << enemy.y << ' ' << enemy.hp << ' '
                   << enemy.alive << ' ' << enemy.attackCooldown << '\n';
        std::vector<std::pair<std::string, std::uint32_t>> counts(snapshot.firedEventCounts.begin(),
                                                                  snapshot.firedEventCounts.end());
        std::sort(counts.begin(), counts.end());
        output << "EVENTS " << counts.size() << '\n';
        for (const auto& fired : counts) output << std::quoted(fired.first) << ' ' << fired.second << '\n';
        output << "END_LEVEL_STATE\n";
    }
}

bool readVersionThreeDungeon(std::istream& input, Dungeon& dungeon, int version, EventRuntime& events,
                             CampaignState& campaignState, StoryState& storyState) {
    std::string label;
    std::string levelId;
    input >> label >> levelId;
    if (!input || label != "LEVEL" || levelId != dungeon.levelId()) return false;

    int width{};
    int height{};
    input >> label >> width >> height;
    if (!input || label != "TILES" || width != dungeon.width() || height != dungeon.height()) return false;
    for (int y = 0; y < dungeon.height(); ++y) {
        for (int x = 0; x < dungeon.width(); ++x) {
            int tileValue{};
            input >> tileValue;
            if (!input || !dungeon.restoreTile(x, y, static_cast<Tile>(tileValue))) return false;
        }
    }

    std::size_t pickupCount{};
    input >> label >> pickupCount;
    if (!input || label != "PICKUPS" || pickupCount > MaxSavedEntities || pickupCount != dungeon.pickups().size()) return false;
    std::vector<Pickup> pickups(pickupCount);
    std::set<std::string> pickupIds;
    for (auto& pickup : pickups) {
        int type{};
        if (version >= 5) input >> std::quoted(pickup.id);
        input >> type >> pickup.x >> pickup.y >> pickup.taken;
        if (!input || type < static_cast<int>(Pickup::Type::Key) || type > static_cast<int>(Pickup::Type::Potion) ||
            !dungeon.inBounds(pickup.x, pickup.y)) {
            return false;
        }
        pickup.type = static_cast<Pickup::Type>(type);
        if (version >= 5) {
            const auto found = std::find_if(dungeon.pickups().begin(), dungeon.pickups().end(),
                [&](const auto& authored) { return authored.id == pickup.id && authored.type == pickup.type; });
            if (pickup.id.empty() || !pickupIds.insert(pickup.id).second || found == dungeon.pickups().end()) return false;
        }
    }
    for (std::size_t i = 0; i < pickups.size() && i < dungeon.pickups().size(); ++i) {
        // Stable identity is authored level data; runtime saves only carry the
        // mutable taken flag and current placement.
        if (version < 5) pickups[i].id = dungeon.pickups()[i].id;
    }
    dungeon.pickups() = std::move(pickups);

    std::size_t enemyCount{};
    input >> label >> enemyCount;
    if (!input || label != "ENEMIES" || enemyCount > MaxSavedEntities || enemyCount != dungeon.enemies().size()) return false;
    std::vector<Enemy> enemies(enemyCount);
    std::set<std::string> enemyIds;
    for (auto& enemy : enemies) {
        if (version >= 5) input >> std::quoted(enemy.id);
        input >> enemy.x >> enemy.y >> enemy.hp >> enemy.alive >> enemy.attackCooldown;
        if (!input || (enemy.alive && enemy.hp <= 0) || !std::isfinite(enemy.attackCooldown) || enemy.attackCooldown < 0.0f ||
            (enemy.alive && (!dungeon.inBounds(enemy.x, enemy.y) || dungeon.blocksMovement(enemy.x, enemy.y)))) {
            return false;
        }
        if (!enemy.alive) enemy.hp = std::max(0, enemy.hp);
        if (version >= 5) {
            const auto found = std::find_if(dungeon.enemies().begin(), dungeon.enemies().end(),
                [&](const auto& authored) { return authored.id == enemy.id; });
            if (enemy.id.empty() || !enemyIds.insert(enemy.id).second || found == dungeon.enemies().end()) return false;
            enemy.typeId = found->typeId;
        }
    }
    for (std::size_t i = 0; i < enemies.size() && i < dungeon.enemies().size(); ++i) {
        // typeId is authored level data, never serialized. Carry it across the
        // replacement so loading a save does not turn every Brute into the
        // default archetype.
        if (version < 5) {
            enemies[i].typeId = dungeon.enemies()[i].typeId;
            enemies[i].id = dungeon.enemies()[i].id;
        }
    }
    dungeon.enemies() = std::move(enemies);

    if (version >= 5) {
        std::size_t eventCount{};
        input >> label >> eventCount;
        if (!input || label != "EVENTS" || eventCount > 4096) return false;
        std::unordered_map<std::string, std::uint32_t> counts;
        for (std::size_t i = 0; i < eventCount; ++i) {
            std::string id;
            std::uint32_t count{};
            input >> std::quoted(id) >> count;
            if (!input || !counts.emplace(id, count).second) return false;
        }
        if (!events.restoreFiredCounts(counts)) return false;
    }
    if (version >= 6 && !readCampaignSnapshots(input, campaignState, dungeon.levelId())) return false;
    if (version >= 7 && !readStoryState(input, storyState)) return false;
    input >> label;
    return input && label == "END";
}

bool readVersionTwoEnemies(std::istream& input, Dungeon& dungeon) {
    for (auto& enemy : dungeon.enemies()) {
        input >> enemy.x >> enemy.y >> enemy.hp >> enemy.alive;
        if (!input || (enemy.alive && enemy.hp <= 0) ||
            (enemy.alive && (!dungeon.inBounds(enemy.x, enemy.y) || dungeon.blocksMovement(enemy.x, enemy.y)))) {
            return false;
        }
        if (!enemy.alive) enemy.hp = std::max(0, enemy.hp);
    }
    return true;
}
} // namespace

bool SaveSystem::save(const std::string& path,
                      const PlayerState& player,
                      const Roster& roster,
                      const Party& party,
                      const Dungeon& dungeon,
                      int keys,
                      int potions,
                      int xp,
                      const EventRuntime* events,
                      const CampaignState* campaignState,
                      const StoryState* storyState) {
    Roster validatedRoster = roster;
    EventRuntime validatedEvents;
    if (keys < 0 || potions < 0 || xp < 0 || !validatedRoster.restore(roster.records()) ||
        !configureWorldEvents(dungeon, validatedEvents) ||
        (events && !validatedEvents.restoreFiredCounts(events->firedCounts()))) return false;
    if (campaignState && campaignState->contains(dungeon.levelId())) return false;
    for (const auto& record : roster.records()) {
        if ((record.status == CharacterStatus::Active) != party.contains(record.id)) return false;
    }
    for (const auto id : party.members()) if (!roster.find(id)) return false;
    std::set<std::string> ids;
    for (const auto& pickup : dungeon.pickups())
        if (pickup.id.empty() || !ids.insert(pickup.id).second) return false;
    for (const auto& enemy : dungeon.enemies())
        if (enemy.id.empty() || !ids.insert(enemy.id).second) return false;
    std::ostringstream output;
    if (!output) return false;

    output << "STONEVEIL_SAVE " << CurrentSaveVersion << '\n';
    output << player.x() << ' ' << player.y() << ' ' << player.direction() << ' ' << keys << ' ' << potions << ' ' << xp << '\n';
    output << roster.records().size() << '\n';
    for (const auto& record : roster.records()) {
        output << record.id << ' ' << record.hp << ' ' << record.xp << ' ' << static_cast<int>(record.status) << '\n';
    }
    output << "PARTY " << party.capacity() << ' ' << party.size();
    for (const auto id : party.members()) output << ' ' << id;
    output << '\n';

    output << "LEVEL " << dungeon.levelId() << '\n';
    output << "TILES " << dungeon.width() << ' ' << dungeon.height() << '\n';
    for (int y = 0; y < dungeon.height(); ++y) {
        for (int x = 0; x < dungeon.width(); ++x) output << static_cast<int>(dungeon.tile(x, y)) << ' ';
        output << '\n';
    }

    output << "PICKUPS " << dungeon.pickups().size() << '\n';
    for (const auto& pickup : dungeon.pickups()) {
        output << std::quoted(pickup.id) << ' ' << static_cast<int>(pickup.type) << ' ' << pickup.x << ' ' << pickup.y << ' ' << pickup.taken << '\n';
    }

    output << "ENEMIES " << dungeon.enemies().size() << '\n';
    for (const auto& enemy : dungeon.enemies()) {
        output << std::quoted(enemy.id) << ' ' << enemy.x << ' ' << enemy.y << ' ' << enemy.hp << ' ' << enemy.alive << ' ' << enemy.attackCooldown << '\n';
    }
    // Stable ordering makes saves reproducible even though runtime lookup is hashed.
    std::vector<std::pair<std::string, std::uint32_t>> counts;
    if (events) counts.assign(events->firedCounts().begin(), events->firedCounts().end());
    std::sort(counts.begin(), counts.end());
    output << "EVENTS " << counts.size() << '\n';
    for (const auto& entry : counts) output << std::quoted(entry.first) << ' ' << entry.second << '\n';
    writeCampaignSnapshots(output, campaignState);
    output << "STORY_FLAGS " << (storyState == nullptr ? 0 : storyState->facts().size()) << '\n';
    if (storyState) for (const auto& fact : storyState->facts())
        output << std::quoted(fact.first) << ' ' << fact.second << '\n';
    output << "END\n";
    std::string error;
    return output && writeFileAtomically(path, output.str(), error);
}

bool SaveSystem::load(const std::string& path,
                      PlayerState& player,
                      Roster& roster,
                      Party& party,
                      Dungeon& dungeon,
                      int& keys,
                      int& potions,
                      int& xp,
                      EventRuntime* events,
                      CampaignState* campaignState,
                      StoryState* storyState) {
    std::ifstream input(path);
    if (!input) return false;

    Dungeon loadedDungeon = dungeon;
    EventRuntime loadedEvents;
    CampaignState loadedCampaignState;
    StoryState loadedStoryState;
    if (!configureWorldEvents(loadedDungeon, loadedEvents)) return false;
    PlayerState loadedPlayer{loadedDungeon.spawnX(), loadedDungeon.spawnY(), loadedDungeon.spawnDirection()};
    Roster loadedRoster = roster;
    Party loadedParty;
    int loadedKeys{};
    int loadedPotions{};
    int loadedXp{};
    int playerX{};
    int playerY{};
    int playerDirection{};

    std::string header;
    input >> header;
    if (header == "STONEVEIL_SAVE") {
        int version{};
        input >> version;
        if (!input || (version < 2 || version > CurrentSaveVersion)) return false;
        input >> playerX >> playerY >> playerDirection >> loadedKeys >> loadedPotions >> loadedXp;
        if (!input || !readRosterAndParty(input, loadedRoster, loadedParty, version)) return false;
        if (version >= 3) {
            if (!readVersionThreeDungeon(input, loadedDungeon, version, loadedEvents, loadedCampaignState, loadedStoryState)) return false;
        } else if (!readVersionTwoEnemies(input, loadedDungeon)) {
            return false;
        }
    } else {
        std::istringstream legacyX(header);
        if (!(legacyX >> playerX)) return false;
        input >> playerY >> playerDirection >> loadedKeys >> loadedPotions >> loadedXp;
        const auto starters = loadedRoster.starterIds();
        if (!input || !loadedRoster.beginNewGame(starters) || !loadedParty.setMembers(starters)) return false;
        for (const auto id : starters) {
            int hp{};
            input >> hp;
            auto* record = loadedRoster.find(id);
            const auto* definition = loadedRoster.definition(id);
            if (!input || record == nullptr || definition == nullptr) return false;
            record->hp = std::clamp(hp, 0, definition->maxHp);
            if (record->hp == 0) {
                record->status = CharacterStatus::Dead;
                loadedParty.remove(id);
            }
        }
        if (!readVersionTwoEnemies(input, loadedDungeon)) return false;
    }

    if (loadedKeys < 0 || loadedPotions < 0 || loadedXp < 0 ||
        !loadedPlayer.restore(playerX, playerY, playerDirection) ||
        !loadedDungeon.inBounds(playerX, playerY) ||
        loadedDungeon.blocksMovement(playerX, playerY, &loadedStoryState)) {
        return false;
    }

    player = loadedPlayer;
    roster = loadedRoster;
    party = loadedParty;
    dungeon = loadedDungeon;
    keys = loadedKeys;
    potions = loadedPotions;
    xp = loadedXp;
    if (events) *events = std::move(loadedEvents);
    if (campaignState) *campaignState = std::move(loadedCampaignState);
    if (storyState) *storyState = std::move(loadedStoryState);
    return true;
}

bool SaveSystem::savedLevelId(const std::string& path, std::string& levelId) {
    levelId.clear();
    std::ifstream input(path);
    if (!input) return false;
    std::string firstLine;
    if (!std::getline(input, firstLine)) return false;
    std::istringstream header(firstLine);
    std::string signature;
    int version{};
    if (!(header >> signature) || signature != "STONEVEIL_SAVE") return true;
    if (!(header >> version) || version < 2 || version > CurrentSaveVersion) return false;
    if (version < 3) return true;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("LEVEL ", 0) != 0) continue;
        std::istringstream record(line);
        std::string label;
        return static_cast<bool>(record >> label >> levelId) && label == "LEVEL" && !levelId.empty();
    }
    return false;
}

} // namespace sv
