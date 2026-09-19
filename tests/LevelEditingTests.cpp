#include "LevelEditing.hpp"
#include "LevelDocument.hpp"
#include "LevelIO.hpp"
#include <cstdlib>
#include <iostream>

#define CHECK(e) do { if (!(e)) { std::cerr << "Failed line " << __LINE__ << ": " << #e << '\n'; std::exit(1); } } while(false)
int main() {
    using namespace sv;
    auto level = LevelDocument::newLevel();
    level.objects.push_back({"object.note", WorldObjectKind::Note, 4, 4, "Note", "Remember", false});
    level.pickups.push_back({Pickup::Type::Key, 5, 4, false, "item.key"});
    Enemy enemy; enemy.x = 6; enemy.y = 4; enemy.id = "enemy.guard";
    level.enemies.push_back(enemy);
    level.map[4][7] = 'D';
    level.doors.push_back({"door.hall", 7, 4, DoorKind::Gate, true});
    level.lights.push_back({4, 4, defaultLightId()});
    level.triggers.push_back({"event.note", TriggerEvent::InteractObject, 4, 4, "object.note", true, "Read"});
    level.triggers.push_back({"event.old", TriggerEvent::InteractObject, 4, 4, "", true, "Old"});
    level.triggers.push_back({"event.cell", TriggerEvent::EnterCell, 4, 4, "", true, "Here"});
    CHECK(LevelIO::validate(level).empty());
    auto selections = LevelEditing::at(level, 4, 4);
    CHECK(selections.size() == 2 && selections[0].kind == SelectionKind::Object && selections[1].kind == SelectionKind::Light);
    auto note = selections[0];
    auto light = selections[1];
    std::string error;
    CHECK(!LevelEditing::move(level, note, 5, 4, error));
    CHECK(!LevelEditing::move(level, note, 0, 0, error));
    CHECK(level.objects[0].x == 4 && level.triggers[0].x == 4);
    CHECK(!LevelEditing::erase(level, note, error));
    CHECK(LevelEditing::move(level, note, 4, 6, error));
    CHECK(level.objects[0].id == "object.note" && level.objects[0].text == "Remember");
    CHECK(level.triggers[0].y == 6 && level.triggers[1].y == 6);
    CHECK(level.triggers[2].y == 4); // Location story stays at its authored cell.
    CHECK(level.lights[0].y == 4);
    CHECK(LevelEditing::move(level, light, 0, 0, error)); // Wall-mounted lights supported.
    CHECK(LevelEditing::locate(level, light, light.x, light.y));
    CHECK(level.lights[0].x == 0 && level.lights[0].y == 0);
    level.lights.push_back({1, 0, defaultLightId()});
    CHECK(!LevelEditing::move(level, light, 1, 0, error));
    auto door = LevelEditing::at(level, 7, 4).front();
    level.triggers.push_back({"event.door", TriggerEvent::OpenDoor, 7, 4, "door.hall", true, "Opened"});
    CHECK(!LevelEditing::move(level, door, 13, 13, error)); // Never overwrite the exit.
    CHECK(LevelEditing::move(level, door, 8, 4, error));
    CHECK(level.map[4][7] == '.' && level.map[4][8] == 'D');
    CHECK(level.doors[0].id == "door.hall" && level.doors[0].locked);
    CHECK(level.triggers.back().x == 8);
    CHECK(!LevelEditing::erase(level, door, error));
    level.triggers.pop_back();
    CHECK(LevelEditing::erase(level, door, error));
    CHECK(level.doors.empty() && level.map[4][8] == '.');
    LevelSelection spawn;
    CHECK(!LevelEditing::erase(level, spawn, error));
    CHECK(LevelEditing::move(level, spawn, 3, 3, error));
    CHECK(level.spawnX == 3 && level.spawnY == 3 && level.spawnDirection == 1);
    auto key = LevelEditing::at(level, 5, 4).front();
    CHECK(LevelEditing::erase(level, key, error));
    CHECK(!LevelEditing::erase(level, key, error));
    CHECK(!LevelEditing::move(level, key, 3, 5, error));
    CHECK(LevelIO::validate(level).empty());
    // Document undo restores the entity and attached-event coordinates together.
    LevelDocument document;
    document.replaceUntitled(level);
    auto edited = document.draft();
    CHECK(LevelEditing::move(edited, note, 8, 8, error));
    document.beginEdit(); document.draft() = edited;
    CHECK(document.undo());
    CHECK(document.draft().objects[0].y == 6 && document.draft().triggers[0].y == 6);
    CHECK(document.redo());
    CHECK(document.draft().objects[0].y == 8 && document.draft().triggers[0].y == 8);
    auto guard = LevelEditing::at(level, 6, 4).front();
    level.triggers.push_back({"event.enemy", TriggerEvent::KillEnemy, 6, 4, "enemy.guard", true, "Defeated"});
    CHECK(LevelEditing::move(level, guard, 7, 7, error));
    CHECK(level.enemies[0].id == "enemy.guard" && level.triggers.back().x == 7);
    CHECK(!LevelEditing::erase(level, guard, error));
    CHECK(LevelIO::validate(level).empty());
    return 0;
}
