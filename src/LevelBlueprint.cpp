#include "LevelBlueprint.hpp"

#include "LevelIO.hpp"
#include "Lighting.hpp"
#include "Material.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace sv {
namespace {

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool quoted = false;
    for (char ch : line) {
        if (!quoted && ch == '/' && !token.empty() && token.back() == '/') {
            token.pop_back();
            break;
        }
        if (ch == '"') {
            quoted = !quoted;
            continue;
        }
        if (!quoted && std::isspace(static_cast<unsigned char>(ch))) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            continue;
        }
        token.push_back(ch);
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

bool parseInt(const std::string& text, int& value) {
    std::istringstream input(text);
    input >> value;
    return input && input.eof();
}

bool parseDirection(const std::string& text, int& direction) {
    const std::string value = upper(text);
    if (value == "N" || value == "NORTH" || value == "0") direction = 0;
    else if (value == "E" || value == "EAST" || value == "1") direction = 1;
    else if (value == "S" || value == "SOUTH" || value == "2") direction = 2;
    else if (value == "W" || value == "WEST" || value == "3") direction = 3;
    else return false;
    return true;
}

bool parseBrush(const std::string& text, char& marker) {
    const std::string value = upper(text);
    if (value == "WALL" || value == "SOLID" || value == "#") marker = '#';
    else if (value == "FLOOR" || value == "OPEN" || value == ".") marker = '.';
    else if (value == "DOOR" || value == "D") marker = 'D';
    else if (value == "SECRET" || value == "SECRET_DOOR" || value == "S") marker = 'S';
    else if (value == "EXIT" || value == "E") marker = 'E';
    else return false;
    return true;
}

bool parseSurfaceKind(const std::string& text, SurfaceKind& surface) {
    const std::string value = upper(text);
    if (value == "WALL" || value == "WALLS") surface = SurfaceKind::Wall;
    else if (value == "FLOOR" || value == "FLOORS") surface = SurfaceKind::Floor;
    else if (value == "CEILING" || value == "CEILINGS") surface = SurfaceKind::Ceiling;
    else return false;
    return true;
}

bool parseWaterFlow(const std::string& text, WaterFlow& flow) {
    const std::string value = upper(text);
    if (value == "STILL" || value == "NONE" || value == "0") flow = WaterFlow::Still;
    else if (value == "N" || value == "NORTH") flow = WaterFlow::North;
    else if (value == "E" || value == "EAST") flow = WaterFlow::East;
    else if (value == "S" || value == "SOUTH") flow = WaterFlow::South;
    else if (value == "W" || value == "WEST") flow = WaterFlow::West;
    else return false;
    return true;
}

std::string materialAlias(const std::string& value) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"cave.mossy", "material.wall.cave.mossy-rock"},
        {"cave.mossy-rock", "material.wall.cave.mossy-rock"},
        {"cave.vines", "material.wall.cave.mossy-rock-vines"},
        {"cave.root-trace", "material.wall.cave.mossy-rock-root-trace"},
        {"cave.wet-limestone", "material.wall.cave.wet-limestone"},
        {"cave.cracked-basalt", "material.wall.cave.cracked-basalt"},
        {"fortress.dressed-stone", "material.wall.fortress.dressed-stone"},
        {"timber.hewn", "material.wall.timber.hewn-planks"},
        {"timber.hewn-planks", "material.wall.timber.hewn-planks"},
        {"stone", "material.floor.cave.uneven-stone"},
        {"cave.uneven-stone", "material.floor.cave.uneven-stone"},
        {"mud", "material.floor.cave.mud"},
        {"cave.mud", "material.floor.cave.mud"},
        {"water", "material.floor.cave.shallow-water"},
        {"cave.shallow-water", "material.floor.cave.shallow-water"},
        {"timber.planks", "material.floor.timber.planks"},
        {"rocky", "material.ceiling.cave.rocky"},
        {"cave.rocky", "material.ceiling.cave.rocky"},
        {"cave.timber-supports", "material.ceiling.cave.timber-supports"},
        {"fortress.vaulted-stone", "material.ceiling.fortress.vaulted-stone"},
        {"timber.rafters", "material.ceiling.timber.rafters"},
    };
    const auto found = aliases.find(value);
    return found == aliases.end() ? value : found->second;
}

std::string lightAlias(const std::string& value) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"sconce", "light.torch.iron-sconce"},
        {"iron-sconce", "light.torch.iron-sconce"},
        {"brazier", "light.torch.pitch-brazier"},
        {"pitch-brazier", "light.torch.pitch-brazier"},
        {"candle", "light.torch.tallow-candle"},
        {"tallow-candle", "light.torch.tallow-candle"},
    };
    const auto found = aliases.find(value);
    return found == aliases.end() ? value : found->second;
}

