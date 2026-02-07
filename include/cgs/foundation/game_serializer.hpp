#pragma once

/// @file game_serializer.hpp
/// @brief GameSerializer providing binary/JSON serialization with schema
///        versioning and compile-time field registration via CGS_SERIALIZABLE.
///
/// Template-heavy header: binary and JSON serialization logic must reside
/// here because they operate on user-defined types through SerializableTraits.
/// When container_system becomes available, only the implementation file and
/// CMakeLists.txt need to change.
/// Part of the Container System Adapter (SDS-MOD-007).

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "cgs/foundation/game_result.hpp"

namespace cgs::foundation {

// ── Forward declarations ────────────────────────────────────────────────────

/// Specialization point for compile-time field registration.
/// Users specialize this via the CGS_SERIALIZABLE macro.
template <typename T>
struct SerializableTraits {
    static constexpr bool is_serializable = false;
};

// ── Field descriptor ────────────────────────────────────────────────────────

/// Describes a single serializable field: its name and pointer-to-member.
template <typename T, typename M>
struct FieldDescriptor {
    const char* name;
    M T::*pointer;
};

/// Create a FieldDescriptor from a name and pointer-to-member.
template <typename T, typename M>
constexpr FieldDescriptor<T, M> field(const char* name, M T::*ptr) {
    return {name, ptr};
}

// ── detail:: implementation helpers ─────────────────────────────────────────

namespace detail {

/// Check if a type has been registered with CGS_SERIALIZABLE.
template <typename T, typename = void>
struct is_serializable : std::false_type {};

template <typename T>
struct is_serializable<
    T, std::void_t<decltype(SerializableTraits<T>::schema_version)>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_serializable_v = is_serializable<T>::value;

// ── Tuple iteration ─────────────────────────────────────────────────────

template <typename Tuple, typename Func, std::size_t... Is>
void forEachFieldImpl(const Tuple& t, Func&& f, std::index_sequence<Is...>) {
    (f(std::get<Is>(t), Is), ...);
}

template <typename Tuple, typename Func>
void forEachField(const Tuple& t, Func&& f) {
    forEachFieldImpl(
        t, std::forward<Func>(f),
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// ── Binary write helpers ────────────────────────────────────────────────

inline void writeBytes(std::vector<uint8_t>& buf, const void* data,
                       std::size_t n) {
    const auto* p = static_cast<const uint8_t*>(data);
    buf.insert(buf.end(), p, p + n);
}

template <typename T>
void writePrimitive(std::vector<uint8_t>& buf, const T& val) {
    writeBytes(buf, &val, sizeof(T));
}

inline void writeString(std::vector<uint8_t>& buf, const std::string& val) {
    auto len = static_cast<uint32_t>(val.size());
    writePrimitive(buf, len);
    writeBytes(buf, val.data(), val.size());
}

template <typename T>
void writeBinaryField(std::vector<uint8_t>& buf, const T& val) {
    if constexpr (std::is_same_v<T, bool>) {
        uint8_t b = val ? 1 : 0;
        writePrimitive(buf, b);
    } else if constexpr (std::is_same_v<T, std::string>) {
        writeString(buf, val);
    } else if constexpr (std::is_arithmetic_v<T>) {
        writePrimitive(buf, val);
    }
}

// ── Binary read helpers ─────────────────────────────────────────────────

struct BinaryReader {
    std::span<const uint8_t> data;
    std::size_t pos = 0;

    [[nodiscard]] bool canRead(std::size_t n) const {
        return pos + n <= data.size();
    }

    template <typename T>
    bool readPrimitive(T& val) {
        if (!canRead(sizeof(T))) return false;
        std::memcpy(&val, data.data() + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }

    bool readString(std::string& val) {
        uint32_t len = 0;
        if (!readPrimitive(len)) return false;
        if (!canRead(len)) return false;
        val.assign(reinterpret_cast<const char*>(data.data() + pos), len);
        pos += len;
        return true;
    }
};

template <typename T>
bool readBinaryField(BinaryReader& reader, T& val) {
    if constexpr (std::is_same_v<T, bool>) {
        uint8_t b = 0;
        if (!reader.readPrimitive(b)) return false;
        val = (b != 0);
        return true;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return reader.readString(val);
    } else if constexpr (std::is_arithmetic_v<T>) {
        return reader.readPrimitive(val);
    }
    return false;
}

// ── JSON write helpers ──────────────────────────────────────────────────

inline std::string escapeJson(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

template <typename T>
void writeJsonField(std::ostringstream& out, const char* name, const T& val) {
    out << '"' << name << "\":";
    if constexpr (std::is_same_v<T, bool>) {
        out << (val ? "true" : "false");
    } else if constexpr (std::is_same_v<T, std::string>) {
        out << '"' << escapeJson(val) << '"';
    } else if constexpr (std::is_floating_point_v<T>) {
        out << val;
    } else if constexpr (std::is_signed_v<T>) {
        out << static_cast<int64_t>(val);
    } else if constexpr (std::is_unsigned_v<T>) {
        out << static_cast<uint64_t>(val);
    }
}

// ── JSON read helpers ───────────────────────────────────────────────────

struct JsonReader {
    std::string_view data;
    std::size_t pos = 0;

    void skipWhitespace() {
        while (pos < data.size() &&
               (data[pos] == ' ' || data[pos] == '\t' ||
                data[pos] == '\n' || data[pos] == '\r')) {
            ++pos;
        }
    }

    bool expect(char c) {
        skipWhitespace();
        if (pos < data.size() && data[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    bool readQuotedString(std::string& out) {
        skipWhitespace();
        if (pos >= data.size() || data[pos] != '"') return false;
        ++pos;
        out.clear();
        while (pos < data.size() && data[pos] != '"') {
            if (data[pos] == '\\') {
                ++pos;
                if (pos >= data.size()) return false;
                switch (data[pos]) {
                    case '"':  out += '"'; break;
                    case '\\': out += '\\'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    default:   out += data[pos]; break;
                }
            } else {
                out += data[pos];
            }
            ++pos;
        }
        if (pos >= data.size()) return false;
        ++pos;  // skip closing quote
        return true;
    }

    /// Read a JSON value token. For strings, returns the unescaped content.
    /// For numbers/bools/null, returns the raw text.
    bool readRawValue(std::string& out) {
        skipWhitespace();
        if (pos >= data.size()) return false;
        if (data[pos] == '"') {
            return readQuotedString(out);
        }
        std::size_t start = pos;
        while (pos < data.size() && data[pos] != ',' && data[pos] != '}' &&
               data[pos] != ' ' && data[pos] != '\t' &&
               data[pos] != '\n' && data[pos] != '\r') {
            ++pos;
        }
        out = std::string(data.substr(start, pos - start));
        return !out.empty();
    }

    /// Skip a JSON value (for unrecognized fields).
    void skipValue() {
        skipWhitespace();
        if (pos >= data.size()) return;
        if (data[pos] == '"') {
            std::string dummy;
            readQuotedString(dummy);
        } else {
            while (pos < data.size() && data[pos] != ',' &&
                   data[pos] != '}') {
                ++pos;
            }
        }
    }
};

template <typename T>
bool parseJsonValue(const std::string& raw, T& val) {
    if constexpr (std::is_same_v<T, bool>) {
        if (raw == "true") { val = true; return true; }
        if (raw == "false") { val = false; return true; }
        return false;
    } else if constexpr (std::is_same_v<T, std::string>) {
        val = raw;
        return true;
    } else if constexpr (std::is_floating_point_v<T>) {
        try {
            if constexpr (std::is_same_v<T, float>) {
                val = std::stof(raw);
            } else {
                val = std::stod(raw);
            }
            return true;
        } catch (...) { return false; }
    } else if constexpr (std::is_integral_v<T>) {
        try {
            if constexpr (std::is_signed_v<T>) {
                val = static_cast<T>(std::stoll(raw));
            } else {
                val = static_cast<T>(std::stoull(raw));
            }
            return true;
        } catch (...) { return false; }
    }
    return false;
}

}  // namespace detail

// ── Binary format constants ─────────────────────────────────────────────────

/// Magic bytes identifying CGS binary serialization format.
inline constexpr uint8_t kBinaryMagic[4] = {'C', 'G', 'S', 'B'};

// ── GameSerializer ──────────────────────────────────────────────────────────

/// Serializer providing binary and JSON encoding with schema versioning.
///
/// Types must be registered with CGS_SERIALIZABLE before use. Binary format
/// is compact (no field names stored); JSON format uses human-readable keys.
/// Schema versioning allows older data to be deserialized into newer structs
/// with default values for added fields.
///
/// Example:
/// @code
///   struct Player {
///       int32_t id = 0;
///       std::string name;
///       int32_t level = 1;
///   };
///   CGS_SERIALIZABLE(Player, 1,
///       field("id", &Player::id),
///       field("name", &Player::name),
///       field("level", &Player::level)
///   );
///
///   GameSerializer s;
///   auto bin = s.serializeBinary(player);
///   auto json = s.serializeJson(player);
///   auto result = s.deserializeBinary<Player>(bin);
/// @endcode
class GameSerializer {
public:
    GameSerializer();
    ~GameSerializer();

    GameSerializer(const GameSerializer&) = delete;
    GameSerializer& operator=(const GameSerializer&) = delete;
    GameSerializer(GameSerializer&&) noexcept;
    GameSerializer& operator=(GameSerializer&&) noexcept;

    // ── Binary serialization ────────────────────────────────────────────

    /// Serialize an object to compact binary format.
    template <typename T>
    [[nodiscard]] std::vector<uint8_t> serializeBinary(const T& obj) const {
        static_assert(detail::is_serializable_v<T>,
                      "Type must be registered with CGS_SERIALIZABLE");

        std::vector<uint8_t> buf;
        buf.reserve(128);

        // Header: magic + version + field count
        detail::writeBytes(buf, kBinaryMagic, 4);

        uint32_t version = SerializableTraits<T>::schema_version;
        detail::writePrimitive(buf, version);

        auto fields = SerializableTraits<T>::fields();
        constexpr auto numFields =
            static_cast<uint32_t>(std::tuple_size_v<decltype(fields)>);
        detail::writePrimitive(buf, numFields);

        // Fields in declaration order
        detail::forEachField(fields, [&](const auto& fd, std::size_t) {
            detail::writeBinaryField(buf, obj.*(fd.pointer));
        });

        return buf;
    }

    /// Deserialize an object from binary format.
    /// Missing fields (from older schema versions) retain default values.
    template <typename T>
    [[nodiscard]] GameResult<T> deserializeBinary(
        std::span<const uint8_t> data) const {
        static_assert(detail::is_serializable_v<T>,
                      "Type must be registered with CGS_SERIALIZABLE");

        detail::BinaryReader reader{data};

        // Verify magic bytes
        uint8_t magic[4]{};
        for (auto& m : magic) {
            if (!reader.readPrimitive(m)) {
                return GameResult<T>::err(GameError(
                    ErrorCode::InvalidBinaryData, "truncated binary header"));
            }
        }
        if (std::memcmp(magic, kBinaryMagic, 4) != 0) {
            return GameResult<T>::err(
                GameError(ErrorCode::InvalidBinaryData, "invalid magic bytes"));
        }

        // Schema version (stored for forward compatibility)
        uint32_t version = 0;
        if (!reader.readPrimitive(version)) {
            return GameResult<T>::err(GameError(
                ErrorCode::InvalidBinaryData, "truncated version field"));
        }

        // Field count from the serialized data
        uint32_t storedFieldCount = 0;
        if (!reader.readPrimitive(storedFieldCount)) {
            return GameResult<T>::err(GameError(
                ErrorCode::InvalidBinaryData, "truncated field count"));
        }

        T obj{};
        auto fields = SerializableTraits<T>::fields();
        constexpr uint32_t currentFieldCount =
            static_cast<uint32_t>(std::tuple_size_v<decltype(fields)>);

        // Read min(stored, current) fields; extra fields keep defaults
        uint32_t fieldsToRead =
            std::min(storedFieldCount, currentFieldCount);

        detail::forEachField(fields, [&](const auto& fd, std::size_t idx) {
            if (static_cast<uint32_t>(idx) < fieldsToRead) {
                detail::readBinaryField(reader, obj.*(fd.pointer));
            }
        });

        return GameResult<T>::ok(std::move(obj));
    }

    // ── JSON serialization ──────────────────────────────────────────────

    /// Serialize an object to JSON string with schema version.
    template <typename T>
    [[nodiscard]] std::string serializeJson(const T& obj) const {
        static_assert(detail::is_serializable_v<T>,
                      "Type must be registered with CGS_SERIALIZABLE");

        std::ostringstream out;
        out << "{\"__v\":" << SerializableTraits<T>::schema_version;

        auto fields = SerializableTraits<T>::fields();
        detail::forEachField(fields, [&](const auto& fd, std::size_t) {
            out << ',';
            detail::writeJsonField(out, fd.name, obj.*(fd.pointer));
        });

        out << '}';
        return out.str();
    }

    /// Deserialize an object from JSON string.
    /// Unrecognized keys are skipped; missing keys retain default values.
    template <typename T>
    [[nodiscard]] GameResult<T> deserializeJson(std::string_view json) const {
        static_assert(detail::is_serializable_v<T>,
                      "Type must be registered with CGS_SERIALIZABLE");

        detail::JsonReader reader{json};

        if (!reader.expect('{')) {
            return GameResult<T>::err(
                GameError(ErrorCode::InvalidJsonData, "expected '{'"));
        }

        T obj{};
        auto fields = SerializableTraits<T>::fields();

        bool first = true;
        while (true) {
            reader.skipWhitespace();
            if (reader.pos >= reader.data.size()) {
                return GameResult<T>::err(GameError(
                    ErrorCode::InvalidJsonData, "unexpected end of JSON"));
            }
            if (reader.data[reader.pos] == '}') {
                ++reader.pos;
                break;
            }

            if (!first) {
                if (!reader.expect(',')) {
                    return GameResult<T>::err(
                        GameError(ErrorCode::InvalidJsonData, "expected ','"));
                }
            }
            first = false;

            // Read key
            std::string key;
            if (!reader.readQuotedString(key)) {
                return GameResult<T>::err(GameError(
                    ErrorCode::InvalidJsonData, "expected key string"));
            }
            if (!reader.expect(':')) {
                return GameResult<T>::err(
                    GameError(ErrorCode::InvalidJsonData, "expected ':'"));
            }

            // Skip version field
            if (key == "__v") {
                reader.skipValue();
                continue;
            }

            // Match against known fields
            bool matched = false;
            detail::forEachField(fields, [&](const auto& fd, std::size_t) {
                if (matched) return;
                if (key != fd.name) return;

                using FieldType =
                    std::remove_reference_t<decltype(obj.*(fd.pointer))>;
                std::string rawVal;
                if (reader.readRawValue(rawVal)) {
                    FieldType val{};
                    if (detail::parseJsonValue(rawVal, val)) {
                        obj.*(fd.pointer) = std::move(val);
                    }
                    matched = true;
                }
            });

            if (!matched) {
                reader.skipValue();
            }
        }

        return GameResult<T>::ok(std::move(obj));
    }

    // ── Singleton ───────────────────────────────────────────────────────

    /// Access the global GameSerializer instance.
    static GameSerializer& instance();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cgs::foundation

// ── CGS_SERIALIZABLE macro ──────────────────────────────────────────────────
/// Register a type for serialization with field descriptors and version.
///
/// @param Type     The struct/class type to register.
/// @param Version  Schema version number (uint32_t).
/// @param ...      field("name", &Type::member) descriptors.
///
/// Example:
/// @code
///   CGS_SERIALIZABLE(PlayerData, 1,
///       field("id", &PlayerData::id),
///       field("name", &PlayerData::name),
///       field("level", &PlayerData::level)
///   );
/// @endcode
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CGS_SERIALIZABLE(Type, Version, ...)                                   \
    template <>                                                                \
    struct cgs::foundation::SerializableTraits<Type> {                         \
        static constexpr bool is_serializable = true;                          \
        static constexpr uint32_t schema_version = Version;                    \
        static constexpr auto fields() {                                       \
            using cgs::foundation::field;                                      \
            return std::make_tuple(__VA_ARGS__);                               \
        }                                                                      \
    }
