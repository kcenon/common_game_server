/// @file network_integration_test.cpp
/// @brief Integration tests for GameNetworkManager (SDS-MOD-004).
///
/// Validates the full pipeline: listen -> client connect -> framed message
/// send/receive -> disconnect, for TCP, UDP, and WebSocket protocols.

#include <gtest/gtest.h>

#include "cgs/foundation/game_network_manager.hpp"
#include "cgs/foundation/network_adapter.hpp"

// kcenon client facades (used only in tests to simulate real clients)
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/facade/websocket_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <kcenon/network/interfaces/i_protocol_client.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace cgs::foundation;
using namespace kcenon::network;

namespace {

// Fixed high ports per protocol to avoid conflicts
constexpr uint16_t kTCPPort = 19001;
constexpr uint16_t kUDPPort = 19002;
constexpr uint16_t kWSPort = 19003;

// Test opcodes
constexpr uint16_t kPingOpcode = 0x01;
constexpr uint16_t kPongOpcode = 0x02;
constexpr uint16_t kEchoOpcode = 0x03;

// Timeouts
constexpr auto kConnectTimeout = std::chrono::seconds(5);
constexpr auto kMessageTimeout = std::chrono::seconds(5);
constexpr auto kWSConnectTimeout = std::chrono::seconds(10);
constexpr auto kStartupDelay = std::chrono::milliseconds(200);

// ---------------------------------------------------------------------------
// TestClient: RAII wrapper around kcenon i_protocol_client using observer pattern
//
// TCP/UDP facades auto-call start() inside create_client(), so the observer
// is set after creation and we fall back to is_connected() if the async
// connected callback already fired before the observer was attached.
// WebSocket facade does NOT auto-start, so start() is called explicitly.
// ---------------------------------------------------------------------------
class TestClient {
public:
    TestClient()
        : connected_(false)
        , disconnected_(false) {}

    ~TestClient() {
        if (client_) {
            (void)client_->stop();
        }
    }

    // Non-copyable, non-movable
    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;

    /// Create a TCP client and connect to the given port.
    /// TCP facade auto-starts the connection inside create_client().
    bool connectTCP(uint16_t port) {
        facade::tcp_facade tcp;
        facade::tcp_facade::client_config cfg{};
        cfg.host = "127.0.0.1";
        cfg.port = port;
        cfg.client_id = "test-tcp-client";
        client_ = tcp.create_client(cfg);
        attachObserver();
        return awaitConnection(kConnectTimeout);
    }

    /// Create a UDP client and connect to the given port.
    /// UDP facade auto-starts the connection inside create_client().
    bool connectUDP(uint16_t port) {
        facade::udp_facade udp;
        client_ = udp.create_client({
            .host = "127.0.0.1",
            .port = port,
            .client_id = "test-udp-client"
        });
        attachObserver();
        return awaitConnection(kConnectTimeout);
    }

    /// Create a WebSocket client and connect to the given port.
    /// WebSocket facade does NOT auto-start; start() is called explicitly.
    bool connectWS(uint16_t port) {
        facade::websocket_facade ws;
        client_ = ws.create_client({
            .client_id = "test-ws-client"
        });
        attachObserver();
        auto result = client_->start("127.0.0.1", port);
        if (result.is_err()) {
            return false;
        }
        return awaitConnection(kWSConnectTimeout);
    }

    /// Send a NetworkMessage (serialized to wire format).
    bool sendMessage(const NetworkMessage& msg) {
        if (!client_) return false;
        auto data = msg.serialize();
        auto result = client_->send(std::move(data));
        return result.is_ok();
    }

    /// Wait for a received message with timeout.
    std::optional<NetworkMessage> waitForMessage(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(kMessageTimeout)) {
        std::unique_lock<std::mutex> lock(recvMutex_);
        if (recvCV_.wait_for(lock, timeout, [this] { return !receivedMessages_.empty(); })) {
            auto data = std::move(receivedMessages_.front());
            receivedMessages_.erase(receivedMessages_.begin());
            return NetworkMessage::deserialize(data);
        }
        return std::nullopt;
    }

    /// Close the client.
    void close() {
        if (client_) {
            (void)client_->stop();
        }
    }

    bool isConnected() const { return connected_.load(); }
    bool isDisconnected() const { return disconnected_.load(); }

