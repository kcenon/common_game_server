#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "cgs/foundation/container_adapter.hpp"
#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"

using namespace cgs::foundation;

// ── Test types ──────────────────────────────────────────────────────────────

struct PlayerV1 {
    int32_t id = 0;
    std::string name;
    int32_t level = 1;
};

CGS_SERIALIZABLE(PlayerV1, 1,
    field("id", &PlayerV1::id),
    field("name", &PlayerV1::name),
    field("level", &PlayerV1::level)
);

// Extended version with new fields for schema evolution tests.
struct PlayerV2 {
    int32_t id = 0;
    std::string name;
    int32_t level = 1;
    double rating = 0.0;
    bool active = true;
};

CGS_SERIALIZABLE(PlayerV2, 2,
    field("id", &PlayerV2::id),
    field("name", &PlayerV2::name),
    field("level", &PlayerV2::level),
    field("rating", &PlayerV2::rating),
    field("active", &PlayerV2::active)
);

// Struct exercising all supported primitive types.
struct AllTypes {
    bool boolVal = false;
    int32_t int32Val = 0;
    int64_t int64Val = 0;
    uint32_t uint32Val = 0;
    uint64_t uint64Val = 0;
    float floatVal = 0.0f;
    double doubleVal = 0.0;
    std::string stringVal;
};

CGS_SERIALIZABLE(AllTypes, 1,
    field("boolVal", &AllTypes::boolVal),
    field("int32Val", &AllTypes::int32Val),
    field("int64Val", &AllTypes::int64Val),
    field("uint32Val", &AllTypes::uint32Val),
    field("uint64Val", &AllTypes::uint64Val),
    field("floatVal", &AllTypes::floatVal),
    field("doubleVal", &AllTypes::doubleVal),
    field("stringVal", &AllTypes::stringVal)
);

// Empty struct (edge case).
struct Empty {};
CGS_SERIALIZABLE(Empty, 1);

// ===========================================================================
// ErrorCode: Serialization subsystem
// ===========================================================================

TEST(SerializationErrorCodeTest, SubsystemLookup) {
    EXPECT_EQ(errorSubsystem(ErrorCode::SerializationError), "Serialization");
    EXPECT_EQ(errorSubsystem(ErrorCode::InvalidBinaryData), "Serialization");
    EXPECT_EQ(errorSubsystem(ErrorCode::InvalidJsonData), "Serialization");
}

TEST(SerializationErrorCodeTest, GameErrorSubsystem) {
    GameError err(ErrorCode::InvalidBinaryData, "bad data");
    EXPECT_EQ(err.subsystem(), "Serialization");
    EXPECT_EQ(err.message(), "bad data");
}

// ===========================================================================
// Binary: roundtrip
// ===========================================================================

TEST(BinarySerializationTest, SimpleRoundtrip) {
    GameSerializer s;
    PlayerV1 src{42, "Alice", 10};
    auto bin = s.serializeBinary(src);
    EXPECT_FALSE(bin.empty());

    auto result = s.deserializeBinary<PlayerV1>(bin);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().id, 42);
    EXPECT_EQ(result.value().name, "Alice");
    EXPECT_EQ(result.value().level, 10);
}

TEST(BinarySerializationTest, AllTypesRoundtrip) {
    GameSerializer s;
    AllTypes src;
    src.boolVal = true;
    src.int32Val = -12345;
    src.int64Val = -9876543210LL;
    src.uint32Val = 4000000000u;
    src.uint64Val = 18000000000000000000ULL;
    src.floatVal = 3.14f;
    src.doubleVal = 2.718281828;
    src.stringVal = "hello world";

    auto bin = s.serializeBinary(src);
    auto result = s.deserializeBinary<AllTypes>(bin);
    ASSERT_TRUE(result.hasValue());

    const auto& v = result.value();
    EXPECT_EQ(v.boolVal, true);
    EXPECT_EQ(v.int32Val, -12345);
    EXPECT_EQ(v.int64Val, -9876543210LL);
    EXPECT_EQ(v.uint32Val, 4000000000u);
    EXPECT_EQ(v.uint64Val, 18000000000000000000ULL);
    EXPECT_FLOAT_EQ(v.floatVal, 3.14f);
    EXPECT_DOUBLE_EQ(v.doubleVal, 2.718281828);
    EXPECT_EQ(v.stringVal, "hello world");
}

TEST(BinarySerializationTest, EmptyStruct) {
    GameSerializer s;
    Empty src;
    auto bin = s.serializeBinary(src);
    auto result = s.deserializeBinary<Empty>(bin);
    ASSERT_TRUE(result.hasValue());
}

