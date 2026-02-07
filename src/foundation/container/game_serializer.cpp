/// @file game_serializer.cpp
/// @brief Non-template parts of GameSerializer (SDS-MOD-007).
///
/// Contains constructor, destructor, move operations, and singleton.
/// Template methods are defined in the header. When container_system
/// becomes available, this file gains the backend integration.

#include "cgs/foundation/game_serializer.hpp"

namespace cgs::foundation {

// ── Impl (placeholder for future container_system state) ────────────────────

struct GameSerializer::Impl {};

// ── Construction / destruction / move ───────────────────────────────────────

GameSerializer::GameSerializer() : impl_(std::make_unique<Impl>()) {}

GameSerializer::~GameSerializer() = default;

GameSerializer::GameSerializer(GameSerializer&&) noexcept = default;

GameSerializer& GameSerializer::operator=(GameSerializer&&) noexcept = default;

// ── Singleton ───────────────────────────────────────────────────────────────

GameSerializer& GameSerializer::instance() {
    static GameSerializer inst;
    return inst;
}

}  // namespace cgs::foundation
