#pragma once

#include "EnemyType.hpp"
#include "Lighting.hpp"
#include "Material.hpp"
#include "WorldAuthoring.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace sv {

enum class Tile : int {
    Floor = 0,
    Wall = 1,
    DoorClosed = 2,
    DoorOpen = 3,
    Exit = 4,
    SecretDoorClosed = 5,
};

struct Pickup {
    enum class Type { Key, Potion };
    Type type{};
    int x{};
    int y{};
    bool taken{false};
    std::string id;
};

struct Enemy {
    int x{};
    int y{};
    int hp{18};
    bool alive{true};
    float attackCooldown{0.0f};

    // Authored. Empty resolves to defaultEnemyTypeId() so pre-v3 levels load.
    std::string typeId;

    // Runtime only. Never serialized: an enemy mid-windup is an animation
    // state, not gameplay state, and saving is blocked while one is resolving.
    bool winding{false};
    float windupRemaining{0.0f};
    float recoveryRemaining{0.0f};
    float moveRemaining{0.0f};

    // Stable authored identity used by story triggers. Runtime movement does
    // not change it.
    std::string id;
};

struct LevelSurfaceDefaults {
    std::string wallMaterial{"material.wall.cave.mossy-rock"};
    std::string floorMaterial{"material.floor.cave.uneven-stone"};
    CeilingMode ceilingMode{CeilingMode::Material};
    std::string ceilingMaterial{"material.ceiling.cave.rocky"};
};

struct CellSurfaceOverride {
    int x{};
    int y{};
    std::string materialId;
    SurfaceKind surface{SurfaceKind::Wall};
    CeilingMode ceilingMode{CeilingMode::Material};
};

enum class WaterFlow {
    Still,
    North,
    East,
    South,
    West,
};

struct WaterCell {
    int x{};
    int y{};
    int depth{1};
    WaterFlow flow{WaterFlow::Still};
    int volumeId{0};
};

struct LevelDefinition {
    static constexpr int DefaultWidth = 16;
    static constexpr int DefaultHeight = 16;
    static constexpr int MinimumDimension = 4;
    static constexpr int MaximumDimension = 64;

    std::string id;
    std::string name;
    std::string musicPath;
    int width{DefaultWidth};
    int height{DefaultHeight};
    std::vector<std::string> map;
    int spawnX{2};
    int spawnY{2};
    int spawnDirection{1};
    std::vector<Pickup> pickups;
    std::vector<Enemy> enemies;
    std::vector<LightPlacement> lights;
    LevelSurfaceDefaults surfaces;
    std::vector<CellSurfaceOverride> surfaceOverrides;
    std::vector<WaterCell> water;
    std::vector<DoorPlacement> doors;
    std::vector<WorldObject> objects;
    std::vector<StoryRoom> rooms;
    std::vector<StoryTrigger> triggers;
};

const LevelDefinition& levelOneDefinition();

class Dungeon {
public:
    Dungeon();
    explicit Dungeon(const LevelDefinition& definition);

    Tile tile(int x, int y) const;
    bool inBounds(int x, int y) const;
    bool blocksMovement(int x, int y) const;
    bool blocksSight(int x, int y) const;
    bool openDoor(int x, int y, bool hasKey);
    bool revealSecret(int x, int y);
    bool restoreTile(int x, int y, Tile tile);

    const std::string& levelId() const { return levelId_; }
    const std::string& name() const { return name_; }
    const std::string& musicPath() const { return musicPath_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int spawnX() const { return spawnX_; }
    int spawnY() const { return spawnY_; }
    int spawnDirection() const { return spawnDirection_; }
    const std::string& materialAt(int x, int y, SurfaceKind surface) const;
    CeilingMode ceilingModeAt(int x, int y) const;
    int waterDepthAt(int x, int y) const;
    WaterFlow waterFlowAt(int x, int y) const;
    int waterVolumeAt(int x, int y) const;
    bool hasWaterAt(int x, int y) const { return waterDepthAt(x, y) > 0; }
    const DoorPlacement* doorAt(int x, int y) const;
    bool doorRequiresKeyAt(int x, int y) const;
    const WorldObject* objectAt(int x, int y) const;
    const StoryRoom* roomAt(int x, int y) const;

    std::vector<Pickup>& pickups() { return pickups_; }
    const std::vector<Pickup>& pickups() const { return pickups_; }
    std::vector<Enemy>& enemies() { return enemies_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    std::vector<LightPlacement>& lights() { return lights_; }
    const std::vector<LightPlacement>& lights() const { return lights_; }
    std::vector<WaterCell>& water() { return water_; }
    const std::vector<WaterCell>& water() const { return water_; }
    const std::vector<DoorPlacement>& doors() const { return doors_; }
    const std::vector<WorldObject>& objects() const { return objects_; }
    const std::vector<StoryRoom>& rooms() const { return rooms_; }
    const std::vector<StoryTrigger>& triggers() const { return triggers_; }

private:
    std::size_t tileIndex(int x, int y) const;

    int width_{LevelDefinition::DefaultWidth};
    int height_{LevelDefinition::DefaultHeight};
    std::string levelId_;
    std::string name_;
    std::string musicPath_;
    int spawnX_{2};
    int spawnY_{2};
    int spawnDirection_{1};
    std::vector<Tile> tiles_;
    LevelSurfaceDefaults surfaces_{};
    std::vector<CellSurfaceOverride> surfaceOverrides_;
    std::vector<Pickup> pickups_;
    std::vector<Enemy> enemies_;
    std::vector<LightPlacement> lights_;
    std::vector<WaterCell> water_;
    std::vector<DoorPlacement> doors_;
    std::vector<WorldObject> objects_;
    std::vector<StoryRoom> rooms_;
    std::vector<StoryTrigger> triggers_;
};

} // namespace sv
