# Cluster System Design

## High-Level Architecture

```text
                    +-------------------+
Client -----------> |    Entrypoint     |
                    |-------------------|
                    | SQL Parser        |
                    | Auth / RBAC       |
                    | Cluster Manager   |
                    | Shard Resolver    |
                    | Query Router      |
                    | Distributed Exec  |
                    +-------------------+
                       /      |      \
                      /       |       \
                     v        v        v
             +-----------+ +-----------+ +-----------+
             | Storage 1 | | Storage 2 | | Storage 3 |
             +-----------+ +-----------+ +-----------+
             | local DB  | | local DB  | | local DB  |
             | BSP-tree  | | BSP-tree  | | BSP-tree  |
             +-----------+ +-----------+ +-----------+
```

---

# System Overview

The cluster architecture consists of:
- a single **Entrypoint node**,
- multiple **Storage nodes**.

The Entrypoint node acts as:
- query coordinator,
- authentication gateway,
- shard router,
- cluster topology manager.

Storage nodes are responsible only for:
- local shard storage,
- local query execution,
- index management,
- persistent storage.

Storage nodes are intentionally isolated from cluster topology knowledge.

---

# Cluster Responsibilities

## Entrypoint Node

Responsibilities:
- SQL parsing,
- authentication,
- RBAC authorization,
- shard resolution,
- query routing,
- distributed query execution,
- scatter-gather orchestration,
- heartbeat monitoring,
- shard ownership management.

The Entrypoint does **not** store user data.

---

## Storage Node

Responsibilities:
- storing local shards,
- local BSP-tree indexes,
- WAL handling,
- query execution on local data,
- persistence management.

Storage nodes are unaware of:
- cluster topology,
- other storage nodes,
- distributed routing.

---

# Sharding Strategy

## Considered Strategies

- Simple modulo sharding
- Classical Consistent Hashing (ring) ✓
- Jump Consistent Hashing
- Rendezvous Hashing (HRW)

The selected strategy is:

# Classical Consistent Hashing (ring)

because it provides:
- stable shard distribution,
- simplified shard migration,
- support for dynamic node addition/removal,
- predictable rebalancing behavior.

---

# Logical Sharding Model

The system uses:

# horizontal row-level sharding.

A logical table is split into multiple independent shards.

Example:

```text
databases:
    app_db
    
tables:
    users/
    orders/
    products/
```

Physical distribution:

```text
StorageNode1:
    users/shard_1
    products/shard_7
    orders/shard_11

StorageNode2:
    users/shard_2
    users/shard_4
    users/shard_15

StorageNode3:
    users/shard_52
    orders/shard_99
```

Each shard contains only a subset of table rows.

---

# Example of Local Shard Storage

Example:

```text
users/shard_1:

    Row14: {id: 14, name: "Alice", age: 30}
    Row27: {id: 27, name: "Bob", age: 25}
    Row33: {id: 33, name: "Charlie", age: 35}
```

Thus:

# a shard is a partition of a logical table.

---

# Shard Resolution

The system separates:
- logical shard computation,
- physical shard ownership.

---

## Step 1 — Row → Shard

```cpp
shard_id = hash(shard_key) % shard_count;
```

Example:
- shard key = `user.id`

---

## Step 2 — Shard → Storage Node

The consistent hash ring resolves:

```text
shard_id -> storage node
```

This separation allows:
- stable shard identifiers,
- simplified rebalancing,
- efficient node scaling.

---

# Shard and Node Configuration

```text
Shard_count = 128
Node_count  = dynamic
```

Important:

# shard_count >> node_count

This enables:
- fine-grained load balancing,
- smoother rebalancing,
- minimal shard migration during topology changes.

Typical configuration:
- 64 shards,
- 128 shards,
- or more.

---

# Query Routing Pipeline

## INSERT Example

```sql
INSERT INTO users VALUES (15, "Alice", 30);
```

Pipeline:

```text
1. Parse SQL
2. Extract shard key (id = 15)
3. Compute shard_id
4. Resolve owner node
5. Forward query to target storage node
```

