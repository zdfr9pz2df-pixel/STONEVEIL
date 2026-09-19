#pragma once

#include <string>
#include <vector>

namespace sv {

// Portrait reaction states.
//
// A character's portrait shows one state at a time, chosen from game conditions
// rather than authored per scene. This module owns the catalog and the pure
// resolver; art and audio attach later by stable ID, exactly like materials and
// lights. Nothing here includes raylib, so it belongs in the core test target.
//
// States come in two flavours:
//   Mood   — continuous, derived from condition (wounded, alert, peering)
//   Reflex — brief, event-driven, interrupts the mood (hit, kill, ally-down)
//
// Resolution is by priority, not by a transition graph: the highest-priority
// state that currently applies wins. A transition graph would be more code and
// harder to reason about for no gain at this scale.

enum class PortraitCategory {
    Mood,
    Reflex,
};

struct PortraitStateDefinition {
    std::string id;            // "portrait.hit" — authored into art and audio manifests
    std::string suffix;        // "hit" — file stem for frames and voice folders
    std::string name;          // human label for the editor
    PortraitCategory category{PortraitCategory::Mood};
    int tier{0};               // 0 = required to ship, 1 = adds life, 2 = polish
    int priority{0};           // higher wins
    float durationSeconds{0.0f};     // reflexes only; moods are continuous
    float barkCooldownSeconds{0.0f}; // minimum gap between voice lines for this state
    std::string fallbackId;    // used when this state has no art yet; "" ends the chain
};

const std::vector<PortraitStateDefinition>& portraitStateCatalog();
const PortraitStateDefinition* findPortraitState(const std::string& id);

// Walks the fallback chain until it reaches a state that is present in
// `availableIds`. Returns an empty string if the chain runs out, which means
// the character has no usable portrait art at all.
std::string resolvePortraitFallback(const std::string& id,
                                    const std::vector<std::string>& availableIds);

// Everything the resolver needs to know about a character right now. Kept as a
// plain snapshot so the resolver stays pure and testable.
struct PortraitConditions {
    bool dead{false};
    int hp{1};
    int maxHp{1};
    bool enemyVisible{false};  // an enemy is in view or adjacent
    bool inDarkness{false};    // the character's cell is effectively unlit
};

// Per-character runtime. One of these lives alongside each CharacterRecord in
// the presentation layer; it holds no authored data and is never serialized.
class PortraitTrack {
public:
    // Fires a reflex. A lower-priority reflex cannot interrupt a higher-priority
    // one already running, so a spoken line never stomps a damage reaction.
    bool trigger(const std::string& reflexId);

    void tick(float dt);
    void clear();

    const std::string& activeReflex() const { return activeReflex_; }

    // The state to display. Death outranks everything; an unexpired reflex
    // outranks any mood; otherwise the highest-priority matching mood wins.
    std::string resolve(const PortraitConditions& conditions) const;

    // Returns true at most once per `barkCooldownSeconds` for a given state, so
    // a character reacting every frame does not chatter.
    bool requestBark(const std::string& stateId);

private:
    struct BarkCooldown {
        std::string stateId;
        float remaining{0.0f};
    };

    std::string activeReflex_;
    float reflexRemaining_{0.0f};
    int activeReflexPriority_{0};
    std::vector<BarkCooldown> barkCooldowns_;
};

} // namespace sv
