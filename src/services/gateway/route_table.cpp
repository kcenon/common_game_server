/// @file route_table.cpp
/// @brief RouteTable implementation.

#include "cgs/service/route_table.hpp"

#include <algorithm>

namespace cgs::service {

void RouteTable::addRoute(uint16_t opcodeMin,
                          uint16_t opcodeMax,
                          std::string service,
                          bool requiresAuth) {
    RouteEntry entry;
    entry.opcodeMin = opcodeMin;
    entry.opcodeMax = opcodeMax;
    entry.service = std::move(service);
    entry.requiresAuth = requiresAuth;
    addRoute(std::move(entry));
}

void RouteTable::addRoute(RouteEntry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    routes_.push_back(std::move(entry));
}

std::optional<RouteMatch> RouteTable::resolve(uint16_t opcode) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& route : routes_) {
        if (opcode >= route.opcodeMin && opcode <= route.opcodeMax) {
            return RouteMatch{route.service, route.requiresAuth};
        }
    }
    return std::nullopt;
}

bool RouteTable::isGatewayOpcode(uint16_t opcode) {
    return opcode <= 0x00FF;
}

const std::vector<RouteEntry>& RouteTable::routes() const {
    return routes_;
}

void RouteTable::removeRoutesForService(std::string_view service) {
    std::lock_guard<std::mutex> lock(mutex_);
    routes_.erase(std::remove_if(routes_.begin(),
                                 routes_.end(),
                                 [&](const RouteEntry& e) { return e.service == service; }),
                  routes_.end());
}

void RouteTable::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    routes_.clear();
}

}  // namespace cgs::service
