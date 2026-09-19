#include "LevelIO.hpp"

#include "EnemyType.hpp"
#include "Lighting.hpp"
#include "Material.hpp"
#include "WorldEvents.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace sv {
namespace {
constexpr int LevelFormatVersion = 6;
constexpr int MinimumLevelFormatVersion = 1;
constexpr std::size_t MaxLevelObjects = 1024;

bool expect(std::istream& input, const char* expected, std::string& error) {
    std::string token;
    input >> token;
    if (!input || token != expected) {
        error = std::string("Expected '") + expected + "'.";
        return false;
    }
    return true;
}

bool isMapMarker(char marker) {
    return marker == '#' || marker == '.' || marker == 'D' || marker == 'S' || marker == 'E';
}

bool markerBlocksMovement(char marker) {
    return marker == '#';
}

bool coordinateIsWalkable(const LevelDefinition& level, int x, int y) {
    if (x < 0 || y < 0 || x >= level.width || y >= level.height ||
        static_cast<std::size_t>(y) >= level.map.size()) return false;
    const auto& row = level.map[static_cast<std::size_t>(y)];
    return static_cast<std::size_t>(x) < row.size() && !markerBlocksMovement(row[static_cast<std::size_t>(x)]);
}

const char* surfaceName(SurfaceKind surface) {
    switch (surface) {
        case SurfaceKind::Wall: return "WALL";
        case SurfaceKind::Floor: return "FLOOR";
        case SurfaceKind::Ceiling: return "CEILING";
    }
    return "WALL";
}

bool parseSurface(const std::string& value, SurfaceKind& surface) {
    if (value == "WALL") surface = SurfaceKind::Wall;
    else if (value == "FLOOR") surface = SurfaceKind::Floor;
    else if (value == "CEILING") surface = SurfaceKind::Ceiling;
    else return false;
    return true;
}

const char* ceilingModeName(CeilingMode mode) {
    return mode == CeilingMode::Sky ? "SKY" : "MATERIAL";
}

bool parseCeilingMode(const std::string& value, CeilingMode& mode) {
    if (value == "MATERIAL") mode = CeilingMode::Material;
    else if (value == "SKY") mode = CeilingMode::Sky;
    else return false;
    return true;
}

const char* waterFlowName(WaterFlow flow) {
    switch (flow) {
        case WaterFlow::Still: return "STILL";
        case WaterFlow::North: return "NORTH";
        case WaterFlow::East: return "EAST";
        case WaterFlow::South: return "SOUTH";
        case WaterFlow::West: return "WEST";
    }
    return "STILL";
}

bool parseWaterFlow(const std::string& value, WaterFlow& flow) {
    if (value == "STILL") flow = WaterFlow::Still;
    else if (value == "NORTH") flow = WaterFlow::North;
    else if (value == "EAST") flow = WaterFlow::East;
    else if (value == "SOUTH") flow = WaterFlow::South;
    else if (value == "WEST") flow = WaterFlow::West;
    else return false;
    return true;
}

bool startsWith(const std::string& value, const char* prefix) {
    const std::string prefixString{prefix};
    return value.rfind(prefixString, 0) == 0;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool supportedMusicExtension(const std::string& path) {
    const auto lower = lowercase(path);
    return (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".wav") ||
        (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".ogg") ||
        (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".mp3") ||
        (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".flac");
}
}

bool LevelIO::load(const std::string& path, LevelDefinition& level, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Could not open level file: " + path;
        return false;
    }

    std::string signature;
    int version{};
    input >> signature >> version;
    if (!input || signature != "STONEVEIL_LEVEL" || version < MinimumLevelFormatVersion ||
        version > LevelFormatVersion) {
        error = "Unsupported STONEVEIL level format.";
        return false;
    }

