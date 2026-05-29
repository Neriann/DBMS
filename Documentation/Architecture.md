# Architecture Overview
include/
├── auth/
│   ├── user.hpp
│   ├── auth_service.hpp
│   ├── jwt_service.hpp
│   ├── password_hasher.hpp
│   ├── session_context.hpp
│   ├── account_storage.hpp
│   └── token_payload.hpp
│
├── rbac/
│    ├── permission.hpp
│    ├── role.hpp
│    ├── policy.hpp
│    ├── access_manager.hpp
│    ├── permission_resolver.hpp
│    └── group.hpp (may be optional depending on design choice)
│
├── cluster/
│    ├── types.hpp
│    ├── node_info.hpp
│    ├── node_state.hpp
│    ├── topology.hpp
│    ├── hash_ring.hpp
│    ├── shard_registry.hpp
│    ├── heartbeat_monitor.hpp
│    ├── cluster_state_storage.hpp
│    ├── rebalance_manager.hpp
│    └── cluster_manager.hpp
│
├── distributed/
│    ├── rpc/
│    │     ├── rpc_types.hpp
│    │     ├── rpc_client.hpp
│    │     └── rpc_server.hpp
│    │
│    ├── routing/
│    │     ├── route_plan.hpp
│    │     ├── shard_resolver.hpp
│    │     └── query_router.hpp
│    │
│    └── execution/
│          ├── distributed_executor.hpp
│          ├── scatter_gather.hpp
│          └── result_merger.hpp
│
├── services/
│    ├── query_service.hpp
│    ├── admin_service.hpp
│    └── telemetry_service.hpp
│
├── server/
│    ├── middleware/
│    │      ├── access_logger.hpp
│    │      ├── telemetry.hpp
│    │      └── auth_middleware.hpp
│    └── routes/
│          ├── auth_routes.hpp
│          └── query_routes.hpp



# Process Flow
HTTP
 ↓
Middleware
 ↓
QueryService
 ↓
QueryRouter
 ↓
DistributedExecutor
 ↓
RPC