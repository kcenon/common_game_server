// auth_load.js — k6 load test for CGS Auth Service (SRS-NFR-008).
//
// Simulates concurrent user authentication requests (register + login)
// against the auth service endpoint.
//
// Usage:
//   k6 run tests/load/auth_load.js                          # smoke
//   k6 run -e SCENARIO=load tests/load/auth_load.js         # load
//   k6 run -e SCENARIO=stress tests/load/auth_load.js       # stress
//   k6 run -e SCENARIO=spike tests/load/auth_load.js        # spike

import http from "k6/http";
import { check, sleep } from "k6";
import { Rate, Trend } from "k6/metrics";

// Custom metrics
const authSuccessRate = new Rate("auth_success_rate");
const loginLatency = new Trend("login_latency", true);

// Configuration from environment
const BASE_URL = __ENV.AUTH_URL || "http://localhost:9001";
const SCENARIO = __ENV.SCENARIO || "smoke";

// Scenario definitions
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
      { duration: "1m", target: 100 },   // Ramp up to 100 users
      { duration: "3m", target: 100 },   // Sustain 100 users
      { duration: "1m", target: 200 },   // Ramp up to 200
      { duration: "3m", target: 200 },   // Sustain 200 users
      { duration: "1m", target: 0 },     // Ramp down
    ],
  },
  stress: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "2m", target: 500 },   // Ramp to 500 users
      { duration: "5m", target: 500 },   // Sustain 500
      { duration: "2m", target: 1000 },  // Push to 1000
      { duration: "5m", target: 1000 },  // Sustain 1000
      { duration: "2m", target: 0 },     // Ramp down
    ],
  },
  spike: {
    executor: "ramping-vus",
    startVUs: 0,
    stages: [
      { duration: "30s", target: 10 },   // Warm up
      { duration: "10s", target: 1000 }, // Spike to 1000
      { duration: "1m", target: 1000 },  // Hold spike
      { duration: "30s", target: 10 },   // Drop back
      { duration: "1m", target: 10 },    // Recovery
      { duration: "10s", target: 0 },    // Ramp down
    ],
  },
};

export const options = {
  scenarios: {
    auth_test: scenarios[SCENARIO],
  },
  thresholds: {
    http_req_duration: ["p(95)<500", "p(99)<1000"],
    auth_success_rate: ["rate>0.95"],
    login_latency: ["p(95)<400"],
  },
};

// Per-VU unique user ID
let counter = 0;

export default function () {
  const userId = `loadtest_user_${__VU}_${counter++}`;
  const password = "LoadTest!Password123";

  // Step 1: Register (POST /auth/register)
  const registerPayload = JSON.stringify({
    username: userId,
    password: password,
    email: `${userId}@loadtest.local`,
  });

  const registerRes = http.post(`${BASE_URL}/auth/register`, registerPayload, {
    headers: { "Content-Type": "application/json" },
    tags: { name: "register" },
  });

  // Step 2: Login (POST /auth/login)
  const loginPayload = JSON.stringify({
    username: userId,
    password: password,
  });

  const loginStart = Date.now();
  const loginRes = http.post(`${BASE_URL}/auth/login`, loginPayload, {
    headers: { "Content-Type": "application/json" },
    tags: { name: "login" },
  });
  loginLatency.add(Date.now() - loginStart);

  const loginOk = check(loginRes, {
    "login status 200": (r) => r.status === 200,
    "login has token": (r) => {
      try {
        const body = JSON.parse(r.body);
        return body.access_token !== undefined;
      } catch (_e) {
        return false;
      }
    },
  });

  authSuccessRate.add(loginOk);

  // Step 3: Validate token (GET /auth/validate)
  if (loginOk && loginRes.body) {
    try {
      const token = JSON.parse(loginRes.body).access_token;
      const validateRes = http.get(`${BASE_URL}/auth/validate`, {
        headers: {
          Authorization: `Bearer ${token}`,
          "Content-Type": "application/json",
        },
        tags: { name: "validate" },
      });

      check(validateRes, {
        "validate status 200": (r) => r.status === 200,
      });
    } catch (_e) {
      // Token parse failed, skip validation
    }
  }

  sleep(Math.random() * 0.5 + 0.1); // 100-600ms think time
}

export function handleSummary(data) {
  const summary = {
    scenario: SCENARIO,
    timestamp: new Date().toISOString(),
    metrics: {
      http_reqs: data.metrics.http_reqs?.values?.count || 0,
      http_req_duration_p95:
        data.metrics.http_req_duration?.values["p(95)"] || 0,
      http_req_duration_p99:
        data.metrics.http_req_duration?.values["p(99)"] || 0,
      auth_success_rate:
        data.metrics.auth_success_rate?.values?.rate || 0,
      login_latency_p95:
        data.metrics.login_latency?.values["p(95)"] || 0,
    },
    thresholds: Object.fromEntries(
      Object.entries(data.metrics)
        .filter(([_, v]) => v.thresholds)
        .map(([k, v]) => [k, v.thresholds])
    ),
  };

  return {
    stdout: JSON.stringify(summary, null, 2) + "\n",
    "tests/load/results/auth_summary.json": JSON.stringify(summary, null, 2),
  };
}
