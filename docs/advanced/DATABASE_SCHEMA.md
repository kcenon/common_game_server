# Database Schema Reference

> Version: 0.1.0.0
> Last Updated: 2026-02-03
> Status: Foundation Reference

## Overview

This document defines the database schema design for the unified game server, including table definitions, relationships, indexing strategies, and sharding considerations.

---

## Table of Contents

1. [Schema Architecture](#schema-architecture)
2. [Account & Authentication](#account--authentication)
3. [Character Data](#character-data)
4. [Inventory & Items](#inventory--items)
5. [World & Maps](#world--maps)
6. [Social Systems](#social-systems)
7. [Combat & Skills](#combat--skills)
8. [Economy & Trading](#economy--trading)
9. [Logs & Analytics](#logs--analytics)
10. [Sharding Strategy](#sharding-strategy)

---

## Schema Architecture

### Database Distribution

```
┌─────────────────────────────────────────────────────────────────┐
│                     Database Architecture                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐           │
│  │   Account   │   │    Game     │   │    Log      │           │
│  │   Database  │   │  Database   │   │  Database   │           │
│  │  (Global)   │   │  (Sharded)  │   │ (Time-series)│          │
│  └─────────────┘   └─────────────┘   └─────────────┘           │
│         │                │                  │                    │
│  - accounts       - characters       - player_logs              │
│  - auth_tokens    - inventories      - combat_logs              │
│  - bans           - skills           - economy_logs             │
│  - sessions       - quests           - error_logs               │
│                   - social           - metrics                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Naming Conventions

| Convention | Example | Description |
|------------|---------|-------------|
| Table names | `player_characters` | Lowercase, plural, snake_case |
| Column names | `character_id` | Lowercase, snake_case |
| Primary keys | `id` or `{table}_id` | Consistent naming |
| Foreign keys | `{referenced_table}_id` | Clear relationship |
| Indexes | `idx_{table}_{columns}` | Descriptive naming |
| Constraints | `fk_{table}_{ref_table}` | Clear relationships |

### Common Column Types

```sql
-- Standard ID type (64-bit for scalability)
CREATE DOMAIN entity_id AS BIGINT;

-- Timestamp with timezone
CREATE DOMAIN timestamp_tz AS TIMESTAMPTZ DEFAULT NOW();

-- Money/Currency (avoid floating point)
CREATE DOMAIN currency AS BIGINT;  -- Store as smallest unit (copper)

-- Position in world
CREATE TYPE world_position AS (
    x REAL,
    y REAL,
    z REAL
);
```

---

## Account & Authentication

### accounts

```sql
-- Core account information
CREATE TABLE accounts (
    id              BIGSERIAL PRIMARY KEY,
    username        VARCHAR(32) NOT NULL UNIQUE,
    email           VARCHAR(255) NOT NULL UNIQUE,
    password_hash   CHAR(64) NOT NULL,          -- SHA-256
    salt            CHAR(32) NOT NULL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_login      TIMESTAMPTZ,
    status          SMALLINT NOT NULL DEFAULT 0, -- 0=active, 1=suspended, 2=banned
    account_type    SMALLINT NOT NULL DEFAULT 0, -- 0=normal, 1=premium, 2=gm
    email_verified  BOOLEAN NOT NULL DEFAULT FALSE,
    two_factor      BOOLEAN NOT NULL DEFAULT FALSE,

    CONSTRAINT chk_username_format CHECK (username ~ '^[a-zA-Z0-9_]{3,32}$'),
    CONSTRAINT chk_email_format CHECK (email ~ '^[^@]+@[^@]+\.[^@]+$')
);

-- Indexes
CREATE INDEX idx_accounts_email ON accounts(email);
CREATE INDEX idx_accounts_status ON accounts(status) WHERE status != 0;
CREATE INDEX idx_accounts_last_login ON accounts(last_login);
```

### auth_tokens

```sql
-- Session tokens for authentication
CREATE TABLE auth_tokens (
    id              BIGSERIAL PRIMARY KEY,
    account_id      BIGINT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    token           CHAR(64) NOT NULL UNIQUE,   -- SHA-256 of actual token
    token_type      SMALLINT NOT NULL,          -- 0=session, 1=refresh, 2=api
    device_id       VARCHAR(64),
    ip_address      INET,
    user_agent      VARCHAR(512),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at      TIMESTAMPTZ NOT NULL,
    last_used_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    revoked         BOOLEAN NOT NULL DEFAULT FALSE,

    CONSTRAINT chk_expires_future CHECK (expires_at > created_at)
);

-- Indexes
CREATE INDEX idx_auth_tokens_account ON auth_tokens(account_id);
CREATE INDEX idx_auth_tokens_expires ON auth_tokens(expires_at) WHERE NOT revoked;
CREATE INDEX idx_auth_tokens_token ON auth_tokens(token) WHERE NOT revoked;

-- Cleanup expired tokens (run periodically)
CREATE OR REPLACE FUNCTION cleanup_expired_tokens()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM auth_tokens
    WHERE expires_at < NOW() - INTERVAL '7 days'
       OR (revoked AND updated_at < NOW() - INTERVAL '1 day');
    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;
```

### account_bans

```sql
-- Ban records
CREATE TABLE account_bans (
    id              BIGSERIAL PRIMARY KEY,
    account_id      BIGINT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    banned_by       BIGINT REFERENCES accounts(id),
    reason          TEXT NOT NULL,
    ban_type        SMALLINT NOT NULL,          -- 0=warning, 1=temp, 2=permanent
    banned_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at      TIMESTAMPTZ,                -- NULL for permanent
    lifted_at       TIMESTAMPTZ,
    lifted_by       BIGINT REFERENCES accounts(id),
    ip_address      INET,

    CONSTRAINT chk_temp_has_expiry CHECK (
        ban_type != 1 OR expires_at IS NOT NULL
    )
);

CREATE INDEX idx_bans_account ON account_bans(account_id);
CREATE INDEX idx_bans_active ON account_bans(account_id, expires_at)
    WHERE lifted_at IS NULL;
```

### ip_bans

```sql
-- IP-level bans
CREATE TABLE ip_bans (
    id              BIGSERIAL PRIMARY KEY,
    ip_address      INET NOT NULL,
    ip_mask         INET,                       -- For range bans
    reason          TEXT NOT NULL,
    banned_by       BIGINT REFERENCES accounts(id),
    banned_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at      TIMESTAMPTZ,

    CONSTRAINT chk_ip_format CHECK (
        family(ip_address) IN (4, 6)
    )
);

CREATE INDEX idx_ip_bans_address ON ip_bans USING GIST (ip_address inet_ops);
```

---

## Character Data

### player_characters

```sql
-- Main character table
CREATE TABLE player_characters (
    id              BIGSERIAL PRIMARY KEY,
    account_id      BIGINT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    name            VARCHAR(24) NOT NULL,
    class_id        SMALLINT NOT NULL,
    gender          SMALLINT NOT NULL DEFAULT 0,
    level           SMALLINT NOT NULL DEFAULT 1,
    experience      BIGINT NOT NULL DEFAULT 0,

    -- Stats
    current_hp      INTEGER NOT NULL,
    max_hp          INTEGER NOT NULL,
    current_mp      INTEGER NOT NULL DEFAULT 0,
    max_mp          INTEGER NOT NULL DEFAULT 0,
    current_stamina INTEGER NOT NULL DEFAULT 100,
    max_stamina     INTEGER NOT NULL DEFAULT 100,

    -- Position
    map_id          INTEGER NOT NULL DEFAULT 1,
    position_x      REAL NOT NULL DEFAULT 0,
    position_y      REAL NOT NULL DEFAULT 0,
    position_z      REAL NOT NULL DEFAULT 0,
    rotation        REAL NOT NULL DEFAULT 0,

    -- Timestamps
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_played     TIMESTAMPTZ,
    total_playtime  INTEGER NOT NULL DEFAULT 0,  -- Seconds

    -- Status
    is_deleted      BOOLEAN NOT NULL DEFAULT FALSE,
    deleted_at      TIMESTAMPTZ,

    CONSTRAINT uq_character_name UNIQUE (name) WHERE NOT is_deleted,
    CONSTRAINT chk_level_range CHECK (level BETWEEN 1 AND 999),
    CONSTRAINT chk_experience_positive CHECK (experience >= 0),
    CONSTRAINT chk_hp_range CHECK (current_hp >= 0 AND current_hp <= max_hp),
    CONSTRAINT chk_name_format CHECK (name ~ '^[a-zA-Z][a-zA-Z0-9]{2,23}$')
);

-- Indexes
CREATE INDEX idx_characters_account ON player_characters(account_id);
CREATE INDEX idx_characters_name ON player_characters(LOWER(name)) WHERE NOT is_deleted;
CREATE INDEX idx_characters_map ON player_characters(map_id) WHERE NOT is_deleted;
CREATE INDEX idx_characters_level ON player_characters(level DESC) WHERE NOT is_deleted;
```

### character_stats

```sql
-- Detailed character statistics
CREATE TABLE character_stats (
    character_id    BIGINT PRIMARY KEY REFERENCES player_characters(id) ON DELETE CASCADE,

    -- Base stats (from level/class)
    strength        SMALLINT NOT NULL DEFAULT 10,
    dexterity       SMALLINT NOT NULL DEFAULT 10,
    intelligence    SMALLINT NOT NULL DEFAULT 10,
    vitality        SMALLINT NOT NULL DEFAULT 10,
    wisdom          SMALLINT NOT NULL DEFAULT 10,
    luck            SMALLINT NOT NULL DEFAULT 10,

    -- Bonus stats (from equipment/buffs)
    bonus_strength      SMALLINT NOT NULL DEFAULT 0,
    bonus_dexterity     SMALLINT NOT NULL DEFAULT 0,
    bonus_intelligence  SMALLINT NOT NULL DEFAULT 0,
    bonus_vitality      SMALLINT NOT NULL DEFAULT 0,
    bonus_wisdom        SMALLINT NOT NULL DEFAULT 0,
    bonus_luck          SMALLINT NOT NULL DEFAULT 0,

    -- Combat stats
    attack_power        INTEGER NOT NULL DEFAULT 0,
    magic_power         INTEGER NOT NULL DEFAULT 0,
    defense             INTEGER NOT NULL DEFAULT 0,
    magic_defense       INTEGER NOT NULL DEFAULT 0,
    critical_rate       REAL NOT NULL DEFAULT 0.05,
    critical_damage     REAL NOT NULL DEFAULT 1.5,
    attack_speed        REAL NOT NULL DEFAULT 1.0,
    move_speed          REAL NOT NULL DEFAULT 1.0,

    -- Resistances (percentage)
    fire_resistance     REAL NOT NULL DEFAULT 0,
    ice_resistance      REAL NOT NULL DEFAULT 0,
    lightning_resistance REAL NOT NULL DEFAULT 0,
    poison_resistance   REAL NOT NULL DEFAULT 0,

    -- Unallocated points
    stat_points         SMALLINT NOT NULL DEFAULT 0,

    updated_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

### character_appearance

```sql
-- Character customization data
CREATE TABLE character_appearance (
    character_id    BIGINT PRIMARY KEY REFERENCES player_characters(id) ON DELETE CASCADE,

    -- Face
    face_type       SMALLINT NOT NULL DEFAULT 0,
    eye_type        SMALLINT NOT NULL DEFAULT 0,
    eye_color       INTEGER NOT NULL DEFAULT 0,  -- RGB packed
    eyebrow_type    SMALLINT NOT NULL DEFAULT 0,
    nose_type       SMALLINT NOT NULL DEFAULT 0,
    mouth_type      SMALLINT NOT NULL DEFAULT 0,

    -- Hair
    hair_style      SMALLINT NOT NULL DEFAULT 0,
    hair_color      INTEGER NOT NULL DEFAULT 0,  -- RGB packed
    facial_hair     SMALLINT NOT NULL DEFAULT 0,

    -- Body
    skin_color      INTEGER NOT NULL DEFAULT 0,  -- RGB packed
    body_type       SMALLINT NOT NULL DEFAULT 0,
    height          REAL NOT NULL DEFAULT 1.0,

    -- Extra customization (stored as JSON for flexibility)
    extra_data      JSONB
);
```

### character_buffs

```sql
-- Active buffs/debuffs on characters
CREATE TABLE character_buffs (
    id              BIGSERIAL PRIMARY KEY,
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    buff_id         INTEGER NOT NULL,
    stacks          SMALLINT NOT NULL DEFAULT 1,
    applied_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at      TIMESTAMPTZ NOT NULL,
    source_id       BIGINT,                     -- Who applied the buff

    CONSTRAINT chk_stacks_positive CHECK (stacks > 0),
    CONSTRAINT chk_expires_future CHECK (expires_at > applied_at)
);

CREATE INDEX idx_buffs_character ON character_buffs(character_id);
CREATE INDEX idx_buffs_expires ON character_buffs(expires_at);
```

---

## Inventory & Items

### item_definitions

```sql
-- Master item definitions (read-heavy, rarely changes)
CREATE TABLE item_definitions (
    id              INTEGER PRIMARY KEY,
    name            VARCHAR(64) NOT NULL,
    description     TEXT,
    item_type       SMALLINT NOT NULL,          -- Weapon, armor, consumable, etc.
    sub_type        SMALLINT NOT NULL DEFAULT 0,
    rarity          SMALLINT NOT NULL DEFAULT 0, -- Common, uncommon, rare, epic, legendary
    level_requirement SMALLINT NOT NULL DEFAULT 1,
    class_restriction INTEGER DEFAULT 0,         -- Bitmask of allowed classes

    -- Stack settings
    max_stack       SMALLINT NOT NULL DEFAULT 1,
    is_tradeable    BOOLEAN NOT NULL DEFAULT TRUE,
    is_deletable    BOOLEAN NOT NULL DEFAULT TRUE,

    -- Base stats (JSON for flexibility)
    base_stats      JSONB,

    -- Visual
    icon_id         INTEGER NOT NULL DEFAULT 0,
    model_id        INTEGER NOT NULL DEFAULT 0,

    -- Price
    buy_price       BIGINT NOT NULL DEFAULT 0,
    sell_price      BIGINT NOT NULL DEFAULT 0,

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_items_type ON item_definitions(item_type, sub_type);
CREATE INDEX idx_items_level ON item_definitions(level_requirement);
```

### character_inventory

```sql
-- Character inventory slots
CREATE TABLE character_inventory (
    id              BIGSERIAL PRIMARY KEY,
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    item_id         INTEGER NOT NULL REFERENCES item_definitions(id),
    slot_type       SMALLINT NOT NULL,          -- 0=inventory, 1=equipment, 2=bank
    slot_index      SMALLINT NOT NULL,
    quantity        INTEGER NOT NULL DEFAULT 1,

    -- Item instance data (for unique items)
    instance_data   JSONB,                      -- Enhancement level, gems, durability, etc.

    -- Binding
    is_bound        BOOLEAN NOT NULL DEFAULT FALSE,
    bound_at        TIMESTAMPTZ,

    -- Timestamps
    acquired_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT uq_inventory_slot UNIQUE (character_id, slot_type, slot_index),
    CONSTRAINT chk_quantity_positive CHECK (quantity > 0),
    CONSTRAINT chk_slot_index_range CHECK (slot_index >= 0)
);

CREATE INDEX idx_inventory_character ON character_inventory(character_id);
CREATE INDEX idx_inventory_item ON character_inventory(item_id);
```

### character_equipment

```sql
-- Equipped items view (for quick access)
CREATE VIEW character_equipment AS
SELECT
    ci.character_id,
    ci.slot_index AS equipment_slot,
    ci.item_id,
    id.name AS item_name,
    id.item_type,
    id.rarity,
    ci.instance_data
FROM character_inventory ci
JOIN item_definitions id ON id.id = ci.item_id
WHERE ci.slot_type = 1;  -- Equipment slots

-- Equipment slot indices:
-- 0: Head
-- 1: Shoulders
-- 2: Chest
-- 3: Hands
-- 4: Legs
-- 5: Feet
-- 6: Main Hand
-- 7: Off Hand
-- 8: Necklace
-- 9: Ring 1
-- 10: Ring 2
-- 11: Trinket 1
-- 12: Trinket 2
-- 13: Back (Cape)
```

### item_instances

```sql
-- Unique item instances (for items with individual properties)
CREATE TABLE item_instances (
    id              BIGSERIAL PRIMARY KEY,
    item_id         INTEGER NOT NULL REFERENCES item_definitions(id),

    -- Enhancement
    enhancement_level SMALLINT NOT NULL DEFAULT 0,

    -- Durability
    current_durability INTEGER,
    max_durability     INTEGER,

    -- Socketed gems
    gem_slots       JSONB,  -- Array of gem item IDs

    -- Random stats
    bonus_stats     JSONB,

    -- Crafted by
    crafted_by      BIGINT REFERENCES player_characters(id),

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT chk_enhancement_range CHECK (enhancement_level BETWEEN 0 AND 20)
);
```

### character_currency

```sql
-- Character currencies
CREATE TABLE character_currency (
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    currency_type   SMALLINT NOT NULL,
    amount          BIGINT NOT NULL DEFAULT 0,

    PRIMARY KEY (character_id, currency_type),
    CONSTRAINT chk_amount_non_negative CHECK (amount >= 0)
);

-- Currency types:
-- 0: Gold (primary)
-- 1: Premium currency
-- 2: Honor points
-- 3: Arena points
-- 4: Guild points
-- etc.
```

---

## World & Maps

### maps

```sql
-- Map/zone definitions
CREATE TABLE maps (
    id              INTEGER PRIMARY KEY,
    name            VARCHAR(64) NOT NULL,
    display_name    VARCHAR(128) NOT NULL,
    map_type        SMALLINT NOT NULL,          -- 0=open world, 1=dungeon, 2=arena, 3=instance

    -- Bounds
    min_x           REAL NOT NULL,
    min_y           REAL NOT NULL,
    min_z           REAL NOT NULL,
    max_x           REAL NOT NULL,
    max_y           REAL NOT NULL,
    max_z           REAL NOT NULL,

    -- Requirements
    min_level       SMALLINT NOT NULL DEFAULT 1,
    max_level       SMALLINT,
    max_players     INTEGER,

    -- Flags
    is_pvp_enabled  BOOLEAN NOT NULL DEFAULT FALSE,
    is_safe_zone    BOOLEAN NOT NULL DEFAULT FALSE,

    -- Spawn point
    spawn_x         REAL NOT NULL,
    spawn_y         REAL NOT NULL,
    spawn_z         REAL NOT NULL,

    -- Parent map for sub-zones
    parent_map_id   INTEGER REFERENCES maps(id),

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

### map_portals

```sql
-- Teleport/portal connections between maps
CREATE TABLE map_portals (
    id              SERIAL PRIMARY KEY,
    from_map_id     INTEGER NOT NULL REFERENCES maps(id),
    to_map_id       INTEGER NOT NULL REFERENCES maps(id),

    -- Portal location
    from_x          REAL NOT NULL,
    from_y          REAL NOT NULL,
    from_z          REAL NOT NULL,
    radius          REAL NOT NULL DEFAULT 2.0,

    -- Destination
    to_x            REAL NOT NULL,
    to_y            REAL NOT NULL,
    to_z            REAL NOT NULL,
    to_rotation     REAL NOT NULL DEFAULT 0,

    -- Requirements
    min_level       SMALLINT DEFAULT 1,
    required_quest  INTEGER,
    required_item   INTEGER,

    is_bidirectional BOOLEAN NOT NULL DEFAULT FALSE,
    is_active       BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE INDEX idx_portals_from ON map_portals(from_map_id) WHERE is_active;
```

### spawn_points

```sql
-- NPC/Monster spawn definitions
CREATE TABLE spawn_points (
    id              SERIAL PRIMARY KEY,
    map_id          INTEGER NOT NULL REFERENCES maps(id),
    entity_type     SMALLINT NOT NULL,          -- 0=NPC, 1=Monster, 2=Object
    entity_id       INTEGER NOT NULL,           -- Template ID

    -- Position
    position_x      REAL NOT NULL,
    position_y      REAL NOT NULL,
    position_z      REAL NOT NULL,
    rotation        REAL NOT NULL DEFAULT 0,

    -- Spawn behavior
    spawn_radius    REAL NOT NULL DEFAULT 0,    -- Random spawn within radius
    respawn_time    INTEGER NOT NULL DEFAULT 60, -- Seconds
    max_count       SMALLINT NOT NULL DEFAULT 1,

    -- Patrol path (if applicable)
    patrol_path_id  INTEGER,

    is_active       BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE INDEX idx_spawns_map ON spawn_points(map_id) WHERE is_active;
```

---

## Social Systems

### guilds

```sql
-- Guild information
CREATE TABLE guilds (
    id              BIGSERIAL PRIMARY KEY,
    name            VARCHAR(32) NOT NULL UNIQUE,
    tag             VARCHAR(8),
    leader_id       BIGINT NOT NULL REFERENCES player_characters(id),

    -- Guild info
    level           SMALLINT NOT NULL DEFAULT 1,
    experience      BIGINT NOT NULL DEFAULT 0,
    description     TEXT,
    announcement    TEXT,

    -- Stats
    member_count    INTEGER NOT NULL DEFAULT 1,
    max_members     INTEGER NOT NULL DEFAULT 50,

    -- Treasury
    gold            BIGINT NOT NULL DEFAULT 0,

    -- Settings
    is_recruiting   BOOLEAN NOT NULL DEFAULT TRUE,
    min_level_requirement SMALLINT NOT NULL DEFAULT 1,

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT chk_guild_name CHECK (name ~ '^[a-zA-Z][a-zA-Z0-9 ]{2,31}$')
);

CREATE INDEX idx_guilds_leader ON guilds(leader_id);
CREATE INDEX idx_guilds_level ON guilds(level DESC);
```

### guild_members

```sql
-- Guild membership
CREATE TABLE guild_members (
    guild_id        BIGINT NOT NULL REFERENCES guilds(id) ON DELETE CASCADE,
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    rank            SMALLINT NOT NULL DEFAULT 4, -- 0=leader, 1=officer, 4=member
    joined_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    -- Contribution tracking
    total_contribution BIGINT NOT NULL DEFAULT 0,
    weekly_contribution BIGINT NOT NULL DEFAULT 0,

    -- Permissions (bitmask)
    permissions     INTEGER NOT NULL DEFAULT 0,

    note            VARCHAR(128),
    officer_note    VARCHAR(128),

    PRIMARY KEY (guild_id, character_id)
);

CREATE INDEX idx_guild_members_character ON guild_members(character_id);
```

### friends_list

```sql
-- Friend relationships
CREATE TABLE friends_list (
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    friend_id       BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    added_at        TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    note            VARCHAR(64),

    PRIMARY KEY (character_id, friend_id),
    CONSTRAINT chk_not_self CHECK (character_id != friend_id)
);

CREATE INDEX idx_friends_reverse ON friends_list(friend_id);
```

### blocked_list

```sql
-- Blocked players
CREATE TABLE blocked_list (
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    blocked_id      BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    blocked_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    reason          VARCHAR(128),

    PRIMARY KEY (character_id, blocked_id)
);
```

### parties

```sql
-- Party/group information
CREATE TABLE parties (
    id              BIGSERIAL PRIMARY KEY,
    leader_id       BIGINT NOT NULL REFERENCES player_characters(id),
    party_type      SMALLINT NOT NULL DEFAULT 0, -- 0=normal, 1=raid
    loot_type       SMALLINT NOT NULL DEFAULT 0, -- 0=ffa, 1=round-robin, 2=master
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE party_members (
    party_id        BIGINT NOT NULL REFERENCES parties(id) ON DELETE CASCADE,
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    role            SMALLINT NOT NULL DEFAULT 0, -- 0=dps, 1=tank, 2=healer
    joined_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    PRIMARY KEY (party_id, character_id)
);

CREATE INDEX idx_party_members_character ON party_members(character_id);
```

### mail

```sql
-- In-game mail system
CREATE TABLE mail (
    id              BIGSERIAL PRIMARY KEY,
    sender_id       BIGINT REFERENCES player_characters(id) ON DELETE SET NULL,
    receiver_id     BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,

    subject         VARCHAR(64) NOT NULL,
    body            TEXT,

    -- Attachments
    gold            BIGINT NOT NULL DEFAULT 0,
    cod_amount      BIGINT NOT NULL DEFAULT 0,   -- Cash on delivery

    -- Status
    is_read         BOOLEAN NOT NULL DEFAULT FALSE,
    is_system       BOOLEAN NOT NULL DEFAULT FALSE,

    sent_at         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at      TIMESTAMPTZ NOT NULL DEFAULT NOW() + INTERVAL '30 days',
    read_at         TIMESTAMPTZ,

    CONSTRAINT chk_gold_non_negative CHECK (gold >= 0),
    CONSTRAINT chk_cod_non_negative CHECK (cod_amount >= 0)
);

CREATE INDEX idx_mail_receiver ON mail(receiver_id, is_read, expires_at);

-- Mail attachments (items)
CREATE TABLE mail_attachments (
    mail_id         BIGINT NOT NULL REFERENCES mail(id) ON DELETE CASCADE,
    slot_index      SMALLINT NOT NULL,
    item_id         INTEGER NOT NULL REFERENCES item_definitions(id),
    quantity        INTEGER NOT NULL DEFAULT 1,
    instance_data   JSONB,

    PRIMARY KEY (mail_id, slot_index)
);
```

---

## Combat & Skills

### skill_definitions

```sql
-- Skill/ability definitions
CREATE TABLE skill_definitions (
    id              INTEGER PRIMARY KEY,
    name            VARCHAR(64) NOT NULL,
    description     TEXT,
    skill_type      SMALLINT NOT NULL,          -- Active, passive, toggle
    class_id        SMALLINT,                   -- NULL = all classes

    -- Requirements
    min_level       SMALLINT NOT NULL DEFAULT 1,
    prerequisite_skill INTEGER REFERENCES skill_definitions(id),

    -- Cost
    mp_cost         INTEGER NOT NULL DEFAULT 0,
    hp_cost         INTEGER NOT NULL DEFAULT 0,
    stamina_cost    INTEGER NOT NULL DEFAULT 0,

    -- Timing
    cast_time       REAL NOT NULL DEFAULT 0,    -- Seconds
    cooldown        REAL NOT NULL DEFAULT 0,    -- Seconds

    -- Range
    range_min       REAL NOT NULL DEFAULT 0,
    range_max       REAL NOT NULL DEFAULT 1,
    aoe_radius      REAL NOT NULL DEFAULT 0,

    -- Effects (JSON for flexibility)
    effects         JSONB NOT NULL DEFAULT '[]',

    -- Visual
    icon_id         INTEGER NOT NULL DEFAULT 0,
    animation_id    INTEGER NOT NULL DEFAULT 0,

    max_level       SMALLINT NOT NULL DEFAULT 10,

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_skills_class ON skill_definitions(class_id);
```

### character_skills

```sql
-- Character learned skills
CREATE TABLE character_skills (
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    skill_id        INTEGER NOT NULL REFERENCES skill_definitions(id),
    skill_level     SMALLINT NOT NULL DEFAULT 1,
    experience      INTEGER NOT NULL DEFAULT 0,

    -- Cooldown state (for persistence across sessions)
    cooldown_until  TIMESTAMPTZ,

    learned_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    PRIMARY KEY (character_id, skill_id),
    CONSTRAINT chk_skill_level_positive CHECK (skill_level > 0)
);
```

### skill_bar

```sql
-- Skill bar/hotkey assignments
CREATE TABLE skill_bar (
    character_id    BIGINT NOT NULL REFERENCES player_characters(id) ON DELETE CASCADE,
    bar_index       SMALLINT NOT NULL,          -- Which skill bar (0-4)
    slot_index      SMALLINT NOT NULL,          -- Slot within bar (0-9)
    skill_id        INTEGER REFERENCES skill_definitions(id),
    item_id         INTEGER REFERENCES item_definitions(id),

    PRIMARY KEY (character_id, bar_index, slot_index),
    CONSTRAINT chk_has_assignment CHECK (skill_id IS NOT NULL OR item_id IS NOT NULL)
);
```

---

## Economy & Trading

### auction_house

```sql
-- Auction house listings
CREATE TABLE auction_house (
    id              BIGSERIAL PRIMARY KEY,
    seller_id       BIGINT NOT NULL REFERENCES player_characters(id),
    item_id         INTEGER NOT NULL REFERENCES item_definitions(id),
    quantity        INTEGER NOT NULL,
    instance_data   JSONB,

    -- Pricing
    starting_bid    BIGINT NOT NULL,
    buyout_price    BIGINT,
    current_bid     BIGINT,
    current_bidder  BIGINT REFERENCES player_characters(id),

    -- Timing
    listed_at       TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at      TIMESTAMPTZ NOT NULL,

    -- Status
    status          SMALLINT NOT NULL DEFAULT 0, -- 0=active, 1=sold, 2=expired, 3=cancelled

    CONSTRAINT chk_prices CHECK (
        starting_bid > 0 AND
        (buyout_price IS NULL OR buyout_price >= starting_bid) AND
        (current_bid IS NULL OR current_bid >= starting_bid)
    )
);

CREATE INDEX idx_auction_item ON auction_house(item_id, status) WHERE status = 0;
CREATE INDEX idx_auction_seller ON auction_house(seller_id);
CREATE INDEX idx_auction_expires ON auction_house(expires_at) WHERE status = 0;
```

### auction_bids

```sql
-- Auction bid history
CREATE TABLE auction_bids (
    id              BIGSERIAL PRIMARY KEY,
    auction_id      BIGINT NOT NULL REFERENCES auction_house(id) ON DELETE CASCADE,
    bidder_id       BIGINT NOT NULL REFERENCES player_characters(id),
    bid_amount      BIGINT NOT NULL,
    bid_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_bids_auction ON auction_bids(auction_id);
```

### trade_history

```sql
-- Player-to-player trade log
CREATE TABLE trade_history (
    id              BIGSERIAL PRIMARY KEY,
    trader1_id      BIGINT NOT NULL REFERENCES player_characters(id),
    trader2_id      BIGINT NOT NULL REFERENCES player_characters(id),

    -- Gold exchanged
    trader1_gold    BIGINT NOT NULL DEFAULT 0,
    trader2_gold    BIGINT NOT NULL DEFAULT 0,

    traded_at       TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Items traded
CREATE TABLE trade_items (
    trade_id        BIGINT NOT NULL REFERENCES trade_history(id) ON DELETE CASCADE,
    from_trader     SMALLINT NOT NULL,          -- 1 or 2
    item_id         INTEGER NOT NULL REFERENCES item_definitions(id),
    quantity        INTEGER NOT NULL,
    instance_data   JSONB
);

CREATE INDEX idx_trades_trader1 ON trade_history(trader1_id);
CREATE INDEX idx_trades_trader2 ON trade_history(trader2_id);
CREATE INDEX idx_trades_time ON trade_history(traded_at);
```

---

## Logs & Analytics

### player_logs

```sql
-- General player activity logs (time-series optimized)
CREATE TABLE player_logs (
    id              BIGSERIAL,
    character_id    BIGINT NOT NULL,
    log_type        SMALLINT NOT NULL,
    log_data        JSONB,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    PRIMARY KEY (created_at, id)
) PARTITION BY RANGE (created_at);

-- Create monthly partitions
CREATE TABLE player_logs_2026_01 PARTITION OF player_logs
    FOR VALUES FROM ('2026-01-01') TO ('2026-02-01');
CREATE TABLE player_logs_2026_02 PARTITION OF player_logs
    FOR VALUES FROM ('2026-02-01') TO ('2026-03-01');
-- ... continue for each month

-- Log types:
-- 0: Login
-- 1: Logout
-- 2: Level up
-- 3: Item acquire
-- 4: Item delete
-- 5: Gold change
-- 6: Map change
-- 7: Death
-- 8: PvP kill
```

### combat_logs

```sql
-- Combat event logs (high volume, partitioned)
CREATE TABLE combat_logs (
    id              BIGSERIAL,
    attacker_id     BIGINT NOT NULL,
    defender_id     BIGINT NOT NULL,
    skill_id        INTEGER,
    damage          INTEGER NOT NULL,
    damage_type     SMALLINT NOT NULL,
    is_critical     BOOLEAN NOT NULL DEFAULT FALSE,
    is_blocked      BOOLEAN NOT NULL DEFAULT FALSE,
    result_hp       INTEGER NOT NULL,
    map_id          INTEGER NOT NULL,
    position_x      REAL,
    position_y      REAL,
    position_z      REAL,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    PRIMARY KEY (created_at, id)
) PARTITION BY RANGE (created_at);

-- Daily partitions for combat logs (high volume)
CREATE TABLE combat_logs_2026_02_01 PARTITION OF combat_logs
    FOR VALUES FROM ('2026-02-01') TO ('2026-02-02');
```

### economy_logs

```sql
-- Economic transaction logs
CREATE TABLE economy_logs (
    id              BIGSERIAL,
    character_id    BIGINT NOT NULL,
    transaction_type SMALLINT NOT NULL,
    currency_type   SMALLINT NOT NULL DEFAULT 0,
    amount          BIGINT NOT NULL,
    balance_after   BIGINT NOT NULL,
    source_type     SMALLINT NOT NULL,          -- NPC, player, system, etc.
    source_id       BIGINT,
    reference_id    BIGINT,                     -- Trade ID, auction ID, etc.
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    PRIMARY KEY (created_at, id)
) PARTITION BY RANGE (created_at);

-- Transaction types:
-- 0: Earn (quest, loot)
-- 1: Spend (NPC purchase)
-- 2: Trade send
-- 3: Trade receive
-- 4: Auction list fee
-- 5: Auction sale
-- 6: Mail send
-- 7: Mail receive
```

### server_metrics

```sql
-- Server performance metrics (time-series)
CREATE TABLE server_metrics (
    id              BIGSERIAL,
    server_id       VARCHAR(32) NOT NULL,
    metric_type     VARCHAR(32) NOT NULL,
    metric_value    DOUBLE PRECISION NOT NULL,
    tags            JSONB,
    recorded_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    PRIMARY KEY (recorded_at, id)
) PARTITION BY RANGE (recorded_at);

-- Metric types: cpu_usage, memory_usage, player_count, tps, latency, etc.
```

---

## Sharding Strategy

### Sharding Key Selection

```
Account Database (Global):
- No sharding (single source of truth for auth)
- Read replicas for login load distribution

Game Database (Sharded by character_id):
- Shard key: character_id % num_shards
- Each shard contains:
  - player_characters
  - character_* tables
  - inventory
  - skills

Log Database (Time-partitioned):
- Partition by time (daily/monthly)
- Auto-drop old partitions
```

### Shard Mapping

```sql
-- Shard mapping table (in global database)
CREATE TABLE shard_mapping (
    shard_id        SMALLINT PRIMARY KEY,
    host            VARCHAR(255) NOT NULL,
    port            INTEGER NOT NULL DEFAULT 5432,
    database_name   VARCHAR(64) NOT NULL,
    is_active       BOOLEAN NOT NULL DEFAULT TRUE,
    weight          SMALLINT NOT NULL DEFAULT 100,  -- For weighted distribution

    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Character to shard assignment
CREATE TABLE character_shards (
    character_id    BIGINT PRIMARY KEY,
    shard_id        SMALLINT NOT NULL REFERENCES shard_mapping(shard_id),
    assigned_at     TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Function to get shard for character
CREATE OR REPLACE FUNCTION get_character_shard(p_character_id BIGINT)
RETURNS shard_mapping AS $$
DECLARE
    v_shard shard_mapping;
BEGIN
    SELECT sm.* INTO v_shard
    FROM character_shards cs
    JOIN shard_mapping sm ON sm.shard_id = cs.shard_id
    WHERE cs.character_id = p_character_id
      AND sm.is_active = TRUE;

    RETURN v_shard;
END;
$$ LANGUAGE plpgsql;
```

### Cross-Shard Operations

```sql
-- For operations that span shards (e.g., finding player by name)
-- Use a global index table

CREATE TABLE global_character_index (
    character_id    BIGINT PRIMARY KEY,
    character_name  VARCHAR(24) NOT NULL,
    account_id      BIGINT NOT NULL,
    shard_id        SMALLINT NOT NULL,
    level           SMALLINT NOT NULL,
    class_id        SMALLINT NOT NULL,
    guild_id        BIGINT,
    is_online       BOOLEAN NOT NULL DEFAULT FALSE,
    last_updated    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX idx_global_char_name ON global_character_index(LOWER(character_name));
CREATE INDEX idx_global_char_account ON global_character_index(account_id);
CREATE INDEX idx_global_char_guild ON global_character_index(guild_id);
CREATE INDEX idx_global_char_online ON global_character_index(is_online) WHERE is_online;
```

---

## Appendix

### Index Guidelines

| Access Pattern | Index Type | Example |
|----------------|------------|---------|
| Exact lookup | B-tree | `CREATE INDEX ON table(column)` |
| Range query | B-tree | `CREATE INDEX ON table(timestamp)` |
| Full-text search | GIN | `CREATE INDEX ON table USING GIN(column)` |
| JSON field | GIN | `CREATE INDEX ON table USING GIN(jsonb_column)` |
| Geo-spatial | GiST | `CREATE INDEX ON table USING GIST(point)` |
| Partial data | Partial | `CREATE INDEX ON table(col) WHERE condition` |

### Maintenance Tasks

```sql
-- Regular maintenance tasks

-- 1. Vacuum and analyze (run daily)
VACUUM ANALYZE player_characters;
VACUUM ANALYZE character_inventory;

-- 2. Reindex (run weekly)
REINDEX TABLE CONCURRENTLY character_inventory;

-- 3. Partition maintenance (run monthly)
-- Drop old partitions
DROP TABLE IF EXISTS player_logs_2025_01;

-- Create new partitions
CREATE TABLE player_logs_2026_03 PARTITION OF player_logs
    FOR VALUES FROM ('2026-03-01') TO ('2026-04-01');

-- 4. Statistics update
ALTER TABLE player_characters ALTER COLUMN level SET STATISTICS 1000;
ANALYZE player_characters(level);
```

### Backup Strategy

```sql
-- Backup configuration

-- Full backup (daily)
pg_dump -Fc game_db > game_db_$(date +%Y%m%d).dump

-- Incremental backup with pg_basebackup + WAL archiving
-- Configure in postgresql.conf:
-- archive_mode = on
-- archive_command = 'cp %p /backup/wal/%f'

-- Point-in-time recovery
pg_restore -d game_db_restored game_db_20260203.dump
```

---

*This document provides the complete database schema design for the unified game server data layer.*