    LevelDefinition loaded;
    int width{};
    int height{};
    std::string ceilingMode;
    if (!expect(input, "ID", error) || !(input >> std::quoted(loaded.id)) ||
        !expect(input, "NAME", error) || !(input >> std::quoted(loaded.name))) {
        if (error.empty()) error = "Malformed level header.";
        return false;
    }
    // Level music arrived in format version 5. Older files load silently and
    // pick up the field the next time they are saved by the editor.
    if (version >= 5 && (!expect(input, "MUSIC", error) || !(input >> std::quoted(loaded.musicPath)))) {
        if (error.empty()) error = "Malformed level music entry.";
        return false;
    }
    if (!expect(input, "SIZE", error) || !(input >> width >> height) ||
        !expect(input, "SPAWN", error) || !(input >> loaded.spawnX >> loaded.spawnY >> loaded.spawnDirection) ||
        !expect(input, "DEFAULT_WALL", error) || !(input >> std::quoted(loaded.surfaces.wallMaterial)) ||
        !expect(input, "DEFAULT_FLOOR", error) || !(input >> std::quoted(loaded.surfaces.floorMaterial)) ||
        !expect(input, "CEILING_MODE", error) || !(input >> ceilingMode) ||
        !parseCeilingMode(ceilingMode, loaded.surfaces.ceilingMode) ||
        !expect(input, "DEFAULT_CEILING", error) || !(input >> std::quoted(loaded.surfaces.ceilingMaterial))) {
        if (error.empty()) error = "Malformed level header.";
        return false;
    }
    if (width < LevelDefinition::MinimumDimension || height < LevelDefinition::MinimumDimension ||
        width > LevelDefinition::MaximumDimension || height > LevelDefinition::MaximumDimension) {
        error = "Level dimensions must be between 4 and 64 cells.";
        return false;
    }
    loaded.width = width;
    loaded.height = height;

    if (!expect(input, "MAP", error)) return false;
    loaded.map.resize(static_cast<std::size_t>(loaded.height));
    for (auto& row : loaded.map) input >> row;
    if (!input || !expect(input, "END_MAP", error)) return false;
    // Legacy door-ID migration below indexes rows. Reject malformed geometry
    // before any migration touches those cells.
    for (const auto& row : loaded.map) {
        if (row.size() != static_cast<std::size_t>(loaded.width)) {
            error = "Every map row must match the level width.";
            return false;
        }
    }

    std::size_t pickupCount{};
    if (!expect(input, "PICKUPS", error) || !(input >> pickupCount) || pickupCount > MaxLevelObjects) {
        error = "Invalid pickup list.";
        return false;
    }
    for (std::size_t i = 0; i < pickupCount; ++i) {
        std::string type;
        Pickup pickup;
        input >> type >> pickup.x >> pickup.y;
        if (input && version >= 6) input >> std::quoted(pickup.id);
        if (!input || (type != "KEY" && type != "POTION")) {
            error = "Invalid pickup entry.";
            return false;
        }
        pickup.type = type == "KEY" ? Pickup::Type::Key : Pickup::Type::Potion;
        loaded.pickups.push_back(pickup);
    }

    std::size_t enemyCount{};
    if (!expect(input, "ENEMIES", error) || !(input >> enemyCount) || enemyCount > MaxLevelObjects) {
        error = "Invalid enemy list.";
        return false;
    }
    for (std::size_t i = 0; i < enemyCount; ++i) {
        Enemy enemy;
        input >> enemy.x >> enemy.y >> enemy.hp;
        // Enemy archetype ids arrived in format version 3. Older levels leave
        // typeId empty, which resolves to the default archetype at runtime.
        if (input && version >= 3) input >> std::quoted(enemy.typeId);
        if (input && version >= 6) input >> std::quoted(enemy.id);
        if (!input) {
            error = "Invalid enemy entry.";
            return false;
        }
        loaded.enemies.push_back(enemy);
    }

    std::size_t overrideCount{};
    if (!expect(input, "SURFACE_OVERRIDES", error) || !(input >> overrideCount) || overrideCount > MaxLevelObjects) {
        error = "Invalid surface override list.";
        return false;
    }
    for (std::size_t i = 0; i < overrideCount; ++i) {
        CellSurfaceOverride surfaceOverride;
        std::string surface;
        std::string mode;
        input >> surfaceOverride.x >> surfaceOverride.y >> surface >> mode >> std::quoted(surfaceOverride.materialId);
        if (!input || !parseSurface(surface, surfaceOverride.surface) || !parseCeilingMode(mode, surfaceOverride.ceilingMode)) {
            error = "Invalid surface override entry.";
            return false;
        }
        loaded.surfaceOverrides.push_back(std::move(surfaceOverride));
    }

