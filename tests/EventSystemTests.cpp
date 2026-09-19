#include "Authoring.hpp"
#include "EventSystem.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(1); } } while (false)
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

void storyTileRunsOnceAndPreservesSpeechOrder() {
    StoryTileBehavior tile;
    tile.id = "story.quiet_corridor";
    tile.name = "Quiet Corridor";
    tile.x = 6;
    tile.y = 4;
    tile.once = true;
    tile.speech.push_back({"character.vanguard", "It's quiet.", {}});
    tile.speech.push_back({"character.ranger", "Too quiet.", {}});

    EventRuntime runtime;
    runtime.addEvent(compileStoryTile(tile));

    Harness harness;
    const EventContext entered{EventTriggerType::PlayerEnterTile, 6, 4, {}, "player"};

    const auto first = runtime.fire(entered, harness.services());
    CHECK(first.eventsRun == 1);
    CHECK(harness.actions.size() == 2);
    CHECK(harness.actions[0].type == EventActionType::Speak);
    CHECK(harness.actions[0].targetId == "character.vanguard");
    CHECK(harness.actions[0].text == "It's quiet.");
    CHECK(harness.actions[1].type == EventActionType::Speak);
    CHECK(harness.actions[1].targetId == "character.ranger");
    CHECK(harness.actions[1].text == "Too quiet.");

    harness.actions.clear();
    const auto second = runtime.fire(entered, harness.services());
    CHECK(second.eventsRun == 0);
    CHECK(harness.actions.empty());
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
    door.lockedSpeech = {"character.vanguard", "It's locked.", {}};

    EventRuntime runtime;
    for (auto event : compileDoorBehavior(door)) runtime.addEvent(std::move(event));

    Harness harness;
    harness.facts["inventory.iron_key.count"] = 0;
    const EventContext interaction{EventTriggerType::InteractObject, 7, 4, "door:7,4", "player"};

    const auto result = runtime.fire(interaction, harness.services());
    CHECK(result.eventsRun == 1);
    CHECK(result.consumed);
    CHECK(harness.actions.size() == 1);
    CHECK(harness.actions[0].type == EventActionType::Speak);
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
    CHECK(result.eventsRun == 1);
    CHECK(result.consumed);
    CHECK(harness.actions.size() == 3);
    CHECK(harness.actions[0].type == EventActionType::OpenDoor);
    CHECK(harness.actions[1].type == EventActionType::ConsumeItem);
    CHECK(harness.actions[2].type == EventActionType::ShowMessage);
}

void permanentlyLockedDoorHasNoSuccessEvent() {
    DoorBehavior door;
    door.id = "door:1,1";
    door.x = 1;
    door.y = 1;
    door.locked = true;
    door.lockedMessage = "It will not move.";

    const auto events = compileDoorBehavior(door);
    CHECK(events.size() == 1);
    CHECK(events[0].id == "door:1,1.interact.locked");
}

EventDefinition messageEvent(const std::string& id, int priority = 0) {
    EventDefinition event;
    event.id = id;
    event.priority = priority;
    EventAction action;
    action.text = id;
    event.actions.push_back(action);
    return event;
}

void nestedDispatchRechecksOnceAndConditions() {
    EventRuntime runtime;
    auto first = messageEvent("first", 10);
    first.occurrence = EventOccurrence::Once;
    auto later = messageEvent("later");
    later.occurrence = EventOccurrence::Once;
    CHECK(runtime.addEvent(first));
    CHECK(runtime.addEvent(later));
    std::vector<std::string> seen;
    EventServices services;
    services.executeAction = [&](const EventAction& action, const EventContext& context) {
        seen.push_back(action.text);
        if (action.text == "first") runtime.fire(context, services);
    };
    runtime.fire({}, services);
    CHECK(seen == std::vector<std::string>({"first", "later"}));
    CHECK(runtime.firedCount("later") == 1);

    CHECK(runtime.clear());
    first.occurrence = EventOccurrence::EveryTime;
    later.conditions.push_back({"allowed", CompareOp::Equal, true});
    CHECK(runtime.addEvent(first));
    CHECK(runtime.addEvent(later));
    bool allowed = true;
    services.readFact = [&](const std::string&) -> EventValue { return allowed; };
    services.executeAction = [&](const EventAction&, const EventContext&) { allowed = false; };
    CHECK(runtime.fire({}, services).eventsRun == 1);
    CHECK(runtime.firedCount("later") == 0);
}

void idsMutationBoundsAndExceptionSafety() {
    EventRuntime runtime;
    CHECK(!runtime.addEvent({}));
    CHECK(runtime.addEvent(messageEvent("repeat")));
    CHECK(!runtime.addEvent(messageEvent("repeat")));
    int actions = 0;
    EventServices services;
    services.executeAction = [&](const EventAction&, const EventContext& context) {
        ++actions;
        CHECK(!runtime.clear());
        CHECK(!runtime.resetRuntimeState());
        CHECK(!runtime.addEvent(messageEvent("during-dispatch")));
        CHECK(!runtime.restoreFiredCounts({}));
        runtime.fire(context, services);
        runtime.fire(context, services); // branching cycle must also be bounded
    };
    CHECK(runtime.fire({}, services).limitReached);
    CHECK(actions > 0 && actions <= 4096);
    CHECK(runtime.resetRuntimeState());
    services.executeAction = [](const EventAction&, const EventContext&) { throw std::runtime_error("test"); };
    try { runtime.fire({}, services); CHECK(false); } catch (const std::runtime_error&) {}
    CHECK(runtime.clear()); // dispatch depth must unwind after callback exceptions
}

void stablePriorityAndStateRestore() {
    EventRuntime runtime;
    CHECK(runtime.addEvent(messageEvent("low", -1)));
    CHECK(runtime.addEvent(messageEvent("first", 1)));
    CHECK(runtime.addEvent(messageEvent("second", 1)));
    Harness harness;
    CHECK(runtime.fire({}, harness.services()).eventsRun == 3);
    CHECK(harness.actions[0].text == "first");
    CHECK(harness.actions[1].text == "second");
    CHECK(harness.actions[2].text == "low");
    CHECK(!runtime.restoreFiredCounts({{"deleted", 1}}));
    CHECK(!runtime.restoreFiredCounts({{"first", 0}}));
    CHECK(runtime.firedCount("first") == 1);
    CHECK(runtime.restoreFiredCounts({{"first", 4}}));
    CHECK(runtime.firedCount("first") == 4);
    CHECK(runtime.firedCount("second") == 0);
}

} // namespace

int main() {
    storyTileRunsOnceAndPreservesSpeechOrder();
    lockedDoorChoosesReadableFailure();
    doorWithKeyRunsSuccessPath();
    permanentlyLockedDoorHasNoSuccessEvent();
    nestedDispatchRechecksOnceAndConditions();
    idsMutationBoundsAndExceptionSafety();
    stablePriorityAndStateRestore();
    return 0;
}
