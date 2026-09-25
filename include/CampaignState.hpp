#pragma once

#include "Dungeon.hpp"
#include "EventSystem.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace sv {

// Mutable per-level state that is parked while the party is somewhere else.
// The active level remains in Game's Dungeon/EventRuntime and is serialized by
// SaveSystem's primary level record; snapshots therefore never duplicate it.
struct LevelRuntimeSnapshot {
    std::string levelId;
    int width{};
    int height{};
    std::vector<Tile> tiles;
    std::vector<Pickup> pickups;
    std::vector<Enemy> enemies;
    std::unordered_map<std::string, std::uint32_t> firedEventCounts;
};

class CampaignState {
public:
    bool capture(const Dungeon& dungeon, const EventRuntime& events);
    bool restore(const std::string& levelId, Dungeon& dungeon, EventRuntime& events) const;
    bool replaceSnapshots(std::vector<LevelRuntimeSnapshot> snapshots);
    bool erase(const std::string& levelId) { return snapshots_.erase(levelId) != 0; }
    void clear() { snapshots_.clear(); }
    bool contains(const std::string& levelId) const { return snapshots_.find(levelId) != snapshots_.end(); }
    const std::map<std::string, LevelRuntimeSnapshot>& snapshots() const { return snapshots_; }

private:
    std::map<std::string, LevelRuntimeSnapshot> snapshots_;
};

} // namespace sv