    // Lights arrived in format version 2. Version 1 levels simply have none.
    if (version >= 2) {
        std::size_t lightCount{};
        if (!expect(input, "LIGHTS", error) || !(input >> lightCount) || lightCount > MaxLevelObjects) {
            error = "Invalid light list.";
            return false;
        }
        for (std::size_t i = 0; i < lightCount; ++i) {
            LightPlacement light;
            input >> light.x >> light.y >> std::quoted(light.lightId);
            if (!input) {
                error = "Invalid light entry.";
                return false;
            }
            loaded.lights.push_back(std::move(light));
        }
    }

    // Water volumes arrived in format version 4. Older levels simply have no
    // mechanical water; visual shallow-water floor materials remain separate.
    if (version >= 4) {
        std::size_t waterCount{};
        if (!expect(input, "WATER", error) || !(input >> waterCount) || waterCount > MaxLevelObjects) {
            error = "Invalid water list.";
            return false;
        }
        for (std::size_t i = 0; i < waterCount; ++i) {
            WaterCell water;
            std::string flow;
            input >> water.x >> water.y >> water.depth >> flow >> water.volumeId;
            if (!input || !parseWaterFlow(flow, water.flow)) {
                error = "Invalid water entry.";
                return false;
            }
            loaded.water.push_back(water);
        }
    }

    if (version >= 6) {
        std::size_t doorCount{};
        if (!expect(input, "DOORS", error) || !(input >> doorCount) || doorCount > MaxLevelObjects) {
            error = "Invalid door metadata list.";
            return false;
        }
        for (std::size_t i = 0; i < doorCount; ++i) {
            DoorPlacement door;
            std::string kind;
            int locked{};
            input >> std::quoted(door.id) >> door.x >> door.y >> kind >> locked;
            if (!input || !parseDoorKind(kind, door.kind) || (locked != 0 && locked != 1)) {
                error = "Invalid door metadata entry.";
                return false;
            }
            door.locked = locked != 0;
            loaded.doors.push_back(std::move(door));
        }

        std::size_t objectCount{};
        if (!expect(input, "OBJECTS", error) || !(input >> objectCount) || objectCount > MaxLevelObjects) {
            error = "Invalid world object list.";
            return false;
        }
        for (std::size_t i = 0; i < objectCount; ++i) {
            WorldObject object;
            std::string kind;
            int blocks{};
            input >> std::quoted(object.id) >> kind >> object.x >> object.y >> blocks >>
                std::quoted(object.name) >> std::quoted(object.text);
            if (!input || !parseWorldObjectKind(kind, object.kind) || (blocks != 0 && blocks != 1)) {
                error = "Invalid world object entry.";
                return false;
            }
            object.blocksMovement = blocks != 0;
            loaded.objects.push_back(std::move(object));
        }

        std::size_t roomCount{};
        if (!expect(input, "ROOMS", error) || !(input >> roomCount) || roomCount > MaxLevelObjects) {
            error = "Invalid story room list.";
            return false;
        }
        for (std::size_t i = 0; i < roomCount; ++i) {
            StoryRoom room;
            input >> std::quoted(room.id) >> room.x >> room.y >> room.width >> room.height >>
                std::quoted(room.name) >> std::quoted(room.purpose) >> std::quoted(room.mood) >>
                std::quoted(room.lore) >> std::quoted(room.intendedFeeling);
            if (!input) {
                error = "Invalid story room entry.";
                return false;
            }
            loaded.rooms.push_back(std::move(room));
        }

        std::size_t triggerCount{};
        if (!expect(input, "TRIGGERS", error) || !(input >> triggerCount) || triggerCount > MaxLevelObjects) {
            error = "Invalid story trigger list.";
            return false;
        }
        for (std::size_t i = 0; i < triggerCount; ++i) {
            StoryTrigger trigger;
            std::string event;
            int once{};
            input >> std::quoted(trigger.id) >> event >> trigger.x >> trigger.y >>
                std::quoted(trigger.subjectId) >> once >> std::quoted(trigger.message);
            if (!input || !parseTriggerEvent(event, trigger.event) || (once != 0 && once != 1)) {
                error = "Invalid story trigger entry.";
                return false;
            }
            trigger.once = once != 0;
            loaded.triggers.push_back(std::move(trigger));
        }
    } else {
        for (std::size_t i = 0; i < loaded.pickups.size(); ++i) {
            loaded.pickups[i].id = "pickup.legacy." + std::to_string(i + 1);
        }
        for (std::size_t i = 0; i < loaded.enemies.size(); ++i) {
            loaded.enemies[i].id = "enemy.legacy." + std::to_string(i + 1);
        }
        int doorNumber = 1;
        for (int y = 0; y < loaded.height; ++y) {
            for (int x = 0; x < loaded.width; ++x) {
                if (loaded.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 'D') {
                    loaded.doors.push_back({"door.legacy." + std::to_string(doorNumber++), x, y,
                                            DoorKind::Door, true});
                }
            }
        }
    }

