// examples/09_database.cpp
//
// Tutorial: Database adapter, prepared statements, transactions
// See: docs/tutorial_database.dox
//
// This example focuses on the parts of the GameDatabase API that
// can be exercised WITHOUT a live PostgreSQL server:
//
//   1. Building a PreparedStatement and binding parameters by name
//   2. Inspecting the resolved SQL string (useful for logging)
//   3. Examining a DatabaseConfig before connecting
//   4. Handling the GameResult<T> returned from GameDatabase::connect
//      when the connection fails
//
// Running queries end-to-end requires a real database, so the
// example does NOT call GameDatabase::query or execute — those are
// documented in the tutorial and exercised by the integration test
// suite that spins up a Postgres container.

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_database.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/foundation/game_result.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using cgs::foundation::DatabaseConfig;
using cgs::foundation::DatabaseType;
using cgs::foundation::ErrorCode;
using cgs::foundation::errorSubsystem;
using cgs::foundation::GameDatabase;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;
using cgs::foundation::PreparedStatement;

int main() {
    // ── Step 1: Configure the connection pool (no I/O yet). ─────────────
    DatabaseConfig config;
    config.connectionString = "host=localhost port=5432 dbname=gamedb";
    config.dbType = DatabaseType::PostgreSQL;
    config.minConnections = 2;
    config.maxConnections = 8;
    config.connectionTimeout = std::chrono::seconds(5);

    std::cout << "config: " << config.connectionString << "\n";
    std::cout << "pool: " << config.minConnections << ".."
              << config.maxConnections << " connections\n";

    // ── Step 2: Build a prepared statement offline and bind parameters.
    //
    // PreparedStatement does not need a live connection to bind and
    // resolve — it just tracks the template and the bound values.
    PreparedStatement stmt("SELECT id, name, level FROM players "
                            "WHERE level >= $min_level AND guild_id = $guild "
                            "ORDER BY name LIMIT $limit");
    stmt.bindInt("min_level", 30)
        .bindString("guild", "Knights of Valor")
        .bindInt("limit", 10);

    std::cout << "resolved SQL:\n  " << stmt.resolve() << "\n";

    // ── Step 3: Re-bind with different parameters. ──────────────────────
    stmt.clearBindings();
    stmt.bindInt("min_level", 50)
        .bindString("guild", "Shadow Covenant")
        .bindInt("limit", 5);

    std::cout << "re-resolved SQL:\n  " << stmt.resolve() << "\n";

    // ── Step 4: Attempt to connect and gracefully handle the failure. ──
    //
    // No PostgreSQL is running in the tutorial environment, so the
    // connect call will return an error. The example demonstrates the
    // GameResult<void> error-handling pattern against that error.
    GameDatabase db;
    auto connectResult = db.connect(config);

    if (!connectResult) {
        const auto& err = connectResult.error();
        std::cout << "connect failed [" << errorSubsystem(err.code())
                  << "/" << static_cast<uint32_t>(err.code())
                  << "]: " << err.message() << "\n";
        // Continue — the rest of the example does not need a live DB.
    } else {
        std::cout << "connected; pool size = " << db.poolSize() << "\n";
        // (In production you would run queries here.)
        db.disconnect();
    }

    // ── Step 5: Demonstrate constructing an error "as if" something
    //         failed so the caller can see how to build a GameError
    //         for their own subsystem boundaries.
    const auto simulatedFailure = GameResult<int>::err(
        GameError(ErrorCode::QueryFailed,
                  "syntax error near ',' at position 42"));
    if (!simulatedFailure) {
        std::cout << "simulated query failure: "
                  << errorSubsystem(simulatedFailure.error().code())
                  << "/" << simulatedFailure.error().message() << "\n";
    }

    return EXIT_SUCCESS;
}
