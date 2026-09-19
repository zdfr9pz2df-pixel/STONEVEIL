#include "LevelEditing.hpp"
#include <algorithm>

namespace sv {
namespace {
template<class T> auto find(T& values, const LevelSelection& s) {
    return std::find_if(values.begin(), values.end(), [&](const auto& value) { return value.id == s.id; });
}
bool cell(const LevelDefinition& level, int x, int y) {
    return x >= 0 && y >= 0 && x < level.width && y < level.height &&
        static_cast<std::size_t>(y) < level.map.size() &&
        static_cast<std::size_t>(x) < level.map[static_cast<std::size_t>(y)].size();
}
bool follows(const StoryTrigger& trigger, const LevelSelection& s, int x, int y) {
    const auto event = s.kind == SelectionKind::Object ? TriggerEvent::InteractObject :
        s.kind == SelectionKind::Enemy ? TriggerEvent::KillEnemy :
        s.kind == SelectionKind::Pickup ? TriggerEvent::PickupItem : TriggerEvent::OpenDoor;
    if (s.kind == SelectionKind::Light || s.kind == SelectionKind::Spawn) return false;
    return trigger.event == event && (trigger.subjectId == s.id ||
        (trigger.subjectId.empty() && trigger.x == x && trigger.y == y));
}
}
std::vector<LevelSelection> LevelEditing::at(const LevelDefinition& level, int x, int y) {
    std::vector<LevelSelection> result;
    if (level.spawnX == x && level.spawnY == y) result.push_back({SelectionKind::Spawn, {}, x, y});
    const auto append = [&](const auto& values, SelectionKind kind) {
        for (const auto& value : values) if (value.x == x && value.y == y) result.push_back({kind, value.id, x, y});
    };
    append(level.objects, SelectionKind::Object);
    append(level.enemies, SelectionKind::Enemy);
    append(level.pickups, SelectionKind::Pickup);
    append(level.doors, SelectionKind::Door);
    for (const auto& light : level.lights) if (light.x == x && light.y == y)
        result.push_back({SelectionKind::Light, light.lightId, x, y});
    return result;
}
bool LevelEditing::locate(const LevelDefinition& level, const LevelSelection& s, int& x, int& y) {
    if (s.kind == SelectionKind::Spawn) { x = level.spawnX; y = level.spawnY; return true; }
    const auto locateIn = [&](const auto& values) {
        const auto it = find(values, s);
        if (it == values.end()) return false;
        x = it->x; y = it->y; return true;
    };
    switch (s.kind) {
    case SelectionKind::Object: return locateIn(level.objects);
    case SelectionKind::Enemy: return locateIn(level.enemies);
    case SelectionKind::Pickup: return locateIn(level.pickups);
    case SelectionKind::Door: return locateIn(level.doors);
    case SelectionKind::Light:
        for (const auto& light : level.lights) if (light.x == s.x && light.y == s.y && light.lightId == s.id) {
            x = light.x; y = light.y; return true;
        }
        return false;
    default: return false;
    }
}
bool LevelEditing::move(LevelDefinition& level, LevelSelection& s, int x, int y, std::string& error) {
    int oldX{}, oldY{};
    if (!locate(level, s, oldX, oldY) || !cell(level, oldX, oldY)) { error = "Selection no longer exists."; return false; }
    if (!cell(level, x, y)) { error = "Choose a cell inside the map."; return false; }
    if (x == oldX && y == oldY) { error = "Already in that cell."; return false; }
    if (s.kind != SelectionKind::Light && (x == 0 || y == 0 || x == level.width-1 || y == level.height-1 ||
        level.map[y][x] == '#' || level.map[y][x] == 'S')) {
        error = "Choose a walkable interior cell."; return false;
    }
    for (const auto& other : at(level, x, y)) {
        if ((s.kind == SelectionKind::Light && other.kind == SelectionKind::Light) ||
            (s.kind != SelectionKind::Light && other.kind != SelectionKind::Light)) {
            error = "That cell already contains an object."; return false;
        }
    }
    if (s.kind == SelectionKind::Door && level.map[y][x] != '.') {
        error = "Move doors onto empty floor; exits and geometry are preserved."; return false;
    }
    auto copy = level;
    const auto moveIn = [&](auto& values) { auto it = find(values, s); it->x = x; it->y = y; };
    switch (s.kind) {
    case SelectionKind::Spawn: copy.spawnX = x; copy.spawnY = y; break;
    case SelectionKind::Object: moveIn(copy.objects); break;
    case SelectionKind::Enemy: moveIn(copy.enemies); break;
    case SelectionKind::Pickup: moveIn(copy.pickups); break;
    case SelectionKind::Door:
        moveIn(copy.doors); copy.map[oldY][oldX] = '.'; copy.map[y][x] = 'D'; break;
    case SelectionKind::Light:
        for (auto& light : copy.lights) if (light.x == oldX && light.y == oldY) { light.x = x; light.y = y; break; }
        break;
    }
    for (auto& trigger : copy.triggers) if (follows(trigger, s, oldX, oldY)) { trigger.x = x; trigger.y = y; }
    level = std::move(copy); s.x = x; s.y = y; error.clear(); return true;
}
bool LevelEditing::erase(LevelDefinition& level, const LevelSelection& s, std::string& error) {
    int x{}, y{};
    if (!locate(level, s, x, y) || !cell(level, x, y)) { error = "Selection no longer exists."; return false; }
    if (s.kind == SelectionKind::Spawn) { error = "The player spawn cannot be deleted; move it instead."; return false; }
    for (const auto& trigger : level.triggers)
        if ((!s.id.empty() && s.kind != SelectionKind::Light && trigger.subjectId == s.id) || follows(trigger, s, x, y)) {
            error = "This object has attached events. Remove or retarget them before deleting."; return false;
        }
    const auto eraseIn = [&](auto& values) { values.erase(find(values, s)); };
    switch (s.kind) {
    case SelectionKind::Object: eraseIn(level.objects); break;
    case SelectionKind::Enemy: eraseIn(level.enemies); break;
    case SelectionKind::Pickup: eraseIn(level.pickups); break;
    case SelectionKind::Door: eraseIn(level.doors); level.map[y][x] = '.'; break;
    case SelectionKind::Light:
        level.lights.erase(std::remove_if(level.lights.begin(), level.lights.end(), [&](const auto& light) {
            return light.x == x && light.y == y;
        }), level.lights.end()); break;
    default: return false;
    }
    error.clear(); return true;
}
}
