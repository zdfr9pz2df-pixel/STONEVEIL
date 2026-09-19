#include "Formation.hpp"

#include <algorithm>

namespace sv {

FormationSlot formationSlotForIndex(std::size_t index) {
    if (index == 0) return FormationSlot::Front;
    if (index == 1) return FormationSlot::Middle;
    return FormationSlot::Rear;
}

const char* formationSlotName(FormationSlot slot) {
    switch (slot) {
        case FormationSlot::Front: return "FRONT";
        case FormationSlot::Middle: return "MIDDLE";
        case FormationSlot::Rear: return "REAR";
    }
    return "REAR";
}

MeleeTargetResult selectMeleeTarget(const std::vector<FormationMember>& formation,
                                    const CombatTuning& tuning) {
    MeleeTargetResult result;

    std::size_t foremost = formation.size();
    for (std::size_t i = 0; i < formation.size(); ++i) {
        if (formation[i].alive && formation[i].id != InvalidCharacterId) {
            foremost = i;
            break;
        }
    }
    if (foremost == formation.size()) return result;

    result.target = formation[foremost].id;
    result.intendedTarget = result.target;

    // Rule 4: the foremost Alive defender at or ahead of the intended target
    // intercepts. Scanning forward from 0 and stopping at `foremost` keeps this
    // deterministic when two characters defend at once.
    for (std::size_t i = 0; i <= foremost; ++i) {
        const auto& member = formation[i];
        if (!member.alive || member.id == InvalidCharacterId || !member.defending) continue;
        result.target = member.id;
        result.intercepted = member.id != result.intendedTarget;
        result.damageScale = std::max(0.0f, tuning.vanguardDefendDamageScale);
        return result;
    }
    return result;
}

} // namespace sv
