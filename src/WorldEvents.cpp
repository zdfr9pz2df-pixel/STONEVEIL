#include "WorldEvents.hpp"

#include "Authoring.hpp"

namespace sv {

bool configureWorldEvents(const Dungeon& dungeon, EventRuntime& runtime) {
    EventRuntime configured;
    for (const auto& trigger : dungeon.triggers()) {
        if (!configured.addEvent(compileStoryTrigger(trigger))) return false;
    }
    for (const auto& placement : dungeon.doors()) {
        if (placement.locked && !placement.unlockFlag.empty()) {
            EventDefinition storyUnlock;
            storyUnlock.id = placement.id + ".interact.story-unlock";
            storyUnlock.name = "Story-unlocked passage";
            storyUnlock.trigger = {EventTriggerType::InteractObject, placement.x, placement.y, placement.id};
            storyUnlock.conditions.push_back({placement.unlockFlag, CompareOp::Equal, true});
            storyUnlock.conditions.push_back({"door." + placement.id + ".closed", CompareOp::Equal, true});
            storyUnlock.occurrence = EventOccurrence::Once;
            storyUnlock.priority = -50;
            storyUnlock.stopAfterRun = true;
            EventAction open;
            open.type = EventActionType::OpenDoor;
            open.targetId = placement.id;
            storyUnlock.actions.push_back(std::move(open));
            EventAction message;
            message.type = EventActionType::ShowMessage;
            message.text = placement.kind == DoorKind::Gate
                ? "The way is recognized. The gate opens."
                : "The way is recognized. The door opens.";
            storyUnlock.actions.push_back(std::move(message));
            if (!configured.addEvent(std::move(storyUnlock))) return false;
        }
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
    for (const auto& object : dungeon.objects()) {
        if (object.kind != WorldObjectKind::Recruit && object.kind != WorldObjectKind::PartyManagement &&
            object.kind != WorldObjectKind::LevelTransition) continue;
        EventDefinition event;
        event.id = object.id + (object.kind == WorldObjectKind::Recruit ? ".recruit" :
            (object.kind == WorldObjectKind::PartyManagement ? ".manage" : ".transition"));
        event.name = object.name;
        event.trigger = {EventTriggerType::InteractObject, -1, -1, object.id};
        event.occurrence = EventOccurrence::EveryTime;
        EventAction action;
        action.type = object.kind == WorldObjectKind::Recruit ? EventActionType::RecruitCharacter :
            (object.kind == WorldObjectKind::PartyManagement ? EventActionType::OpenPartyManagement :
             EventActionType::TransitionLevel);
        action.intValue = static_cast<int>(object.characterId);
        action.targetId = object.destinationLevelId;
        action.assetId = object.destinationArrivalId;
        action.text = object.text;
        event.actions.push_back(std::move(action));
        if (!configured.addEvent(std::move(event))) return false;
    }
    runtime = std::move(configured);
    return true;
}

EventFireResult dispatchWorldEvent(EventRuntime& runtime, const EventContext& context,
                                   Dungeon& dungeon, int& campaignKeys,
                                   const WorldEventPresentation& presentation,
                                   StoryState* storyState) {
    EventServices services;
    services.readFact = [&](const std::string& fact) -> EventValue {
        if (fact == "inventory.iron-key.count") return campaignKeys;
        if (storyState && storyState->contains(fact)) return storyState->value(fact);
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
                    const bool storyUnlocked = storyState && !door.unlockFlag.empty() &&
                        storyState->value(door.unlockFlag);
                    if (dungeon.openDoor(door.x, door.y, !door.locked || campaignKeys > 0 || storyUnlocked)) {
                        if (presentation.cue) presentation.cue(WorldEventCue::DoorOpened);
                        runtime.fire({EventTriggerType::OpenDoor, door.x, door.y, door.id}, services);
                    }
                    break;
                }
                break;
            case EventActionType::SendSignal:
                runtime.fire({EventTriggerType::Signal, current.x, current.y, action.targetId}, services);
                break;
            case EventActionType::SetFact:
                if (storyState) {
                    const auto* value = std::get_if<bool>(&action.value);
                    if (value) storyState->set(action.targetId, *value);
                }
                break;
            case EventActionType::RecruitCharacter:
                if (presentation.recruit && presentation.recruit(static_cast<CharacterId>(action.intValue))) {
                    if (presentation.message) presentation.message(action.text.empty() ?
                        "A new companion joins the reserve roster." : action.text);
                } else if (presentation.message) presentation.message("This character is already recruited or unavailable.");
                break;
            case EventActionType::OpenPartyManagement:
                if (presentation.message && !action.text.empty()) presentation.message(action.text);
                if (presentation.openPartyManagement) presentation.openPartyManagement();
                break;
            case EventActionType::TransitionLevel:
                if (presentation.message && !action.text.empty()) presentation.message(action.text);
                if (!presentation.transitionLevel || !presentation.transitionLevel(action.targetId, action.assetId)) {
                    if (presentation.message) presentation.message("That passage is not connected to a valid arrival point.");
                }
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
