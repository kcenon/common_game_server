// examples/02_foundation_adapters.cpp
//
// Tutorial: Foundation adapters and ServiceLocator
// See: docs/tutorial_foundation_adapters.dox
//
// This example demonstrates how to:
//   1. Use the built-in GameLogger adapter directly
//   2. Inject custom context into log entries via LogContext
//   3. Control per-category verbosity at runtime
//   4. Register a service (including a custom interface + in-memory
//      fake implementation) into a ServiceLocator
//   5. Retrieve services by interface type and use them polymorphically
//
// The combination of GameLogger + ServiceLocator is how every service,
// plugin, and game system in common_game_server obtains its
// collaborators without hardcoding concrete classes.

#include "cgs/foundation/game_logger.hpp"
#include "cgs/foundation/service_locator.hpp"
#include "cgs/foundation/types.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using cgs::foundation::GameLogger;
using cgs::foundation::LogCategory;
using cgs::foundation::LogContext;
using cgs::foundation::LogLevel;
using cgs::foundation::PlayerId;
using cgs::foundation::ServiceLocator;

// ─── A custom service interface ────────────────────────────────────────────
//
// Any abstract class can be registered in the ServiceLocator. The
// important thing is that consumers depend on the INTERFACE, not the
// concrete implementation. This keeps game logic unit-testable.

class IAchievementTracker {
public:
    virtual ~IAchievementTracker() = default;

    /// Record that a player completed an achievement.
    virtual void Unlock(PlayerId player, const std::string& achievementId) = 0;

    /// Return the number of achievements a player has earned.
    [[nodiscard]] virtual std::size_t Count(PlayerId player) const = 0;
};

// ─── In-memory fake implementation for tests ──────────────────────────────
//
// Real production code would persist to the database adapter. Here we
// keep a simple in-memory map so the example stays self-contained and
// runs without any I/O.

class InMemoryAchievementTracker final : public IAchievementTracker {
public:
    void Unlock(PlayerId player, const std::string& achievementId) override {
        entries_.push_back({player, achievementId});
    }

    [[nodiscard]] std::size_t Count(PlayerId player) const override {
        std::size_t n = 0;
        for (const auto& entry : entries_) {
            if (entry.player == player) {
                ++n;
            }
        }
        return n;
    }

private:
    struct Entry {
        PlayerId player;
        std::string achievementId;
    };
    std::vector<Entry> entries_;
};

// ─── Service consumer ──────────────────────────────────────────────────────
//
// Game logic that needs both a logger and an achievement tracker pulls
// them from the ServiceLocator. Notice that this function depends only
// on the interfaces — it does not know which concrete classes are
// wired up.

void GrantVictoryAchievement(ServiceLocator& services, PlayerId player) {
    auto* logger = services.get<GameLogger>();
    auto* tracker = services.get<IAchievementTracker>();

    if (logger == nullptr || tracker == nullptr) {
        std::cerr << "missing required service\n";
        return;
    }

    tracker->Unlock(player, "first_victory");

    LogContext ctx;
    ctx.playerId = player;
    ctx.extra["achievement"] = "first_victory";
    ctx.extra["total"] = std::to_string(tracker->Count(player));
    logger->logWithContext(LogLevel::Info, LogCategory::Core,
                           "Achievement unlocked", ctx);
}

// ─── main ──────────────────────────────────────────────────────────────────

int main() {
    // Step 1 — Construct a ServiceLocator. Normally the application
    // startup code does this once and hands the locator to every
    // subsystem that needs services.
    ServiceLocator services;

    // Step 2 — Register a GameLogger. GameLogger has a singleton
    // interface (GameLogger::instance()), but for testability we wrap
    // it in a unique_ptr and store it in the locator. This makes it
    // trivial to swap for a fake in tests.
    services.add<GameLogger>(std::make_unique<GameLogger>());

    // Step 3 — Tune per-category verbosity. The Core category defaults
    // to Info. Dial ECS up to Trace for deep debugging; dial Network
    // down to Warning to silence heartbeat noise.
    if (auto* logger = services.get<GameLogger>()) {
        logger->setCategoryLevel(LogCategory::ECS, LogLevel::Trace);
        logger->setCategoryLevel(LogCategory::Network, LogLevel::Warning);
    }

    // Step 4 — Register a custom service. The locator stores it as a
    // shared_ptr<IAchievementTracker> internally, so it outlives
    // every consumer that holds a raw pointer via get<T>().
    services.add<IAchievementTracker>(std::make_unique<InMemoryAchievementTracker>());

    // Step 5 — Invoke game logic that uses the services.
    const PlayerId hero{42};
    GrantVictoryAchievement(services, hero);

    // Step 6 — Verify the tracker observed the unlock.
    auto* tracker = services.get<IAchievementTracker>();
    std::cout << "player " << hero.value() << " has "
              << (tracker ? tracker->Count(hero) : 0)
              << " achievement(s)\n";

    // Step 7 — Demonstrate querying before registration.
    class IMatchmaker {
    public:
        virtual ~IMatchmaker() = default;
    };
    if (services.has<IMatchmaker>()) {
        std::cout << "matchmaker registered\n";
    } else {
        std::cout << "matchmaker not registered (expected)\n";
    }

    // Step 8 — Clean up explicitly. clear() removes all services;
    // destruction order is deterministic (reverse of registration).
    services.clear();

    return EXIT_SUCCESS;
}
