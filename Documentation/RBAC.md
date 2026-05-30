# RBAC System Design

# Overview

The RBAC subsystem is responsible for:

* authentication,
* authorization,
* permission resolution,
* access validation.

The system is designed to be:

* transport-independent,
* modular,
* extensible,
* compatible with distributed cluster architecture.

Authentication and authorization are intentionally separated.

---

# High-Level Architecture

```text
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
│   ├── permission.hpp
│   ├── group.hpp
│   ├── policy.hpp
│   ├── permission_resolver.hpp
│   └── access_manager.hpp
│
├── services/
│   ├── query_service.hpp
│   └── admin_service.hpp
│
├── server/
│   ├── middleware/
│   │    ├── auth_middleware.hpp
│   │    ├── access_logger.hpp
│   │    └── telemetry.hpp
│   │
│   └── routes/
│        ├── auth_routes.hpp
│        ├── query_routes.hpp
│        └── admin_routes.hpp
```

---

# Authentication vs Authorization

The system separates:

## Authentication (auth/)

Responsible for:

* user identity verification,
* password validation,
* JWT generation,
* JWT validation,
* session context creation.

Authentication answers:

```text
Who is the user?
```

---

## Authorization (rbac/)

Responsible for:

* permission validation,
* role/group resolution,
* policy evaluation,
* access checks.

Authorization answers:

```text
What is the user allowed to do?
```

---

# auth/ Subsystem

## user.hpp

Represents authenticated users.

Example fields:

```cpp
id
username
password_hash
salt
is_admin
```

---

## auth_service.hpp

Responsible for:

* login,
* registration,
* password verification,
* token issuing.

---

## jwt_service.hpp

Responsible for:

* JWT generation,
* JWT validation,
* signature verification,
* payload extraction.

JWT is used for stateless authentication.

---

## password_hasher.hpp

Responsible for:

* password hashing,
* salt generation,
* password verification.

Passwords are never stored in plaintext.

---

## session_context.hpp

Stores request authentication context.

Example:

```cpp
user_id
username
is_authenticated
```

---

## account_storage.hpp

Persistent storage abstraction for:

* users,
* credentials,
* account metadata.

---

## token_payload.hpp

Defines JWT payload structure.

Example:

```cpp
user_id
username
issued_at
expiration
```

---

# rbac/ Subsystem

## permission.hpp

Defines:

* permission types,
* access operations,
* resource scopes.

Example:

```cpp
enum class Permission {
    ReadTable,
    WriteTable,
    CreateTable,
    DropTable,
    CreateDatabase,
    DropDatabase
};
```

---

## group.hpp

Defines:

* user groups,
* group membership.

Groups simplify:

* shared access management,
* permission inheritance,
* administration.

---

## policy.hpp

Defines access control policies.

A policy describes:

* who receives permissions,
* which resources are affected,
* whether access is allowed.

Example schema:

```text
subject_type
subject_id
database_name
table_name
permission
allowed
```

Where:

```text
subject_type:
    user
    group
    default
```

Policies may represent:

* user-specific permissions,
* group permissions,
* default database permissions.

---

## permission_resolver.hpp

Responsible for:

* effective permission computation,
* policy aggregation,
* permission conflict resolution.

The resolver merges:

* default permissions,
* group permissions,
* user-specific permissions.

Resolution priority:

```text
User override
    >
Group permissions
    >
Default permissions
```

The resolver itself is stateless.

---

## access_manager.hpp

High-level authorization orchestration layer.

Responsible for:

* validating access requests,
* invoking PermissionResolver,
* enforcing authorization decisions.

The AccessManager does not:

* parse JWT,
* store users,
* handle HTTP routes,
* persist RBAC metadata.

---

# Persistent RBAC Storage

```text
./data/
├── users.tbl
├── groups.tbl
├── user_groups.tbl
└── permissions.tbl
```

---

# Table Descriptions

## users.tbl

Stores registered users.

Schema:

```text
id
username
password_hash
salt
is_admin
```

---

## groups.tbl

Stores RBAC groups.

Schema:

```text
id
name
```

---

## user_groups.tbl

Many-to-many mapping between:

* users,
* groups.

Schema:

```text
user_id
group_id
```

---

## permissions.tbl

Stores access policies.

Schema:

```text
subject_type
subject_id
database_name
table_name
permission
allowed
```

---

# Access Flow

## Authenticated Request Pipeline

```text
HTTP Request
↓
Auth Middleware
↓
JWT Validation
↓
user_id extraction
↓
AccountStorage
↓
User loading
↓
AccessManager
↓
PermissionResolver
↓
Policy evaluation
↓
Authorization result
↓
Route Handler / QueryService
```

---

# Server Responsibilities

The server must:

1. verify JWT signature,
2. validate token expiration,
3. extract authenticated user identity,
4. load user metadata,
5. evaluate RBAC permissions,
6. execute the requested operation only if authorized.

---

# Transport Independence

The RBAC subsystem is transport-independent.

RBAC does not depend on:

* HTTP,
* REST routes,
* middleware implementation,
* RPC transport.

RBAC operates only on:

* authenticated user identity,
* requested resource,
* requested operation.

This allows the same RBAC logic to be reused for:

* HTTP APIs,
* internal RPC,
* CLI tools,
* distributed cluster requests.

---

# Example Permissions

Example operations:

```text
ReadTable
WriteTable
CreateTable
DropTable
CreateDatabase
DropDatabase
ManageUsers
GrantPermissions
```

---

# API

Administrative endpoints are protected by RBAC.

Example endpoints:

```text
# Auth
POST /login
POST /register

# User query API
POST /query

# Admin API
POST   /admin/groups
POST   /admin/groups/{group_id}/users

POST   /admin/permissions/grant
POST   /admin/permissions/revoke
```

---

# Security Guarantees

The system guarantees:

* passwords are never stored in plaintext,
* authenticated requests are verified using JWT,
* authorization is centralized,
* permission resolution is deterministic,
* RBAC logic is isolated from transport layer implementation.

---

# Future Improvements

Potential future extensions:

```text
- deny-overrides policy model
- distributed permission cache
- token revocation lists
- refresh tokens
- MFA authentication
- audit logging
- row-level permissions
- column-level permissions
```
