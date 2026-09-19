#include "EnemyType.hpp"

#include <algorithm>

namespace sv {

const std::vector<EnemyDefinition>& enemyCatalog() {
    static const std::vector<EnemyDefinition> enemies = {
        // id              name       hp  pow rng aggro move  windup recovery
        {"enemy.prowler",  "Prowler", 18, 6,  1,  7,    1.00f, 0.70f, 0.70f},
        {"enemy.brute",    "Brute",   40, 9,  1,  8,    1.70f, 1.00f, 1.00f},
    };
    return enemies;
}

const EnemyDefinition* findEnemyType(const std::string& id) {
    const auto& enemies = enemyCatalog();
    const auto found = std::find_if(enemies.begin(), enemies.end(), [&id](const EnemyDefinition& enemy) {
        return enemy.id == id;
    });
    return found == enemies.end() ? nullptr : &*found;
}

const std::string& defaultEnemyTypeId() {
    static const std::string fallback = "enemy.prowler";
    return fallback;
}

const EnemyDefinition& enemyTypeOrDefault(const std::string& id) {
    const auto* found = findEnemyType(id);
    if (found != nullptr) return *found;
    const auto* fallback = findEnemyType(defaultEnemyTypeId());
    static const EnemyDefinition emergency{};
    return fallback != nullptr ? *fallback : emergency;
}

} // namespace sv
