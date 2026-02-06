#include <gtest/gtest.h>

#include "cgs/cgs.hpp"

TEST(VersionTest, MajorMinorPatch) {
    EXPECT_EQ(cgs::Version::major, 0);
    EXPECT_EQ(cgs::Version::minor, 1);
    EXPECT_EQ(cgs::Version::patch, 0);
}

TEST(VersionTest, VersionString) {
    EXPECT_STREQ(cgs::Version::string, "0.1.0");
}

TEST(ResultTest, OkValue) {
    auto result = cgs::Result<int>::ok(42);
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(result.hasError());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorValue) {
    auto result = cgs::Result<int>::err(cgs::Error("something failed"));
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().message, "something failed");
    EXPECT_EQ(result.error().code, -1);
}

TEST(ResultTest, ErrorWithCode) {
    auto result = cgs::Result<int>::err(cgs::Error(404, "not found"));
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code, 404);
    EXPECT_EQ(result.error().message, "not found");
}

TEST(ResultTest, ValueOr) {
    auto ok = cgs::Result<int>::ok(10);
    auto err = cgs::Result<int>::err(cgs::Error("fail"));
    EXPECT_EQ(ok.valueOr(0), 10);
    EXPECT_EQ(err.valueOr(0), 0);
}

TEST(ResultTest, BoolConversion) {
    auto ok = cgs::Result<int>::ok(1);
    auto err = cgs::Result<int>::err(cgs::Error("fail"));
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_FALSE(static_cast<bool>(err));
}

TEST(ResultVoidTest, Ok) {
    auto result = cgs::Result<void>::ok();
    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(result.hasError());
}

TEST(ResultVoidTest, Error) {
    auto result = cgs::Result<void>::err(cgs::Error("void error"));
    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error().message, "void error");
}
