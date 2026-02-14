/// @file combat_system.cpp
/// @brief CombatSystem implementation.
///
/// Implements the three-phase combat tick:
///   1. Spell cast timer updates
///   2. Aura duration/periodic tick processing
///   3. Damage event pipeline (base → crit → mitigation → final)
///
/// @see SRS-GML-002.4
/// @see SDS-MOD-021

#include "cgs/game/combat_system.hpp"

#include <algorithm>
#include <cmath>

namespace cgs::game {

CombatSystem::CombatSystem(cgs::ecs::ComponentStorage<SpellCast>& spellCasts,
                           cgs::ecs::ComponentStorage<AuraHolder>& auraHolders,
                           cgs::ecs::ComponentStorage<DamageEvent>& damageEvents,
                           cgs::ecs::ComponentStorage<Stats>& stats,
                           cgs::ecs::ComponentStorage<ThreatList>& threatLists)
    : spellCasts_(spellCasts),
      auraHolders_(auraHolders),
      damageEvents_(damageEvents),
      stats_(stats),
      threatLists_(threatLists) {}

void CombatSystem::Execute(float deltaTime) {
    updateSpellCasts(deltaTime);
    updateAuras(deltaTime);
    processDamageEvents();
}

cgs::ecs::SystemAccessInfo CombatSystem::GetAccessInfo() const {
    cgs::ecs::SystemAccessInfo info;
    cgs::ecs::Write<SpellCast, AuraHolder, DamageEvent, Stats, ThreatList>::Apply(info);
    return info;
}

// ── Static damage calculation ───────────────────────────────────────────

int32_t CombatSystem::CalculateDamage(int32_t baseDamage,
                                      DamageType type,
                                      bool isCritical,
                                      const DamageCalcParams& params) {
    if (baseDamage <= 0) {
        return 0;
    }

    auto damage = static_cast<float>(baseDamage);

    // Apply critical strike multiplier.
    if (isCritical) {
        damage *= DamageCalcParams::kCritMultiplier;
    }

    // Apply mitigation based on damage type.
    float mitigation = 0.0f;
    if (type == DamageType::Physical) {
        // Armor-based mitigation.
        float armor = static_cast<float>(std::max(params.armor, 0));
        mitigation = armor / (armor + DamageCalcParams::kArmorConstant);
    } else {
        // Resistance-based mitigation.
        auto typeIndex = static_cast<std::size_t>(type);
        int32_t resistance = 0;
        if (typeIndex < params.resistances.size()) {
            resistance = std::max(params.resistances[typeIndex], 0);
        }
        float res = static_cast<float>(resistance);
        mitigation = res / (res + DamageCalcParams::kResistanceConstant);
    }

    damage *= (1.0f - mitigation);

    // Floor to at least 1 damage if base was positive.
    return std::max(static_cast<int32_t>(std::floor(damage)), 1);
}

// ── Spell cast updates ──────────────────────────────────────────────────

void CombatSystem::updateSpellCasts(float deltaTime) {
    for (std::size_t i = 0; i < spellCasts_.Size(); ++i) {
        auto entityId = spellCasts_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& cast = spellCasts_.Get(entity);

        if (cast.state == CastState::Casting || cast.state == CastState::Channeling) {
            cast.remainingTime -= deltaTime;
            if (cast.remainingTime <= 0.0f) {
                cast.remainingTime = 0.0f;
                cast.state = CastState::Complete;
            }
        }
    }
}

// ── Aura tick processing ────────────────────────────────────────────────

void CombatSystem::updateAuras(float deltaTime) {
    for (std::size_t i = 0; i < auraHolders_.Size(); ++i) {
        auto entityId = auraHolders_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& holder = auraHolders_.Get(entity);

        for (auto& aura : holder.auras) {
            aura.remainingTime -= deltaTime;

            // Process periodic ticks.
            if (aura.tickInterval > 0.0f) {
                aura.tickTimer -= deltaTime;
                while (aura.tickTimer <= 0.0f && aura.remainingTime > -aura.tickInterval) {
                    // Apply tick damage/heal to entity stats.
                    if (stats_.Has(entity) && aura.tickDamage != 0) {
                        auto& entityStats = stats_.Get(entity);
                        int32_t effectiveDamage = aura.tickDamage * aura.stacks;
                        entityStats.SetHealth(entityStats.health - effectiveDamage);
                    }
                    aura.tickTimer += aura.tickInterval;
                }
            }
        }

        // Remove expired auras.
        holder.RemoveExpired();
    }
}

// ── Damage event processing ─────────────────────────────────────────────

void CombatSystem::processDamageEvents() {
    for (std::size_t i = 0; i < damageEvents_.Size(); ++i) {
        auto entityId = damageEvents_.EntityAt(i);
        cgs::ecs::Entity entity(entityId, 0);
        auto& event = damageEvents_.Get(entity);

        if (event.isProcessed) {
            continue;
        }

        // Build mitigation parameters from victim's stats.
        DamageCalcParams params;
        if (stats_.Has(event.victim)) {
            // Use attribute indices for armor/resistances:
            // attribute[0] = armor, attribute[1..6] = resistances by DamageType.
            auto& victimStats = stats_.Get(event.victim);
            params.armor = victimStats.attributes[0];
            for (std::size_t r = 0; r < kDamageTypeCount && (r + 1) < kMaxAttributes; ++r) {
                params.resistances[r] = victimStats.attributes[r + 1];
            }
        }

        // Calculate final damage.
        event.finalDamage = CalculateDamage(event.baseDamage, event.type, event.isCritical, params);

        // Apply damage to victim's health.
        if (stats_.Has(event.victim)) {
            auto& victimStats = stats_.Get(event.victim);
            victimStats.SetHealth(victimStats.health - event.finalDamage);
        }

        // Add threat to victim's threat list.
        if (threatLists_.Has(event.victim)) {
            auto& threats = threatLists_.Get(event.victim);
            threats.AddThreat(event.attacker, static_cast<float>(event.finalDamage));
        }

        event.isProcessed = true;
    }
}

}  // namespace cgs::game
