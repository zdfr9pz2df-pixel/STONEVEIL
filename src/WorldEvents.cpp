#include "WorldEvents.hpp"

#include "Authoring.hpp"

namespace sv {

bool configureWorldEvents(const Dungeon& dungeon, EventRuntime& runtime) {
    EventRuntime configured;
    for (const auto& trigger : dungeon.triggers()) {
        if (!configured.addEvent(compileStoryTrigger(trigger))) return false;
    }
    for (const auto& placement : dungeon.doors()) {
        DoorBehavior door;
        door.id = placement.id;
        door.x = placement.x;
        door.y = placement.y;
        door.locked = placement.locked;
        door.requiredItemId = "item.iron-key";
        door.requiredItemCountFact = "inventory.iron-key.count";
        door.lockedMessage = "The iron lock needs a key.";
        door.openMessage = placement.locked ? "The lock gives. The door opens." : "The door opens.";
        for (auto event : compileDoorBehavior(door)) {
            // Authored interaction reactions run before this consuming branch.
            // Successful opens cannot consume a second key through recursion.
            event.conditions.push_back({"door." + door.id + ".closed", CompareOp::Equal, true});
            event.occurrence = event.id == door.id + ".interact.success"
                ? EventOccurrence::Once : EventOccurrence::EveryTime;
            event.priority -= 200;
            if (!configured.addEvent(std::move(event))) return false;
        }
    }
    runtime = std::move(configured);
    return true;
}

EventFireResult dispatchWorldEvent(EventRuntime& runtime, const EventContext& context,
                                   Dungeon& dungeon, int& campaignKeys,
                                   const WorldEventPresentation& presentation) {
    EventServices services;
    services.readFact = [&](const std::string& fact) -> EventValue {
        if (fact == "inventory.iron-key.count") return campaignKeys;
        for (const auto& door : dungeon.doors()) {
            if (fact == "door." + door.id + ".closed")
                return dungeon.tile(door.x, door.y) == Tile::DoorClosed;
        }
        return {};
    };
    services.executeAction = [&](const EventAction& action, const EventContext& current) {
        switch (action.type) {
            case EventActionType::ShowMessage:
            case EventActionType::Speak:
                if (presentation.message && !action.text.empty()) presentation.message(action.text);
                break;
            case EventActionType::ConsumeItem:
                if (action.targetId == "item.iron-key" && action.intValue > 0 && campaignKeys >= action.intValue)
                    campaignKeys -= action.intValue;
                break;
            case EventActionType::OpenDoor:
                for (const auto& door : dungeon.doors()) {
                    if (door.id != action.targetId) continue;
                    if (dungeon.openDoor(door.x, door.y, !door.locked || campaignKeys > 0)) {
                        if (presentation.cue) presentation.cue(WorldEventCue::DoorOpened);
                        runtime.fire({EventTriggerType::OpenDoor, door.x, door.y, door.id}, services);
                    }
                    break;
                }
                break;
            case EventActionType::SendSignal:
                runtime.fire({EventTriggerType::Signal, current.x, current.y, action.targetId}, services);
                break;
            default:
                // These PR compiler actions have no editor surface yet. Never
                // present an unsupported action as having executed successfully.
                if (presentation.message) presentation.message("This event action is not supported by the current runtime adapter.");
                break;
        }
    };
    const auto result = runtime.fire(context, services);
    if (context.type == EventTriggerType::InteractObject && result.consumed &&
        dungeon.tile(context.x, context.y) == Tile::DoorClosed && presentation.cue)
        presentation.cue(WorldEventCue::DoorLocked);
    if (result.limitReached && presentation.message)
        presentation.message("An event loop was stopped. Check this level's event connections.");
    return result;
}

} // namespace sv
