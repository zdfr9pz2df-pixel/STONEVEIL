#include "CombatTuning.hpp"

namespace sv {

CombatTuning& combatTuning() {
    static CombatTuning tuning{};
    return tuning;
}

void resetCombatTuning() {
    combatTuning() = CombatTuning{};
}

} // namespace sv
