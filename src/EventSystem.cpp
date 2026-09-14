#include "EventSystem.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace sv {
namespace {

constexpr int MaxNestedDispatchDepth = 64;

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

void EventRuntime::clear() {
    definitions_.clear();
    firedCounts_.clear();
    fireDepth_ = 0;
}

void EventRuntime::resetRuntimeState() {
    firedCounts_.clear();
    fireDepth_ = 0;
}

void EventRuntime::addEvent(EventDefinition definition) {
    if (definition.id.empty()) {
        definition.id = "event:" + std::to_string(definitions_.size());
    }

    const auto existing = std::find_if(definitions_.begin(), definitions_.end(), [&](const EventDefinition& event) {
        return event.id == definition.id;
    });

    if (existing != definitions_.end()) {
        firedCounts_.erase(existing->id);
        *existing = std::move(definition);
        return;
    }

    definitions_.push_back(std::move(definition));
}

EventFireResult EventRuntime::fire(const EventContext& context, const EventServices& services) {
    if (fireDepth_ >= MaxNestedDispatchDepth) return {};
    ++fireDepth_;

    std::vector<std::size_t> candidates;
    candidates.reserve(definitions_.size());

    for (std::size_t i = 0; i < definitions_.size(); ++i) {
        const auto& definition = definitions_[i];
        if (!triggerMatches(definition.trigger, context)) continue;
        if (definition.occurrence == EventOccurrence::Once && firedCount(definition.id) > 0) continue;
        if (!conditionsPass(definition, services)) continue;
        candidates.push_back(i);
    }

    std::stable_sort(candidates.begin(), candidates.end(), [&](std::size_t left, std::size_t right) {
        return definitions_[left].priority > definitions_[right].priority;
    });

    EventFireResult result;
    for (const std::size_t index : candidates) {
        const auto& definition = definitions_[index];

        // Mark the event before actions run so a Once event cannot recursively
        // re-enter itself through a signal action.
        ++firedCounts_[definition.id];
        ++result.eventsRun;

        for (const auto& action : definition.actions) {
            if (services.executeAction) services.executeAction(action, context);
        }

        if (definition.stopAfterRun) {
            result.consumed = true;
            break;
        }
    }

    --fireDepth_;
    return result;
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
