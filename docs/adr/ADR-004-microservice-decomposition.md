---
doc_id: "CGS-ADR-004"
doc_title: "ADR-004: Five-Service Microservice Decomposition"
doc_version: "1.0.0"
doc_date: "2026-04-08"
doc_status: "Accepted"
project: "common_game_server"
category: "ADR"
---

# ADR-004: Five-Service Microservice Decomposition

> **SSOT**: This document is the single source of truth for **ADR-004**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2026-02-03 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

`common_game_server` needs to scale horizontally to serve 10,000+ concurrent
users per cluster. A monolithic process cannot achieve this on commodity
hardware — it would saturate memory, CPU, or both.

The natural seams for splitting a game server are:

- **Authentication** — JWT issuance, session storage. Bursty during login
  surges; otherwise idle. Different scaling profile than gameplay.
- **Network ingress** — TCP/WebSocket accept loop, TLS termination, routing.
  CPU-bound; needs many replicas.
- **World simulation** — ECS tick loop, game logic. Memory-bound (entity
  state); needs CPU pinning.
- **Lobby / matchmaking** — Coordinates players into matches; periodic
  background work; not latency-critical.
- **Database access** — Connection pooling and prepared statements; needs
  affinity to a primary DB instance.

The four legacy projects took different approaches:
- One was monolithic (single process)
- One used 3 services (gateway, game, db)
- Two used different 4-service splits

## Decision

**Decompose the framework into 5 standalone services.**

| Service | Responsibility |
|---------|---------------|
| **AuthServer** | JWT RS256 issuance/verification, session store, login rate limit |
| **GatewayServer** | Client TCP/WebSocket termination, routing, load balancing, rate limit |
| **GameServer** | World simulation (ECS), game tick loop @ 20 Hz, map instances |
| **LobbyServer** | ELO matchmaking, party management, region-based queues |
| **DBProxy** | PostgreSQL connection pool, prepared statements, query routing |

Additional decisions:

1. **Shared `service_runner` template** — All services share one bootstrap
   template (config loading, signal handling, graceful shutdown, health probes)
2. **YAML configuration** — Each service has a single YAML config file
3. **Inter-service communication** — gRPC over TLS (or local UNIX sockets in dev)
4. **Service binaries** — Each service is a standalone executable, not a library
5. **Kubernetes-first deployment** — All services have Deployment + Service +
   HPA + PDB manifests; DBProxy uses StatefulSet for connection affinity

## Alternatives Considered

### Monolithic process

- **Pros**: Simpler deployment; no inter-service network overhead.
- **Cons**: Cannot scale beyond a single machine; restart impacts all
  functionality; fault isolation is poor.

### Many services (10+)

- **Pros**: Maximum granularity; each service has a single responsibility.
- **Cons**: Network overhead between services dominates; deployment
  complexity explodes; debugging cross-service flows becomes painful.

### 3 services (gateway / game / db)

- **Pros**: Simpler than 5; matches the original architecture of one of the
  legacy projects.
- **Cons**: AuthServer cannot scale independently from gameplay; LobbyServer
  is forced into the GameServer process, creating CPU contention.

## Consequences

### Positive

- **Independent scaling** — Each service scales according to its own load profile
- **Fault isolation** — A crashed AuthServer doesn't bring down active gameplay
- **Specialized hardware** — DBProxy can run on a node close to the DB; GameServer
  on CPU-pinned nodes
- **Gradual rollouts** — Each service can be upgraded independently
- **Rate-limit at the edge** — GatewayServer enforces token-bucket rate limits
  before requests reach backend services

### Negative

- **5 binaries to ship** — 5x deployment artifacts. Mitigated by shared base
  Dockerfile and shared CI build matrix.
- **Inter-service latency** — gRPC adds ~1 ms per hop. Mitigated by co-locating
  services on the same Kubernetes node where possible.
- **Distributed debugging** — Issues span multiple services. Mitigated by
  correlation IDs flowing through every service hop and by structured logging.
- **Configuration drift risk** — 5 YAML files can drift apart. Mitigated by
  the shared `service_runner` template enforcing common config keys.

## Service-to-Service Topology

```
              ┌─────────────────┐
              │  Game Clients   │
              └────────┬────────┘
                       │ TLS 1.3
              ┌────────▼────────┐
              │ GatewayServer   │ (HPA, n replicas)
              └────────┬────────┘
                       │ gRPC/TLS
       ┌───────────────┼───────────────┐
       │               │               │
       ▼               ▼               ▼
┌──────────┐    ┌──────────┐    ┌──────────┐
│AuthServer│    │GameServer│    │LobbyServer│
└────┬─────┘    └────┬─────┘    └────┬─────┘
     │               │                │
     └───────────────┼────────────────┘
                     │ gRPC/TLS
              ┌──────▼──────┐
              │  DBProxy    │ (StatefulSet)
              └──────┬──────┘
                     │
              ┌──────▼──────┐
              │ PostgreSQL  │
              └─────────────┘
```

## References

- [`../ARCHITECTURE.md#layer-3---service-layer`](../ARCHITECTURE.md) — Service layer overview
- [`../guides/DEPLOYMENT_GUIDE.md`](../guides/DEPLOYMENT_GUIDE.md) — Production deployment
- [`../guides/CONFIGURATION_GUIDE.md`](../guides/CONFIGURATION_GUIDE.md) — Service configuration
- [`../PRODUCTION_QUALITY.md`](../PRODUCTION_QUALITY.md) — SLOs per service
- [`ADR-001-unified-game-server-architecture.md`](ADR-001-unified-game-server-architecture.md) — Why unified framework
