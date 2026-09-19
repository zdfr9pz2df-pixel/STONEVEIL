#include "Dungeon.hpp"

#include <algorithm>

namespace sv {

Dungeon::Dungeon() : Dungeon(levelOneDefinition()) {}

Dungeon::Dungeon(const LevelDefinition& definition)
    : width_(std::clamp(definition.width, 1, LevelDefinition::MaximumDimension)),
      height_(std::clamp(definition.height, 1, LevelDefinition::MaximumDimension)),
      levelId_(definition.id),
      name_(definition.name),
      musicPath_(definition.musicPath),
      spawnX_(definition.spawnX),
      spawnY_(definition.spawnY),
      spawnDirection_(definition.spawnDirection),
      tiles_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), Tile::Wall),
      surfaces_(definition.surfaces),
      surfaceOverrides_(definition.surfaceOverrides),
      doors_(definition.doors),
      objects_(definition.objects),
      rooms_(definition.rooms),
      triggers_(definition.triggers) {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const bool hasRow = static_cast<std::size_t>(y) < definition.map.size();
            const char marker = hasRow && x < static_cast<int>(definition.map[static_cast<std::size_t>(y)].size())
                ? definition.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]
                : '#';
            switch (marker) {
                case '#': tiles_[tileIndex(x, y)] = Tile::Wall; break;
                case 'D': tiles_[tileIndex(x, y)] = Tile::DoorClosed; break;
                case 'E': tiles_[tileIndex(x, y)] = Tile::Exit; break;
                case 'S': tiles_[tileIndex(x, y)] = Tile::SecretDoorClosed; break;
                default: tiles_[tileIndex(x, y)] = Tile::Floor; break;
            }
        }
    }
    pickups_ = definition.pickups;
    enemies_ = definition.enemies;
    for (const auto& light : definition.lights) {
        if (inBounds(light.x, light.y)) lights_.push_back(light);
    }
    for (const auto& water : definition.water) {
        if (inBounds(water.x, water.y) && water.depth > 0) water_.push_back(water);
    }
}

Tile Dungeon::tile(int x, int y) const {
    if (!inBounds(x, y)) return Tile::Wall;
    return tiles_[tileIndex(x, y)];
}

bool Dungeon::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

bool Dungeon::blocksMovement(int x, int y) const {
    const auto t = tile(x, y);
    if (t == Tile::Wall || t == Tile::DoorClosed || t == Tile::SecretDoorClosed) return true;
    const auto* object = objectAt(x, y);
    return object != nullptr && object->blocksMovement;
}

bool Dungeon::blocksSight(int x, int y) const {
    const auto t = tile(x, y);
    return t == Tile::Wall || t == Tile::DoorClosed || t == Tile::SecretDoorClosed;
}

bool Dungeon::openDoor(int x, int y, bool hasKey) {
    if (!inBounds(x, y) || tiles_[tileIndex(x, y)] != Tile::DoorClosed) return false;
    if (doorRequiresKeyAt(x, y) && !hasKey) return false;
    tiles_[tileIndex(x, y)] = Tile::DoorOpen;
    return true;
}

bool Dungeon::revealSecret(int x, int y) {
    if (!inBounds(x, y) || tiles_[tileIndex(x, y)] != Tile::SecretDoorClosed) return false;
    tiles_[tileIndex(x, y)] = Tile::DoorOpen;
    return true;
}

bool Dungeon::restoreTile(int x, int y, Tile tileValue) {
    if (!inBounds(x, y)) return false;
    const int value = static_cast<int>(tileValue);
    if (value < static_cast<int>(Tile::Floor) || value > static_cast<int>(Tile::SecretDoorClosed)) return false;
    tiles_[tileIndex(x, y)] = tileValue;
    return true;
}

std::size_t Dungeon::tileIndex(int x, int y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

const std::string& Dungeon::materialAt(int x, int y, SurfaceKind surface) const {
    for (const auto& surfaceOverride : surfaceOverrides_) {
        if (surfaceOverride.x == x && surfaceOverride.y == y && surfaceOverride.surface == surface &&
            surfaceOverride.ceilingMode != CeilingMode::Sky) {
            return surfaceOverride.materialId;
        }
    }
    if (surface == SurfaceKind::Wall) return surfaces_.wallMaterial;
    if (surface == SurfaceKind::Floor) return surfaces_.floorMaterial;
    return surfaces_.ceilingMaterial;
}

CeilingMode Dungeon::ceilingModeAt(int x, int y) const {
    for (const auto& surfaceOverride : surfaceOverrides_) {
        if (surfaceOverride.x == x && surfaceOverride.y == y && surfaceOverride.surface == SurfaceKind::Ceiling) {
            return surfaceOverride.ceilingMode;
        }
    }
    return surfaces_.ceilingMode;
}

int Dungeon::waterDepthAt(int x, int y) const {
    for (const auto& water : water_) {
        if (water.x == x && water.y == y) return water.depth;
    }
    return 0;
}

WaterFlow Dungeon::waterFlowAt(int x, int y) const {
    for (const auto& water : water_) {
        if (water.x == x && water.y == y) return water.flow;
    }
    return WaterFlow::Still;
}

int Dungeon::waterVolumeAt(int x, int y) const {
    for (const auto& water : water_) {
        if (water.x == x && water.y == y) return water.volumeId;
    }
    return -1;
}

const DoorPlacement* Dungeon::doorAt(int x, int y) const {
    for (const auto& door : doors_) {
        if (door.x == x && door.y == y) return &door;
    }
    return nullptr;
}

bool Dungeon::doorRequiresKeyAt(int x, int y) const {
    const auto* door = doorAt(x, y);
    // Levels written before door metadata existed preserve their locked-door
    // behavior instead of silently becoming easier.
    return door == nullptr || door->locked;
}

const WorldObject* Dungeon::objectAt(int x, int y) const {
    for (const auto& object : objects_) {
        if (object.kind != WorldObjectKind::ArrivalPoint && object.x == x && object.y == y) return &object;
    }
    return nullptr;
}

const StoryRoom* Dungeon::roomAt(int x, int y) const {
    for (const auto& room : rooms_) {
        if (room.contains(x, y)) return &room;
    }
    return nullptr;
}

} // namespace sv
