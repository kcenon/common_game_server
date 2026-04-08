// examples/10_networking.cpp
//
// Tutorial: Network adapter, messages, and signals
// See: docs/tutorial_networking.dox
//
// This example focuses on the portable, offline parts of the
// GameNetworkManager API:
//
//   1. Constructing a NetworkMessage with an opcode and payload
//   2. Serializing / deserializing the binary wire format
//   3. Subscribing to Signal<T> lifecycle events
//   4. Emitting a signal manually so the slot runs
//   5. Registering a message handler that matches an opcode
//
// The example does NOT actually bind a socket or listen. Real
// networking tests live in the integration suite. Here we exercise
// every part of the API that does not require a live peer.

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_network_manager.hpp"
#include "cgs/foundation/signal.hpp"
#include "cgs/foundation/types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using cgs::foundation::ErrorCode;
using cgs::foundation::GameNetworkManager;
using cgs::foundation::NetworkMessage;
using cgs::foundation::Protocol;
using cgs::foundation::protocolName;
using cgs::foundation::SessionId;
using cgs::foundation::Signal;

namespace {

// Opcode constants for the example's custom "chat" protocol.
// In production you would gather these in a shared header so
// both the client and the server agree on the wire format.
constexpr uint16_t kOpcodeChatMessage = 0x0100;
constexpr uint16_t kOpcodePing = 0x0001;

/// Construct a chat message with a UTF-8 text payload.
NetworkMessage BuildChatMessage(std::string_view text) {
    NetworkMessage msg;
    msg.opcode = kOpcodeChatMessage;
    msg.payload.assign(text.begin(), text.end());
    return msg;
}

/// Pretty-print a NetworkMessage for tracing.
void PrintMessage(const NetworkMessage& msg) {
    std::cout << "  opcode 0x" << std::hex << msg.opcode << std::dec
              << ", payload " << msg.payload.size() << " bytes: \"";
    std::cout.write(reinterpret_cast<const char*>(msg.payload.data()),
                    static_cast<std::streamsize>(msg.payload.size()));
    std::cout << "\"\n";
}

}  // namespace

int main() {
    // ── Step 1: Build and serialize a message. ──────────────────────────
    auto original = BuildChatMessage("hello, world!");
    std::cout << "original:\n";
    PrintMessage(original);

    const auto bytes = original.serialize();
    std::cout << "serialized " << bytes.size() << " bytes on the wire\n";

    // ── Step 2: Deserialize the bytes back into a message. ──────────────
    auto decoded = NetworkMessage::deserialize(bytes);
    if (!decoded.has_value()) {
        std::cerr << "deserialize failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "decoded:\n";
    PrintMessage(*decoded);

    // ── Step 3: Exercise a Signal<SessionId> without a live network. ───
    //
    // GameNetworkManager exposes onConnected / onDisconnected / onError
    // as Signal<...> members. The underlying Signal type is generic;
    // we construct a standalone one here to demonstrate connect/emit.
    Signal<SessionId> onLogin;
    std::vector<SessionId> captured;

    auto slotId = onLogin.connect([&captured](SessionId sid) {
        captured.push_back(sid);
    });

    onLogin.emit(SessionId(42));
    onLogin.emit(SessionId(1337));
    std::cout << "slots observed " << captured.size() << " login event(s)\n";

    onLogin.disconnect(slotId);
    onLogin.emit(SessionId(9999));  // no slots connected -> no-op
    std::cout << "after disconnect, slots observed "
              << captured.size() << " login event(s)\n";

    // ── Step 4: Register a message handler on a real manager. ──────────
    //
    // Constructing GameNetworkManager is cheap; calling listen() is
    // what actually opens a socket. We skip the listen step but do
    // register a handler to show the API.
    GameNetworkManager net;

    net.registerHandler(kOpcodePing, [](SessionId sid, const NetworkMessage& msg) {
        std::cout << "  received ping from session " << sid.value()
                  << " (" << msg.payload.size() << "-byte payload)\n";
    });

    // Subscribe to the manager's built-in signals so game logic can
    // react to connect / disconnect / error events.
    net.onConnected.connect([](SessionId sid) {
        std::cout << "  session " << sid.value() << " connected\n";
    });
    net.onDisconnected.connect([](SessionId sid) {
        std::cout << "  session " << sid.value() << " disconnected\n";
    });
    net.onError.connect([](SessionId sid, ErrorCode code) {
        std::cout << "  session " << sid.value() << " error "
                  << static_cast<uint32_t>(code) << "\n";
    });

    // ── Step 5: Talk about protocols without binding. ──────────────────
    std::cout << "available protocols: "
              << protocolName(Protocol::TCP) << ", "
              << protocolName(Protocol::UDP) << ", "
              << protocolName(Protocol::WebSocket) << "\n";

    // Calling listen() with no server environment would typically
    // return a GameResult<void> error with code ListenFailed. We
    // skip it in the example and document the pattern in the
    // tutorial instead.

    return EXIT_SUCCESS;
}
