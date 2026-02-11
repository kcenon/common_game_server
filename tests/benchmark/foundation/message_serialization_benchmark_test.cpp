/// @file message_serialization_benchmark_test.cpp
/// @brief Message serialization throughput benchmark (SRS-NFR-001).
///
/// Measures NetworkMessage serialize/deserialize throughput with realistic
/// game payloads of various sizes.  This complements the existing
/// network_throughput_test.cpp which measures TCP dispatch throughput.
///
/// SRS-NFR-001: Message throughput ≥300,000 msg/sec.
///
/// Focus areas:
///   - Serialization-only throughput (encoding)
///   - Deserialization-only throughput (decoding + validation)
///   - Round-trip throughput (serialize → deserialize)
///   - Payload size impact on throughput

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "cgs/foundation/game_network_manager.hpp"
#include "cgs/foundation/game_serializer.hpp"

using namespace cgs::foundation;

namespace {

// Benchmark parameters
constexpr int kMessageCount = 100000;
constexpr int kIterations = 10;
constexpr double kMinThroughput = 300000.0; // SRS-NFR-001: ≥300K msg/sec

/// Create a realistic game message with the given payload size.
NetworkMessage makeGameMessage(uint16_t opcode, std::size_t payloadSize) {
    NetworkMessage msg;
    msg.opcode = opcode;
    msg.payload.resize(payloadSize);

    // Fill with pseudo-realistic game data pattern
    for (std::size_t i = 0; i < payloadSize; ++i) {
        msg.payload[i] = static_cast<uint8_t>(
            (i * 37 + opcode) & 0xFF);
    }
    return msg;
}

/// Payload sizes representing typical game messages
struct PayloadProfile {
    const char* name;
    uint16_t opcode;
    std::size_t size;
};

const PayloadProfile kProfiles[] = {
    {"Heartbeat",       0x01,   4},   // 4 bytes: timestamp
    {"Movement",        0x10,  32},   // position(12) + rotation(16) + flags(4)
    {"Chat",            0x20, 128},   // channel(4) + sender(32) + message(92)
    {"SpellCast",       0x30,  24},   // spell_id(4) + target(4) + position(12) + flags(4)
    {"InventoryUpdate", 0x40, 256},   // slot updates, item data
    {"WorldState",      0x50, 512},   // snapshot of nearby entities
};
constexpr int kProfileCount =
    static_cast<int>(sizeof(kProfiles) / sizeof(kProfiles[0]));

} // anonymous namespace

// ===========================================================================
// Benchmark Fixture
// ===========================================================================

class MessageSerializationBenchmark : public ::testing::Test {};

// ===========================================================================
// Serialization Throughput (64-byte messages)
// ===========================================================================

TEST_F(MessageSerializationBenchmark, SerializeThroughput) {
    auto msg = makeGameMessage(0x10, 58); // 6 header + 58 payload = 64 bytes

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        auto wire = msg.serialize();
        (void)wire.size();
    }

    std::vector<double> throughputs;
    throughputs.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < kMessageCount; ++i) {
            auto wire = msg.serialize();
            (void)wire.size(); // prevent dead-code elimination
        }

        auto end = std::chrono::high_resolution_clock::now();
        double seconds =
            std::chrono::duration<double>(end - start).count();
        double tp = static_cast<double>(kMessageCount) / seconds;
        throughputs.push_back(tp);
    }

    std::sort(throughputs.begin(), throughputs.end());
    double medianTp = throughputs[throughputs.size() / 2];
    double minTp = throughputs.front();
    double maxTp = throughputs.back();

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Serialization Throughput (64-byte msg)          |\n"
              << "+-------------------------------------------------+\n"
              << "|  Messages:      " << std::setw(10) << kMessageCount
              << "                    |\n"
              << "|  Iterations:    " << std::setw(10) << kIterations
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << "|  Min:           " << std::setw(10) << std::fixed
              << std::setprecision(0) << minTp << " msg/sec          |\n"
              << "|  Median:        " << std::setw(10) << medianTp
              << " msg/sec          |\n"
              << "|  Max:           " << std::setw(10) << maxTp
              << " msg/sec          |\n"
              << "+-------------------------------------------------+\n"
              << "|  Requirement:   " << std::setw(10) << kMinThroughput
              << " msg/sec          |\n"
              << "|  Status:        " << std::setw(10)
              << (medianTp >= kMinThroughput ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_GE(medianTp, kMinThroughput)
        << "Serialization throughput " << medianTp
        << " msg/sec below " << kMinThroughput << " msg/sec (SRS-NFR-001)";
}

// ===========================================================================
// Deserialization Throughput
// ===========================================================================