    if (!expect(input, "END", error)) return false;
    const auto errors = validate(loaded);
    if (!errors.empty()) {
        error = errors.front();
        return false;
    }
    level = std::move(loaded);
    error.clear();
    return true;
}

bool LevelIO::save(const std::string& path, const LevelDefinition& level, std::string& error) {
    const auto errors = validate(level);
    if (!errors.empty()) {
        error = errors.front();
        return false;
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "Could not write level file: " + path;
        return false;
    }
    output << "STONEVEIL_LEVEL " << LevelFormatVersion << '\n';
    output << "ID " << std::quoted(level.id) << '\n';
    output << "NAME " << std::quoted(level.name) << '\n';
    output << "MUSIC " << std::quoted(level.musicPath) << '\n';
    output << "SIZE " << level.width << ' ' << level.height << '\n';
    output << "SPAWN " << level.spawnX << ' ' << level.spawnY << ' ' << level.spawnDirection << '\n';
    output << "DEFAULT_WALL " << std::quoted(level.surfaces.wallMaterial) << '\n';
    output << "DEFAULT_FLOOR " << std::quoted(level.surfaces.floorMaterial) << '\n';
    output << "CEILING_MODE " << ceilingModeName(level.surfaces.ceilingMode) << '\n';
    output << "DEFAULT_CEILING " << std::quoted(level.surfaces.ceilingMaterial) << '\n';
    output << "MAP\n";
    for (const auto& row : level.map) output << row << '\n';
    output << "END_MAP\n";
    output << "PICKUPS " << level.pickups.size() << '\n';
    for (const auto& pickup : level.pickups) {
        output << (pickup.type == Pickup::Type::Key ? "KEY" : "POTION") << ' ' << pickup.x << ' ' << pickup.y
               << ' ' << std::quoted(pickup.id) << '\n';
    }
    output << "ENEMIES " << level.enemies.size() << '\n';
    for (const auto& enemy : level.enemies) {
        output << enemy.x << ' ' << enemy.y << ' ' << enemy.hp << ' '
               << std::quoted(enemy.typeId.empty() ? defaultEnemyTypeId() : enemy.typeId) << ' '
               << std::quoted(enemy.id) << '\n';
    }
    output << "SURFACE_OVERRIDES " << level.surfaceOverrides.size() << '\n';
    for (const auto& surfaceOverride : level.surfaceOverrides) {
        output << surfaceOverride.x << ' ' << surfaceOverride.y << ' ' << surfaceName(surfaceOverride.surface) << ' '
               << ceilingModeName(surfaceOverride.ceilingMode) << ' ' << std::quoted(surfaceOverride.materialId) << '\n';
    }
    output << "LIGHTS " << level.lights.size() << '\n';
    for (const auto& light : level.lights) {
        output << light.x << ' ' << light.y << ' ' << std::quoted(light.lightId) << '\n';
    }
    output << "WATER " << level.water.size() << '\n';
    for (const auto& water : level.water) {
        output << water.x << ' ' << water.y << ' ' << water.depth << ' '
               << waterFlowName(water.flow) << ' ' << water.volumeId << '\n';
    }
    output << "DOORS " << level.doors.size() << '\n';
    for (const auto& door : level.doors) {
        output << std::quoted(door.id) << ' ' << door.x << ' ' << door.y << ' ' << doorKindName(door.kind)
               << ' ' << (door.locked ? 1 : 0) << '\n';
    }
    output << "OBJECTS " << level.objects.size() << '\n';
    for (const auto& object : level.objects) {
        output << std::quoted(object.id) << ' ' << worldObjectKindName(object.kind) << ' ' << object.x << ' '
               << object.y << ' ' << (object.blocksMovement ? 1 : 0) << ' ' << std::quoted(object.name) << ' '
               << std::quoted(object.text) << '\n';
    }
    output << "ROOMS " << level.rooms.size() << '\n';
    for (const auto& room : level.rooms) {
        output << std::quoted(room.id) << ' ' << room.x << ' ' << room.y << ' ' << room.width << ' '
               << room.height << ' ' << std::quoted(room.name) << ' ' << std::quoted(room.purpose) << ' '
               << std::quoted(room.mood) << ' ' << std::quoted(room.lore) << ' '
               << std::quoted(room.intendedFeeling) << '\n';
    }
    output << "TRIGGERS " << level.triggers.size() << '\n';
    for (const auto& trigger : level.triggers) {
        output << std::quoted(trigger.id) << ' ' << triggerEventName(trigger.event) << ' ' << trigger.x << ' '
               << trigger.y << ' ' << std::quoted(trigger.subjectId) << ' ' << (trigger.once ? 1 : 0) << ' '
               << std::quoted(trigger.message) << '\n';
    }
    output << "END\n";
    if (!output) {
        error = "Failed while writing level data.";
        return false;
    }
    error.clear();
    return true;
}

