#pragma once

/// @file combat_system.hpp
/// @brief CombatSystem: per-tick combat processing.
///
/// Processes spell casts, aura ticks, and damage events each frame.
/// Integrates with Stats component to apply health/mana changes.
///
/// @see SRS-GML-002.4
/// @see SDS-MOD-021

#include <string_view>

#include "cgs/ecs/component_storage.hpp"
#include "cgs/ecs/system_scheduler.hpp"
#include "cgs/game/combat_components.hpp"
#include "cgs/game/components.hpp"

namespace cgs::game {

/// Damage calculation parameters for the mitigation pipeline.
///
/// Armor and resistance values reduce incoming damage.
/// The formula: finalDamage = baseDamage * critMultiplier * (1 - mitigation)
/// where mitigation = armor_or_resistance / (armor_or_resistance + constant).
struct DamageCalcParams {
    int32_t armor = 0;
    /// Per-type magic resistance values.
    /// Index using static_cast<size_t>(DamageType).
    std::array<int32_t, kDamageTypeCount> resistances{};

    /// Armor mitigation constant (higher = less mitigation per armor point).
    static constexpr float kArmorConstant = 400.0f;

    /// Resistance mitigation constant.
    static constexpr float kResistanceConstant = 200.0f;

    /// Critical strike damage multiplier.
    static constexpr float kCritMultiplier = 2.0f;
};

/// System that processes combat logic each tick.
///
/// Execution order within a single tick:
///   1. Update spell cast timers (Casting → Complete)
///   2. Tick auras (duration, periodic effects)
///   3. Process damage events (mitigation pipeline → Stats update → Threat)
class CombatSystem final : public cgs::ecs::ISystem {
public:
    CombatSystem(cgs::ecs::ComponentStorage<SpellCast>& spellCasts,
                 cgs::ecs::ComponentStorage<AuraHolder>& auraHolders,
                 cgs::ecs::ComponentStorage<DamageEvent>& damageEvents,
                 cgs::ecs::ComponentStorage<Stats>& stats,
                 cgs::ecs::ComponentStorage<ThreatList>& threatLists);

    void Execute(float deltaTime) override;

    [[nodiscard]] cgs::ecs::SystemStage GetStage() const override {
        return cgs::ecs::SystemStage::Update;
    }

    [[nodiscard]] std::string_view GetName() const override {
        return "CombatSystem";
    }

    [[nodiscard]] cgs::ecs::SystemAccessInfo GetAccessInfo() const override;

    /// Calculate final damage after mitigation.
    ///
    /// This is a pure function exposed for testability.
    [[nodiscard]] static int32_t CalculateDamage(
        int32_t baseDamage,
        DamageType type,
        bool isCritical,
        const DamageCalcParams& params);

private:
    /// Update spell cast timers.
    void updateSpellCasts(float deltaTime);

    /// Tick aura durations and periodic effects.
    void updateAuras(float deltaTime);

    /// Process pending damage events.
    void processDamageEvents();

    cgs::ecs::ComponentStorage<SpellCast>& spellCasts_;
    cgs::ecs::ComponentStorage<AuraHolder>& auraHolders_;
    cgs::ecs::ComponentStorage<DamageEvent>& damageEvents_;
    cgs::ecs::ComponentStorage<Stats>& stats_;
    cgs::ecs::ComponentStorage<ThreatList>& threatLists_;
};

} // namespace cgs::game
