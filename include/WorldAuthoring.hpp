#pragma once

#include "EventSystem.hpp"
#include "Character.hpp"
#include "StoryState.hpp"
#include <string>
#include <vector>

namespace sv {

enum class DoorKind { Door, Gate };

struct DoorPlacement {
    std::string id;
    int x{};
    int y{};
    DoorKind kind{DoorKind::Door};
    bool locked{true};
    // A locked door can also be opened by story progression without
    // consuming a key. Blank keeps the original key-only behavior.
    std::string unlockFlag;
};

enum class WorldObjectKind {
    Prop,
    Shrine,
    Note,
    Corpse,
    Npc,
    Recruit,
    PartyManagement,
    ArrivalPoint,
    LevelTransition,
};

struct WorldObject {
    std::string id;
    WorldObjectKind kind{WorldObjectKind::Prop};
    int x{};
    int y{};
    std::string name;
    std::string text;
    bool blocksMovement{false};
    CharacterId characterId{InvalidCharacterId};
    int facing{0};
    std::string destinationLevelId;
    std::string destinationArrivalId;
    // Optional campaign-state visibility gates. A required flag must be true;
    // a hidden flag removes the object once it becomes true.
    std::string requiredFlag;
    std::string hiddenWhenFlag;
};

struct StoryRoom {
    std::string id;
    int x{};
    int y{};
    int width{1};
    int height{1};
    std::string name;
    std::string purpose;
    std::string mood;
    std::string lore;
    std::string intendedFeeling;

    bool contains(int cellX, int cellY) const;
};

enum class TriggerEvent {
    EnterCell,
    EnterRoom,
    OpenDoor,
    KillEnemy,
    PickupItem,
    InteractObject,
};

struct StoryTrigger {
    std::string id;
    TriggerEvent event{TriggerEvent::EnterCell};
    int x{};
    int y{};
    std::string subjectId;
    bool once{true};
    std::string message;
    std::string requiredFlag;
    std::string setFlag;
    bool setFlagValue{true};
};

const char* doorKindName(DoorKind kind);
bool parseDoorKind(const std::string& value, DoorKind& kind);
const char* worldObjectKindName(WorldObjectKind kind);
bool parseWorldObjectKind(const std::string& value, WorldObjectKind& kind);
const char* triggerEventName(TriggerEvent event);
bool parseTriggerEvent(const std::string& value, TriggerEvent& event);
EventTriggerType eventTriggerType(TriggerEvent event);
EventDefinition compileStoryTrigger(const StoryTrigger& trigger);
bool worldObjectActive(const WorldObject& object, const StoryState* storyState);

// Runtime state for authored narrative triggers. It deliberately owns only
// fired-once IDs; the authored trigger data remains on Dungeon/LevelDefinition.
class TriggerSystem {
public:
    void reset();
    std::vector<std::string> fire(const std::vector<StoryTrigger>& triggers,
                                  TriggerEvent event,
                                  int x,
                                  int y,
                                  const std::string& subjectId = {});
    bool hasFired(const std::string& triggerId) const;

private:
    EventRuntime runtime_;
};

} // namespace sv