std::vector<std::string> LevelIO::validate(const LevelDefinition& level) {
    std::vector<std::string> errors;
    std::set<std::string> authoredIds;
    const auto validateId = [&errors, &authoredIds](const std::string& id, const char* label) {
        if (id.empty()) {
            errors.push_back(std::string(label) + " needs a stable ID.");
            return;
        }
        if (!authoredIds.insert(id).second) errors.push_back("Authored object IDs must be unique within a level.");
    };
    if (level.id.empty()) errors.push_back("Level ID cannot be empty.");
    if (level.name.empty()) errors.push_back("Level name cannot be empty.");
    if (!level.musicPath.empty()) {
        if (level.musicPath.size() > 240) errors.push_back("Level music path is too long.");
        if (!startsWith(level.musicPath, "content/audio/")) {
            errors.push_back("Level music must use a content/audio/ relative path.");
        }
        if (level.musicPath.find("..") != std::string::npos ||
            level.musicPath.find('\\') != std::string::npos ||
            level.musicPath.find(':') != std::string::npos ||
            level.musicPath.front() == '/') {
            errors.push_back("Level music path must be a safe relative path.");
        }
        if (!supportedMusicExtension(level.musicPath)) {
            errors.push_back("Level music must be a .wav, .ogg, .mp3, or .flac file.");
        }
    }
    if (level.spawnDirection < 0 || level.spawnDirection >= 4) errors.push_back("Spawn direction must be between 0 and 3.");
    const bool validDimensions = level.width >= LevelDefinition::MinimumDimension &&
        level.height >= LevelDefinition::MinimumDimension &&
        level.width <= LevelDefinition::MaximumDimension &&
        level.height <= LevelDefinition::MaximumDimension;
    if (!validDimensions) errors.push_back("Level dimensions must be between 4 and 64 cells.");
    if (validDimensions && level.map.size() != static_cast<std::size_t>(level.height)) {
        errors.push_back("Map row count must match the level height.");
    }

    const auto validateMaterial = [&errors](const std::string& id, SurfaceKind expected, const char* label) {
        const auto* material = findMaterial(id);
        if (material == nullptr || material->surface != expected) errors.push_back(std::string(label) + " references an invalid material.");
    };
    validateMaterial(level.surfaces.wallMaterial, SurfaceKind::Wall, "Default wall");
    validateMaterial(level.surfaces.floorMaterial, SurfaceKind::Floor, "Default floor");
    if (level.surfaces.ceilingMode == CeilingMode::Material) {
        validateMaterial(level.surfaces.ceilingMaterial, SurfaceKind::Ceiling, "Default ceiling");
    }

    int exitCount = 0;
    if (validDimensions) for (int y = 0; y < level.height; ++y) {
        if (static_cast<std::size_t>(y) >= level.map.size()) continue;
        if (level.map[static_cast<std::size_t>(y)].size() != static_cast<std::size_t>(level.width)) {
            errors.push_back("Every map row must match the level width.");
            continue;
        }
        for (int x = 0; x < level.width; ++x) {
            const char marker = level.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (!isMapMarker(marker)) errors.push_back("Map contains an unknown structure marker.");
            if (marker == 'E') ++exitCount;
            if ((x == 0 || y == 0 || x == level.width - 1 || y == level.height - 1) && marker != '#') {
                errors.push_back("The map boundary must be enclosed by walls.");
            }
        }
    }
    if (exitCount != 1) errors.push_back("A level must contain exactly one exit.");
    if (!coordinateIsWalkable(level, level.spawnX, level.spawnY)) errors.push_back("Player spawn must be on a walkable cell.");

    std::set<std::pair<int, int>> occupiedGameplayCells;
    occupiedGameplayCells.insert({level.spawnX, level.spawnY});
    for (const auto& pickup : level.pickups) {
        if (!coordinateIsWalkable(level, pickup.x, pickup.y)) errors.push_back("A pickup is outside the walkable map.");
        if (!occupiedGameplayCells.insert({pickup.x, pickup.y}).second) errors.push_back("A gameplay cell holds overlapping objects.");
        validateId(pickup.id, "Pickup");
    }
    for (const auto& enemy : level.enemies) {
        if (!coordinateIsWalkable(level, enemy.x, enemy.y) || enemy.hp <= 0) errors.push_back("An enemy has an invalid placement or health value.");
        if (!occupiedGameplayCells.insert({enemy.x, enemy.y}).second) errors.push_back("A gameplay cell holds overlapping objects.");
        validateId(enemy.id, "Enemy");
        if (!enemy.typeId.empty() && findEnemyType(enemy.typeId) == nullptr) {
            errors.push_back("An enemy references an unknown archetype id.");
        }
    }
    if (level.lights.size() > Lighting::MaxLightsPerLevel) {
        errors.push_back("A level cannot hold more than 512 lights.");
    }
    std::set<std::pair<int, int>> occupiedLightCells;
    for (const auto& light : level.lights) {
        if (light.x < 0 || light.y < 0 || light.x >= level.width || light.y >= level.height) {
            errors.push_back("A light is placed outside the map.");
            continue;
        }
        if (!occupiedLightCells.insert({light.x, light.y}).second) {
            errors.push_back("A cell holds more than one light.");
        }
        if (findLight(light.lightId) == nullptr) {
            errors.push_back("A light references an unknown light type.");
        }
    }

    std::set<std::tuple<int, int, SurfaceKind>> overriddenSurfaces;
    for (const auto& surfaceOverride : level.surfaceOverrides) {
        if (surfaceOverride.x < 0 || surfaceOverride.y < 0 || surfaceOverride.x >= level.width ||
            surfaceOverride.y >= level.height) {
            errors.push_back("A surface override is outside the map.");
            continue;
        }
        if (!overriddenSurfaces.insert({surfaceOverride.x, surfaceOverride.y, surfaceOverride.surface}).second) {
            errors.push_back("A cell has duplicate overrides for the same surface.");
        }
        if (surfaceOverride.ceilingMode == CeilingMode::Sky && surfaceOverride.surface != SurfaceKind::Ceiling) {
            errors.push_back("Sky can only be assigned to a ceiling surface.");
            continue;
        }
        if (surfaceOverride.ceilingMode != CeilingMode::Sky) {
            validateMaterial(surfaceOverride.materialId, surfaceOverride.surface, "Surface override");
        }
        if (static_cast<std::size_t>(surfaceOverride.y) < level.map.size()) {
            const auto& row = level.map[static_cast<std::size_t>(surfaceOverride.y)];
            if (static_cast<std::size_t>(surfaceOverride.x) >= row.size()) continue;
            const bool solid = row[static_cast<std::size_t>(surfaceOverride.x)] == '#';
            if (surfaceOverride.surface == SurfaceKind::Wall && !solid) {
                errors.push_back("A wall override must target a solid wall cell.");
            }
            if (surfaceOverride.surface != SurfaceKind::Wall && solid) {
                errors.push_back("A floor or ceiling override must target a walkable cell.");
            }
        }
    }

    std::set<std::pair<int, int>> occupiedWaterCells;
    for (const auto& water : level.water) {
        if (water.x < 0 || water.y < 0 || water.x >= level.width || water.y >= level.height) {
            errors.push_back("A water cell is outside the map.");
            continue;
        }
        if (water.depth <= 0 || water.depth > 3) {
            errors.push_back("Water depth must be between 1 and 3.");
        }
        if (water.volumeId < 0) {
            errors.push_back("Water volume id cannot be negative.");
        }
        if (!occupiedWaterCells.insert({water.x, water.y}).second) {
            errors.push_back("A cell has duplicate water state.");
        }
        if (!coordinateIsWalkable(level, water.x, water.y)) {
            errors.push_back("Water must target a walkable cell.");
        }
    }

    std::set<std::pair<int, int>> doorCells;
    for (const auto& door : level.doors) {
        validateId(door.id, "Door");
        if (!coordinateIsWalkable(level, door.x, door.y) ||
            level.map[static_cast<std::size_t>(door.y)][static_cast<std::size_t>(door.x)] != 'D') {
            errors.push_back("Door metadata must target a door cell.");
            continue;
        }
        if (!doorCells.insert({door.x, door.y}).second) errors.push_back("A door cell has duplicate metadata.");
    }
    if (validDimensions) {
        for (int y = 0; y < level.height; ++y) {
            if (static_cast<std::size_t>(y) >= level.map.size() ||
                level.map[static_cast<std::size_t>(y)].size() != static_cast<std::size_t>(level.width)) continue;
            for (int x = 0; x < level.width; ++x) {
                if (level.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 'D' &&
                    doorCells.find({x, y}) == doorCells.end()) {
                    errors.push_back("Every normal door needs door metadata.");
                }
            }
        }
    }

    std::set<std::pair<int, int>> objectCells;
    for (const auto& object : level.objects) {
        validateId(object.id, "World object");
        if (!coordinateIsWalkable(level, object.x, object.y)) errors.push_back("A world object is outside the walkable map.");
        if (object.name.empty()) errors.push_back("World objects need a display name.");
        if (!objectCells.insert({object.x, object.y}).second) errors.push_back("A cell holds more than one world object.");
        if (!occupiedGameplayCells.insert({object.x, object.y}).second) errors.push_back("A gameplay cell holds overlapping objects.");
    }

    for (const auto& room : level.rooms) {
        validateId(room.id, "Story room");
        if (room.width <= 0 || room.height <= 0 || room.x < 0 || room.y < 0 ||
            room.x + room.width > level.width || room.y + room.height > level.height) {
            errors.push_back("A story room has invalid bounds.");
        }
        if (room.name.empty()) errors.push_back("Story rooms need a name.");
    }

    for (const auto& trigger : level.triggers) {
        if (!trigger.subjectId.empty() && authoredIds.find(trigger.subjectId) == authoredIds.end()) {
            errors.push_back("A story trigger references an unknown authored subject ID.");
        }
        validateId(trigger.id, "Story trigger");
        if (trigger.x < 0 || trigger.y < 0 || trigger.x >= level.width || trigger.y >= level.height) {
            errors.push_back("A story trigger is outside the map.");
        }
        if (trigger.message.empty()) errors.push_back("Story triggers need message text.");
    }

    if (validDimensions && coordinateIsWalkable(level, level.spawnX, level.spawnY)) {
        std::vector<unsigned char> visited(static_cast<std::size_t>(level.width) * static_cast<std::size_t>(level.height));
        const auto indexOf = [&level](int x, int y) {
            return static_cast<std::size_t>(y) * static_cast<std::size_t>(level.width) + static_cast<std::size_t>(x);
        };
        std::queue<std::pair<int, int>> frontier;
        frontier.push({level.spawnX, level.spawnY});
        visited[indexOf(level.spawnX, level.spawnY)] = 1;
        constexpr int dx[4] = {0, 1, 0, -1};
        constexpr int dy[4] = {-1, 0, 1, 0};
        bool reachedExit = false;
        while (!frontier.empty()) {
            const auto [x, y] = frontier.front();
            frontier.pop();
            if (level.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 'E') reachedExit = true;
            for (int direction = 0; direction < 4; ++direction) {
                const int nextX = x + dx[direction];
                const int nextY = y + dy[direction];
                if (!coordinateIsWalkable(level, nextX, nextY) || visited[indexOf(nextX, nextY)]) continue;
                visited[indexOf(nextX, nextY)] = 1;
                frontier.push({nextX, nextY});
            }
        }
        if (!reachedExit) errors.push_back("The exit is not structurally reachable from the spawn.");
    }
    if (errors.empty()) {
        EventRuntime compiled;
        if (!configureWorldEvents(Dungeon{level}, compiled))
            errors.push_back("A story trigger ID conflicts with a door's generated behavior ID. Rename the trigger ID.");
    }
    return errors;
}

} // namespace sv