---

# Distributed Query Execution

Queries containing shard keys:

```sql
SELECT * FROM users WHERE id == 15;
```

are routed to a single node.

Queries without shard keys:

```sql
SELECT * FROM users WHERE age > 18;
```

require:

# scatter-gather execution.

Pipeline:

```text
1. Broadcast query to all storage nodes
2. Execute locally on each node
3. Collect partial results
4. Merge final response
```

---

# Why Fixed Shard Keys Are Required

## A shard key defines:

deterministic data placement,
shard computation,
query routing.

## Without a fixed shard key:

rows cannot be placed consistently,
queries cannot determine target shards,
distributed routing becomes impossible.

Therefore each table must define a fixed shard key before data insertion.

## Current Design Decision:
- Each table has a single shard key - `id`.
- Future extensions may allow multiple shard keys or composite keys.

# Cluster Subsystems

## cluster/

Responsible for:
- topology management,
- shard ownership,
- heartbeat monitoring,
- node registry,
- consistent hash ring,
- rebalancing.

---

## distributed/

Responsible for:
- distributed query execution,
- scatter-gather,
- remote execution,
- distributed result aggregation,
- query routing.

---

# Physical Cluster Storage

```text
./data/
│
├── metadata/
│    ├── nodes.tbl
│    ├── shard_ownership.tbl
│    └── cluster_state.json
│
├── node_1/
│    └── databases/
│         ├── db1/
│         │    ├── users/
│         │    │    ├── shard_0/
│         │    │    ├── shard_7/
│         │    │    └── shard_12/
│         │    │
│         │    └── orders/
│         │         ├── shard_2/
│         │         └── shard_9/
│         │
│         └── db2/
│
├── node_2/
│    └── databases/
│
└── node_3/
     └── databases/
```

---

# Metadata Storage

## nodes.tbl

Contains registered storage nodes.

### Schema

```text
node_id
host
port
alive
```

### Example

| node_id | host     | port | alive |
|----------|----------|------|--------|
| node1    | 127.0.0.1 | 9001 | true |
| node2    | 127.0.0.1 | 9002 | true |
| node3    | 127.0.0.1 | 9003 | true |

---

## shard_ownership.tbl

Contains shard placement information.

### Schema

```text
shard_id
database_name
table_name
owner_node
```

### Example

| shard_id | owner_node |
|----------|-------------|
| 0        | node1       |
| 1        | node2       |
| 2        | node1       |

---

## cluster_state.json

Contains global cluster configuration.

```json
{
    "shard_count": 4096,
    "replication_factor": 1
}
```

Where:
- `shard_count` — total number of logical shards,
- `replication_factor` — number of shard replicas.

Current implementation:

```text
replication_factor = 1
```

meaning:
- no replication,
- each shard has exactly one owner node.

---

# Future Improvements

Planned future extensions:

```text
heartbeat_state.tbl
rebalance_tasks.tbl
replica_ownership.tbl
```

Potential future features:
- replication,
- automatic failover,
- asynchronous rebalancing,
- distributed WAL,
- replica synchronization.

---

# Current Entrypoint API

The current implementation starts the balancer as:

```text
entrypointer [port] [data_dir]
```

Default values:
- `port = 8080`
- `data_dir = ./data/entrypoint`

Storage nodes are regular `dbms_server` processes:

```text
dbms_server [port] [data_dir]
```

Storage nodes can be added and removed without restarting Entrypoint:

```http
POST /nodes
Content-Type: application/json

{"id":"node1","host":"127.0.0.1","port":9001}
```

```http
DELETE /nodes/node1
```

```http
GET /nodes
```

`POST /query` on Entrypoint forwards the original SQL request to Storage nodes.
Requests with an extractable shard key (`id`) are routed to one Storage node.
Requests without an extractable shard key are executed as scatter-gather.

Current implementation limits:
- `replication_factor = 1`
- adding/removing nodes rebalances registered shard ownership
- registered shard directories are moved between local node storage directories during rebalancing
- multi-row `INSERT` is routed by the first row's `id`
