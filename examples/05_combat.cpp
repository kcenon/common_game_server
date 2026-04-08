// examples/05_combat.cpp
//
// Tutorial: Combat system — damage types, auras, spell casting
// See: docs/tutorial_combat.dox
//
// This example walks through one complete combat encounter:
//
//   1. A "player" entity casts a fire spell on a "boss" entity.
//   2. The boss has fire resistance, so mitigation reduces the hit.
//   3. The spell applies a burning aura that ticks for 3 seconds.
//   4. CombatSystem processes the cast, damage event, and aura ticks
//      each frame.
//
// The whole flow lives in ECS components — no OOP hierarchy, no
// virtual dispatch per actor. CombatSystem reads and writes those
// components in a fixed order.

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/combat_system.hpp"
#include "cgs/game/combat_types.hpp"
#include "cgs/game/components.hpp"  // Stats

#include <cstdint>
#include <cstdlib>
#include <iostream>

using cgs::ecs::ComponentStorage;
using cgs::ecs::Entity;
using cgs::ecs::EntityManager;
using cgs::game::AuraHolder;
using cgs::game::AuraInstance;
using cgs::game::CastState;
using cgs::game::CombatSystem;
using cgs::game::DamageCalcParams;
using cgs::game::DamageEvent;
using cgs::game::DamageType;
using cgs::game::kDamageTypeCount;
using cgs::game::SpellCast;
using cgs::game::Stats;
using cgs::game::ThreatList;

int main() {
    // ── Step 1: Set up ECS storages for all combat components. ──────────
    ComponentStorage<Stats> stats;
    ComponentStorage<SpellCast> spellCasts;
    ComponentStorage<AuraHolder> auraHolders;
    ComponentStorage<DamageEvent> damageEvents;
    ComponentStorage<ThreatList> threatLists;

    EntityManager em;
    em.RegisterStorage(&stats);
    em.RegisterStorage(&spellCasts);
    em.RegisterStorage(&auraHolders);
    em.RegisterStorage(&damageEvents);
    em.RegisterStorage(&threatLists);

    // ── Step 2: Spawn the player. ──────────────────────────────────────
    const Entity player = em.Create();
    {
        Stats s;
        s.maxHealth = 1000;
        s.maxMana = 500;
        s.SetHealth(1000);
        s.SetMana(500);
        stats.Add(player, s);
    }
    spellCasts.Add(player, SpellCast{});  // Idle by default.

    // ── Step 3: Spawn the boss with fire resistance. ───────────────────
    const Entity boss = em.Create();
    {
        Stats s;
        s.maxHealth = 10000;
        s.maxMana = 0;
        s.SetHealth(10000);
        stats.Add(boss, s);
    }
    auraHolders.Add(boss, AuraHolder{});
    threatLists.Add(boss, ThreatList{});

    // ── Step 4: Compute mitigation for a fire attack. ──────────────────
    //
    // Armor mitigates Physical damage; per-type resistance mitigates
    // everything else. The boss has 300 fire resistance and 200 armor.
    DamageCalcParams params;
    params.armor = 200;
    params.resistances[static_cast<std::size_t>(DamageType::Fire)] = 300;

    // The CalculateDamage helper is a pure static function — it does
    // not touch any components, so you can unit-test it in isolation.
    const int32_t base = 500;
    const int32_t mitigated =
        CombatSystem::CalculateDamage(base, DamageType::Fire, /*isCritical*/ false, params);
    std::cout << "base " << base << " fire damage -> " << mitigated
              << " after mitigation\n";

    // And a critical hit doubles the base before mitigation.
    const int32_t crit =
        CombatSystem::CalculateDamage(base, DamageType::Fire, /*isCritical*/ true, params);
    std::cout << "crit " << base << " fire damage -> " << crit << " after mitigation\n";

    // ── Step 5: Begin a spell cast on the player. ──────────────────────
    //
    // The cast takes 2 seconds of real time. Each tick of the combat
    // system decreases remainingTime by deltaTime, eventually switching
    // the SpellCast to Complete.
    constexpr uint32_t kFireballId = 101;
    spellCasts.Get(player).Begin(kFireballId, boss, /*duration*/ 2.0f);
    std::cout << "player begins casting spell " << kFireballId << "\n";

    // ── Step 6: Queue a DamageEvent to fire immediately alongside the
    //         cast. In production code this would be emitted by a spell
    //         handler at cast-complete; we place it here to exercise
    //         the damage pipeline in isolation.
    damageEvents.Add(boss, DamageEvent{
        .attacker = player,
        .victim = boss,
        .type = DamageType::Fire,
        .baseDamage = base,
        .finalDamage = 0,
        .isCritical = false,
        .isProcessed = false,
    });

    // ── Step 7: Apply a 3-second burning aura that ticks every 1 s. ────
    AuraInstance burning;
    burning.auraId = 201;
    burning.caster = player;
    burning.stacks = 1;
    burning.duration = 3.0f;
    burning.remainingTime = 3.0f;
    burning.tickInterval = 1.0f;
    burning.tickTimer = 1.0f;  // first tick after 1 second
    burning.tickDamage = 50;
    burning.tickDamageType = DamageType::Fire;
    auraHolders.Get(boss).AddOrStack(burning);

    // ── Step 8: Construct CombatSystem and tick the world. ─────────────
    CombatSystem combat(spellCasts, auraHolders, damageEvents, stats, threatLists);

    constexpr float kDelta = 0.5f;         // 500 ms per tick
    constexpr int kTickCount = 6;          // 3 seconds of wall-clock time
    for (int tick = 0; tick < kTickCount; ++tick) {
        combat.Execute(kDelta);
        std::cout << "tick " << tick << ": boss HP = "
                  << stats.Get(boss).health << "\n";
    }

    // ── Step 9: Inspect final state. ───────────────────────────────────
    const auto& holder = auraHolders.Get(boss);
    std::cout << "boss has " << holder.auras.size()
              << " active aura(s); burning present = "
              << (holder.HasAura(201) ? "yes" : "no") << "\n";

    return EXIT_SUCCESS;
}
