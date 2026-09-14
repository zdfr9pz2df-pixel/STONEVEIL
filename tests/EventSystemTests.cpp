#include "Authoring.hpp"
#include "EventSystem.hpp"

#include <cassert>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using namespace sv;

struct Harness {
    std::unordered_map<std::string, EventValue> facts;
    std::vector<EventAction> actions;

    EventServices services() {
        EventServices result;
        result.readFact = [this](const std::string& name) -> EventValue {
            const auto it = facts.find(name);
            return it == facts.end() ? EventValue{} : it->second;
        };
        result.executeAction = [this](const EventAction& action, const EventContext&) {
            actions.push_back(action);
        };
        return result;
    }
};

void storyTileRunsOnce() {
    StoryTileBehavior tile;
    tile.id = "story.quiet_corridor";
    tile.name = "Quiet Corridor";
    tile.x = 6;
    tile.y = 4;
    tile.once = true;
    tile.speech.push_back({"Vanguard", "It's quiet... too quiet.", {}});

    EventRuntime runtime;
    runtime.addEvent(compileStoryTile(tile));

    Harness harness;
    const EventContext entered{EventTriggerType::PlayerEnterTile, 6, 4, {}, "player"};

    const auto first = runtime.fire(entered, harness.services());
    assert(first.eventsRun == 1);
    assert(harness.actions.size() == 1);
    assert(harness.actions[0].type == EventActionType::Speak);
    assert(harness.actions[0].targetId == "Vanguard");

    harness.actions.clear();
    const auto second = runtime.fire(entered, harness.services());
    assert(second.eventsRun == 0);
    assert(harness.actions.empty());
}

void lockedDoorChoosesReadableFailure() {
    DoorBehavior door;
    door.id = "door:7,4";
    door.name = "Iron Door";
    door.x = 7;
    door.y = 4;
    door.locked = true;
    door.requiredItemId = "iron_key";
    door.requiredItemCountFact = "inventory.iron_key.count";
    door.lockedSpeech = {"Vanguard", "It's locked.", {}};

    EventRuntime runtime;
    for (auto event : compileDoorBehavior(door)) runtime.addEvent(std::move(event));

    Harness harness;
    harness.facts["inventory.iron_key.count"] = 0;
    const EventContext interaction{EventTriggerType::InteractObject, 7, 4, "door:7,4", "player"};

    const auto result = runtime.fire(interaction, harness.services());
    assert(result.eventsRun == 1);
    assert(result.consumed);
    assert(harness.actions.size() == 1);
    assert(harness.actions[0].type == EventActionType::Speak);
}

void doorWithKeyRunsSuccessPath() {
    DoorBehavior door;
    door.id = "door:7,4";
    door.name = "Iron Door";
    door.x = 7;
    door.y = 4;
    door.locked = true;
    door.requiredItemId = "iron_key";
    door.requiredItemCountFact = "inventory.iron_key.count";
    door.requiredItemCount = 1;
    door.openMessage = "The lock gives. The door opens.";

    EventRuntime runtime;
    for (auto event : compileDoorBehavior(door)) runtime.addEvent(std::move(event));

    Harness harness;
    harness.facts["inventory.iron_key.count"] = 1;
    const EventContext interaction{EventTriggerType::InteractObject, 7, 4, "door:7,4", "player"};

    const auto result = runtime.fire(interaction, harness.services());
    assert(result.eventsRun == 1);
    assert(result.consumed);
    assert(harness.actions.size() == 3);
    assert(harness.actions[0].type == EventActionType::OpenDoor);
    assert(harness.actions[1].type == EventActionType::ConsumeItem);
    assert(harness.actions[2].type == EventActionType::ShowMessage);
}

void permanentlyLockedDoorHasNoSuccessEvent() {
    DoorBehavior door;
    door.id = "door:1,1";
    door.x = 1;
    door.y = 1;
    door.locked = true;
    door.lockedMessage = "It will not move.";

    const auto events = compileDoorBehavior(door);
    assert(events.size() == 1);
    assert(events[0].id == "door:1,1.interact.locked");
}

} // namespace

int main() {
    storyTileRunsOnce();
    lockedDoorChoosesReadableFailure();
    doorWithKeyRunsSuccessPath();
    permanentlyLockedDoorHasNoSuccessEvent();
    return 0;
}
