#include "WorldAuthoring.hpp"

#include <algorithm>

namespace sv {

bool StoryRoom::contains(int cellX, int cellY) const {
    return cellX >= x && cellY >= y && cellX < x + width && cellY < y + height;
}

const char* doorKindName(DoorKind kind) {
    return kind == DoorKind::Gate ? "GATE" : "DOOR";
}

bool parseDoorKind(const std::string& value, DoorKind& kind) {
    if (value == "DOOR") kind = DoorKind::Door;
    else if (value == "GATE") kind = DoorKind::Gate;
    else return false;
    return true;
}

const char* worldObjectKindName(WorldObjectKind kind) {
    switch (kind) {
        case WorldObjectKind::Prop: return "PROP";
        case WorldObjectKind::Shrine: return "SHRINE";
        case WorldObjectKind::Note: return "NOTE";
        case WorldObjectKind::Corpse: return "CORPSE";
        case WorldObjectKind::Npc: return "NPC";
        case WorldObjectKind::Recruit: return "RECRUIT";
        case WorldObjectKind::PartyManagement: return "PARTY_MANAGEMENT";
        case WorldObjectKind::ArrivalPoint: return "ARRIVAL_POINT";
        case WorldObjectKind::LevelTransition: return "LEVEL_TRANSITION";
    }
    return "PROP";
}

bool parseWorldObjectKind(const std::string& value, WorldObjectKind& kind) {
    if (value == "PROP") kind = WorldObjectKind::Prop;
    else if (value == "SHRINE") kind = WorldObjectKind::Shrine;
    else if (value == "NOTE") kind = WorldObjectKind::Note;
    else if (value == "CORPSE") kind = WorldObjectKind::Corpse;
    else if (value == "NPC") kind = WorldObjectKind::Npc;
    else if (value == "RECRUIT") kind = WorldObjectKind::Recruit;
    else if (value == "PARTY_MANAGEMENT") kind = WorldObjectKind::PartyManagement;
    else if (value == "ARRIVAL_POINT") kind = WorldObjectKind::ArrivalPoint;
    else if (value == "LEVEL_TRANSITION") kind = WorldObjectKind::LevelTransition;
    else return false;
    return true;
}

const char* triggerEventName(TriggerEvent event) {
    switch (event) {
        case TriggerEvent::EnterCell: return "ENTER_CELL";
        case TriggerEvent::EnterRoom: return "ENTER_ROOM";
        case TriggerEvent::OpenDoor: return "OPEN_DOOR";
        case TriggerEvent::KillEnemy: return "KILL_ENEMY";
        case TriggerEvent::PickupItem: return "PICKUP_ITEM";
        case TriggerEvent::InteractObject: return "INTERACT_OBJECT";
    }
    return "ENTER_CELL";
}

bool parseTriggerEvent(const std::string& value, TriggerEvent& event) {
    if (value == "ENTER_CELL") event = TriggerEvent::EnterCell;
    else if (value == "ENTER_ROOM") event = TriggerEvent::EnterRoom;
    else if (value == "OPEN_DOOR") event = TriggerEvent::OpenDoor;
    else if (value == "KILL_ENEMY") event = TriggerEvent::KillEnemy;
    else if (value == "PICKUP_ITEM") event = TriggerEvent::PickupItem;
    else if (value == "INTERACT_OBJECT") event = TriggerEvent::InteractObject;
    else return false;
    return true;
}

void TriggerSystem::reset() {
    runtime_.clear();
}

EventTriggerType eventTriggerType(TriggerEvent event) {
    switch (event) {
        case TriggerEvent::EnterCell: return EventTriggerType::PlayerEnterTile;
        case TriggerEvent::EnterRoom: return EventTriggerType::EnterRoom;
        case TriggerEvent::OpenDoor: return EventTriggerType::OpenDoor;
        case TriggerEvent::KillEnemy: return EventTriggerType::KillEnemy;
        case TriggerEvent::PickupItem: return EventTriggerType::PickupItem;
        case TriggerEvent::InteractObject: return EventTriggerType::InteractObject;
    }
    return EventTriggerType::Signal;
}

EventDefinition compileStoryTrigger(const StoryTrigger& trigger) {
    EventDefinition result;
    result.id = trigger.id;
    // Stable subjects follow moving enemies; coordinates are only a fallback
    // for older coordinate-addressed editor events.
    result.trigger = {eventTriggerType(trigger.event),
        trigger.subjectId.empty() ? trigger.x : -1,
        trigger.subjectId.empty() ? trigger.y : -1, trigger.subjectId};
    result.occurrence = trigger.once ? EventOccurrence::Once : EventOccurrence::EveryTime;
    if (!trigger.requiredFlag.empty())
        result.conditions.push_back({trigger.requiredFlag, CompareOp::Equal, true});
    if (!trigger.message.empty()) {
        EventAction action;
        action.text = trigger.message;
        result.actions.push_back(std::move(action));
    }
    if (!trigger.setFlag.empty()) {
        EventAction action;
        action.type = EventActionType::SetFact;
        action.targetId = trigger.setFlag;
        action.value = true;
        result.actions.push_back(std::move(action));
    }
    return result;
}

std::vector<std::string> TriggerSystem::fire(const std::vector<StoryTrigger>& triggers,
                                             TriggerEvent event,
                                             int x,
                                             int y,
                                             const std::string& subjectId) {
    std::vector<std::string> messages;
    for (const auto& trigger : triggers) runtime_.addEvent(compileStoryTrigger(trigger));
    EventServices services;
    services.executeAction = [&](const EventAction& action, const EventContext&) {
        if (!action.text.empty()) messages.push_back(action.text);
    };
    runtime_.fire({eventTriggerType(event), x, y, subjectId}, services);
    return messages;
}

bool TriggerSystem::hasFired(const std::string& triggerId) const {
    return runtime_.firedCount(triggerId) > 0;
}

} // namespace sv
