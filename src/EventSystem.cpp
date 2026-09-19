#include "EventSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace sv {
namespace {

constexpr int MaxNestedDispatchDepth = 64;
constexpr int MaxDispatchWork = 4096;

bool numericValue(const EventValue& value, double& out) {
    if (const auto* integer = std::get_if<int>(&value)) {
        out = static_cast<double>(*integer);
        return true;
    }
    if (const auto* number = std::get_if<double>(&value)) {
        out = *number;
        return true;
    }
    return false;
}

bool valuesEqual(const EventValue& left, const EventValue& right) {
    double leftNumber = 0.0;
    double rightNumber = 0.0;
    if (numericValue(left, leftNumber) && numericValue(right, rightNumber)) {
        return std::abs(leftNumber - rightNumber) < 0.000001;
    }
    return left == right;
}

bool compareValues(const EventValue& left, CompareOp op, const EventValue& right) {
    if (op == CompareOp::Equal) return valuesEqual(left, right);
    if (op == CompareOp::NotEqual) return !valuesEqual(left, right);

    double leftNumber = 0.0;
    double rightNumber = 0.0;
    if (!numericValue(left, leftNumber) || !numericValue(right, rightNumber)) return false;

    switch (op) {
        case CompareOp::LessThan: return leftNumber < rightNumber;
        case CompareOp::LessOrEqual: return leftNumber <= rightNumber;
        case CompareOp::GreaterThan: return leftNumber > rightNumber;
        case CompareOp::GreaterOrEqual: return leftNumber >= rightNumber;
        case CompareOp::Equal:
        case CompareOp::NotEqual:
            break;
    }
    return false;
}

} // namespace

bool EventRuntime::clear() {
    if (fireDepth_ != 0) return false;
    definitions_.clear();
    firedCounts_.clear();
    return true;
}

bool EventRuntime::resetRuntimeState() {
    if (fireDepth_ != 0) return false;
    firedCounts_.clear();
    return true;
}

bool EventRuntime::addEvent(EventDefinition definition) {
    if (fireDepth_ != 0 || definition.id.empty()) return false;

    const auto existing = std::find_if(definitions_.begin(), definitions_.end(), [&](const EventDefinition& event) {
        return event.id == definition.id;
    });

    if (existing != definitions_.end()) return false;

    definitions_.push_back(std::move(definition));
    return true;
}

EventFireResult EventRuntime::fire(const EventContext& context, const EventServices& services) {
    if (fireDepth_ == 0) {
        dispatchBudget_ = MaxDispatchWork;
        limitReached_ = false;
    }
    if (fireDepth_ >= MaxNestedDispatchDepth || dispatchBudget_ <= 0) {
        limitReached_ = true;
        return {0, false, true};
    }
    ++fireDepth_;
    struct DispatchGuard {
        int& depth;
        ~DispatchGuard() { --depth; }
    } guard{fireDepth_};

    std::vector<std::size_t> candidates;
    candidates.reserve(definitions_.size());

    for (std::size_t i = 0; i < definitions_.size(); ++i) {
        const auto& definition = definitions_[i];
        if (!triggerMatches(definition.trigger, context)) continue;
        candidates.push_back(i);
    }

    std::stable_sort(candidates.begin(), candidates.end(), [&](std::size_t left, std::size_t right) {
        return definitions_[left].priority > definitions_[right].priority;
    });

    EventFireResult result;
    for (const std::size_t index : candidates) {
        const auto& definition = definitions_[index];
        // Earlier actions (including nested dispatch) can change both facts and
        // one-shot state. Evaluate at execution time, not candidate collection.
        if (definition.occurrence == EventOccurrence::Once && firedCount(definition.id) > 0) continue;
        if (!conditionsPass(definition, services)) continue;
        if (dispatchBudget_ <= 0) { limitReached_ = true; break; }
        --dispatchBudget_;

        // Mark the event before actions run so a Once event cannot recursively
        // re-enter itself through a signal action.
        auto& count = firedCounts_[definition.id];
        if (count < std::numeric_limits<std::uint32_t>::max()) ++count;
        ++result.eventsRun;

        for (const auto& action : definition.actions) {
            if (dispatchBudget_ <= 0) { limitReached_ = true; break; }
            --dispatchBudget_;
            if (services.executeAction) services.executeAction(action, context);
        }

        if (definition.stopAfterRun) {
            result.consumed = true;
            break;
        }
    }

    result.limitReached = limitReached_;
    return result;
}

bool EventRuntime::restoreFiredCounts(const std::unordered_map<std::string, std::uint32_t>& counts) {
    if (fireDepth_ != 0) return false;
    for (const auto& entry : counts) {
        if (entry.second == 0 || std::none_of(definitions_.begin(), definitions_.end(),
            [&](const auto& definition) { return definition.id == entry.first; })) return false;
    }
    firedCounts_ = counts;
    return true;
}

std::uint32_t EventRuntime::firedCount(const std::string& eventId) const {
    const auto it = firedCounts_.find(eventId);
    return it == firedCounts_.end() ? 0U : it->second;
}

bool EventRuntime::triggerMatches(const EventTrigger& trigger, const EventContext& context) const {
    if (trigger.type != context.type) return false;
    if (trigger.x >= 0 && trigger.x != context.x) return false;
    if (trigger.y >= 0 && trigger.y != context.y) return false;
    if (!trigger.subjectId.empty() && trigger.subjectId != context.subjectId) return false;
    return true;
}

bool EventRuntime::conditionsPass(const EventDefinition& definition, const EventServices& services) const {
    if (definition.conditions.empty()) return true;
    if (!services.readFact) return false;

    for (const auto& condition : definition.conditions) {
        const EventValue current = services.readFact(condition.fact);
        if (!compareValues(current, condition.op, condition.value)) return false;
    }
    return true;
}

} // namespace sv