    /// Wait for disconnected state.
    bool waitForDisconnected(std::chrono::milliseconds timeout) {
        return disconnectedFuture_.wait_for(timeout) == std::future_status::ready;
    }

private:
    /// Attach the callback_adapter observer to the client.
    void attachObserver() {
        auto adapter = std::make_shared<interfaces::callback_adapter>();
        adapter->on_connected([this]() {
            connected_.store(true);
            std::call_once(connectedOnce_, [this] {
                connectedPromise_.set_value();
            });
        }).on_receive([this](std::span<const uint8_t> data) {
            std::lock_guard<std::mutex> lock(recvMutex_);
            receivedMessages_.emplace_back(data.begin(), data.end());
            recvCV_.notify_one();
        }).on_disconnected([this](std::optional<std::string_view> /*reason*/) {
            disconnected_.store(true);
            connected_.store(false);
            std::call_once(disconnectedOnce_, [this] {
                disconnectedPromise_.set_value();
            });
        }).on_error([](std::error_code /*ec*/) {
        });

        client_->set_observer(adapter);
    }

    /// Wait for the connected state via callback or is_connected() poll.
    bool awaitConnection(std::chrono::milliseconds timeout) {
        if (!client_) return false;

        // If the connection completed before the observer was attached,
        // the callback was missed. Fall back to is_connected().
        if (client_->is_connected()) {
            connected_.store(true);
            return true;
        }

        return connectedFuture_.wait_for(timeout) == std::future_status::ready;
    }

    std::shared_ptr<interfaces::i_protocol_client> client_;
    std::atomic<bool> connected_;
    std::atomic<bool> disconnected_;

    // Connection synchronization
    std::promise<void> connectedPromise_;
    std::future<void> connectedFuture_{connectedPromise_.get_future()};
    std::once_flag connectedOnce_;

    std::promise<void> disconnectedPromise_;
    std::future<void> disconnectedFuture_{disconnectedPromise_.get_future()};
    std::once_flag disconnectedOnce_;

    // Received message buffer
    std::mutex recvMutex_;
    std::condition_variable recvCV_;
    std::vector<std::vector<uint8_t>> receivedMessages_;
};

// Build a simple test message with opcode and text payload
NetworkMessage makeMessage(uint16_t opcode, const std::string& text) {
    NetworkMessage msg;
    msg.opcode = opcode;
    msg.payload.assign(text.begin(), text.end());
    return msg;
}

} // anonymous namespace

// ===========================================================================
// Test Fixture
// ===========================================================================

class NetworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr_ = std::make_unique<GameNetworkManager>();
    }

    void TearDown() override {
        if (mgr_) {
            mgr_->stopAll();
        }
    }

    /// Register an echo handler: receives a message and sends it back.
    void registerEchoHandler(uint16_t opcode) {
        mgr_->registerHandler(opcode, [this](SessionId sid, const NetworkMessage& msg) {
            (void)mgr_->send(sid, msg);
        });
    }

    /// Register a handler that converts ping to pong.
    void registerPingPongHandler() {
        mgr_->registerHandler(kPingOpcode, [this](SessionId sid, const NetworkMessage& msg) {
            auto reply = makeMessage(kPongOpcode, std::string(msg.payload.begin(), msg.payload.end()));
            (void)mgr_->send(sid, reply);
        });
    }

    std::unique_ptr<GameNetworkManager> mgr_;
};

// ===========================================================================
// TCP Loopback Test
// ===========================================================================

TEST_F(NetworkIntegrationTest, TCPLoopback) {
    registerEchoHandler(kEchoOpcode);

    auto listenResult = mgr_->listen(kTCPPort, Protocol::TCP);
    ASSERT_TRUE(listenResult.hasValue())
        << "Failed to listen on TCP port " << kTCPPort;

    std::this_thread::sleep_for(kStartupDelay);

    TestClient client;
    ASSERT_TRUE(client.connectTCP(kTCPPort)) << "Client failed to connect via TCP";

    auto sent = makeMessage(kEchoOpcode, "Hello TCP");
    ASSERT_TRUE(client.sendMessage(sent));

    auto received = client.waitForMessage();
    ASSERT_TRUE(received.has_value()) << "No echo response received";
    EXPECT_EQ(received->opcode, kEchoOpcode);
    EXPECT_EQ(received->payload, sent.payload);
}

// ===========================================================================
// UDP Loopback Test
// ===========================================================================

TEST_F(NetworkIntegrationTest, UDPLoopback) {
    registerEchoHandler(kEchoOpcode);

    auto listenResult = mgr_->listen(kUDPPort, Protocol::UDP);
    ASSERT_TRUE(listenResult.hasValue())
        << "Failed to listen on UDP port " << kUDPPort;

    std::this_thread::sleep_for(kStartupDelay);

    TestClient client;
    ASSERT_TRUE(client.connectUDP(kUDPPort)) << "Client failed to connect via UDP";

    auto sent = makeMessage(kEchoOpcode, "Hello UDP");
    ASSERT_TRUE(client.sendMessage(sent));

    auto received = client.waitForMessage();
    ASSERT_TRUE(received.has_value()) << "No echo response received";
    EXPECT_EQ(received->opcode, kEchoOpcode);
    EXPECT_EQ(received->payload, sent.payload);
}

// ===========================================================================
// WebSocket Loopback Test
// ===========================================================================