TEST_F(MessageSerializationBenchmark, DeserializeThroughput) {
    auto msg = makeGameMessage(0x10, 58);
    auto wireData = msg.serialize();

    // Warmup
    for (int i = 0; i < 1000; ++i) {
        auto result = NetworkMessage::deserialize(wireData);
        ASSERT_TRUE(result.has_value());
    }

    std::vector<double> throughputs;
    throughputs.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < kMessageCount; ++i) {
            auto result = NetworkMessage::deserialize(wireData);
            (void)result; // prevent dead-code elimination
        }

        auto end = std::chrono::high_resolution_clock::now();
        double seconds =
            std::chrono::duration<double>(end - start).count();
        double tp = static_cast<double>(kMessageCount) / seconds;
        throughputs.push_back(tp);
    }

    std::sort(throughputs.begin(), throughputs.end());
    double medianTp = throughputs[throughputs.size() / 2];

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Deserialization Throughput (64-byte msg)        |\n"
              << "+-------------------------------------------------+\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(0) << medianTp << " msg/sec          |\n"
              << "|  Requirement:   " << std::setw(10) << kMinThroughput
              << " msg/sec          |\n"
              << "|  Status:        " << std::setw(10)
              << (medianTp >= kMinThroughput ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_GE(medianTp, kMinThroughput)
        << "Deserialization throughput " << medianTp
        << " msg/sec below " << kMinThroughput << " msg/sec (SRS-NFR-001)";
}

// ===========================================================================
// Round-Trip Throughput (Serialize + Deserialize)
// ===========================================================================

TEST_F(MessageSerializationBenchmark, RoundTripThroughput) {
    auto msg = makeGameMessage(0x10, 58);

    std::vector<double> throughputs;
    throughputs.reserve(static_cast<std::size_t>(kIterations));

    for (int iter = 0; iter < kIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < kMessageCount; ++i) {
            auto wire = msg.serialize();
            auto result = NetworkMessage::deserialize(wire);
            (void)result;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double seconds =
            std::chrono::duration<double>(end - start).count();
        double tp = static_cast<double>(kMessageCount) / seconds;
        throughputs.push_back(tp);
    }

    std::sort(throughputs.begin(), throughputs.end());
    double medianTp = throughputs[throughputs.size() / 2];

    std::cout << "\n"
              << "+-------------------------------------------------+\n"
              << "|  Round-Trip Throughput (64-byte msg)             |\n"
              << "+-------------------------------------------------+\n"
              << "|  Median:        " << std::setw(10) << std::fixed
              << std::setprecision(0) << medianTp << " msg/sec          |\n"
              << "|  Requirement:   " << std::setw(10) << kMinThroughput
              << " msg/sec          |\n"
              << "|  Status:        " << std::setw(10)
              << (medianTp >= kMinThroughput ? "PASS" : "FAIL")
              << "                    |\n"
              << "+-------------------------------------------------+\n"
              << std::endl;

    EXPECT_GE(medianTp, kMinThroughput)
        << "Round-trip throughput " << medianTp
        << " msg/sec below " << kMinThroughput << " msg/sec (SRS-NFR-001)";
}

// ===========================================================================
// Payload Size Impact on Throughput
// ===========================================================================

TEST_F(MessageSerializationBenchmark, PayloadSizeImpact) {
    std::cout << "\n"
              << "+---------------------------------------------------+\n"
              << "|  Payload Size Impact on Throughput                  |\n"
              << "|  (SRS-NFR-001: ≥300K msg/sec)                      |\n"
              << "+---------------------------------------------------+\n";

    bool allPass = true;

    for (int p = 0; p < kProfileCount; ++p) {
        const auto& profile = kProfiles[p];
        auto msg = makeGameMessage(profile.opcode, profile.size);

        // Measure round-trip throughput
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < kMessageCount; ++i) {
            auto wire = msg.serialize();
            auto result = NetworkMessage::deserialize(wire);
            (void)result;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double seconds =
            std::chrono::duration<double>(end - start).count();
        double tp = static_cast<double>(kMessageCount) / seconds;
        double wireSize =
            static_cast<double>(profile.size) + 6.0; // 4-len + 2-opcode

        bool pass = (tp >= kMinThroughput);
        if (!pass) { allPass = false; }

        std::cout << "|  " << std::left << std::setw(16) << profile.name
                  << std::right
                  << "  wire=" << std::setw(4) << static_cast<int>(wireSize)
                  << "B  tp=" << std::setw(10) << std::fixed
                  << std::setprecision(0) << tp
                  << "  " << (pass ? "PASS" : "FAIL") << " |\n";
    }

    std::cout << "+---------------------------------------------------+\n"
              << "|  Requirement:     " << std::setw(10) << std::fixed
              << std::setprecision(0) << kMinThroughput
              << " msg/sec              |\n"
              << "|  All Profiles:    " << std::setw(10)
              << (allPass ? "PASS" : "FAIL")
              << "                      |\n"
              << "+---------------------------------------------------+\n"
              << std::endl;

    // Small messages (heartbeat, movement, spellcast) must exceed threshold
    for (int p = 0; p < kProfileCount; ++p) {
        if (kProfiles[p].size <= 64) {
            auto msg = makeGameMessage(kProfiles[p].opcode, kProfiles[p].size);
            auto tpStart = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < kMessageCount; ++i) {
                auto wire = msg.serialize();
                auto result = NetworkMessage::deserialize(wire);
                (void)result;
            }
            auto tpEnd = std::chrono::high_resolution_clock::now();
            double seconds =
                std::chrono::duration<double>(tpEnd - tpStart).count();
            double tp = static_cast<double>(kMessageCount) / seconds;

            EXPECT_GE(tp, kMinThroughput)
                << kProfiles[p].name << " (" << kProfiles[p].size
                << " bytes) throughput " << tp
                << " below " << kMinThroughput << " msg/sec";
        }
    }
}
