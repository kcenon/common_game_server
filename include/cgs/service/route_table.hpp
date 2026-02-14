#pragma once

/// @file route_table.hpp
/// @brief Opcode-range-based message routing for the Gateway Server.
///
/// Maps incoming message opcodes to downstream service identifiers
/// so the gateway can forward traffic to the correct backend.
///
/// @see SRS-SVC-002.3

#include "cgs/service/gateway_types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cgs::service {

/// Lookup result from a route table query.
struct RouteMatch {
    /// Downstream service identifier.
    std::string service;

    /// Whether this route requires the client to be authenticated.
    bool requiresAuth = true;
};

/// Opcode-range-based routing table.
///
/// Example:
/// @code
///   RouteTable routes;
///   routes.addRoute(0x0100, 0x01FF, "game");
///   routes.addRoute(0x0200, 0x02FF, "lobby");
///
///   auto match = routes.resolve(0x0150);
///   // match->service == "game"
/// @endcode
class RouteTable {
public:
    /// Add a route mapping an opcode range to a service.
    void addRoute(uint16_t opcodeMin,
                  uint16_t opcodeMax,
                  std::string service,
                  bool requiresAuth = true);

    /// Add a route from a RouteEntry.
    void addRoute(RouteEntry entry);

    /// Resolve an opcode to the matching route, if any.
    [[nodiscard]] std::optional<RouteMatch> resolve(uint16_t opcode) const;

    /// Check whether an opcode is a reserved gateway-level opcode (0x0000-0x00FF).
    [[nodiscard]] static bool isGatewayOpcode(uint16_t opcode);

    /// Get all registered routes.
    [[nodiscard]] const std::vector<RouteEntry>& routes() const;

    /// Remove all routes for a specific service.
    void removeRoutesForService(std::string_view service);

    /// Remove all routes.
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<RouteEntry> routes_;
};

}  // namespace cgs::service
