#include "Dungeon.hpp"

namespace sv {

const LevelDefinition& levelOneDefinition() {
    static const LevelDefinition definition = [] {
        LevelDefinition level = {
        "gatehouse.level-1",
        "LEVEL 1  THE GATEHOUSE",
        "content/audio/music/gatehouse-drone.wav",
        16,
        16,
        {
            "################",
            "#......#.......#",
            "#......#.......#",
            "#..##..#..###..#",
            "#..##..D..#....#",
            "#......#..#....#",
            "###.####..#....#",
            "#............#.#",
            "#.....#......#.#",
            "#.####.S.#####.#",
            "#.#...##.......#",
            "#.#....#..###..#",
            "#...##....#....#",
            "#...##....#..E.#",
            "#..............#",
            "################",
        },
        2,
        2,
        1,
        {
            {Pickup::Type::Key, 5, 2, false},
            {Pickup::Type::Potion, 10, 8, false},
            {Pickup::Type::Potion, 6, 9, false},
        },
        {
            {10, 2, 18, true, 0.0f},
            {11, 8, 24, true, 0.0f},
            {6, 13, 30, true, 0.0f},
        },
        {
            {3, 3, "light.torch.iron-sconce"},
            {7, 6, "light.torch.iron-sconce"},
            {2, 10, "light.torch.tallow-candle"},
            {10, 13, "light.torch.pitch-brazier"},
        },
        };
        level.pickups[0].id = "pickup.gatehouse.key-1";
        level.pickups[1].id = "pickup.gatehouse.potion-1";
        level.pickups[2].id = "pickup.gatehouse.potion-2";
        level.enemies[0].id = "enemy.gatehouse.prowler-1";
        level.enemies[1].id = "enemy.gatehouse.prowler-2";
        level.enemies[2].id = "enemy.gatehouse.brute-1";
        level.doors.push_back({"door.gatehouse.main-lock", 7, 4, DoorKind::Door, true});
        level.objects = {
            {"note.gatehouse.watch-order", WorldObjectKind::Note, 3, 2, "Watch Order",
             "Third bell: keep the lower gate barred. Something below has learned our knocking code.", false},
            {"shrine.gatehouse.veil", WorldObjectKind::Shrine, 3, 10, "Veil Shrine",
             "The worn stone is warm beneath your hand, though no flame burns here.", false},
            {"corpse.gatehouse.jailer", WorldObjectKind::Corpse, 12, 7, "The Jailer",
             "The corpse still grips an empty key ring. The final key was taken deeper in.", false},
            {"npc.gatehouse.wounded-scout", WorldObjectKind::Npc, 12, 12, "Wounded Scout",
             "Do not trust the quiet beyond the iron gate. The quiet is how it hunts.", false},
        };
        level.rooms = {
            {"room.gatehouse.entry", 1, 1, 6, 5, "The Abandoned Watch",
             "Teach observation, keys, and the first choice of route.", "Stale vigilance",
             "The garrison expected an attack from outside. The danger came from below.", "Unease and curiosity"},
            {"room.gatehouse.lock", 8, 1, 7, 6, "The Sealed Watch Hall",
             "Make the first locked threshold feel consequential.", "Held breath",
             "The main lock was turned after the last watch failed to answer.", "Commitment"},
            {"room.gatehouse.crossing", 1, 7, 13, 3, "The Broken Crossing",
             "Reveal the hidden route and the cost of haste.", "Exposed",
             "Boot marks split here: soldiers to the gate, one barefoot trail into stone.", "Suspicion"},
            {"room.gatehouse.lower", 1, 10, 14, 5, "The Lower Gate",
             "Deliver the level's warning and point toward the descent.", "Cold anticipation",
             "Those who survived the first breach refused to speak its name.", "Dread with forward pull"},
        };
        level.triggers = {
            {"trigger.gatehouse.arrival", TriggerEvent::EnterRoom, 1, 1, "room.gatehouse.entry", true,
             "The watch hall smells of wet ash. Someone abandoned it in the middle of a shift."},
            {"trigger.gatehouse.key", TriggerEvent::PickupItem, 5, 2, "pickup.gatehouse.key-1", true,
             "An iron key bears fresh scratches around its teeth. It was used in panic."},
            {"trigger.gatehouse.door", TriggerEvent::OpenDoor, 7, 4, "door.gatehouse.main-lock", true,
             "The lock opens on a breath of colder air from the sealed hall."},
            {"trigger.gatehouse.first-kill", TriggerEvent::KillEnemy, 10, 2, "enemy.gatehouse.prowler-1", true,
             "The prowler falls wearing a strip of Gatehouse uniform around one wrist."},
            {"trigger.gatehouse.watch-order", TriggerEvent::InteractObject, 3, 2, "note.gatehouse.watch-order", true,
             "Third bell: keep the lower gate barred. Something below has learned our knocking code."},
            {"trigger.gatehouse.lower-warning", TriggerEvent::EnterRoom, 1, 10, "room.gatehouse.lower", true,
             "The lower stones hum faintly. The dungeon is no longer merely abandoned; it is listening."},
        };
        return level;
    }();
    return definition;
}

} // namespace sv
