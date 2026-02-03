# Configuration Guide

> Version: 0.1.0.0
> Last Updated: 2026-02-03
> Status: Foundation Reference

## Overview

This document defines the configuration system for the unified game server, including YAML configuration formats, environment variables, runtime configuration, and hot-reload capabilities.

---

## Table of Contents

1. [Configuration Architecture](#configuration-architecture)
2. [Server Configuration](#server-configuration)
3. [Database Configuration](#database-configuration)
4. [Network Configuration](#network-configuration)
5. [Game Configuration](#game-configuration)
6. [Logging Configuration](#logging-configuration)
7. [Security Configuration](#security-configuration)
8. [Environment Variables](#environment-variables)
9. [Hot Reload](#hot-reload)
10. [Configuration Validation](#configuration-validation)

---

## Configuration Architecture

### Configuration Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                    Configuration Layers                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Priority (High to Low):                                         │
│                                                                  │
│  1. Command-line arguments    --port=8080                       │
│  2. Environment variables     GAME_SERVER_PORT=8080             │
│  3. Local config file         ./config/local.yaml               │
│  4. Environment config        ./config/production.yaml          │
│  5. Default config            ./config/default.yaml             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Configuration File Structure

```
config/
├── default.yaml           # Base configuration (committed)
├── development.yaml       # Development overrides (committed)
├── staging.yaml          # Staging overrides (committed)
├── production.yaml       # Production overrides (committed)
├── local.yaml            # Local developer overrides (gitignored)
├── secrets/              # Secret files (gitignored)
│   ├── database.yaml
│   ├── encryption.yaml
│   └── api_keys.yaml
└── game/                 # Game-specific configs
    ├── balance.yaml
    ├── items.yaml
    ├── skills.yaml
    └── maps.yaml
```

### Configuration Loader

```cpp
// config_loader.h
#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace config {

class ConfigLoader {
public:
    // Load configuration with environment awareness
    static YAML::Node load(const std::string& environment = "") {
        YAML::Node config;

        // 1. Load default config
        config = loadAndMerge(config, "config/default.yaml");

        // 2. Load environment-specific config
        std::string env = environment;
        if (env.empty()) {
            env = getEnvironmentName();
        }

        std::string envFile = "config/" + env + ".yaml";
        if (std::filesystem::exists(envFile)) {
            config = loadAndMerge(config, envFile);
        }

        // 3. Load local overrides (if exists)
        if (std::filesystem::exists("config/local.yaml")) {
            config = loadAndMerge(config, "config/local.yaml");
        }

        // 4. Load secrets
        loadSecrets(config);

        // 5. Apply environment variable overrides
        applyEnvironmentOverrides(config);

        return config;
    }

private:
    static YAML::Node loadAndMerge(YAML::Node base, const std::string& path);
    static void loadSecrets(YAML::Node& config);
    static void applyEnvironmentOverrides(YAML::Node& config);
    static std::string getEnvironmentName();
};

} // namespace config
```

---

## Server Configuration

### default.yaml

```yaml
# =============================================================================
# Game Server Default Configuration
# =============================================================================

# -----------------------------------------------------------------------------
# Server Identity
# -----------------------------------------------------------------------------
server:
  id: "game-server-1"
  name: "Game Server"
  version: "1.0.0"
  environment: "development"

  # Cluster settings
  cluster:
    enabled: false
    node_id: 1
    total_nodes: 1
    discovery:
      type: "static"  # static, consul, etcd, kubernetes
      endpoints: []

# -----------------------------------------------------------------------------
# Service Ports
# -----------------------------------------------------------------------------
ports:
  game: 7777          # Main game port (TCP)
  game_udp: 7778      # UDP port for unreliable messages
  websocket: 8080     # WebSocket for web clients
  admin: 9090         # Admin/metrics port
  grpc: 50051         # Internal gRPC communication

# -----------------------------------------------------------------------------
# Resource Limits
# -----------------------------------------------------------------------------
limits:
  max_players: 1000
  max_connections: 1200
  max_players_per_ip: 5
  connection_queue_size: 100

  # Rate limiting
  rate_limit:
    requests_per_second: 100
    burst_size: 200
    ban_threshold: 500
    ban_duration_seconds: 300

# -----------------------------------------------------------------------------
# Threading
# -----------------------------------------------------------------------------
threading:
  # Thread pool configuration
  io_threads: 4           # Network I/O threads
  worker_threads: 8       # Game logic workers
  db_threads: 4           # Database operation threads
  background_threads: 2   # Background tasks

  # Task queue settings
  task_queue:
    max_size: 10000
    warning_threshold: 8000

# -----------------------------------------------------------------------------
# Performance
# -----------------------------------------------------------------------------
performance:
  # Tick rates (Hz)
  game_tick_rate: 20      # Main game loop (50ms per tick)
  network_tick_rate: 60   # Network send rate
  physics_tick_rate: 30   # Physics simulation

  # Batching
  batch:
    enabled: true
    max_batch_size: 100
    max_batch_delay_ms: 50

  # Memory
  memory:
    preallocate_entities: 10000
    entity_pool_growth: 1000
    component_pool_initial: 50000

# -----------------------------------------------------------------------------
# Timeouts
# -----------------------------------------------------------------------------
timeouts:
  connection_timeout_ms: 30000
  login_timeout_ms: 60000
  idle_timeout_ms: 300000       # 5 minutes
  heartbeat_interval_ms: 10000
  heartbeat_timeout_ms: 30000
```

### production.yaml

```yaml
# =============================================================================
# Production Configuration Overrides
# =============================================================================

server:
  environment: "production"

  cluster:
    enabled: true
    discovery:
      type: "kubernetes"
      namespace: "game-server"
      service: "game-server-headless"

limits:
  max_players: 5000
  max_connections: 6000
  rate_limit:
    requests_per_second: 50
    ban_duration_seconds: 600

threading:
  io_threads: 8
  worker_threads: 16
  db_threads: 8

performance:
  game_tick_rate: 30
  memory:
    preallocate_entities: 50000
    component_pool_initial: 200000

# Enable all production optimizations
optimizations:
  enable_compression: true
  enable_batching: true
  enable_delta_encoding: true
  connection_pooling: true
```

---

## Database Configuration

### Database Settings

```yaml
# =============================================================================
# Database Configuration
# =============================================================================

database:
  # Primary database (PostgreSQL)
  primary:
    driver: "postgresql"
    host: "${DB_HOST:localhost}"
    port: ${DB_PORT:5432}
    database: "${DB_NAME:game_db}"
    username: "${DB_USER:game_user}"
    password: "${DB_PASSWORD}"
    schema: "public"

    # Connection pool
    pool:
      min_connections: 10
      max_connections: 100
      idle_timeout_ms: 60000
      connection_timeout_ms: 5000
      max_lifetime_ms: 3600000  # 1 hour

    # SSL settings
    ssl:
      enabled: true
      mode: "verify-full"  # disable, allow, prefer, require, verify-ca, verify-full
      ca_file: "/etc/ssl/certs/db-ca.pem"
      cert_file: "/etc/ssl/certs/db-client.pem"
      key_file: "/etc/ssl/private/db-client.key"

  # Read replicas
  replicas:
    enabled: true
    load_balance: "round-robin"  # round-robin, random, least-connections
    servers:
      - host: "${DB_REPLICA_1:localhost}"
        port: 5432
        weight: 100
      - host: "${DB_REPLICA_2:localhost}"
        port: 5432
        weight: 100

  # Redis cache
  cache:
    driver: "redis"
    host: "${REDIS_HOST:localhost}"
    port: ${REDIS_PORT:6379}
    password: "${REDIS_PASSWORD}"
    database: 0

    # Cluster mode
    cluster:
      enabled: false
      nodes:
        - "redis-1:6379"
        - "redis-2:6379"
        - "redis-3:6379"

    # Cache settings
    settings:
      default_ttl_seconds: 3600
      max_memory: "2gb"
      eviction_policy: "allkeys-lru"

  # Query settings
  query:
    slow_query_threshold_ms: 100
    log_slow_queries: true
    statement_timeout_ms: 30000

    # Prepared statements
    prepared_statements:
      enabled: true
      cache_size: 1000

  # Migrations
  migrations:
    auto_migrate: false  # Never auto-migrate in production
    directory: "./migrations"
```

### Sharding Configuration

```yaml
# =============================================================================
# Database Sharding Configuration
# =============================================================================

sharding:
  enabled: true
  strategy: "hash"  # hash, range, directory

  # Shard definitions
  shards:
    - id: 0
      host: "${SHARD_0_HOST:shard-0.db.local}"
      port: 5432
      database: "game_shard_0"
      weight: 100

    - id: 1
      host: "${SHARD_1_HOST:shard-1.db.local}"
      port: 5432
      database: "game_shard_1"
      weight: 100

    - id: 2
      host: "${SHARD_2_HOST:shard-2.db.local}"
      port: 5432
      database: "game_shard_2"
      weight: 100

    - id: 3
      host: "${SHARD_3_HOST:shard-3.db.local}"
      port: 5432
      database: "game_shard_3"
      weight: 100

  # Virtual shards for future scaling
  virtual_shards: 256

  # Routing
  routing:
    # Tables that use sharding
    sharded_tables:
      - "player_characters"
      - "character_inventory"
      - "character_skills"
      - "character_stats"

    # Global tables (replicated to all shards)
    global_tables:
      - "item_definitions"
      - "skill_definitions"
      - "maps"
```

---

## Network Configuration

### Network Settings

```yaml
# =============================================================================
# Network Configuration
# =============================================================================

network:
  # Bind addresses
  bind:
    address: "0.0.0.0"
    ipv6: true

  # TCP settings
  tcp:
    nodelay: true
    keepalive: true
    keepalive_idle_seconds: 60
    keepalive_interval_seconds: 10
    keepalive_count: 3

    # Buffer sizes
    send_buffer_size: 65536
    recv_buffer_size: 65536

  # UDP settings
  udp:
    enabled: true
    buffer_size: 8192
    max_packet_size: 1400  # MTU safe

  # WebSocket settings
  websocket:
    enabled: true
    path: "/ws"
    ping_interval_seconds: 30
    ping_timeout_seconds: 10
    max_message_size: 65536

    # Compression
    compression:
      enabled: true
      threshold_bytes: 128

  # Protocol settings
  protocol:
    version: "1.0"
    encryption: true
    compression: true
    compression_threshold: 128

    # Packet handling
    max_packet_size: 65536
    max_packets_per_frame: 100

  # TLS/SSL
  tls:
    enabled: true
    cert_file: "/etc/ssl/certs/server.pem"
    key_file: "/etc/ssl/private/server.key"
    ca_file: "/etc/ssl/certs/ca.pem"
    protocols:
      - "TLSv1.2"
      - "TLSv1.3"
    ciphers: "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384"
```

### Load Balancer Integration

```yaml
# =============================================================================
# Load Balancer Configuration
# =============================================================================

load_balancer:
  # Health check endpoint
  health_check:
    enabled: true
    path: "/health"
    port: 9090
    interval_seconds: 10

  # Proxy protocol (for real client IP)
  proxy_protocol:
    enabled: true
    version: 2
    trusted_proxies:
      - "10.0.0.0/8"
      - "172.16.0.0/12"
      - "192.168.0.0/16"

  # Graceful shutdown
  graceful_shutdown:
    enabled: true
    drain_timeout_seconds: 30
    connection_drain: true
```

---

## Game Configuration

### Game Balance

```yaml
# =============================================================================
# Game Balance Configuration
# config/game/balance.yaml
# =============================================================================

balance:
  # Experience curve
  experience:
    base_exp: 100
    exp_multiplier: 1.15
    max_level: 100

    # Level exp table formula: base_exp * (level ^ exp_multiplier)
    # Can also define explicit table
    level_table:
      1: 0
      2: 100
      3: 250
      # ... or use formula

  # Combat formulas
  combat:
    # Damage formula components
    damage:
      base_multiplier: 1.0
      attack_scaling: 0.8
      skill_scaling: 1.2
      level_scaling: 0.1

      # Critical hits
      critical:
        base_rate: 0.05
        damage_multiplier: 1.5
        max_rate: 0.75

      # Defense reduction
      defense:
        formula: "logarithmic"  # linear, logarithmic, diminishing
        cap: 0.90  # Maximum damage reduction

    # Combat speed
    attack_speed:
      base: 1.0
      min: 0.5
      max: 3.0

  # Stat formulas
  stats:
    hp_per_vitality: 10
    mp_per_wisdom: 5
    attack_per_strength: 2
    magic_per_intelligence: 2
    defense_per_vitality: 0.5

  # Movement
  movement:
    base_speed: 5.0       # Units per second
    run_multiplier: 1.5
    mount_multiplier: 2.0

  # Regeneration
  regeneration:
    hp_per_second: 0.01   # Percentage of max HP
    mp_per_second: 0.02
    stamina_per_second: 0.05

    # Out of combat bonus
    out_of_combat:
      delay_seconds: 10
      multiplier: 5.0
```

### World Settings

```yaml
# =============================================================================
# World Configuration
# config/game/world.yaml
# =============================================================================

world:
  # Map settings
  maps:
    default_map: 1
    safe_zone_map: 1

  # Spawn settings
  spawn:
    respawn_time_base_seconds: 300
    respawn_time_penalty_multiplier: 1.5
    max_respawn_time_seconds: 600

    # Death penalty
    death_penalty:
      enabled: true
      exp_loss_percent: 0.01
      item_drop: false

  # View distance
  visibility:
    entity_view_distance: 100.0
    player_view_distance: 150.0
    npc_view_distance: 80.0
    item_view_distance: 50.0

    # Update rates
    position_update_distance: 0.5
    position_broadcast_rate: 10  # Hz

  # Time system
  time:
    enabled: true
    day_duration_minutes: 60
    start_hour: 6

  # Weather
  weather:
    enabled: true
    change_interval_minutes: 30
    types:
      - name: "clear"
        weight: 50
      - name: "cloudy"
        weight: 25
      - name: "rain"
        weight: 15
      - name: "storm"
        weight: 10
```

### Item Configuration

```yaml
# =============================================================================
# Item System Configuration
# config/game/items.yaml
# =============================================================================

items:
  # Inventory
  inventory:
    slots: 100
    bank_slots: 200
    trade_slots: 20

    # Stack limits by item type
    stack_limits:
      consumable: 999
      material: 9999
      currency: 999999999

  # Equipment
  equipment:
    slots:
      - name: "head"
        slot_id: 0
      - name: "shoulders"
        slot_id: 1
      - name: "chest"
        slot_id: 2
      - name: "hands"
        slot_id: 3
      - name: "legs"
        slot_id: 4
      - name: "feet"
        slot_id: 5
      - name: "main_hand"
        slot_id: 6
      - name: "off_hand"
        slot_id: 7
      - name: "necklace"
        slot_id: 8
      - name: "ring_1"
        slot_id: 9
      - name: "ring_2"
        slot_id: 10

  # Enhancement
  enhancement:
    max_level: 20
    success_rates:
      - level: 1
        rate: 1.0
      - level: 5
        rate: 0.8
      - level: 10
        rate: 0.5
      - level: 15
        rate: 0.3
      - level: 20
        rate: 0.1

    # Failure penalty
    failure:
      destroy: false
      downgrade: true
      downgrade_levels: 1

  # Rarity colors and drop rates
  rarity:
    - name: "common"
      id: 0
      color: "#FFFFFF"
      drop_weight: 1000
    - name: "uncommon"
      id: 1
      color: "#00FF00"
      drop_weight: 400
    - name: "rare"
      id: 2
      color: "#0080FF"
      drop_weight: 100
    - name: "epic"
      id: 3
      color: "#A020F0"
      drop_weight: 20
    - name: "legendary"
      id: 4
      color: "#FF8000"
      drop_weight: 2

  # Durability
  durability:
    enabled: true
    repair_cost_multiplier: 0.1
    break_at_zero: false
    damage_penalty_at_zero: 0.5
```

---

## Logging Configuration

### Logging Settings

```yaml
# =============================================================================
# Logging Configuration
# =============================================================================

logging:
  # Default log level
  level: "info"  # trace, debug, info, warn, error, critical

  # Output targets
  outputs:
    # Console output
    console:
      enabled: true
      format: "text"  # text, json
      colors: true
      timestamp: true

    # File output
    file:
      enabled: true
      path: "/var/log/game-server"
      filename: "game-server.log"
      format: "json"

      # Rotation
      rotation:
        max_size_mb: 100
        max_files: 10
        max_age_days: 30
        compress: true

    # Remote logging (Elasticsearch, Loki, etc.)
    remote:
      enabled: false
      type: "elasticsearch"
      endpoint: "${LOG_ENDPOINT:http://localhost:9200}"
      index: "game-server-logs"
      batch_size: 100
      flush_interval_seconds: 5

  # Category-specific log levels
  categories:
    network: "info"
    database: "info"
    game: "info"
    combat: "debug"
    economy: "info"
    security: "warn"
    performance: "info"

  # Sampling for high-volume logs
  sampling:
    enabled: true
    rules:
      - category: "combat"
        rate: 0.1  # Log 10% of combat events
      - category: "movement"
        rate: 0.01  # Log 1% of movement events

  # Sensitive data filtering
  filtering:
    enabled: true
    patterns:
      - "password"
      - "token"
      - "secret"
      - "credit_card"
    replacement: "[REDACTED]"
```

---

## Security Configuration

### Security Settings

```yaml
# =============================================================================
# Security Configuration
# =============================================================================

security:
  # Authentication
  authentication:
    # Session settings
    session:
      token_length: 64
      expiry_hours: 24
      refresh_enabled: true
      refresh_expiry_days: 30

    # Password requirements
    password:
      min_length: 8
      require_uppercase: true
      require_lowercase: true
      require_number: true
      require_special: false
      hash_algorithm: "argon2id"
      hash_iterations: 4
      hash_memory_kb: 65536
      hash_parallelism: 4

    # Two-factor authentication
    two_factor:
      enabled: true
      issuer: "Game Server"
      algorithm: "SHA1"
      digits: 6
      period: 30

    # Login protection
    login:
      max_attempts: 5
      lockout_duration_minutes: 15
      captcha_after_attempts: 3

  # Encryption
  encryption:
    # Packet encryption
    packets:
      enabled: true
      algorithm: "AES-256-GCM"
      key_exchange: "ECDH-X25519"

    # Database encryption
    database:
      encrypt_sensitive: true
      algorithm: "AES-256-CBC"
      key_rotation_days: 90

  # Anti-cheat
  anti_cheat:
    enabled: true
    checks:
      speed_hack: true
      teleport_hack: true
      damage_hack: true
      dupe_detection: true
      packet_manipulation: true

    # Thresholds
    thresholds:
      max_speed_multiplier: 1.2
      max_position_deviation: 5.0
      max_damage_deviation: 1.5

    # Actions
    actions:
      warning_threshold: 3
      kick_threshold: 5
      ban_threshold: 10
      auto_ban: true

  # IP security
  ip_security:
    # Whitelist/blacklist
    whitelist:
      enabled: false
      ips: []

    blacklist:
      enabled: true
      ips: []
      update_interval_minutes: 5

    # GeoIP
    geoip:
      enabled: false
      database_path: "/etc/geoip/GeoLite2-Country.mmdb"
      blocked_countries: []
      allowed_countries: []

  # API security
  api:
    # Rate limiting
    rate_limit:
      enabled: true
      requests_per_minute: 60
      burst: 100

    # CORS
    cors:
      enabled: true
      allowed_origins:
        - "https://game.example.com"
      allowed_methods:
        - "GET"
        - "POST"
      allowed_headers:
        - "Authorization"
        - "Content-Type"

    # API keys
    api_keys:
      enabled: true
      header_name: "X-API-Key"
      query_param: "api_key"
```

---

## Environment Variables

### Environment Variable Reference

```yaml
# =============================================================================
# Environment Variables Reference
# =============================================================================

# This file documents all supported environment variables
# Variables use the format: ${VAR_NAME:default_value}

environment_variables:
  # Server
  - name: GAME_SERVER_ENV
    description: "Server environment (development, staging, production)"
    default: "development"
    required: false

  - name: GAME_SERVER_ID
    description: "Unique server identifier"
    default: "game-server-1"
    required: false

  # Ports
  - name: GAME_PORT
    description: "Main game server port"
    default: "7777"
    required: false

  - name: GAME_UDP_PORT
    description: "UDP port for unreliable messages"
    default: "7778"
    required: false

  - name: ADMIN_PORT
    description: "Admin/metrics port"
    default: "9090"
    required: false

  # Database
  - name: DB_HOST
    description: "Primary database host"
    default: "localhost"
    required: true

  - name: DB_PORT
    description: "Database port"
    default: "5432"
    required: false

  - name: DB_NAME
    description: "Database name"
    default: "game_db"
    required: true

  - name: DB_USER
    description: "Database username"
    required: true
    secret: true

  - name: DB_PASSWORD
    description: "Database password"
    required: true
    secret: true

  # Redis
  - name: REDIS_HOST
    description: "Redis server host"
    default: "localhost"
    required: false

  - name: REDIS_PORT
    description: "Redis port"
    default: "6379"
    required: false

  - name: REDIS_PASSWORD
    description: "Redis password"
    required: false
    secret: true

  # Security
  - name: JWT_SECRET
    description: "JWT signing secret"
    required: true
    secret: true

  - name: ENCRYPTION_KEY
    description: "Master encryption key"
    required: true
    secret: true

  # Logging
  - name: LOG_LEVEL
    description: "Global log level"
    default: "info"
    required: false

  - name: LOG_ENDPOINT
    description: "Remote logging endpoint"
    required: false

  # Kubernetes/Docker
  - name: POD_NAME
    description: "Kubernetes pod name"
    required: false

  - name: POD_NAMESPACE
    description: "Kubernetes namespace"
    required: false

  - name: NODE_NAME
    description: "Kubernetes node name"
    required: false
```

### .env File Template

```bash
# =============================================================================
# Environment Configuration (.env)
# Copy this file to .env and fill in the values
# =============================================================================

# Server
GAME_SERVER_ENV=development
GAME_SERVER_ID=game-server-local

# Ports
GAME_PORT=7777
GAME_UDP_PORT=7778
ADMIN_PORT=9090

# Database
DB_HOST=localhost
DB_PORT=5432
DB_NAME=game_db
DB_USER=game_user
DB_PASSWORD=your_secure_password_here

# Redis
REDIS_HOST=localhost
REDIS_PORT=6379
REDIS_PASSWORD=

# Security (generate these with secure random)
JWT_SECRET=your_jwt_secret_here_min_32_chars
ENCRYPTION_KEY=your_encryption_key_here_32_bytes

# Logging
LOG_LEVEL=debug
```

---

## Hot Reload

### Hot Reload Configuration

```yaml
# =============================================================================
# Hot Reload Configuration
# =============================================================================

hot_reload:
  enabled: true

  # Files to watch
  watch:
    paths:
      - "config/game/"
      - "config/balance/"
    extensions:
      - ".yaml"
      - ".yml"
      - ".json"
    ignore:
      - "*.bak"
      - "*.tmp"

  # Debounce settings
  debounce:
    delay_ms: 1000
    max_wait_ms: 5000

  # Validation before reload
  validation:
    enabled: true
    schema_path: "config/schemas/"
    fail_on_error: true

  # Notification
  notification:
    log_changes: true
    broadcast_to_admins: true
    metric_name: "config_reload"

  # Reload hooks
  hooks:
    pre_reload:
      - name: "backup_current"
        script: "/scripts/backup_config.sh"
    post_reload:
      - name: "notify_cluster"
        script: "/scripts/notify_reload.sh"
```

### Hot Reload Implementation

```cpp
// hot_reload.h
#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace config {

class HotReloadManager {
public:
    using ReloadCallback = std::function<void(const YAML::Node&)>;

    HotReloadManager(const std::string& watchPath);
    ~HotReloadManager();

    // Start watching for changes
    void start();
    void stop();

    // Register reload handlers
    void onReload(const std::string& configName, ReloadCallback callback);

    // Manual reload trigger
    bool reloadConfig(const std::string& configName);
    bool reloadAll();

private:
    void watchLoop();
    bool validateConfig(const std::string& path, const YAML::Node& config);
    void notifyReload(const std::string& configName, const YAML::Node& config);

    std::filesystem::path watchPath_;
    std::unordered_map<std::string, ReloadCallback> callbacks_;
    std::unordered_map<std::string, std::filesystem::file_time_type> lastModified_;
    std::atomic<bool> running_{false};
    std::thread watchThread_;
};

// Usage example
class GameBalanceConfig {
public:
    static GameBalanceConfig& instance();

    void registerForReload(HotReloadManager& manager) {
        manager.onReload("balance", [this](const YAML::Node& config) {
            reload(config);
        });
    }

    // Thread-safe access
    float getDamageMultiplier() const {
        std::shared_lock lock(mutex_);
        return damageMultiplier_;
    }

private:
    void reload(const YAML::Node& config);

    mutable std::shared_mutex mutex_;
    float damageMultiplier_ = 1.0f;
    // ... other balance values
};

} // namespace config
```

---

## Configuration Validation

### JSON Schema Validation

```yaml
# config/schemas/server.schema.yaml
$schema: "http://json-schema.org/draft-07/schema#"
title: "Server Configuration"
type: object

required:
  - server
  - ports

properties:
  server:
    type: object
    required:
      - id
      - name
    properties:
      id:
        type: string
        pattern: "^[a-z0-9-]+$"
        minLength: 1
        maxLength: 64
      name:
        type: string
        minLength: 1
        maxLength: 128
      environment:
        type: string
        enum: ["development", "staging", "production"]

  ports:
    type: object
    properties:
      game:
        type: integer
        minimum: 1024
        maximum: 65535
      admin:
        type: integer
        minimum: 1024
        maximum: 65535

  limits:
    type: object
    properties:
      max_players:
        type: integer
        minimum: 1
        maximum: 100000
      max_connections:
        type: integer
        minimum: 1
```

### Validation Implementation

```cpp
// config_validator.h
#pragma once

#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

namespace config {

struct ValidationError {
    std::string path;
    std::string message;
    std::string value;
};

struct ValidationResult {
    bool valid;
    std::vector<ValidationError> errors;
    std::vector<std::string> warnings;
};

class ConfigValidator {
public:
    explicit ConfigValidator(const std::string& schemaPath);

    // Validate configuration
    ValidationResult validate(const YAML::Node& config);
    ValidationResult validateFile(const std::string& configPath);

    // Quick validation
    bool isValid(const YAML::Node& config);

private:
    YAML::Node schema_;

    void validateNode(
        const YAML::Node& node,
        const YAML::Node& schema,
        const std::string& path,
        std::vector<ValidationError>& errors
    );

    void validateType(
        const YAML::Node& node,
        const std::string& expectedType,
        const std::string& path,
        std::vector<ValidationError>& errors
    );

    void validateConstraints(
        const YAML::Node& node,
        const YAML::Node& schema,
        const std::string& path,
        std::vector<ValidationError>& errors
    );
};

} // namespace config
```

---

## Appendix

### Configuration Best Practices

| Practice | Description |
|----------|-------------|
| Environment separation | Use separate files for each environment |
| Secret management | Never commit secrets, use env vars or secret files |
| Schema validation | Validate all configs before loading |
| Default values | Always provide sensible defaults |
| Documentation | Document all configuration options |
| Version control | Track non-secret configs in git |
| Hot reload | Support runtime config changes where safe |

### Quick Reference

```bash
# View effective configuration
./game-server --dump-config

# Validate configuration
./game-server --validate-config

# Override configuration via CLI
./game-server --config.server.id=my-server --config.ports.game=8888

# Use specific environment
GAME_SERVER_ENV=production ./game-server

# Generate default configuration
./game-server --generate-config > config/default.yaml
```

---

*This document provides the complete configuration system design for the unified game server.*
