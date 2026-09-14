#include "Authoring.hpp"

#include <utility>

namespace sv {

std::string spatialObjectId(const std::string& kind, int x, int y) {
    return kind + ":" + std::to_string(x) + "," + std::to_string(y);
}

EventDefinition compileStoryTile(const StoryTileBehavior& behavior) {
    EventDefinition event;
    event.id = behavior.id.empty() ? spatialObjectId("story", behavior.x, behavior.y) : behavior.id;
    event.name = behavior.name.empty() ? "Story Moment" : behavior.name;
    event.trigger = {EventTriggerType::PlayerEnterTile, behavior.x, behavior.y, {}};
    event.occurrence = behavior.once ? EventOccurrence::Once : EventOccurrence::EveryTime;

    if (!behavior.soundAssetId.empty()) {
        EventAction action;
        action.type = EventActionType::PlaySound;
        action.assetId = behavior.soundAssetId;
        event.actions.push_back(std::move(action));
    }

    if (!behavior.musicAssetId.empty()) {
        EventAction action;
        action.type = EventActionType::ChangeMusic;
        action.assetId = behavior.musicAssetId;
        event.actions.push_back(std::move(action));
    }

    for (const auto& cue : behavior.speech) {
        EventAction action;
        action.type = EventActionType::Speak;
        action.targetId = cue.speakerId;
        action.text = cue.text;
        action.assetId = cue.voiceAssetId;
        event.actions.push_back(std::move(action));
    }

    event.actions.insert(event.actions.end(), behavior.thenActions.begin(), behavior.thenActions.end());
    return event;
}

std::vector<EventDefinition> compileDoorBehavior(const DoorBehavior& behavior) {
    std::vector<EventDefinition> events;
    const std::string doorId = behavior.id.empty() ? spatialObjectId("door", behavior.x, behavior.y) : behavior.id;
    const bool hasUnlockCondition = behavior.locked && !behavior.requiredItemCountFact.empty();

    if (!behavior.locked || hasUnlockCondition) {
        EventDefinition success;
        success.id = doorId + ".interact.success";
        success.name = behavior.name.empty() ? "Door Interaction" : behavior.name;
        success.trigger = {EventTriggerType::InteractObject, behavior.x, behavior.y, doorId};
        success.priority = 100;
        success.stopAfterRun = true;

        if (hasUnlockCondition) {
            success.conditions.push_back({
                behavior.requiredItemCountFact,
                CompareOp::GreaterOrEqual,
                behavior.requiredItemCount,
            });
        }

        if (!behavior.openSoundAssetId.empty()) {
            EventAction action;
            action.type = EventActionType::PlaySound;
            action.targetId = doorId;
            action.assetId = behavior.openSoundAssetId;
            success.actions.push_back(std::move(action));
        }

        if (behavior.boobyTrapped && behavior.trapDamage > 0) {
            EventAction action;
            action.type = EventActionType::DamageParty;
            action.targetId = "party";
            action.intValue = behavior.trapDamage;
            success.actions.push_back(std::move(action));
        }

        if (behavior.opens) {
            EventAction action;
            action.type = EventActionType::OpenDoor;
            action.targetId = doorId;
            success.actions.push_back(std::move(action));
        }

        if (hasUnlockCondition && !behavior.requiredItemId.empty()) {
            EventAction action;
            action.type = EventActionType::ConsumeItem;
            action.targetId = behavior.requiredItemId;
            action.intValue = behavior.requiredItemCount;
            success.actions.push_back(std::move(action));
        }

        if (!behavior.openMessage.empty()) {
            EventAction action;
            action.type = EventActionType::ShowMessage;
            action.text = behavior.openMessage;
            success.actions.push_back(std::move(action));
        }

        if (!behavior.cutsceneId.empty()) {
            EventAction action;
            action.type = EventActionType::StartCutscene;
            action.targetId = behavior.cutsceneId;
            success.actions.push_back(std::move(action));
        }

        success.actions.insert(success.actions.end(), behavior.afterOpenActions.begin(), behavior.afterOpenActions.end());
        events.push_back(std::move(success));
    }

    if (behavior.locked) {
        EventDefinition blocked;
        blocked.id = doorId + ".interact.locked";
        blocked.name = behavior.name.empty() ? "Locked Door" : behavior.name + " — Locked";
        blocked.trigger = {EventTriggerType::InteractObject, behavior.x, behavior.y, doorId};
        blocked.priority = 90;
        blocked.stopAfterRun = true;

        if (hasUnlockCondition) {
            blocked.conditions.push_back({
                behavior.requiredItemCountFact,
                CompareOp::LessThan,
                behavior.requiredItemCount,
            });
        }

        if (!behavior.lockedSpeech.text.empty()) {
            EventAction action;
            action.type = EventActionType::Speak;
            action.targetId = behavior.lockedSpeech.speakerId;
            action.text = behavior.lockedSpeech.text;
            action.assetId = behavior.lockedSpeech.voiceAssetId;
            blocked.actions.push_back(std::move(action));
        } else if (!behavior.lockedMessage.empty()) {
            EventAction action;
            action.type = EventActionType::ShowMessage;
            action.text = behavior.lockedMessage;
            blocked.actions.push_back(std::move(action));
        }

        events.push_back(std::move(blocked));
    }

    return events;
}

} // namespace sv