bool materialMatches(const std::string& id, SurfaceKind surface) {
    const auto* material = findMaterial(id);
    return material != nullptr && material->surface == surface;
}

void enclose(LevelDefinition& level) {
    for (int y = 0; y < level.height; ++y) {
        level.map[static_cast<std::size_t>(y)][0] = '#';
        level.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(level.width - 1)] = '#';
    }
    std::fill(level.map.front().begin(), level.map.front().end(), '#');
    std::fill(level.map.back().begin(), level.map.back().end(), '#');
}

bool inBounds(const LevelDefinition& level, int x, int y) {
    return x >= 0 && y >= 0 && x < level.width && y < level.height;
}

void clearExistingExit(LevelDefinition& level) {
    for (auto& row : level.map) std::replace(row.begin(), row.end(), 'E', '.');
}

bool hasSurfaceOverride(const LevelDefinition& level, int x, int y, SurfaceKind surface) {
    for (const auto& surfaceOverride : level.surfaceOverrides) {
        if (surfaceOverride.x == x && surfaceOverride.y == y && surfaceOverride.surface == surface) return true;
    }
    return false;
}

std::string lineError(int lineNumber, const std::string& message) {
    return "Blueprint line " + std::to_string(lineNumber) + ": " + message;
}

} // namespace

