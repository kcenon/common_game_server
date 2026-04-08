// examples/11_client.cpp
//
// Tutorial: Writing a client against GameNetworkManager
// See: docs/tutorial_client.dox
//
// This example runs a full server + client round-trip inside a single
// process. It is the companion program to docs/tutorial_client.dox
// and mirrors the pattern used by the project's integration test
// suite (tests/integration/foundation/network_integration_test.cpp).
//
// Architecture:
//
//   [main thread]
//       |
//       |  1) create GameNetworkManager, register ping-pong handler
//       |  2) listen(kPort, Protocol::TCP)
//       |
//       +---[kcenon network worker threads] ------ server accept loop
//       |
//       |  3) create tcp_facade::client_config, create_client()
//       |  4) attach callback_adapter observer (on_connected, on_receive, ...)
//       |  5) send ping opcode with a text payload
//       |
//       +---[kcenon network worker threads] ------ client I/O
//       |
//       |  6) wait (with timeout) until observer pushes the pong reply
//       |  7) verify payload, close client, stopAll() the server
//       |  8) exit 0
//
// Because the server and client live in the same process, the whole
// round-trip is deterministic — no external infrastructure is needed
// and CI can run the binary as a regression check.

#include "cgs/foundation/game_network_manager.hpp"

#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <kcenon/network/interfaces/i_protocol_client.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

using cgs::foundation::GameNetworkManager;
using cgs::foundation::NetworkMessage;
using cgs::foundation::Protocol;
using cgs::foundation::SessionId;

// Shared opcode constants. In a real project these go in a header both
// the client and the server include so the wire contract stays in sync.
constexpr uint16_t kOpcodePing = 0x0001;
constexpr uint16_t kOpcodePong = 0x0002;

// A fixed high port to avoid collisions with common services.
constexpr uint16_t kPort = 19101;

// Conservative timeouts so the example still terminates if something
// goes wrong in CI.
constexpr auto kConnectTimeout = std::chrono::seconds(5);
constexpr auto kReplyTimeout = std::chrono::seconds(5);
constexpr auto kStartupDelay = std::chrono::milliseconds(200);

// ─── Helper: build a NetworkMessage from text ───────────────────────────
NetworkMessage MakeMessage(uint16_t opcode, const std::string& text) {
    NetworkMessage msg;
    msg.opcode = opcode;
    msg.payload.assign(text.begin(), text.end());
    return msg;
}

// ─── TestClient: minimal RAII client wrapper ────────────────────────────
//
// This is a condensed version of tests/integration/foundation/
// network_integration_test.cpp's TestClient. In production code you
// would factor out the shared bits into a reusable helper header.
class LoopbackClient {
public:
    /// Connect a TCP client to 127.0.0.1:port. Blocks up to `timeout`
    /// for the connected-state callback. Returns false on timeout.
    bool ConnectTCP(uint16_t port, std::chrono::milliseconds timeout) {
        kcenon::network::facade::tcp_facade tcp;
        kcenon::network::facade::tcp_facade::client_config cfg{};
        cfg.host = "127.0.0.1";
        cfg.port = port;
        cfg.client_id = "tutorial-client";

        // tcp_facade::create_client auto-starts the client, so the
        // on_connected callback may fire before we even attach the
        // observer. We handle that case by falling back to
        // is_connected() below.
        client_ = tcp.create_client(cfg);
        AttachObserver();

        if (client_ && client_->is_connected()) {
            connected_.store(true);
            return true;
        }
        return connectedFuture_.wait_for(timeout) == std::future_status::ready;
    }

    /// Send a fully-framed NetworkMessage. Returns false if the
    /// facade rejects the write (e.g., the connection died).
    bool Send(const NetworkMessage& msg) {
        if (!client_) {
            return false;
        }
        auto data = msg.serialize();
        auto result = client_->send(std::move(data));
        return result.is_ok();
    }

    /// Wait for one inbound message (blocks up to `timeout`).
    /// Returns nullopt if nothing arrived or the bytes failed to
    /// deserialize back into a NetworkMessage.
    std::optional<NetworkMessage> WaitForMessage(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(recvMutex_);
        if (!recvCv_.wait_for(lock, timeout, [this] { return !received_.empty(); })) {
            return std::nullopt;
        }
        auto bytes = std::move(received_.front());
        received_.erase(received_.begin());
        return NetworkMessage::deserialize(bytes);
    }

