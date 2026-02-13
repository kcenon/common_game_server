/// @file gateway_session_manager.cpp
/// @brief GatewaySessionManager implementation.

#include "cgs/service/gateway_session_manager.hpp"

#include <chrono>

namespace cgs::service {

GatewaySessionManager::GatewaySessionManager(uint32_t maxSessions) : maxSessions_(maxSessions) {}

bool GatewaySessionManager::createSession(cgs::foundation::SessionId sessionId,
                                          std::string remoteAddress) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessions_.size() >= static_cast<std::size_t>(maxSessions_)) {
        return false;
    }

    if (sessions_.count(sessionId) > 0) {
        return false;
    }

    ClientSession session;
    session.sessionId = sessionId;
    session.state = ClientState::Unauthenticated;
    session.remoteAddress = std::move(remoteAddress);
    session.connectedAt = std::chrono::steady_clock::now();
    session.lastActivity = session.connectedAt;

    sessions_.emplace(sessionId, std::move(session));
    return true;
}

bool GatewaySessionManager::authenticateSession(cgs::foundation::SessionId sessionId,
                                                TokenClaims claims,
                                                uint64_t userId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    if (it->second.state != ClientState::Unauthenticated) {
        return false;
    }

    it->second.state = ClientState::Authenticated;
    it->second.claims = std::move(claims);
    it->second.userId = userId;
    it->second.lastActivity = std::chrono::steady_clock::now();
    return true;
}

bool GatewaySessionManager::beginMigration(cgs::foundation::SessionId sessionId,
                                           std::string targetService) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    if (it->second.state != ClientState::Authenticated) {
        return false;
    }

    it->second.state = ClientState::Migrating;
    it->second.currentService = std::move(targetService);
    it->second.lastActivity = std::chrono::steady_clock::now();
    return true;
}

bool GatewaySessionManager::completeMigration(cgs::foundation::SessionId sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    if (it->second.state != ClientState::Migrating) {
        return false;
    }

    it->second.state = ClientState::Authenticated;
    it->second.lastActivity = std::chrono::steady_clock::now();
    return true;
}

void GatewaySessionManager::removeSession(cgs::foundation::SessionId sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sessionId);
}

std::optional<ClientSession> GatewaySessionManager::getSession(
    cgs::foundation::SessionId sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void GatewaySessionManager::touchSession(cgs::foundation::SessionId sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
}

bool GatewaySessionManager::setCurrentService(cgs::foundation::SessionId sessionId,
                                              std::string service) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    it->second.currentService = std::move(service);
    return true;
}

std::vector<ClientSession> GatewaySessionManager::getSessionsByState(ClientState state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ClientSession> result;
    for (const auto& [id, session] : sessions_) {
        if (session.state == state) {
            result.push_back(session);
        }
    }
    return result;
}

std::size_t GatewaySessionManager::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

std::size_t GatewaySessionManager::sessionCount(ClientState state) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::size_t count = 0;
    for (const auto& [id, session] : sessions_) {
        if (session.state == state) {
            ++count;
        }
    }
    return count;
}

std::vector<cgs::foundation::SessionId> GatewaySessionManager::findIdleSessions(
    std::chrono::seconds idleTimeout) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto cutoff = std::chrono::steady_clock::now() - idleTimeout;
    std::vector<cgs::foundation::SessionId> result;
    for (const auto& [id, session] : sessions_) {
        if (session.state == ClientState::Authenticated && session.lastActivity < cutoff) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<cgs::foundation::SessionId> GatewaySessionManager::findExpiredAuthSessions(
    std::chrono::seconds authTimeout) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto cutoff = std::chrono::steady_clock::now() - authTimeout;
    std::vector<cgs::foundation::SessionId> result;
    for (const auto& [id, session] : sessions_) {
        if (session.state == ClientState::Unauthenticated && session.connectedAt < cutoff) {
            result.push_back(id);
        }
    }
    return result;
}

}  // namespace cgs::service
