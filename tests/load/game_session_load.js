// game_session_load.js — k6 load test for game session lifecycle.
//
// Simulates the full player session flow:
//   1. Authenticate via auth service
//   2. Connect through gateway
//   3. Join a game instance
//   4. Send periodic game actions (move, attack, chat)
//   5. Leave the instance
//
// This validates end-to-end system behavior under CCU load.
//
// Usage:
//   k6 run tests/load/game_session_load.js                      # smoke
//   k6 run -e SCENARIO=ccu_1k tests/load/game_session_load.js   # 1K CCU

import http from "k6/http";
import { check, sleep, group } from "k6";
import { Rate, Counter, Trend } from "k6/metrics";

// Custom metrics
const sessionSuccessRate = new Rate("session_success_rate");
const sessionsCompleted = new Counter("sessions_completed");
const sessionDuration = new Trend("session_duration_ms", true);
const joinLatency = new Trend("game_join_latency", true);

// Configuration
const GATEWAY_HTTP = __ENV.GATEWAY_HTTP || "http://localhost:8080";
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
      { duration: "1m", target: 50 },
      { duration: "3m", target: 50 },
      { duration: "1m", target: 0 },
    ],
  },
  ccu_1k: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "2m", target: 250 },
      { duration: "2m", target: 500 },
      { duration: "2m", target: 1000 },   // Target: 1K CCU
      { duration: "5m", target: 1000 },   // Sustain
      { duration: "2m", target: 0 },
    ],
  },
  ccu_10k: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "3m", target: 2500 },
      { duration: "3m", target: 5000 },
      { duration: "3m", target: 10000 },
      { duration: "5m", target: 10000 },
      { duration: "3m", target: 0 },
    ],
  },
};

export const options = {
  scenarios: {
    game_session: scenarios[SCENARIO],
  },
  thresholds: {
    session_success_rate: ["rate>0.90"],
    game_join_latency: ["p(95)<2000"],
    session_duration_ms: ["p(95)<30000"],
    http_req_duration: ["p(95)<1000"],
  },
};

let userCounter = 0;

export default function () {
  const sessionStart = Date.now();
  const userId = `session_${__VU}_${userCounter++}`;
  let success = false;

  group("authenticate", function () {
    // Register
    http.post(
      `${AUTH_URL}/auth/register`,
      JSON.stringify({
        username: userId,
        password: "SessionTest!123",
        email: `${userId}@loadtest.local`,
      }),
      {
        headers: { "Content-Type": "application/json" },
        tags: { name: "session_register" },
      }
    );

    // Login
    const loginRes = http.post(
      `${AUTH_URL}/auth/login`,
      JSON.stringify({
        username: userId,
        password: "SessionTest!123",
      }),
      {
        headers: { "Content-Type": "application/json" },
        tags: { name: "session_login" },
      }
    );

    check(loginRes, {
      "login ok": (r) => r.status === 200,
    });
  });

  group("join_game", function () {
    const joinStart = Date.now();
    const joinRes = http.post(
      `${GATEWAY_HTTP}/game/join`,
      JSON.stringify({
        player_id: userId,
        instance_id: Math.floor(Math.random() * 10), // Random instance 0-9
      }),
      {
        headers: { "Content-Type": "application/json" },
        tags: { name: "game_join" },
      }
    );
    joinLatency.add(Date.now() - joinStart);

    check(joinRes, {
      "join accepted": (r) => r.status === 200 || r.status === 201,
    });
  });

  group("game_actions", function () {
    // Simulate 10 game actions (approx 1 second of gameplay at 10 tps)
    for (let i = 0; i < 10; i++) {
      const actionRes = http.post(
        `${GATEWAY_HTTP}/game/action`,
        JSON.stringify({
          player_id: userId,
          action: ["move", "attack", "use_skill", "chat"][
            Math.floor(Math.random() * 4)
          ],
          payload: {
            x: Math.random() * 1000,
            y: Math.random() * 1000,
            target: Math.floor(Math.random() * 100),
          },
          seq: i,
        }),
        {
          headers: { "Content-Type": "application/json" },
          tags: { name: "game_action" },
        }
      );

      check(actionRes, {
        "action accepted": (r) => r.status === 200 || r.status === 202,
      });

      sleep(0.1); // 100ms between actions
    }
    success = true;
  });

  group("leave_game", function () {
    const leaveRes = http.post(
      `${GATEWAY_HTTP}/game/leave`,
      JSON.stringify({ player_id: userId }),
      {
        headers: { "Content-Type": "application/json" },
        tags: { name: "game_leave" },
      }
    );

    check(leaveRes, {
      "leave ok": (r) => r.status === 200,
    });
  });

  sessionDuration.add(Date.now() - sessionStart);
  sessionSuccessRate.add(success);
  if (success) {
    sessionsCompleted.add(1);
  }

  sleep(Math.random() * 2 + 1); // 1-3s between sessions
}

export function handleSummary(data) {
  const summary = {
    scenario: SCENARIO,
    timestamp: new Date().toISOString(),
    metrics: {
      vus_max: data.metrics.vus_max?.values?.max || 0,
      sessions_completed:
        data.metrics.sessions_completed?.values?.count || 0,
      session_success_rate:
        data.metrics.session_success_rate?.values?.rate || 0,
      game_join_latency_p95:
        data.metrics.game_join_latency?.values["p(95)"] || 0,
      session_duration_p95:
        data.metrics.session_duration_ms?.values["p(95)"] || 0,
      http_reqs: data.metrics.http_reqs?.values?.count || 0,
      http_req_duration_avg:
        data.metrics.http_req_duration?.values?.avg || 0,
      http_req_duration_p95:
        data.metrics.http_req_duration?.values["p(95)"] || 0,
    },
  };

  return {
    stdout: JSON.stringify(summary, null, 2) + "\n",
    "tests/load/results/game_session_summary.json": JSON.stringify(
      summary,
      null,
      2
    ),
  };
}