TEST(BinarySerializationTest, EmptyString) {
    GameSerializer s;
    PlayerV1 src{1, "", 5};
    auto bin = s.serializeBinary(src);
    auto result = s.deserializeBinary<PlayerV1>(bin);
    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value().name.empty());
}

TEST(BinarySerializationTest, MagicBytesPresent) {
    GameSerializer s;
    PlayerV1 src{1, "x", 1};
    auto bin = s.serializeBinary(src);
    ASSERT_GE(bin.size(), 4u);
    EXPECT_EQ(bin[0], 'C');
    EXPECT_EQ(bin[1], 'G');
    EXPECT_EQ(bin[2], 'S');
    EXPECT_EQ(bin[3], 'B');
}

// ===========================================================================
// Binary: schema versioning
// ===========================================================================

TEST(BinarySchemaTest, OlderDataNewerSchema) {
    // Serialize as V1 (3 fields), deserialize as V2 (5 fields).
    // New fields (rating, active) should keep their defaults.
    GameSerializer s;
    PlayerV1 v1{99, "Bob", 20};
    auto bin = s.serializeBinary(v1);

    auto result = s.deserializeBinary<PlayerV2>(bin);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().id, 99);
    EXPECT_EQ(result.value().name, "Bob");
    EXPECT_EQ(result.value().level, 20);
    EXPECT_DOUBLE_EQ(result.value().rating, 0.0);  // default
    EXPECT_TRUE(result.value().active);             // default
}

TEST(BinarySchemaTest, NewerDataOlderSchema) {
    // Serialize as V2 (5 fields), deserialize as V1 (3 fields).
    // Extra fields in the binary are simply not read.
    GameSerializer s;
    PlayerV2 v2{7, "Eve", 30, 4.5, false};
    auto bin = s.serializeBinary(v2);

    auto result = s.deserializeBinary<PlayerV1>(bin);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().id, 7);
    EXPECT_EQ(result.value().name, "Eve");
    EXPECT_EQ(result.value().level, 30);
}

// ===========================================================================
// Binary: error cases
// ===========================================================================

TEST(BinaryErrorTest, EmptyData) {
    GameSerializer s;
    std::vector<uint8_t> empty;
    auto result = s.deserializeBinary<PlayerV1>(empty);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidBinaryData);
}

TEST(BinaryErrorTest, InvalidMagic) {
    GameSerializer s;
    std::vector<uint8_t> bad = {0xFF, 0xFF, 0xFF, 0xFF,
                                 0x01, 0x00, 0x00, 0x00,
                                 0x01, 0x00, 0x00, 0x00};
    auto result = s.deserializeBinary<PlayerV1>(bad);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidBinaryData);
}

TEST(BinaryErrorTest, TruncatedHeader) {
    GameSerializer s;
    // Only magic, no version or field count.
    std::vector<uint8_t> truncated = {'C', 'G', 'S', 'B'};
    auto result = s.deserializeBinary<PlayerV1>(truncated);
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidBinaryData);
}

// ===========================================================================
// JSON: roundtrip
// ===========================================================================

TEST(JsonSerializationTest, SimpleRoundtrip) {
    GameSerializer s;
    PlayerV1 src{42, "Alice", 10};
    auto json = s.serializeJson(src);

    auto result = s.deserializeJson<PlayerV1>(json);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().id, 42);
    EXPECT_EQ(result.value().name, "Alice");
    EXPECT_EQ(result.value().level, 10);
}

TEST(JsonSerializationTest, AllTypesRoundtrip) {
    GameSerializer s;
    AllTypes src;
    src.boolVal = true;
    src.int32Val = -42;
    src.int64Val = 123456789012345LL;
    src.uint32Val = 3000000000u;
    src.uint64Val = 9999999999ULL;
    src.floatVal = 1.5f;
    src.doubleVal = 99.99;
    src.stringVal = "test";

    auto json = s.serializeJson(src);
    auto result = s.deserializeJson<AllTypes>(json);
    ASSERT_TRUE(result.hasValue());

    const auto& v = result.value();
    EXPECT_EQ(v.boolVal, true);
    EXPECT_EQ(v.int32Val, -42);
    EXPECT_EQ(v.int64Val, 123456789012345LL);
    EXPECT_EQ(v.uint32Val, 3000000000u);
    EXPECT_EQ(v.uint64Val, 9999999999ULL);
    EXPECT_FLOAT_EQ(v.floatVal, 1.5f);
    EXPECT_DOUBLE_EQ(v.doubleVal, 99.99);
    EXPECT_EQ(v.stringVal, "test");
}

TEST(JsonSerializationTest, EmptyStruct) {
    GameSerializer s;
    Empty src;
    auto json = s.serializeJson(src);
    EXPECT_TRUE(json.find("__v") != std::string::npos);

    auto result = s.deserializeJson<Empty>(json);
    ASSERT_TRUE(result.hasValue());
}