    void Close() {
        if (client_) {
            (void)client_->stop();
        }
    }

private:
    /// Install the callback_adapter observer. All callbacks run on a
    /// kcenon network worker thread; the example synchronizes with the
    /// main thread via a promise (for connect) and a condvar queue
    /// (for received bytes).
    void AttachObserver() {
        auto adapter = std::make_shared<kcenon::network::interfaces::callback_adapter>();
        adapter
            ->on_connected([this]() {
                connected_.store(true);
                try {
                    connectedPromise_.set_value();
                } catch (const std::future_error&) {
                    // Already set — harmless; means is_connected() was
                    // true when the observer was attached.
                }
            })
            .on_receive([this](std::span<const uint8_t> data) {
                std::lock_guard<std::mutex> lock(recvMutex_);
                received_.emplace_back(data.begin(), data.end());
                recvCv_.notify_one();
            })
            .on_disconnected([this](std::optional<std::string_view> /*reason*/) {
                connected_.store(false);
            })
            .on_error([](std::error_code /*ec*/) {
                // Real applications should log this via GameLogger.
            });

        client_->set_observer(adapter);
    }

    std::shared_ptr<kcenon::network::interfaces::i_protocol_client> client_;

    std::atomic<bool> connected_{false};
    std::promise<void> connectedPromise_;
    std::future<void> connectedFuture_{connectedPromise_.get_future()};

    std::mutex recvMutex_;
    std::condition_variable recvCv_;
    std::vector<std::vector<uint8_t>> received_;
};

int main() {
    // ── Step 1: Start a GameNetworkManager with a ping-pong handler. ──
    //
    // The handler echoes ping payloads back with an opcode of Pong.
    // It runs on the server-side I/O thread — keep it short.
    GameNetworkManager server;
    server.registerHandler(kOpcodePing, [&server](SessionId sid, const NetworkMessage& msg) {
        auto reply = MakeMessage(kOpcodePong,
                                  std::string(msg.payload.begin(), msg.payload.end()));
        (void)server.send(sid, reply);
    });

    auto listenResult = server.listen(kPort, Protocol::TCP);
    if (!listenResult) {
        std::cerr << "listen failed: " << listenResult.error().message() << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "server listening on TCP 127.0.0.1:" << kPort << "\n";

    // A small sleep lets the kcenon acceptor finish binding before
    // the client attempts a connection. Real production code would
    // poll or use an explicit "ready" signal; for a tutorial example
    // a 200 ms delay is adequate and deterministic.
    std::this_thread::sleep_for(kStartupDelay);

    // ── Step 2: Construct the client and connect. ─────────────────────
    LoopbackClient client;
    if (!client.ConnectTCP(kPort, kConnectTimeout)) {
        std::cerr << "client connect failed or timed out\n";
        server.stopAll();
        return EXIT_FAILURE;
    }
    std::cout << "client connected\n";

    // ── Step 3: Send a ping and wait for the pong reply. ──────────────
    const auto ping = MakeMessage(kOpcodePing, "hello from client");
    if (!client.Send(ping)) {
        std::cerr << "client send failed\n";
        client.Close();
        server.stopAll();
        return EXIT_FAILURE;
    }
    std::cout << "client sent ping (" << ping.payload.size() << " bytes)\n";

    auto reply = client.WaitForMessage(kReplyTimeout);
    if (!reply) {
        std::cerr << "no reply received within timeout\n";
        client.Close();
        server.stopAll();
        return EXIT_FAILURE;
    }

    std::string replyText(reply->payload.begin(), reply->payload.end());
    std::cout << "client received opcode 0x" << std::hex << reply->opcode
              << std::dec << " payload \"" << replyText << "\"\n";

    // ── Step 4: Verify the round-trip. ────────────────────────────────
    const bool ok = (reply->opcode == kOpcodePong) && (reply->payload == ping.payload);
    std::cout << (ok ? "round-trip OK" : "round-trip MISMATCH") << "\n";

    // ── Step 5: Clean shutdown. Always stop the client before the
    //         server so the session disconnect callbacks fire cleanly.
    client.Close();
    server.stopAll();

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
