#include "Portrait.hpp"

#include <algorithm>
#include <cstddef>

namespace sv {
namespace {

constexpr float WoundedFraction = 0.60f;
constexpr float CriticalFraction = 0.25f;

float healthFraction(const PortraitConditions& conditions) {
    if (conditions.maxHp <= 0) return 0.0f;
    const float fraction = static_cast<float>(conditions.hp) / static_cast<float>(conditions.maxHp);
    return std::clamp(fraction, 0.0f, 1.0f);
}

bool moodApplies(const PortraitStateDefinition& state, const PortraitConditions& conditions) {
    const float fraction = healthFraction(conditions);
    if (state.id == "portrait.critical") return fraction < CriticalFraction;
    if (state.id == "portrait.wounded") return fraction < WoundedFraction;
    if (state.id == "portrait.alert") return conditions.enemyVisible;
    if (state.id == "portrait.peering") return conditions.inDarkness && !conditions.enemyVisible;
    if (state.id == "portrait.idle") return true;
    return false;
}

} // namespace

const std::vector<PortraitStateDefinition>& portraitStateCatalog() {
    // Tier 0 is the minimum shippable set: six states per character. Everything
    // above it degrades through `fallbackId`, so art can land incrementally
    // without a code change and without a character ever rendering blank.
    static const std::vector<PortraitStateDefinition> states = {
        // id                      suffix         name            category                    tier pri  dur   bark  fallback
        {"portrait.dead",          "dead",        "Dead",         PortraitCategory::Mood,     0,   0,   0.0f, 0.0f, ""},
        {"portrait.idle",          "idle",        "Idle",         PortraitCategory::Mood,     0,   10,  0.0f, 0.0f, ""},
        {"portrait.peering",       "peering",     "Peering",      PortraitCategory::Mood,     2,   30,  0.0f, 8.0f, "portrait.idle"},
        {"portrait.alert",         "alert",       "Alert",        PortraitCategory::Mood,     0,   50,  0.0f, 6.0f, "portrait.idle"},
        {"portrait.wounded",       "wounded",     "Wounded",      PortraitCategory::Mood,     0,   40,  0.0f, 0.0f, "portrait.idle"},
        {"portrait.critical",      "critical",    "Critical",     PortraitCategory::Mood,     0,   60,  0.0f, 12.0f, "portrait.wounded"},

        {"portrait.talking",       "talking",     "Talking",      PortraitCategory::Reflex,   1,   105, 1.40f, 0.0f, "portrait.idle"},
        {"portrait.pickup",        "pickup",      "Pickup",       PortraitCategory::Reflex,   2,   110, 0.80f, 3.0f, "portrait.idle"},
        {"portrait.heal",          "heal",        "Healed",       PortraitCategory::Reflex,   2,   115, 0.80f, 4.0f, "portrait.idle"},
        {"portrait.blocked",       "blocked",     "Frustrated",   PortraitCategory::Reflex,   1,   120, 0.50f, 5.0f, "portrait.alert"},
        {"portrait.kill",          "kill",        "Kill",         PortraitCategory::Reflex,   1,   130, 0.80f, 4.0f, "portrait.alert"},
        {"portrait.hit",           "hit",         "Struck",       PortraitCategory::Reflex,   0,   140, 0.60f, 1.5f, "portrait.idle"},
        {"portrait.spell-fizzle",  "fizzle",      "Spell Lost",   PortraitCategory::Reflex,   2,   150, 1.00f, 4.0f, "portrait.blocked"},
        {"portrait.hurt-badly",    "hurt-badly",  "Badly Hurt",   PortraitCategory::Reflex,   2,   170, 1.00f, 6.0f, "portrait.hit"},
        {"portrait.ally-down",     "ally-down",   "Ally Down",    PortraitCategory::Reflex,   1,   190, 2.50f, 0.0f, "portrait.hit"},
    };
    return states;
}

const PortraitStateDefinition* findPortraitState(const std::string& id) {
    const auto& states = portraitStateCatalog();
    const auto found = std::find_if(states.begin(), states.end(), [&id](const PortraitStateDefinition& state) {
        return state.id == id;
    });
    return found == states.end() ? nullptr : &*found;
}

std::string resolvePortraitFallback(const std::string& id,
                                    const std::vector<std::string>& availableIds) {
    std::string current = id;
    // The catalog is acyclic, but guard anyway so a bad edit cannot hang the
    // renderer.
    for (std::size_t guard = 0; guard < portraitStateCatalog().size() + 1; ++guard) {
        if (current.empty()) return {};
        if (std::find(availableIds.begin(), availableIds.end(), current) != availableIds.end()) return current;
        const auto* state = findPortraitState(current);
        if (state == nullptr) return {};
        current = state->fallbackId;
    }
    return {};
}

bool PortraitTrack::trigger(const std::string& reflexId) {
    const auto* state = findPortraitState(reflexId);
    if (state == nullptr || state->category != PortraitCategory::Reflex) return false;
    if (reflexRemaining_ > 0.0f && state->priority < activeReflexPriority_) return false;

    activeReflex_ = state->id;
    activeReflexPriority_ = state->priority;
    reflexRemaining_ = state->durationSeconds;
    return true;
}

void PortraitTrack::tick(float dt) {
    if (dt <= 0.0f) return;
    if (reflexRemaining_ > 0.0f) {
        reflexRemaining_ = std::max(0.0f, reflexRemaining_ - dt);
        if (reflexRemaining_ == 0.0f) {
            activeReflex_.clear();
            activeReflexPriority_ = 0;
        }
    }
    for (auto& cooldown : barkCooldowns_) {
        cooldown.remaining = std::max(0.0f, cooldown.remaining - dt);
    }
}

void PortraitTrack::clear() {
    activeReflex_.clear();
    activeReflexPriority_ = 0;
    reflexRemaining_ = 0.0f;
    barkCooldowns_.clear();
}

std::string PortraitTrack::resolve(const PortraitConditions& conditions) const {
    if (conditions.dead) return "portrait.dead";
    if (reflexRemaining_ > 0.0f && !activeReflex_.empty()) return activeReflex_;

    const PortraitStateDefinition* best = nullptr;
    for (const auto& state : portraitStateCatalog()) {
        if (state.category != PortraitCategory::Mood) continue;
        if (state.id == "portrait.dead") continue;
        if (!moodApplies(state, conditions)) continue;
        if (best == nullptr || state.priority > best->priority) best = &state;
    }
    return best == nullptr ? std::string{"portrait.idle"} : best->id;
}

bool PortraitTrack::requestBark(const std::string& stateId) {
    const auto* state = findPortraitState(stateId);
    if (state == nullptr) return false;

    const auto found = std::find_if(barkCooldowns_.begin(), barkCooldowns_.end(),
                                    [&stateId](const BarkCooldown& cooldown) {
                                        return cooldown.stateId == stateId;
                                    });
    if (found != barkCooldowns_.end()) {
        if (found->remaining > 0.0f) return false;
        found->remaining = state->barkCooldownSeconds;
        return true;
    }
    barkCooldowns_.push_back({stateId, state->barkCooldownSeconds});
    return true;
}

} // namespace sv