TEST(JsonSerializationTest, ContainsVersionField) {
    GameSerializer s;
    PlayerV1 src{1, "x", 1};
    auto json = s.serializeJson(src);
    EXPECT_TRUE(json.find("\"__v\":1") != std::string::npos);
}

TEST(JsonSerializationTest, ContainsVersionFieldV2) {
    GameSerializer s;
    PlayerV2 src{};
    auto json = s.serializeJson(src);
    EXPECT_TRUE(json.find("\"__v\":2") != std::string::npos);
}

// ===========================================================================
// JSON: special characters
// ===========================================================================

TEST(JsonEscapingTest, EscapedCharacters) {
    GameSerializer s;
    PlayerV1 src{1, "line1\nline2\ttab \"quoted\" \\back", 1};
    auto json = s.serializeJson(src);

    auto result = s.deserializeJson<PlayerV1>(json);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().name, "line1\nline2\ttab \"quoted\" \\back");
}

TEST(JsonEscapingTest, EmptyString) {
    GameSerializer s;
    PlayerV1 src{1, "", 5};
    auto json = s.serializeJson(src);
    auto result = s.deserializeJson<PlayerV1>(json);
    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value().name.empty());
}

// ===========================================================================
// JSON: schema versioning
// ===========================================================================

TEST(JsonSchemaTest, UnknownKeysSkipped) {
    // JSON with extra keys not in the schema
    GameSerializer s;
    std::string_view json =
        R"({"__v":3,"id":1,"name":"x","level":5,"unknown_key":"val"})";
    auto result = s.deserializeJson<PlayerV1>(json);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().id, 1);
    EXPECT_EQ(result.value().name, "x");
    EXPECT_EQ(result.value().level, 5);
}

TEST(JsonSchemaTest, MissingKeysKeepDefaults) {
    // JSON with only some fields
    GameSerializer s;
    std::string_view json = R"({"__v":1,"id":42})";
    auto result = s.deserializeJson<PlayerV1>(json);
    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(result.value().id, 42);
    EXPECT_TRUE(result.value().name.empty());  // default
    EXPECT_EQ(result.value().level, 1);        // default
}

// ===========================================================================
// JSON: error cases
// ===========================================================================

TEST(JsonErrorTest, EmptyString) {
    GameSerializer s;
    auto result = s.deserializeJson<PlayerV1>("");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidJsonData);
}

TEST(JsonErrorTest, NotAnObject) {
    GameSerializer s;
    auto result = s.deserializeJson<PlayerV1>("[]");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidJsonData);
}

TEST(JsonErrorTest, MalformedJsonMissingColon) {
    GameSerializer s;
    auto result = s.deserializeJson<PlayerV1>("{\"id\" 42}");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidJsonData);
}

TEST(JsonErrorTest, PlainTextNotJson) {
    GameSerializer s;
    auto result = s.deserializeJson<PlayerV1>("not json at all");
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidJsonData);
}

// ===========================================================================
// Construction and move semantics
// ===========================================================================

TEST(GameSerializerConstructionTest, DefaultConstruction) {
    GameSerializer s;
    PlayerV1 src{1, "test", 1};
    auto bin = s.serializeBinary(src);
    EXPECT_FALSE(bin.empty());
}

TEST(GameSerializerConstructionTest, MoveConstruction) {
    GameSerializer a;
    GameSerializer b(std::move(a));
    PlayerV1 src{1, "test", 1};
    auto bin = b.serializeBinary(src);
    EXPECT_FALSE(bin.empty());
}

TEST(GameSerializerConstructionTest, MoveAssignment) {
    GameSerializer a;
    GameSerializer b;
    b = std::move(a);
    PlayerV1 src{1, "test", 1};
    auto bin = b.serializeBinary(src);
    EXPECT_FALSE(bin.empty());
}

TEST(GameSerializerConstructionTest, NonCopyable) {
    static_assert(!std::is_copy_constructible_v<GameSerializer>);
    static_assert(!std::is_copy_assignable_v<GameSerializer>);
}

// ===========================================================================
// Singleton
// ===========================================================================

TEST(GameSerializerSingletonTest, InstanceReturnsSameObject) {
    auto& a = GameSerializer::instance();
    auto& b = GameSerializer::instance();
    EXPECT_EQ(&a, &b);
}

// ===========================================================================
// Aggregate header
// ===========================================================================

TEST(ContainerAdapterTest, AggregateHeaderIncludesAll) {
    GameSerializer s;
    PlayerV1 src{1, "x", 1};
    auto bin = s.serializeBinary(src);
    auto json = s.serializeJson(src);
    EXPECT_FALSE(bin.empty());
    EXPECT_FALSE(json.empty());
}
