#include <gtest/gtest.h>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/entity.hpp"
#include "cgs/ecs/entity_manager.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/combat_system.hpp"
#include "cgs/game/combat_types.hpp"
#include "cgs/game/components.hpp"

using namespace cgs::ecs;
using namespace cgs::game;

// ═══════════════════════════════════════════════════════════════════════════
// SpellCast component tests (SRS-GML-002.1)
// ═══════════════════════════════════════════════════════════════════════════

TEST(SpellCastTest, DefaultIsIdle) {
    SpellCast cast;
    EXPECT_EQ(cast.state, CastState::Idle);
    EXPECT_EQ(cast.spellId, 0u);
    EXPECT_FLOAT_EQ(cast.castTime, 0.0f);
    EXPECT_FLOAT_EQ(cast.remainingTime, 0.0f);
}

TEST(SpellCastTest, BeginCasting) {
    SpellCast cast;
    Entity target(1, 0);
    cast.Begin(42, target, 2.5f);

    EXPECT_EQ(cast.spellId, 42u);
    EXPECT_EQ(cast.target, target);
    EXPECT_EQ(cast.state, CastState::Casting);
    EXPECT_FLOAT_EQ(cast.castTime, 2.5f);
    EXPECT_FLOAT_EQ(cast.remainingTime, 2.5f);
}

TEST(SpellCastTest, InterruptCasting) {
    SpellCast cast;
    cast.Begin(10, Entity(1, 0), 3.0f);
    cast.Interrupt();

    EXPECT_EQ(cast.state, CastState::Interrupted);
    EXPECT_FLOAT_EQ(cast.remainingTime, 0.0f);
}

TEST(SpellCastTest, InterruptIdleIsNoop) {
    SpellCast cast;
    cast.Interrupt();
    EXPECT_EQ(cast.state, CastState::Idle);
}

TEST(SpellCastTest, Reset) {
    SpellCast cast;
    cast.Begin(10, Entity(1, 0), 3.0f);
    cast.Reset();

    EXPECT_EQ(cast.state, CastState::Idle);
    EXPECT_EQ(cast.spellId, 0u);
    EXPECT_FALSE(cast.target.isValid());
}

