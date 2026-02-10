#pragma once

/// @file combat_components.hpp
/// @brief Combat ECS components: SpellCast, AuraHolder, DamageEvent, ThreatList.
///
/// Each struct is a plain data component for sparse-set storage via
/// ComponentStorage<T>.  The CombatSystem operates on these components
/// to implement the full combat loop.
///
/// @see SRS-GML-002.1 .. SRS-GML-002.5
/// @see SDS-MOD-021

#include <algorithm>
#include <cstdint>
#include <vector>

#include "cgs/ecs/entity.hpp"
#include "cgs/game/combat_types.hpp"

namespace cgs::game {

// ── SpellCast (SRS-GML-002.1) ───────────────────────────────────────────

/// Active spell cast state for an entity.
///
/// Tracks the spell being cast, the target, and the remaining cast time.
/// The CombatSystem drives the state machine each tick.
struct SpellCast {
    uint32_t spellId = 0;
    cgs::ecs::Entity target;
    CastState state = CastState::Idle;
    float castTime = 0.0f;        ///< Total cast duration (seconds).
    float remainingTime = 0.0f;   ///< Time left until cast completes.

    /// Begin casting a spell.
    void Begin(uint32_t spell, cgs::ecs::Entity tgt, float duration) noexcept {
        spellId = spell;
        target = tgt;
        castTime = duration;
        remainingTime = duration;
        state = CastState::Casting;
    }

    /// Interrupt the current cast.
    void Interrupt() noexcept {
        if (state == CastState::Casting || state == CastState::Channeling) {
            state = CastState::Interrupted;
            remainingTime = 0.0f;
        }
    }

    /// Reset to idle state.
    void Reset() noexcept {
        spellId = 0;
        target = cgs::ecs::Entity::invalid();
        state = CastState::Idle;
        castTime = 0.0f;
        remainingTime = 0.0f;
    }
};

// ── AuraHolder (SRS-GML-002.2) ──────────────────────────────────────────

/// A single active aura (buff or debuff) on an entity.
struct AuraInstance {
    uint32_t auraId = 0;
    cgs::ecs::Entity caster;
    int32_t stacks = 1;
    float duration = 0.0f;        ///< Total duration (seconds).
    float remainingTime = 0.0f;   ///< Time until expiry.
    float tickInterval = 0.0f;    ///< Periodic tick interval (0 = no tick).
    float tickTimer = 0.0f;       ///< Time until next tick.
    int32_t tickDamage = 0;       ///< Damage/heal per tick (negative = heal).
    DamageType tickDamageType = DamageType::Magic;
};

/// Collection of active auras on an entity.
///
/// Provides helpers for adding, removing, and stacking auras.
struct AuraHolder {
    std::vector<AuraInstance> auras;

    /// Add a new aura or stack onto an existing one (same auraId + caster).
    ///
    /// @return Reference to the added/stacked aura instance.
    AuraInstance& AddOrStack(const AuraInstance& aura) {
        // Check for existing aura from same caster.
        for (auto& existing : auras) {
            if (existing.auraId == aura.auraId
                && existing.caster == aura.caster) {
                existing.stacks = std::min(
                    existing.stacks + aura.stacks,
                    kMaxAuraStacks);
                existing.remainingTime = aura.duration;
                existing.duration = aura.duration;
                return existing;
            }
        }
        auras.push_back(aura);
        return auras.back();
    }

    /// Remove all auras with the given ID.
    void RemoveById(uint32_t auraId) {
        std::erase_if(auras, [auraId](const AuraInstance& a) {
            return a.auraId == auraId;
        });
    }

    /// Remove expired auras (remainingTime <= 0).
    void RemoveExpired() {
        std::erase_if(auras, [](const AuraInstance& a) {
            return a.remainingTime <= 0.0f;
        });
    }

    /// Check if the entity has an aura with the given ID.
    [[nodiscard]] bool HasAura(uint32_t auraId) const {
        return std::any_of(auras.begin(), auras.end(),
                           [auraId](const AuraInstance& a) {
                               return a.auraId == auraId;
                           });
    }

    /// Get total stack count for a specific aura ID.
    [[nodiscard]] int32_t GetStacks(uint32_t auraId) const {
        int32_t total = 0;
        for (const auto& a : auras) {
            if (a.auraId == auraId) {
                total += a.stacks;
            }
        }
        return total;
    }
};

// ── DamageEvent (SRS-GML-002.3, SRS-GML-002.5) ─────────────────────────

/// A pending damage event to be processed by the CombatSystem.
///
/// The CombatSystem reads DamageEvent components, applies the damage
/// pipeline (base → modifiers → mitigation → final), updates Stats,
/// and then removes the event.
struct DamageEvent {
    cgs::ecs::Entity attacker;
    cgs::ecs::Entity victim;
    DamageType type = DamageType::Physical;
    int32_t baseDamage = 0;
    int32_t finalDamage = 0;     ///< Computed by the damage pipeline.
    bool isCritical = false;
    bool isProcessed = false;    ///< Set to true after CombatSystem handles it.
};

// ── ThreatList (SRS-GML-002 — Threat/Aggro) ────────────────────────────

/// A single threat entry linking an attacker to a threat amount.
struct ThreatEntry {
    cgs::ecs::Entity source;
    float threat = 0.0f;
};

/// Ordered list of threat sources for AI targeting.
///
/// The entity with the highest threat is the current target.
struct ThreatList {
    std::vector<ThreatEntry> entries;

    /// Add or increase threat from a source.
    void AddThreat(cgs::ecs::Entity source, float amount) {
        for (auto& e : entries) {
            if (e.source == source) {
                e.threat += amount;
                sortDescending();
                return;
            }
        }
        entries.push_back({source, amount});
        sortDescending();
    }

    /// Remove a source from the threat list.
    void Remove(cgs::ecs::Entity source) {
        std::erase_if(entries, [source](const ThreatEntry& e) {
            return e.source == source;
        });
    }

    /// Get the entity with the highest threat (top aggro).
    [[nodiscard]] cgs::ecs::Entity GetTopThreat() const {
        if (entries.empty()) {
            return cgs::ecs::Entity::invalid();
        }
        return entries.front().source;
    }

    /// Get total threat from a specific source.
    [[nodiscard]] float GetThreat(cgs::ecs::Entity source) const {
        for (const auto& e : entries) {
            if (e.source == source) {
                return e.threat;
            }
        }
        return 0.0f;
    }

    /// Clear all threat entries.
    void Clear() { entries.clear(); }

private:
    void sortDescending() {
        std::sort(entries.begin(), entries.end(),
                  [](const ThreatEntry& a, const ThreatEntry& b) {
                      return a.threat > b.threat;
                  });
    }
};

} // namespace cgs::game
