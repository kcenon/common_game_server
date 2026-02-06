#pragma once

/// @file service_locator.hpp
/// @brief Type-safe dependency injection container.

#include <any>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace cgs::foundation {

/// Lightweight service locator for runtime dependency injection.
///
/// Services are registered by their interface type and retrieved via
/// type-safe get<T>() calls. This allows decoupling between modules
/// while keeping runtime flexibility.
///
/// Example:
/// @code
///   ServiceLocator locator;
///   locator.add<ILogger>(std::make_unique<ConsoleLogger>());
///
///   auto* logger = locator.get<ILogger>();
///   if (logger) logger->info("hello");
/// @endcode
class ServiceLocator {
public:
    ServiceLocator() = default;
    ~ServiceLocator() = default;

    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;
    ServiceLocator(ServiceLocator&&) = default;
    ServiceLocator& operator=(ServiceLocator&&) = default;

    /// Register a service by its interface type T.
    /// Replaces any previously registered service of the same type.
    template <typename T>
    void add(std::unique_ptr<T> service) {
        auto key = std::type_index(typeid(T));
        services_[key] = std::make_any<std::shared_ptr<T>>(std::move(service));
    }

    /// Retrieve a registered service (nullptr if not found).
    template <typename T>
    [[nodiscard]] T* get() const {
        auto key = std::type_index(typeid(T));
        auto it = services_.find(key);
        if (it == services_.end()) {
            return nullptr;
        }
        auto ptr = std::any_cast<std::shared_ptr<T>>(&it->second);
        return ptr ? ptr->get() : nullptr;
    }

    /// Check if a service of type T is registered.
    template <typename T>
    [[nodiscard]] bool has() const {
        auto key = std::type_index(typeid(T));
        return services_.count(key) > 0;
    }

    /// Remove a registered service of type T.
    template <typename T>
    void remove() {
        auto key = std::type_index(typeid(T));
        services_.erase(key);
    }

    /// Remove all registered services.
    void clear() { services_.clear(); }

    /// Number of registered services.
    [[nodiscard]] std::size_t size() const noexcept { return services_.size(); }

private:
    std::unordered_map<std::type_index, std::any> services_;
};

} // namespace cgs::foundation