TEST(SpellCastTest, StateTransitionCastingToComplete) {
    ComponentStorage<SpellCast> casts;
    ComponentStorage<AuraHolder> auras;
    ComponentStorage<DamageEvent> damages;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threats;

    Entity e(0, 0);
    auto& cast = casts.Add(e);
    cast.Begin(100, Entity(1, 0), 1.0f);

    CombatSystem system(casts, auras, damages, stats, threats);

    // Tick 0.5s — still casting.
    system.Execute(0.5f);
    EXPECT_EQ(casts.Get(e).state, CastState::Casting);
    EXPECT_NEAR(casts.Get(e).remainingTime, 0.5f, 1e-5f);

    // Tick another 0.6s — should complete.
    system.Execute(0.6f);
    EXPECT_EQ(casts.Get(e).state, CastState::Complete);
    EXPECT_FLOAT_EQ(casts.Get(e).remainingTime, 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// AuraHolder component tests (SRS-GML-002.2)
// ═══════════════════════════════════════════════════════════════════════════

TEST(AuraHolderTest, AddNewAura) {
    AuraHolder holder;
    AuraInstance aura;
    aura.auraId = 1;
    aura.caster = Entity(0, 0);
    aura.stacks = 1;
    aura.duration = 10.0f;
    aura.remainingTime = 10.0f;

    holder.AddOrStack(aura);
    EXPECT_EQ(holder.auras.size(), 1u);
    EXPECT_TRUE(holder.HasAura(1));
    EXPECT_EQ(holder.GetStacks(1), 1);
}

TEST(AuraHolderTest, StackExistingAura) {
    AuraHolder holder;
    Entity caster(0, 0);

    AuraInstance aura;
    aura.auraId = 1;
    aura.caster = caster;
    aura.stacks = 1;
    aura.duration = 10.0f;
    aura.remainingTime = 10.0f;

    holder.AddOrStack(aura);
    holder.AddOrStack(aura);  // Stack again.

    EXPECT_EQ(holder.auras.size(), 1u);
    EXPECT_EQ(holder.GetStacks(1), 2);
}

TEST(AuraHolderTest, StackClampsToMax) {
    AuraHolder holder;
    Entity caster(0, 0);

    AuraInstance aura;
    aura.auraId = 1;
    aura.caster = caster;
    aura.stacks = kMaxAuraStacks;
    aura.duration = 10.0f;
    aura.remainingTime = 10.0f;

    holder.AddOrStack(aura);
    holder.AddOrStack(aura);

    EXPECT_EQ(holder.GetStacks(1), kMaxAuraStacks);
}

TEST(AuraHolderTest, DifferentCastersDoNotStack) {
    AuraHolder holder;

    AuraInstance aura1;
    aura1.auraId = 1;
    aura1.caster = Entity(0, 0);
    aura1.stacks = 1;
    aura1.duration = 10.0f;
    aura1.remainingTime = 10.0f;

    AuraInstance aura2 = aura1;
    aura2.caster = Entity(1, 0);  // Different caster.

    holder.AddOrStack(aura1);
    holder.AddOrStack(aura2);

    EXPECT_EQ(holder.auras.size(), 2u);
    EXPECT_EQ(holder.GetStacks(1), 2);  // Total stacks from both.
}

TEST(AuraHolderTest, RemoveById) {
    AuraHolder holder;

    AuraInstance aura;
    aura.auraId = 5;
    aura.caster = Entity(0, 0);
    aura.duration = 10.0f;
    aura.remainingTime = 10.0f;
    holder.AddOrStack(aura);

    aura.auraId = 10;
    holder.AddOrStack(aura);

    holder.RemoveById(5);
    EXPECT_FALSE(holder.HasAura(5));
    EXPECT_TRUE(holder.HasAura(10));
}

TEST(AuraHolderTest, RemoveExpired) {
    AuraHolder holder;

    AuraInstance live;
    live.auraId = 1;
    live.caster = Entity(0, 0);
    live.duration = 10.0f;
    live.remainingTime = 5.0f;
    holder.AddOrStack(live);

    AuraInstance expired;
    expired.auraId = 2;
    expired.caster = Entity(0, 0);
    expired.duration = 10.0f;
    expired.remainingTime = 0.0f;
    holder.AddOrStack(expired);

    holder.RemoveExpired();
    EXPECT_TRUE(holder.HasAura(1));
    EXPECT_FALSE(holder.HasAura(2));
}

TEST(AuraHolderTest, DurationTickDown) {
    ComponentStorage<SpellCast> casts;
    ComponentStorage<AuraHolder> auraStorage;
    ComponentStorage<DamageEvent> damages;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threats;

    Entity e(0, 0);
    auto& holder = auraStorage.Add(e);

    AuraInstance aura;
    aura.auraId = 1;
    aura.caster = Entity(1, 0);
    aura.duration = 5.0f;
    aura.remainingTime = 5.0f;
    holder.AddOrStack(aura);

    CombatSystem system(casts, auraStorage, damages, stats, threats);

    system.Execute(2.0f);
    EXPECT_NEAR(auraStorage.Get(e).auras[0].remainingTime, 3.0f, 1e-5f);

    // Tick enough to expire.
    system.Execute(4.0f);
    EXPECT_TRUE(auraStorage.Get(e).auras.empty());
}

TEST(AuraHolderTest, PeriodicTickDamage) {
    ComponentStorage<SpellCast> casts;
    ComponentStorage<AuraHolder> auraStorage;
    ComponentStorage<DamageEvent> damages;
    ComponentStorage<Stats> statsStorage;
    ComponentStorage<ThreatList> threats;

    Entity e(0, 0);

    // Set up stats with 100 HP.
    auto& entityStats = statsStorage.Add(e, Stats{100, 100, 0, 0, {}});
    (void)entityStats;

    // Apply a DoT aura: 10 damage per tick, 1s interval, 5s duration.
    auto& holder = auraStorage.Add(e);
    AuraInstance dot;
    dot.auraId = 99;
    dot.caster = Entity(1, 0);
    dot.stacks = 1;
    dot.duration = 5.0f;
    dot.remainingTime = 5.0f;
    dot.tickInterval = 1.0f;
    dot.tickTimer = 1.0f;
    dot.tickDamage = 10;
    holder.AddOrStack(dot);

    CombatSystem system(casts, auraStorage, damages, statsStorage, threats);

    // Tick 1 second — one tick of 10 damage.
    system.Execute(1.0f);
    EXPECT_EQ(statsStorage.Get(e).health, 90);

    // Tick another second — another tick.
    system.Execute(1.0f);
    EXPECT_EQ(statsStorage.Get(e).health, 80);
}

// ═══════════════════════════════════════════════════════════════════════════
// Damage calculation tests (SRS-GML-002.3, SRS-GML-002.5)
// ═══════════════════════════════════════════════════════════════════════════

TEST(DamageCalcTest, NoDamageForZeroBase) {
    DamageCalcParams params;
    EXPECT_EQ(CombatSystem::CalculateDamage(0, DamageType::Physical, false, params), 0);
}

TEST(DamageCalcTest, NoDamageForNegativeBase) {
    DamageCalcParams params;
    EXPECT_EQ(CombatSystem::CalculateDamage(-10, DamageType::Physical, false, params), 0);
}

TEST(DamageCalcTest, UnmitigatedPhysicalDamage) {
    DamageCalcParams params;  // 0 armor.
    int32_t damage = CombatSystem::CalculateDamage(100, DamageType::Physical, false, params);
    EXPECT_EQ(damage, 100);
}

TEST(DamageCalcTest, CriticalStrikeDoublesDamage) {
    DamageCalcParams params;
    int32_t normal = CombatSystem::CalculateDamage(100, DamageType::Physical, false, params);
    int32_t crit = CombatSystem::CalculateDamage(100, DamageType::Physical, true, params);
    EXPECT_EQ(crit, normal * 2);
}

TEST(DamageCalcTest, ArmorMitigation) {
    DamageCalcParams params;
    params.armor = 400;  // 400/(400+400) = 50% mitigation.

    int32_t damage = CombatSystem::CalculateDamage(100, DamageType::Physical, false, params);
    EXPECT_EQ(damage, 50);
}

TEST(DamageCalcTest, HighArmorDoesNotReduceToZero) {
    DamageCalcParams params;
    params.armor = 10000;  // Very high armor.

    int32_t damage = CombatSystem::CalculateDamage(10, DamageType::Physical, false, params);
    EXPECT_GE(damage, 1);  // Minimum 1 damage.
}

TEST(DamageCalcTest, MagicResistance) {
    DamageCalcParams params;
    // Fire resistance at index 2 (DamageType::Fire).
    params.resistances[static_cast<std::size_t>(DamageType::Fire)] = 200;
    // 200/(200+200) = 50% mitigation.

    int32_t damage = CombatSystem::CalculateDamage(100, DamageType::Fire, false, params);
    EXPECT_EQ(damage, 50);
}

TEST(DamageCalcTest, DifferentDamageTypesUseDifferentResistances) {
    DamageCalcParams params;
    params.resistances[static_cast<std::size_t>(DamageType::Frost)] = 200;  // 50%
    params.resistances[static_cast<std::size_t>(DamageType::Shadow)] = 0;   // 0%

    int32_t frostDamage = CombatSystem::CalculateDamage(100, DamageType::Frost, false, params);
    int32_t shadowDamage = CombatSystem::CalculateDamage(100, DamageType::Shadow, false, params);

    EXPECT_EQ(frostDamage, 50);
    EXPECT_EQ(shadowDamage, 100);
}

TEST(DamageCalcTest, CritWithArmor) {
    DamageCalcParams params;
    params.armor = 400;  // 50% mitigation.

    // 100 * 2 (crit) * 0.5 (armor) = 100.
    int32_t damage = CombatSystem::CalculateDamage(100, DamageType::Physical, true, params);
    EXPECT_EQ(damage, 100);
}

// ═══════════════════════════════════════════════════════════════════════════
// DamageEvent processing tests (SRS-GML-002.4)
// ═══════════════════════════════════════════════════════════════════════════

class CombatSystemTest : public ::testing::Test {
protected:
    ComponentStorage<SpellCast> spellCasts;
    ComponentStorage<AuraHolder> auraHolders;
    ComponentStorage<DamageEvent> damageEvents;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threatLists;

    Entity attacker{0, 0};
    Entity victim{1, 0};

    void SetUp() override {
        // Set up victim with 1000 HP and some armor.
        Stats victimStats;
        victimStats.maxHealth = 1000;
        victimStats.health = 1000;
        victimStats.attributes[0] = 100;  // armor
        stats.Add(victim, std::move(victimStats));

        // Give victim a threat list.
        threatLists.Add(victim);
    }
};

TEST_F(CombatSystemTest, ProcessesDamageEvent) {
    // Create damage event entity.
    Entity dmgEntity(2, 0);
    DamageEvent event;
    event.attacker = attacker;
    event.victim = victim;
    event.type = DamageType::Physical;
    event.baseDamage = 100;
    damageEvents.Add(dmgEntity, std::move(event));

    CombatSystem system(spellCasts, auraHolders, damageEvents, stats, threatLists);
    system.Execute(0.016f);

    // Check final damage was computed.
    const auto& processed = damageEvents.Get(dmgEntity);
    EXPECT_TRUE(processed.isProcessed);
    EXPECT_GT(processed.finalDamage, 0);
    EXPECT_LT(processed.finalDamage, 100);  // Reduced by armor.

    // Check victim's health decreased.
    EXPECT_LT(stats.Get(victim).health, 1000);
}

TEST_F(CombatSystemTest, DamageGeneratesThreat) {
    Entity dmgEntity(2, 0);
    DamageEvent event;
    event.attacker = attacker;
    event.victim = victim;
    event.type = DamageType::Physical;
    event.baseDamage = 100;
    damageEvents.Add(dmgEntity, std::move(event));

    CombatSystem system(spellCasts, auraHolders, damageEvents, stats, threatLists);
    system.Execute(0.016f);

    // Verify threat was generated.
    const auto& threats = threatLists.Get(victim);
    EXPECT_GT(threats.GetThreat(attacker), 0.0f);
    EXPECT_EQ(threats.GetTopThreat(), attacker);
}

TEST_F(CombatSystemTest, MagicDamageUsesResistance) {
    // Add fire resistance.
    auto& victimStats = stats.Get(victim);
    victimStats.attributes[static_cast<std::size_t>(DamageType::Fire) + 1] = 200;

    Entity dmgEntity(2, 0);
    DamageEvent event;
    event.attacker = attacker;
    event.victim = victim;
    event.type = DamageType::Fire;
    event.baseDamage = 100;
    damageEvents.Add(dmgEntity, std::move(event));

    CombatSystem system(spellCasts, auraHolders, damageEvents, stats, threatLists);
    system.Execute(0.016f);

    // 200/(200+200) = 50% mitigation → 50 damage.
    EXPECT_EQ(damageEvents.Get(dmgEntity).finalDamage, 50);
}

TEST_F(CombatSystemTest, CriticalHit) {
    Entity dmgEntity(2, 0);
    DamageEvent event;
    event.attacker = attacker;
    event.victim = victim;
    event.type = DamageType::Physical;
    event.baseDamage = 100;
    event.isCritical = true;
    damageEvents.Add(dmgEntity, std::move(event));

    CombatSystem system(spellCasts, auraHolders, damageEvents, stats, threatLists);
    system.Execute(0.016f);

    const auto& processed = damageEvents.Get(dmgEntity);
    // Crit + armor: 100*2 * (1 - 100/500) = 200 * 0.8 = 160.
    EXPECT_EQ(processed.finalDamage, 160);
}

TEST_F(CombatSystemTest, HealthDoesNotGoBelowZero) {
    Entity dmgEntity(2, 0);
    DamageEvent event;
    event.attacker = attacker;
    event.victim = victim;
    event.type = DamageType::Physical;
    event.baseDamage = 50000;  // Massive damage.
    damageEvents.Add(dmgEntity, std::move(event));

    CombatSystem system(spellCasts, auraHolders, damageEvents, stats, threatLists);
    system.Execute(0.016f);

    EXPECT_EQ(stats.Get(victim).health, 0);  // Clamped by Stats::SetHealth.
}

// ═══════════════════════════════════════════════════════════════════════════
// ThreatList tests
// ═══════════════════════════════════════════════════════════════════════════

TEST(ThreatListTest, EmptyListReturnsInvalid) {
    ThreatList threats;
    EXPECT_FALSE(threats.GetTopThreat().isValid());
}

TEST(ThreatListTest, AddThreatAndGetTop) {
    ThreatList threats;
    Entity a(0, 0);
    Entity b(1, 0);

    threats.AddThreat(a, 100.0f);
    threats.AddThreat(b, 200.0f);

    EXPECT_EQ(threats.GetTopThreat(), b);
    EXPECT_FLOAT_EQ(threats.GetThreat(a), 100.0f);
    EXPECT_FLOAT_EQ(threats.GetThreat(b), 200.0f);
}

TEST(ThreatListTest, AccumulateThreat) {
    ThreatList threats;
    Entity a(0, 0);

    threats.AddThreat(a, 50.0f);
    threats.AddThreat(a, 75.0f);

    EXPECT_FLOAT_EQ(threats.GetThreat(a), 125.0f);
}

TEST(ThreatListTest, RemoveSource) {
    ThreatList threats;
    Entity a(0, 0);
    Entity b(1, 0);

    threats.AddThreat(a, 100.0f);
    threats.AddThreat(b, 200.0f);
    threats.Remove(b);

    EXPECT_EQ(threats.GetTopThreat(), a);
    EXPECT_FLOAT_EQ(threats.GetThreat(b), 0.0f);
}

TEST(ThreatListTest, Clear) {
    ThreatList threats;
    threats.AddThreat(Entity(0, 0), 100.0f);
    threats.Clear();
    EXPECT_TRUE(threats.entries.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Integration: full combat scenario
// ═══════════════════════════════════════════════════════════════════════════

TEST(CombatIntegration, FullCombatScenario) {
    // Set up all storages.
    ComponentStorage<SpellCast> spellCasts;
    ComponentStorage<AuraHolder> auraHolders;
    ComponentStorage<DamageEvent> damageEvents;
    ComponentStorage<Stats> statsStorage;
    ComponentStorage<ThreatList> threatLists;

    EntityManager entityManager;
    entityManager.RegisterStorage(&spellCasts);
    entityManager.RegisterStorage(&auraHolders);
    entityManager.RegisterStorage(&damageEvents);
    entityManager.RegisterStorage(&statsStorage);
    entityManager.RegisterStorage(&threatLists);

    // Create a player.
    Entity player = entityManager.Create();
    Stats playerStats;
    playerStats.maxHealth = 500;
    playerStats.health = 500;
    playerStats.maxMana = 200;
    playerStats.mana = 200;
    statsStorage.Add(player, std::move(playerStats));
    spellCasts.Add(player);

    // Create a monster.
    Entity monster = entityManager.Create();
    Stats monsterStats;
    monsterStats.maxHealth = 1000;
    monsterStats.health = 1000;
    monsterStats.attributes[0] = 200;  // armor
    statsStorage.Add(monster, std::move(monsterStats));
    threatLists.Add(monster);
    auraHolders.Add(monster);

    CombatSystem system(spellCasts, auraHolders, damageEvents, statsStorage, threatLists);

    // Phase 1: Player begins casting a spell.
    auto& playerCast = spellCasts.Get(player);
    playerCast.Begin(/*spellId=*/1, monster, /*castTime=*/2.0f);

    // Tick 1.5 seconds — still casting.
    system.Execute(1.5f);
    EXPECT_EQ(spellCasts.Get(player).state, CastState::Casting);

    // Tick another 0.6 seconds — cast completes.
    system.Execute(0.6f);
    EXPECT_EQ(spellCasts.Get(player).state, CastState::Complete);

    // Phase 2: Spell hit — create damage event.
    Entity dmgEntity = entityManager.Create();
    DamageEvent event;
    event.attacker = player;
    event.victim = monster;
    event.type = DamageType::Physical;
    event.baseDamage = 150;
    damageEvents.Add(dmgEntity, std::move(event));

    // Phase 3: Apply a DoT aura to the monster.
    auto& monsterAuras = auraHolders.Get(monster);
    AuraInstance dot;
    dot.auraId = 42;
    dot.caster = player;
    dot.stacks = 1;
    dot.duration = 3.0f;
    dot.remainingTime = 3.0f;
    dot.tickInterval = 1.0f;
    dot.tickTimer = 1.0f;
    dot.tickDamage = 20;
    monsterAuras.AddOrStack(dot);

    // Phase 4: Process damage + start ticking auras.
    system.Execute(0.016f);

    // Verify damage event was processed.
    EXPECT_TRUE(damageEvents.Get(dmgEntity).isProcessed);
    EXPECT_GT(damageEvents.Get(dmgEntity).finalDamage, 0);

    // Monster health should have decreased.
    int32_t healthAfterHit = statsStorage.Get(monster).health;
    EXPECT_LT(healthAfterHit, 1000);

    // Threat should have been generated.
    EXPECT_GT(threatLists.Get(monster).GetThreat(player), 0.0f);

    // Phase 5: Tick auras for a few seconds.
    system.Execute(1.0f);  // 1 DoT tick.
    int32_t healthAfterDot1 = statsStorage.Get(monster).health;
    EXPECT_LT(healthAfterDot1, healthAfterHit);
    EXPECT_EQ(healthAfterHit - healthAfterDot1, 20);  // 20 damage per tick.

    system.Execute(1.0f);  // Another DoT tick.
    int32_t healthAfterDot2 = statsStorage.Get(monster).health;
    EXPECT_EQ(healthAfterDot1 - healthAfterDot2, 20);

    // Phase 6: DoT expires after 3 total seconds.
    system.Execute(1.5f);
    EXPECT_TRUE(auraHolders.Get(monster).auras.empty());

    // Clean up.
    entityManager.Destroy(player);
    entityManager.Destroy(monster);
    entityManager.Destroy(dmgEntity);
}

TEST(CombatIntegration, SchedulerRegistration) {
    ComponentStorage<SpellCast> casts;
    ComponentStorage<AuraHolder> auras;
    ComponentStorage<DamageEvent> damages;
    ComponentStorage<Stats> stats;
    ComponentStorage<ThreatList> threats;

    SystemScheduler scheduler;
    auto& system = scheduler.Register<CombatSystem>(casts, auras, damages, stats, threats);

    EXPECT_EQ(system.GetName(), "CombatSystem");
    EXPECT_EQ(scheduler.SystemCount(), 1u);
    EXPECT_TRUE(scheduler.Build());

    // Execute with no entities (should not crash).
    scheduler.Execute(1.0f / 60.0f);
}
