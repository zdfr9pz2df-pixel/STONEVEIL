#include "CampaignState.hpp"

#include "WorldEvents.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace sv {
namespace {
bool basicSnapshotIsValid(const LevelRuntimeSnapshot& snapshot) {
    if (snapshot.levelId.empty() || snapshot.width < 1 || snapshot.height < 1 ||
        snapshot.width > LevelDefinition::MaximumDimension || snapshot.height > LevelDefinition::MaximumDimension ||
        snapshot.tiles.size() != static_cast<std::size_t>(snapshot.width) * static_cast<std::size_t>(snapshot.height) ||
        snapshot.pickups.size() > 1024 || snapshot.enemies.size() > 1024 || snapshot.firedEventCounts.size() > 4096) return false;
    for (const auto tile : snapshot.tiles) {
        const int value = static_cast<int>(tile);
        if (value < static_cast<int>(Tile::Floor) || value > static_cast<int>(Tile::SecretDoorClosed)) return false;
    }
    std::set<std::string> ids;
    for (const auto& pickup : snapshot.pickups) {
        if (pickup.id.empty() || !ids.insert(pickup.id).second || pickup.x < 0 || pickup.y < 0 ||
            pickup.x >= snapshot.width || pickup.y >= snapshot.height) return false;
    }
    for (const auto& enemy : snapshot.enemies) {
        if (enemy.id.empty() || !ids.insert(enemy.id).second || !std::isfinite(enemy.attackCooldown) ||
            enemy.attackCooldown < 0.0f || (enemy.alive && enemy.hp <= 0) || enemy.x < 0 || enemy.y < 0 ||
            enemy.x >= snapshot.width || enemy.y >= snapshot.height) return false;
    }
    for (const auto& fired : snapshot.firedEventCounts)
        if (fired.first.empty() || fired.second == 0) return false;
    return true;
}
}

bool CampaignState::capture(const Dungeon& dungeon, const EventRuntime& events) {
    if (dungeon.levelId().empty()) return false;
    LevelRuntimeSnapshot snapshot;
    snapshot.levelId = dungeon.levelId();
    snapshot.width = dungeon.width();
    snapshot.height = dungeon.height();
    snapshot.tiles.reserve(static_cast<std::size_t>(snapshot.width) * static_cast<std::size_t>(snapshot.height));
    for (int y = 0; y < snapshot.height; ++y)
        for (int x = 0; x < snapshot.width; ++x) snapshot.tiles.push_back(dungeon.tile(x, y));
    snapshot.pickups = dungeon.pickups();
    snapshot.enemies = dungeon.enemies();
    for (auto& enemy : snapshot.enemies) {
        enemy.winding = false;
        enemy.windupRemaining = 0.0f;
        enemy.recoveryRemaining = 0.0f;
        enemy.moveRemaining = 0.0f;
    }
    snapshot.firedEventCounts = events.firedCounts();
    if (!basicSnapshotIsValid(snapshot)) return false;
    snapshots_[snapshot.levelId] = std::move(snapshot);
    return true;
}

bool CampaignState::restore(const std::string& levelId, Dungeon& dungeon, EventRuntime& events) const {
    const auto found = snapshots_.find(levelId);
    if (found == snapshots_.end()) return false;
    const auto& snapshot = found->second;
    if (!basicSnapshotIsValid(snapshot) || snapshot.levelId != dungeon.levelId() ||
        snapshot.width != dungeon.width() || snapshot.height != dungeon.height()) return false;

    Dungeon restored = dungeon;
    EventRuntime restoredEvents;
    if (!configureWorldEvents(restored, restoredEvents)) return false;
    std::size_t tileIndex = 0;
    for (int y = 0; y < restored.height(); ++y)
        for (int x = 0; x < restored.width(); ++x)
            if (!restored.restoreTile(x, y, snapshot.tiles[tileIndex++])) return false;

    if (snapshot.pickups.size() != restored.pickups().size() || snapshot.enemies.size() != restored.enemies().size()) return false;
    std::vector<Pickup> pickups;
    pickups.reserve(snapshot.pickups.size());
    for (const auto& saved : snapshot.pickups) {
        const auto authored = std::find_if(restored.pickups().begin(), restored.pickups().end(), [&](const auto& value) {
            return value.id == saved.id && value.type == saved.type;
        });
        if (authored == restored.pickups().end() || !restored.inBounds(saved.x, saved.y)) return false;
        pickups.push_back(saved);
    }
    std::vector<Enemy> enemies;
    enemies.reserve(snapshot.enemies.size());
    for (auto saved : snapshot.enemies) {
        const auto authored = std::find_if(restored.enemies().begin(), restored.enemies().end(), [&](const auto& value) {
            return value.id == saved.id;
        });
        if (authored == restored.enemies().end() || !restored.inBounds(saved.x, saved.y) ||
            (saved.alive && restored.blocksMovement(saved.x, saved.y))) return false;
        saved.typeId = authored->typeId;
        saved.winding = false;
        saved.windupRemaining = saved.recoveryRemaining = saved.moveRemaining = 0.0f;
        enemies.push_back(std::move(saved));
    }
    restored.pickups() = std::move(pickups);
    restored.enemies() = std::move(enemies);
    if (!restoredEvents.restoreFiredCounts(snapshot.firedEventCounts)) return false;
    dungeon = std::move(restored);
    events = std::move(restoredEvents);
    return true;
}

bool CampaignState::replaceSnapshots(std::vector<LevelRuntimeSnapshot> snapshots) {
    std::map<std::string, LevelRuntimeSnapshot> replacement;
    for (auto& snapshot : snapshots) {
        if (!basicSnapshotIsValid(snapshot) || !replacement.emplace(snapshot.levelId, std::move(snapshot)).second) return false;
    }
    snapshots_ = std::move(replacement);
    return true;
}

} // namespace sv
