#pragma once

namespace sv {

// Every timing value the 0.2 prototype needs, in one place.
//
// The point of this struct is playtest velocity: these numbers are expected to
// be wrong on the first pass and to change repeatedly. Nothing outside this file
// should contain a literal combat timing value — if you find yourself typing
// `0.75f` into Game.cpp or CombatSystem.cpp, add a field here instead.
//
// Units: seconds, except projectileSpeed which is cells per second.
struct CombatTuning {
    // --- player commitment -------------------------------------------------
    // Movement is not free. A step or a turn locks further movement for this
    // long, which is the first (deliberately crude) candidate answer to
    // square-dancing. Turning is cheaper than stepping so the player can always
    // look at what is hitting them.
    float playerMoveRecovery{0.26f};
    float playerTurnRecovery{0.16f};
    // Attacking is gated separately from turning, so facing a new threat is
    // never mistaken for swinging at it.
    float playerAttackRecovery{0.55f};

    // --- enemy pacing ------------------------------------------------------
    float enemyMoveRecovery{0.62f};
    float enemyAttackWindup{0.70f};    // telegraph: visible, interruptible window
    float enemyAttackRecovery{1.05f};  // punish window after the blow lands

    // --- projectiles -------------------------------------------------------
    float projectileSpeed{5.0f};       // cells per second
    float turretFireInterval{2.40f};
    float turretWindup{0.55f};

    // --- party action cooldowns -------------------------------------------
    // Striker is deliberately ~2x faster than Vanguard; Caster is slowest.
    float vanguardAttackCooldown{0.95f};
    float strikerAttackCooldown{0.45f};
    float casterAttackCooldown{1.40f};
    float casterHealCooldown{6.00f};

    // --- vanguard defend ---------------------------------------------------
    // While defending, the Vanguard intercepts melee aimed at anyone behind it
    // and takes reduced damage. Defend is a held stance with a floor duration so
    // it cannot be tapped on and off every frame.
    float vanguardDefendMinimum{0.40f};
    float vanguardDefendDamageScale{0.45f};

    // --- downed / bleed-out ------------------------------------------------
    // Deliberately generous for the prototype so the UI and retreat flow can be
    // exercised without the tester constantly losing characters. Not balanced.
    float downedBleedOutSeconds{45.0f};
    float reviveSeconds{2.50f};

    // --- save safety -------------------------------------------------------
    // Saving is blocked while combat danger is resolving, plus this grace period
    // after the last dangerous thing clears, so saving cannot be frame-timed
    // into a window that is technically safe but reads as unsafe.
    float saveDangerGrace{0.60f};
};

// Single mutable instance so debug keys can retune at runtime without a rebuild.
CombatTuning& combatTuning();
void resetCombatTuning();

} // namespace sv
