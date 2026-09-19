#include "CombatSystem.hpp"

#include "CombatTuning.hpp"
#include "Dungeon.hpp"
#include "EnemyType.hpp"
#include "Formation.hpp"
#include "Party.hpp"
#include "Player.hpp"
#include "Roster.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace sv {

CombatSystem::CombatSystem() : CombatSystem(std::random_device{}()) {}

CombatSystem::CombatSystem(std::uint32_t randomSeed) : random_(randomSeed) {}

void CombatSystem::reset() {
    partyAttackCooldown_ = 0.0f;
}

void CombatSystem::tick(float dt) {
    partyAttackCooldown_ = std::max(0.0f, partyAttackCooldown_ - dt);
}

CombatEvent CombatSystem::partyAttack(Roster& roster, const Party& party, Dungeon& dungeon, const PlayerState& player) {
    if (partyAttackCooldown_ > 0.0f) return {};
    partyAttackCooldown_ = combatTuning().playerAttackRecovery;

    const int index = frontEnemyIndex(dungeon, player, 1);
    if (index < 0) return {"Your weapons cut empty air.", 1.0f, 0};

    int damage = 0;
    for (const auto id : party.members()) {
        const auto* record = roster.find(id);
        const auto* definition = findCharacterDefinition(id);
        if (record != nullptr && definition != nullptr && record->alive()) damage += definition->power;
    }
    damage = std::max(1, damage / 2 + randomInt(0, 5));

    auto& enemy = dungeon.enemies()[static_cast<std::size_t>(index)];
    enemy.hp -= damage;
    if (enemy.hp <= 0) {
        enemy.hp = 0;
        enemy.alive = false;
        for (const auto id : party.members()) {
            auto* record = roster.find(id);
            if (record != nullptr && record->alive()) record->xp += 25;
        }
        return {"Enemy felled. +25 XP", 2.0f, 25};
    }
    return {"Party strikes for " + std::to_string(damage) + ".", 1.2f, 0};
}

CombatEvent CombatSystem::updateEnemies(float dt,
                                        Roster& roster,
                                        Party& party,
                                        Dungeon& dungeon,
                                        const PlayerState& player) {
    CombatEvent latestEvent;
    const auto& tuning = combatTuning();
    auto& enemies = dungeon.enemies();

    for (std::size_t i = 0; i < enemies.size(); ++i) {
        auto& enemy = enemies[i];
        if (!enemy.alive) continue;

        const EnemyDefinition& type = enemyTypeOrDefault(enemy.typeId);
        enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
        enemy.moveRemaining = std::max(0.0f, enemy.moveRemaining - dt);

        // Recovery is a hard commitment: the enemy is open and cannot act.
        if (enemy.recoveryRemaining > 0.0f) {
            enemy.recoveryRemaining = std::max(0.0f, enemy.recoveryRemaining - dt);
            continue;
        }

        const int distance = std::abs(enemy.x - player.x()) + std::abs(enemy.y - player.y());

        // A committed windup resolves where the player is NOW, not where they
        // were when it started. Stepping out of range during the telegraph is
        // the intended counterplay.
        if (enemy.winding) {
            enemy.windupRemaining = std::max(0.0f, enemy.windupRemaining - dt);
            if (enemy.windupRemaining > 0.0f) continue;

            enemy.winding = false;
            enemy.recoveryRemaining = tuning.enemyAttackRecovery * type.recoveryScale;
            enemy.attackCooldown = enemy.recoveryRemaining;

            if (distance > type.meleeRange) {
                latestEvent = {type.name + " swings through empty air.", 1.0f, 0};
                continue;
            }

            const auto line = formationFor(roster, party);
            const auto pick = selectMeleeTarget(line, tuning);
            lastMeleeTarget_ = pick;
            ++meleeResolutions_;
            if (!pick.valid()) continue;

            const int rolled = type.power + randomInt(-1, 2);
            const int damage = std::max(1, static_cast<int>(static_cast<float>(rolled) * pick.damageScale));
            const auto* definition = findCharacterDefinition(pick.target);
            const std::string name = definition != nullptr ? definition->name : "A companion";
            if (roster.damage(pick.target, damage)) {
                party.remove(pick.target);
                latestEvent = {name + " has died. The loss is permanent.", 2.5f, 0};
            } else {
                latestEvent = {name + " takes " + std::to_string(damage) + " damage.", 1.0f, 0};
            }
            continue;
        }

        // Begin a telegraphed attack when in range and off cooldown.
        if (distance <= type.meleeRange && enemy.attackCooldown <= 0.0f) {
            enemy.winding = true;
            enemy.windupRemaining = tuning.enemyAttackWindup * type.windupScale;
            continue;
        }

        if (distance > type.aggroRadius || enemy.moveRemaining > 0.0f) continue;

        int nextX = enemy.x;
        int nextY = enemy.y;
        const int stepX = (player.x() > enemy.x) - (player.x() < enemy.x);
        const int stepY = (player.y() > enemy.y) - (player.y() < enemy.y);
        if (std::abs(player.x() - enemy.x) >= std::abs(player.y() - enemy.y)) nextX += stepX;
        else nextY += stepY;

        if (!dungeon.blocksMovement(nextX, nextY) && !(nextX == player.x() && nextY == player.y()) &&
            !enemyAt(dungeon, nextX, nextY, static_cast<int>(i))) {
            enemy.x = nextX;
            enemy.y = nextY;
            enemy.moveRemaining = tuning.enemyMoveRecovery * type.moveRecoveryScale;
        }
    }
    return latestEvent;
}

std::vector<FormationMember> CombatSystem::formationFor(const Roster& roster, const Party& party) {
    std::vector<FormationMember> line;
    line.reserve(party.members().size());
    for (const auto id : party.members()) {
        const auto* record = roster.find(id);
        // `defending` stays false until the Vanguard Defend action exists; the
        // interception branch is already implemented and tested behind it.
        line.push_back({id, record != nullptr && record->alive(), false});
    }
    return line;
}

bool CombatSystem::dangerActive(const Dungeon& dungeon) {
    for (const auto& enemy : dungeon.enemies()) {
        if (!enemy.alive) continue;
        if (enemy.winding || enemy.recoveryRemaining > 0.0f) return true;
    }
    return false;
}

bool CombatSystem::enemyAt(const Dungeon& dungeon, int x, int y, int ignoreIndex) const {
    for (std::size_t i = 0; i < dungeon.enemies().size(); ++i) {
        const auto& enemy = dungeon.enemies()[i];
        if (static_cast<int>(i) != ignoreIndex && enemy.alive && enemy.x == x && enemy.y == y) return true;
    }
    return false;
}

int CombatSystem::frontEnemyIndex(const Dungeon& dungeon, const PlayerState& player, int maxDistance) const {
    int x = player.x();
    int y = player.y();
    for (int distance = 1; distance <= maxDistance; ++distance) {
        x += PlayerState::directionX(player.direction());
        y += PlayerState::directionY(player.direction());
        if (dungeon.blocksSight(x, y)) return -1;
        for (std::size_t i = 0; i < dungeon.enemies().size(); ++i) {
            const auto& enemy = dungeon.enemies()[i];
            if (enemy.alive && enemy.x == x && enemy.y == y) return static_cast<int>(i);
        }
    }
    return -1;
}

int CombatSystem::randomInt(int minimum, int maximum) {
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(random_);
}

} // namespace sv
