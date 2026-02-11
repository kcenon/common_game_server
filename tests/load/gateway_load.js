// gateway_load.js — k6 load test for CGS Gateway Service (SRS-NFR-007).
//
// Simulates concurrent client connections through the gateway, exercising
// authentication handshake + message routing under sustained load.
//
// Usage:
//   k6 run tests/load/gateway_load.js                          # smoke
//   k6 run -e SCENARIO=load tests/load/gateway_load.js         # load
//   k6 run -e SCENARIO=ccu_1k tests/load/gateway_load.js       # 1K CCU
//   k6 run -e SCENARIO=ccu_10k tests/load/gateway_load.js      # 10K CCU

import http from "k6/http";
import ws from "k6/ws";
import { check, sleep } from "k6";
import { Rate, Counter, Trend } from "k6/metrics";

// Custom metrics
const connectionSuccessRate = new Rate("connection_success_rate");
const messagesRouted = new Counter("messages_routed");
const connectionLatency = new Trend("connection_latency", true);
const messageLatency = new Trend("message_latency", true);

// Configuration from environment
const GATEWAY_HTTP = __ENV.GATEWAY_HTTP || "http://localhost:8080";
const GATEWAY_WS = __ENV.GATEWAY_WS || "ws://localhost:8081";
const AUTH_URL = __ENV.AUTH_URL || "http://localhost:9001";
const SCENARIO = __ENV.SCENARIO || "smoke";

const scenarios = {
  smoke: {
    executor: "constant-vus",
    vus: 5,
    duration: "30s",
  },
  load: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "1m", target: 100 },
      { duration: "5m", target: 100 },
      { duration: "1m", target: 0 },
    ],
  },
  ccu_1k: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "2m", target: 500 },
      { duration: "2m", target: 1000 },   // SRS-NFR-008: 1K per node
      { duration: "5m", target: 1000 },   // Sustain at 1K CCU
      { duration: "2m", target: 0 },
    ],
  },
  ccu_10k: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "3m", target: 2000 },
      { duration: "3m", target: 5000 },
      { duration: "3m", target: 10000 },  // SRS-NFR-010: 10K per cluster
      { duration: "5m", target: 10000 },
      { duration: "3m", target: 0 },
    ],
  },
};

export const options = {
  scenarios: {
    gateway_test: scenarios[SCENARIO],
  },
  thresholds: {
    http_req_duration: ["p(95)<1000"],
    connection_success_rate: ["rate>0.95"],
    connection_latency: ["p(95)<2000"],
    message_latency: ["p(95)<100"],
  },
};

let userCounter = 0;

// Authenticate and obtain a token from the auth service.
function getToken() {
  const userId = `gw_load_${__VU}_${userCounter++}`;
  const payload = JSON.stringify({
    username: userId,
    password: "GwLoadTest!123",
    email: `${userId}@loadtest.local`,
  });

  // Register (may fail if user exists, that is fine)
  http.post(`${AUTH_URL}/auth/register`, payload, {
    headers: { "Content-Type": "application/json" },
    tags: { name: "gw_register" },
  });

  const loginRes = http.post(
    `${AUTH_URL}/auth/login`,
    JSON.stringify({
      username: userId,
      password: "GwLoadTest!123",
    }),
    {
      headers: { "Content-Type": "application/json" },
      tags: { name: "gw_login" },
    }
  );

  try {
    return JSON.parse(loginRes.body).access_token || "";
  } catch (_e) {
    return "";
  }
}

export default function () {
  // Phase 1: Obtain auth token
  const token = getToken();

  // Phase 2: HTTP health-check through gateway
  const connectStart = Date.now();
  const healthRes = http.get(`${GATEWAY_HTTP}/health`, {
    headers: { Authorization: `Bearer ${token}` },
    tags: { name: "gw_health" },
  });
  connectionLatency.add(Date.now() - connectStart);

  const connected = check(healthRes, {
    "gateway reachable": (r) => r.status === 200 || r.status === 204,
  });
  connectionSuccessRate.add(connected);

  // Phase 3: WebSocket session simulation
  const wsUrl = `${GATEWAY_WS}/game?token=${token}`;
  const wsRes = ws.connect(wsUrl, {}, function (socket) {
    socket.on("open", function () {
      // Send a ping/heartbeat message
      const msg = JSON.stringify({
        type: "ping",
        timestamp: Date.now(),
      });
      socket.send(msg);
      messagesRouted.add(1);

      // Simulate periodic game messages
      for (let i = 0; i < 5; i++) {
        const sendStart = Date.now();
        socket.send(
          JSON.stringify({
            type: "game_action",
            action: "move",
            payload: { x: Math.random() * 100, y: Math.random() * 100 },
            seq: i,
          })
        );
        messagesRouted.add(1);
        messageLatency.add(Date.now() - sendStart);
        sleep(0.1); // 100ms between messages (10 msg/sec per user)
      }
    });

    socket.on("message", function () {
      messagesRouted.add(1);
    });

    socket.on("error", function (e) {
      console.error(`WebSocket error: ${e.error()}`);
    });

    socket.setTimeout(function () {
      socket.close();
    }, 3000); // Keep connection alive for 3 seconds
  });

  check(wsRes, {
    "ws connection status 101": (r) => r && r.status === 101,
  });

  sleep(Math.random() * 1 + 0.5); // 500-1500ms think time
}

export function handleSummary(data) {
  const summary = {
    scenario: SCENARIO,
    timestamp: new Date().toISOString(),
    metrics: {
      http_reqs: data.metrics.http_reqs?.values?.count || 0,
      vus_max: data.metrics.vus_max?.values?.max || 0,
      connection_success_rate:
        data.metrics.connection_success_rate?.values?.rate || 0,
      messages_routed:
        data.metrics.messages_routed?.values?.count || 0,
      connection_latency_p95:
        data.metrics.connection_latency?.values["p(95)"] || 0,
      message_latency_p95:
        data.metrics.message_latency?.values["p(95)"] || 0,
    },
  };

  return {
    stdout: JSON.stringify(summary, null, 2) + "\n",
    "tests/load/results/gateway_summary.json": JSON.stringify(
      summary,
      null,
      2
    ),
  };
}
