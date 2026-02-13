#pragma once

/// @file plugin_export.hpp
/// @brief Macros for exporting and registering plugins.
///
/// Dynamic plugins use CGS_PLUGIN_EXPORT to generate the C entry point
/// that PluginManager calls via dlsym/GetProcAddress.
///
/// Static plugins use CGS_PLUGIN_REGISTER to add themselves to the
/// compile-time registry so that PluginManager can discover them
/// without dynamic linking.
///
/// @see SDS-MOD-020

#include "cgs/plugin/iplugin.hpp"

#include <functional>
#include <string>
#include <vector>

// ── Dynamic plugin export ────────────────────────────────────────────────

/// Generate a C-linkage factory function for a dynamically loaded plugin.
///
/// Usage (in a .cpp file compiled into a shared library):
/// @code
///   class MyPlugin : public cgs::plugin::IPlugin { ... };
///   CGS_PLUGIN_EXPORT(MyPlugin)
/// @endcode
///
/// The PluginManager loads the shared library and calls `CgsCreatePlugin()`
/// to obtain the IPlugin instance.
#define CGS_PLUGIN_EXPORT(PluginClass)                    \
    extern "C" {                                          \
    cgs::plugin::IPlugin* CgsCreatePlugin() {             \
        return new PluginClass();                         \
    }                                                     \
    void CgsDestroyPlugin(cgs::plugin::IPlugin* plugin) { \
        delete plugin;                                    \
    }                                                     \
    }

// ── Static plugin registration ──────────────────────────────────────────

namespace cgs::plugin {

/// Factory function type for creating a static plugin instance.
using PluginFactory = std::function<std::unique_ptr<IPlugin>()>;

/// Entry in the static plugin registry.
struct StaticPluginEntry {
    std::string name;
    PluginFactory factory;
};

/// Global registry of statically linked plugins.
///
/// Populated at static-init time by CGS_PLUGIN_REGISTER macros.
/// PluginManager queries this list when RegisterStaticPlugins() is called.
inline std::vector<StaticPluginEntry>& StaticPluginRegistry() {
    static std::vector<StaticPluginEntry> registry;
    return registry;
}

namespace detail {

/// RAII helper that registers a factory on construction.
struct StaticPluginRegistrar {
    StaticPluginRegistrar(const char* name, PluginFactory factory) {
        StaticPluginRegistry().push_back({name, std::move(factory)});
    }
};

}  // namespace detail
}  // namespace cgs::plugin

/// Register a plugin for static (compile-time) linking.
///
/// Usage (in a .cpp file linked into the main executable):
/// @code
///   class MyPlugin : public cgs::plugin::IPlugin { ... };
///   CGS_PLUGIN_REGISTER(MyPlugin, "MyPlugin")
/// @endcode
#define CGS_PLUGIN_REGISTER(PluginClass, PluginName)                                              \
    static ::cgs::plugin::detail::StaticPluginRegistrar                                           \
    cgs_static_plugin_##PluginClass##_registrar(PluginName,                                       \
                                                []() -> std::unique_ptr<::cgs::plugin::IPlugin> { \
                                                    return std::make_unique<PluginClass>();       \
                                                })