TEST_F(NetworkIntegrationTest, WebSocketLoopback) {
    registerEchoHandler(kEchoOpcode);

    auto listenResult = mgr_->listen(kWSPort, Protocol::WebSocket);
    ASSERT_TRUE(listenResult.hasValue())
        << "Failed to listen on WebSocket port " << kWSPort;

    std::this_thread::sleep_for(kStartupDelay);

    TestClient client;
    ASSERT_TRUE(client.connectWS(kWSPort)) << "Client failed to connect via WebSocket";

    auto sent = makeMessage(kEchoOpcode, "Hello WebSocket");
    ASSERT_TRUE(client.sendMessage(sent));

    auto received = client.waitForMessage();
    ASSERT_TRUE(received.has_value()) << "No echo response received";
    EXPECT_EQ(received->opcode, kEchoOpcode);
    EXPECT_EQ(received->payload, sent.payload);
}

// ===========================================================================
// TCP Multi-Session Broadcast
// ===========================================================================

TEST_F(NetworkIntegrationTest, TCPMultiSession) {
    constexpr uint16_t kBroadcastPort = 19004;
    constexpr int kClientCount = 4;

    // Track connected sessions
    std::atomic<int> connectedCount{0};
    std::mutex connMutex;
    std::condition_variable connCV;

    mgr_->onConnected.connect([&](SessionId /*sid*/) {
        connectedCount.fetch_add(1);
        connCV.notify_all();
    });

    // Handler echoes the message back via broadcast
    mgr_->registerHandler(kEchoOpcode, [this](SessionId /*sid*/, const NetworkMessage& msg) {
        (void)mgr_->broadcast(msg);
    });

    auto listenResult = mgr_->listen(kBroadcastPort, Protocol::TCP);
    ASSERT_TRUE(listenResult.hasValue());

    std::this_thread::sleep_for(kStartupDelay);

    // Connect multiple clients
    std::vector<std::unique_ptr<TestClient>> clients;
    for (int i = 0; i < kClientCount; ++i) {
        auto c = std::make_unique<TestClient>();
        ASSERT_TRUE(c->connectTCP(kBroadcastPort))
            << "Client " << i << " failed to connect";
        clients.push_back(std::move(c));
    }

    // Wait for all connections
    {
        std::unique_lock lock(connMutex);
        ASSERT_TRUE(connCV.wait_for(lock, kConnectTimeout,
            [&] { return connectedCount.load() >= kClientCount; }))
            << "Not all clients connected. Got " << connectedCount.load();
    }

    EXPECT_EQ(mgr_->sessionCount(), static_cast<std::size_t>(kClientCount));

    // First client sends a message, all should receive via broadcast
    auto sent = makeMessage(kEchoOpcode, "broadcast msg");
    ASSERT_TRUE(clients[0]->sendMessage(sent));

    for (std::size_t i = 0; i < static_cast<std::size_t>(kClientCount); ++i) {
        auto received = clients[i]->waitForMessage();
        EXPECT_TRUE(received.has_value())
            << "Client " << i << " did not receive broadcast";
        if (received) {
            EXPECT_EQ(received->opcode, kEchoOpcode);
            EXPECT_EQ(received->payload, sent.payload);
        }
    }
}

// ===========================================================================
// TCP Session Lifecycle (onConnected/onDisconnected signal counts)
// ===========================================================================