bool LevelBlueprint::parse(const std::string& text, LevelDefinition& level, std::string& error) {
    std::istringstream lines(text);
    std::string line;
    bool sawSignature = false;
    bool sawExit = false;
    LevelDefinition draft;
    draft.id = "blueprint.imported";
    draft.name = "IMPORTED BLUEPRINT";
    draft.width = 16;
    draft.height = 16;
    draft.spawnX = 1;
    draft.spawnY = 1;
    draft.spawnDirection = 1;
    draft.map.assign(static_cast<std::size_t>(draft.height), std::string(static_cast<std::size_t>(draft.width), '#'));
    enclose(draft);

    int lineNumber = 0;
    while (std::getline(lines, line)) {
        ++lineNumber;
        const auto tokens = tokenize(line);
        if (tokens.empty()) continue;

        const std::string command = upper(tokens[0]);
        if (command == "STONEVEIL_BLUEPRINT") {
            if (tokens.size() < 2 || tokens[1] != "1") {
                error = lineError(lineNumber, "expected STONEVEIL_BLUEPRINT 1.");
                return false;
            }
            sawSignature = true;
            continue;
        }
        if (!sawSignature) {
            error = lineError(lineNumber, "first command must be STONEVEIL_BLUEPRINT 1.");
            return false;
        }
        if (command == "END") break;

        if (command == "NAME") {
            if (tokens.size() < 2) {
                error = lineError(lineNumber, "NAME needs text.");
                return false;
            }
            draft.name = tokens[1];
            draft.id = "blueprint." + draft.name;
            std::replace(draft.id.begin(), draft.id.end(), ' ', '-');
        } else if (command == "SIZE") {
            int width{};
            int height{};
            if (tokens.size() < 3 || !parseInt(tokens[1], width) || !parseInt(tokens[2], height) ||
                width < LevelDefinition::MinimumDimension || height < LevelDefinition::MinimumDimension ||
                width > LevelDefinition::MaximumDimension || height > LevelDefinition::MaximumDimension) {
                error = lineError(lineNumber, "SIZE needs width and height between 4 and 64.");
                return false;
            }
            draft.width = width;
            draft.height = height;
            draft.map.assign(static_cast<std::size_t>(draft.height),
                             std::string(static_cast<std::size_t>(draft.width), '#'));
            draft.spawnX = std::clamp(draft.spawnX, 1, draft.width - 2);
            draft.spawnY = std::clamp(draft.spawnY, 1, draft.height - 2);
            draft.pickups.clear();
            draft.enemies.clear();
            draft.lights.clear();
            draft.surfaceOverrides.clear();
            sawExit = false;
            enclose(draft);
        } else if (command == "DEFAULTS") {
            for (std::size_t index = 1; index + 1 < tokens.size(); index += 2) {
                const std::string key = upper(tokens[index]);
                if (key == "WALL") {
                    const std::string id = materialAlias(tokens[index + 1]);
                    if (!materialMatches(id, SurfaceKind::Wall)) {
                        error = lineError(lineNumber, "unknown wall material.");
                        return false;
                    }
                    draft.surfaces.wallMaterial = id;
                } else if (key == "FLOOR") {
                    const std::string id = materialAlias(tokens[index + 1]);
                    if (!materialMatches(id, SurfaceKind::Floor)) {
                        error = lineError(lineNumber, "unknown floor material.");
                        return false;
                    }
                    draft.surfaces.floorMaterial = id;
                } else if (key == "CEILING") {
                    if (upper(tokens[index + 1]) == "SKY") {
                        draft.surfaces.ceilingMode = CeilingMode::Sky;
                    } else {
                        const std::string id = materialAlias(tokens[index + 1]);
                        if (!materialMatches(id, SurfaceKind::Ceiling)) {
                            error = lineError(lineNumber, "unknown ceiling material.");
                            return false;
                        }
                        draft.surfaces.ceilingMode = CeilingMode::Material;
                        draft.surfaces.ceilingMaterial = id;
                    }
                }
            }
        } else if (command == "FILL") {
            char marker{};
            if (tokens.size() < 2 || !parseBrush(tokens[1], marker)) {
                error = lineError(lineNumber, "FILL needs wall or floor.");
                return false;
            }
            draft.map.assign(static_cast<std::size_t>(draft.height),
                             std::string(static_cast<std::size_t>(draft.width), marker));
            enclose(draft);
        } else if (command == "ROOM") {
            int x{};
            int y{};
            int width{};
            int height{};
            char marker = '.';
            if (tokens.size() < 5 || !parseInt(tokens[1], x) || !parseInt(tokens[2], y) ||
                !parseInt(tokens[3], width) || !parseInt(tokens[4], height)) {
                error = lineError(lineNumber, "ROOM needs x y width height.");
                return false;
            }
            if (tokens.size() >= 6 && !parseBrush(tokens[5], marker)) {
                error = lineError(lineNumber, "ROOM brush must be floor, wall, door, secret, or exit.");
                return false;
            }
            for (int yy = y; yy < y + height; ++yy) {
                for (int xx = x; xx < x + width; ++xx) {
                    if (inBounds(draft, xx, yy)) draft.map[static_cast<std::size_t>(yy)][static_cast<std::size_t>(xx)] = marker;
                }
            }
            enclose(draft);
        } else if (command == "DOOR" || command == "SECRET") {
            int x{};
            int y{};
            if (tokens.size() < 3 || !parseInt(tokens[1], x) || !parseInt(tokens[2], y) || !inBounds(draft, x, y)) {
                error = lineError(lineNumber, "door needs an in-bounds x y.");
                return false;
            }
            draft.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = command == "SECRET" ? 'S' : 'D';
        } else if (command == "SPAWN") {
            int x{};
            int y{};
            int direction{};
            if (tokens.size() < 4 || !parseInt(tokens[1], x) || !parseInt(tokens[2], y) ||
                !parseDirection(tokens[3], direction) || !inBounds(draft, x, y)) {
                error = lineError(lineNumber, "SPAWN needs x y direction.");
                return false;
            }
            draft.spawnX = x;
            draft.spawnY = y;
            draft.spawnDirection = direction;
            if (draft.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '#') {
                draft.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = '.';
            }
        } else if (command == "EXIT") {
            int x{};
            int y{};
            if (tokens.size() < 3 || !parseInt(tokens[1], x) || !parseInt(tokens[2], y) || !inBounds(draft, x, y)) {
                error = lineError(lineNumber, "EXIT needs an in-bounds x y.");
                return false;
            }
            clearExistingExit(draft);
            draft.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = 'E';
            sawExit = true;
        } else if (command == "LIGHT") {
            int x{};
            int y{};
            if (tokens.size() < 4 || !parseInt(tokens[2], x) || !parseInt(tokens[3], y) || !inBounds(draft, x, y)) {
                error = lineError(lineNumber, "LIGHT needs id x y.");
                return false;
            }
            const std::string id = lightAlias(tokens[1]);
            if (findLight(id) == nullptr) {
                error = lineError(lineNumber, "unknown light id.");
                return false;
            }
            draft.lights.push_back({x, y, id});
        } else if (command == "PICKUP") {
            int x{};
            int y{};
            if (tokens.size() < 4 || !parseInt(tokens[2], x) || !parseInt(tokens[3], y) || !inBounds(draft, x, y)) {
                error = lineError(lineNumber, "PICKUP needs key|potion x y.");
                return false;
            }
            Pickup pickup;
            pickup.x = x;
            pickup.y = y;
            const std::string type = upper(tokens[1]);
            if (type == "KEY") pickup.type = Pickup::Type::Key;
            else if (type == "POTION") pickup.type = Pickup::Type::Potion;
            else {
                error = lineError(lineNumber, "pickup type must be key or potion.");
                return false;
            }
            draft.pickups.push_back(pickup);
        } else if (command == "ENEMY") {
            int x{};
            int y{};
            int hp{18};
            if (tokens.size() < 3 || !parseInt(tokens[1], x) || !parseInt(tokens[2], y) || !inBounds(draft, x, y) ||
                (tokens.size() >= 4 && !parseInt(tokens[3], hp))) {
                error = lineError(lineNumber, "ENEMY needs x y optional hp.");
                return false;
            }
            draft.enemies.push_back({x, y, hp});
        } else if (command == "SURFACE") {
            SurfaceKind surface{};
            int x{};
            int y{};
            int width{};
            int height{};
            if (tokens.size() < 7 || !parseSurfaceKind(tokens[1], surface) || !parseInt(tokens[2], x) ||
                !parseInt(tokens[3], y) || !parseInt(tokens[4], width) || !parseInt(tokens[5], height)) {
                error = lineError(lineNumber, "SURFACE needs kind x y width height material.");
                return false;
            }
            const bool sky = surface == SurfaceKind::Ceiling && upper(tokens[6]) == "SKY";
            const std::string id = sky ? std::string{} : materialAlias(tokens[6]);
            if (!sky && !materialMatches(id, surface)) {
                error = lineError(lineNumber, "surface material does not match surface kind.");
                return false;
            }
            for (int yy = y; yy < y + height; ++yy) {
                for (int xx = x; xx < x + width; ++xx) {
                    if (!inBounds(draft, xx, yy)) continue;
                    const bool solid = draft.map[static_cast<std::size_t>(yy)][static_cast<std::size_t>(xx)] == '#';
                    if ((surface == SurfaceKind::Wall && !solid) || (surface != SurfaceKind::Wall && solid)) continue;
                    draft.surfaceOverrides.push_back({xx, yy, id, surface, sky ? CeilingMode::Sky : CeilingMode::Material});
                }
            }
        } else if (command == "WATER") {
            int x{};
            int y{};
            int width{};
            int height{};
            int depth{};
            int volumeId{0};
            WaterFlow flow{WaterFlow::Still};
            if (tokens.size() < 6 || !parseInt(tokens[1], x) || !parseInt(tokens[2], y) ||
                !parseInt(tokens[3], width) || !parseInt(tokens[4], height) || !parseInt(tokens[5], depth) ||
                depth <= 0 || depth > 3) {
                error = lineError(lineNumber, "WATER needs x y width height depth(1-3) optional flow optional volume.");
                return false;
            }
            if (tokens.size() >= 7 && !parseWaterFlow(tokens[6], flow)) {
                error = lineError(lineNumber, "water flow must be still, north, east, south, or west.");
                return false;
            }
            if (tokens.size() >= 8 && (!parseInt(tokens[7], volumeId) || volumeId < 0)) {
                error = lineError(lineNumber, "water volume id must be zero or greater.");
                return false;
            }
            for (int yy = y; yy < y + height; ++yy) {
                for (int xx = x; xx < x + width; ++xx) {
                    if (!inBounds(draft, xx, yy)) continue;
                    if (draft.map[static_cast<std::size_t>(yy)][static_cast<std::size_t>(xx)] == '#') continue;
                    draft.water.push_back({xx, yy, depth, flow, volumeId});
                    if (!hasSurfaceOverride(draft, xx, yy, SurfaceKind::Floor) &&
                        draft.surfaces.floorMaterial != "material.floor.cave.shallow-water") {
                        draft.surfaceOverrides.push_back({xx, yy, "material.floor.cave.shallow-water",
                                                          SurfaceKind::Floor, CeilingMode::Material});
                    }
                }
            }
        } else {
            error = lineError(lineNumber, "unknown command '" + tokens[0] + "'.");
            return false;
        }
    }

    if (!sawSignature) {
        error = "Blueprint is missing STONEVEIL_BLUEPRINT 1.";
        return false;
    }
    if (!sawExit) {
        error = "Blueprint needs exactly one EXIT command.";
        return false;
    }

    for (std::size_t i = 0; i < draft.pickups.size(); ++i) {
        draft.pickups[i].id = "pickup.blueprint." + std::to_string(i + 1);
    }
    for (std::size_t i = 0; i < draft.enemies.size(); ++i) {
        draft.enemies[i].id = "enemy.blueprint." + std::to_string(i + 1);
    }
    int doorNumber = 1;
    for (int y = 0; y < draft.height; ++y) {
        for (int x = 0; x < draft.width; ++x) {
            if (draft.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 'D') {
                draft.doors.push_back({"door.blueprint." + std::to_string(doorNumber++), x, y,
                                       DoorKind::Door, true});
            }
        }
    }

    const auto validationErrors = LevelIO::validate(draft);
    if (!validationErrors.empty()) {
        error = "Blueprint produced an invalid level: " + validationErrors.front();
        return false;
    }
    level = std::move(draft);
    error.clear();
    return true;
}

} // namespace sv
