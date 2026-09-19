#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace sv {

using EventValue = std::variant<std::monostate, bool, int, double, std::string>;

enum class EventTriggerType {
    PlayerEnterTile,
    PlayerLeaveTile,
    InteractObject,
    Signal,
    EnterRoom,
    OpenDoor,
    KillEnemy,
    PickupItem,
};

enum class CompareOp {
    Equal,
    NotEqual,
    LessThan,
    LessOrEqual,
    GreaterThan,
    GreaterOrEqual,
};

enum class EventActionType {
    ShowMessage,
    Speak,
    OpenDoor,
    ConsumeItem,
    DamageParty,
    StartDialogue,
    StartCutscene,
    PlaySound,
    ChangeMusic,
    SetFact,
    SendSignal,
    RecruitCharacter,
    OpenPartyManagement,
    TransitionLevel,
};

enum class EventOccurrence {
    EveryTime,
    Once,
};

struct EventTrigger {
    EventTriggerType type{EventTriggerType::PlayerEnterTile};
    int x{-1};
    int y{-1};
    std::string subjectId;
};

struct EventContext {
    EventTriggerType type{EventTriggerType::PlayerEnterTile};
    int x{-1};
    int y{-1};
    std::string subjectId;
    std::string actorId{"player"};
};

struct EventCondition {
    std::string fact;
    CompareOp op{CompareOp::Equal};
    EventValue value{};
};

struct EventAction {
    EventActionType type{EventActionType::ShowMessage};
    std::string targetId;
    std::string text;
    std::string assetId;
    EventValue value{};
    int intValue{0};
    double numberValue{0.0};
};

struct EventDefinition {
    std::string id;
    std::string name;
    EventTrigger trigger;
    std::vector<EventCondition> conditions;
    std::vector<EventAction> actions;
    EventOccurrence occurrence{EventOccurrence::EveryTime};
    int priority{0};
    bool stopAfterRun{false};
};

struct EventServices {
    // Fact queries must be read-only; actions may dispatch nested events.
    std::function<EventValue(const std::string&)> readFact;
    std::function<void(const EventAction&, const EventContext&)> executeAction;
};

struct EventFireResult {
    int eventsRun{0};
    bool consumed{false};
    bool limitReached{false};
};

class EventRuntime {
public:
    // Definition/state mutation is rejected while dispatching. IDs must be
    // explicit and unique; adding an event never resets a previous one-shot.
    bool clear();
    bool resetRuntimeState();
    bool addEvent(EventDefinition definition);
    EventFireResult fire(const EventContext& context, const EventServices& services);

    const std::vector<EventDefinition>& definitions() const { return definitions_; }
    std::uint32_t firedCount(const std::string& eventId) const;
    const std::unordered_map<std::string, std::uint32_t>& firedCounts() const { return firedCounts_; }
    bool restoreFiredCounts(const std::unordered_map<std::string, std::uint32_t>& counts);

private:
    bool triggerMatches(const EventTrigger& trigger, const EventContext& context) const;
    bool conditionsPass(const EventDefinition& definition, const EventServices& services) const;

    std::vector<EventDefinition> definitions_;
    std::unordered_map<std::string, std::uint32_t> firedCounts_;
    int fireDepth_{0};
    int dispatchBudget_{0};
    bool limitReached_{false};
};

} // namespace sv