TEST_F(NetworkIntegrationTest, TCPSessionLifecycle) {
    constexpr uint16_t kLifecyclePort = 19005;
    constexpr int kClientCount = 3;

    std::atomic<int> connectCount{0};
    std::atomic<int> disconnectCount{0};
    SessionId closedSid;
    std::vector<SessionId> sessionIds;
    std::mutex connMutex;
    std::condition_variable connCV;
    std::mutex dcMutex;
    std::condition_variable dcCV;

    mgr_->onConnected.connect([&](SessionId sid) {
        {
            std::lock_guard<std::mutex> lock(connMutex);
            sessionIds.push_back(sid);
        }
        connectCount.fetch_add(1);
        connCV.notify_all();
    });
    mgr_->onDisconnected.connect([&](SessionId sid) {
        closedSid = sid;
        disconnectCount.fetch_add(1);
        dcCV.notify_all();
    });

    auto listenResult = mgr_->listen(kLifecyclePort, Protocol::TCP);
    ASSERT_TRUE(listenResult.hasValue());

    std::this_thread::sleep_for(kStartupDelay);

    // Create 3 clients
    std::vector<std::unique_ptr<TestClient>> clients;
    for (int i = 0; i < kClientCount; ++i) {
        auto c = std::make_unique<TestClient>();
        ASSERT_TRUE(c->connectTCP(kLifecyclePort));
        clients.push_back(std::move(c));
    }

    // Verify: onConnected fired for all 3 sessions
    {
        std::unique_lock lock(connMutex);
        ASSERT_TRUE(connCV.wait_for(lock, kConnectTimeout,
            [&] { return connectCount.load() >= kClientCount; }))
            << "Not all connections detected. Got " << connectCount.load();
    }
    EXPECT_EQ(connectCount.load(), kClientCount);
    EXPECT_EQ(mgr_->sessionCount(), static_cast<std::size_t>(kClientCount));

    // Verify: sessionInfo() returns valid info for each session
    for (const auto& sid : sessionIds) {
        auto info = mgr_->sessionInfo(sid);
        ASSERT_TRUE(info.has_value()) << "Session info missing for " << sid.value();
        EXPECT_EQ(info->protocol, Protocol::TCP);
    }

    // Close 1 session from server side, verify onDisconnected fires
    auto targetSid = sessionIds[0];
    mgr_->close(targetSid);

    {
        std::unique_lock lock(dcMutex);
        ASSERT_TRUE(dcCV.wait_for(lock, kConnectTimeout,
            [&] { return disconnectCount.load() >= 1; }))
            << "Disconnect signal not received";
    }

    EXPECT_EQ(disconnectCount.load(), 1);
    EXPECT_EQ(closedSid, targetSid);
    EXPECT_EQ(mgr_->sessionCount(), static_cast<std::size_t>(kClientCount - 1));

    // Closed session should no longer have session info
    EXPECT_FALSE(mgr_->sessionInfo(targetSid).has_value());

    // Remaining sessions should still be valid
    for (std::size_t i = 1; i < sessionIds.size(); ++i) {
        EXPECT_TRUE(mgr_->sessionInfo(sessionIds[i]).has_value());
    }
}

// ===========================================================================
// TCP Send to Invalid Session
// ===========================================================================

TEST_F(NetworkIntegrationTest, TCPSendToInvalidSession) {
    auto invalidSid = SessionId(999999);
    auto msg = makeMessage(kEchoOpcode, "should fail");

    auto result = mgr_->send(invalidSid, msg);
    ASSERT_TRUE(result.hasError());
    EXPECT_EQ(result.error().code(), ErrorCode::SessionNotFound);
}

// ===========================================================================
// TCP Opcode Dispatch
// ===========================================================================

TEST_F(NetworkIntegrationTest, TCPOpcodeDispatch) {
    constexpr uint16_t kDispatchPort = 19006;
    constexpr uint16_t kOpA = 0x10;
    constexpr uint16_t kOpB = 0x20;

    // Handler A: echoes with opcode A
    mgr_->registerHandler(kOpA, [this](SessionId sid, const NetworkMessage& msg) {
        auto reply = makeMessage(kOpA, "reply-A");
        (void)mgr_->send(sid, reply);
    });

    // Handler B: echoes with opcode B
    mgr_->registerHandler(kOpB, [this](SessionId sid, const NetworkMessage& msg) {
        auto reply = makeMessage(kOpB, "reply-B");
        (void)mgr_->send(sid, reply);
    });

    auto listenResult = mgr_->listen(kDispatchPort, Protocol::TCP);
    ASSERT_TRUE(listenResult.hasValue());

    std::this_thread::sleep_for(kStartupDelay);

    TestClient client;
    ASSERT_TRUE(client.connectTCP(kDispatchPort));

    // Send opcode A
    ASSERT_TRUE(client.sendMessage(makeMessage(kOpA, "msg-A")));
    auto replyA = client.waitForMessage();
    ASSERT_TRUE(replyA.has_value());
    EXPECT_EQ(replyA->opcode, kOpA);

    // Send opcode B
    ASSERT_TRUE(client.sendMessage(makeMessage(kOpB, "msg-B")));
    auto replyB = client.waitForMessage();
    ASSERT_TRUE(replyB.has_value());
    EXPECT_EQ(replyB->opcode, kOpB);
}

// ===========================================================================
// TCP Ping-Pong (opcode transformation)
// ===========================================================================

TEST_F(NetworkIntegrationTest, TCPPingPong) {
    constexpr uint16_t kPingPongPort = 19007;

    registerPingPongHandler();

    auto listenResult = mgr_->listen(kPingPongPort, Protocol::TCP);
    ASSERT_TRUE(listenResult.hasValue());

    std::this_thread::sleep_for(kStartupDelay);

    TestClient client;
    ASSERT_TRUE(client.connectTCP(kPingPongPort));

    auto ping = makeMessage(kPingOpcode, "ping!");
    ASSERT_TRUE(client.sendMessage(ping));

    auto pong = client.waitForMessage();
    ASSERT_TRUE(pong.has_value());
    EXPECT_EQ(pong->opcode, kPongOpcode);
    EXPECT_EQ(std::string(pong->payload.begin(), pong->payload.end()), "ping!");
}
