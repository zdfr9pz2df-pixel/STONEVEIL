#pragma once

#include "Character.hpp"
#include "CombatTuning.hpp"

#include <cstddef>
#include <vector>

namespace sv {

// Deterministic melee targeting.
//
// THE RULE, in full, so a player can always answer "why did that hit her?":
//
//   1. Active party members are ordered by party index. Index 0 is FRONT,
//      index 1 is MIDDLE, index 2 is REAR. Party order is the formation; there
//      is no separate formation array to get out of sync.
//   2. A member is a valid melee target only while Alive. Downed members are
//      not struck again, and Dead members are not in the party.
//   3. The default target is the FOREMOST valid member — lowest index.
//   4. If any Alive member is Defending and stands at or ahead of that target
//      (lower or equal index), the Defender intercepts and is hit instead, for
//      reduced damage.
//   5. If no member is Alive there is no target, and the caller should enter the
//      party-defeated state rather than picking someone.
//
// There is deliberately no threat, aggro or randomness here. Ranged enemies may
// break rule 3 later; they do not break rules 2 or 5.

enum class FormationSlot {
    Front = 0,
    Middle = 1,
    Rear = 2,
};

FormationSlot formationSlotForIndex(std::size_t index);
const char* formationSlotName(FormationSlot slot);

struct FormationMember {
    CharacterId id{InvalidCharacterId};
    bool alive{false};
    bool defending{false};
};

struct MeleeTargetResult {
    CharacterId target{InvalidCharacterId};
    CharacterId intendedTarget{InvalidCharacterId};
    bool intercepted{false};
    float damageScale{1.0f};

    bool valid() const { return target != InvalidCharacterId; }
};

MeleeTargetResult selectMeleeTarget(const std::vector<FormationMember>& formation,
                                    const CombatTuning& tuning);

} // namespace sv
