#pragma once

#include <string>
#include <vector>

namespace sv {

// Enemy archetypes as data, following the same catalog shape as materials,
// lights and portrait states. Levels store only the stable id, so stats can be
// retuned without rewriting authored maps.
//
// Timing fields are SCALES applied to the shared values in CombatTuning rather
// than absolute seconds. That keeps one global feel dial (tuning) and one
// per-archetype personality dial (these scales), instead of two competing sets
// of magic numbers.
struct EnemyDefinition {
    std::string id;
    std::string name;
    int maxHp{18};
    int power{6};
    int meleeRange{1};          // in cells, Manhattan
    int aggroRadius{7};
    float moveRecoveryScale{1.0f};
    float windupScale{1.0f};
    float recoveryScale{1.0f};
};

const std::vector<EnemyDefinition>& enemyCatalog();
const EnemyDefinition* findEnemyType(const std::string& id);

// Levels written before the enemy-type field existed resolve to this.
const std::string& defaultEnemyTypeId();

// Never returns null: falls back to the default archetype for unknown ids.
const EnemyDefinition& enemyTypeOrDefault(const std::string& id);

} // namespace sv
