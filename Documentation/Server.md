# Server Usage

This document describes how to run the HTTP server, authenticate users, grant RBAC permissions, and execute SQL queries through the server API.

## Build

```bash
cmake -B build
cmake --build build
```

The server binary is:

```bash
./build/dbms_server
```

## Start The Server

```bash
./build/dbms_server 8080 ./data
```

Arguments:

```text
./build/dbms_server [port] [data_dir]
```

Defaults:

```text
port: 8080
data_dir: ./data
```

The server stores database files and auth/RBAC tables in `data_dir`.

Important files:

```text
./data/users.tbl
./data/groups.tbl
./data/user_groups.tbl
./data/permissions.tbl
./data/roles.tbl
./data/jwt_secret
./data/access.log
```

`jwt_secret` is used to sign and verify JWT tokens. If the file does not exist,
the server creates it on startup. Keep this file stable between restarts:
changing or deleting it invalidates previously issued tokens.

## HTTP API

Public endpoints:

```text
POST /register
POST /login
GET  /metrics
```

Protected endpoints:

```text
POST /query
POST /admin/groups
POST /admin/groups/{group_id}/users
POST /admin/permissions/grant
POST /admin/permissions/revoke
```

Protected endpoints require:

```text
Authorization: Bearer <jwt_token>
```

## Recommended Client Flow

`dbms_client` supports authentication and automatically attaches JWT tokens to protected requests.

Register:

```bash
./build/dbms_client --register root rootpass
```

Login:

```bash
./build/dbms_client --login root rootpass
```

Both commands save the JWT token to:

```text
~/.dbms_client_token
```

Run SQL through `/query`:

```bash
printf 'SELECT * FROM app.users;\n' | ./build/dbms_client
```

Run SQL from a file:

```bash
./build/dbms_client script.sql
```

Use a custom token file:

```bash
./build/dbms_client --login root rootpass --token-file ./root.token
./build/dbms_client --token-file ./root.token script.sql
```

Pass a token explicitly:

```bash
./build/dbms_client --token "$ROOT_TOKEN" script.sql
```

Create a group:

```bash
./build/dbms_client --create-group analysts
```

Add a user to a group:

```bash
./build/dbms_client --add-user-to-group "$ALICE_ID" "$GROUP_ID"
```

Grant permission through the admin API:

```bash
./build/dbms_client --grant-permission user "$ALICE_ID" app users read_table
./build/dbms_client --grant-permission group "$GROUP_ID" app users read_table
./build/dbms_client --grant-permission default "" app users read_table
```

Revoke permission:

```bash
./build/dbms_client --revoke-permission user "$ALICE_ID" app users read_table
```

For database-level permissions, pass an empty table name:

```bash
./build/dbms_client --grant-permission user "$ROOT_ID" app "" create_database
```

## Raw HTTP: Register

```bash
curl -s -X POST http://127.0.0.1:8080/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"root","password":"rootpass"}'
```

Response:

```json
{"token":"..."}
```

Save the token:

```bash
ROOT_TOKEN=$(curl -s -X POST http://127.0.0.1:8080/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"root","password":"rootpass"}' | jq -r .token)
```

## Raw HTTP: Login

```bash
ROOT_TOKEN=$(curl -s -X POST http://127.0.0.1:8080/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"root","password":"rootpass"}' | jq -r .token)
```

## Bootstrap Admin

When `users.tbl` is empty, the first registered user becomes admin automatically.
Register or log in as that first user:

```bash
ROOT_TOKEN=$(curl -s -X POST http://127.0.0.1:8080/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"root","password":"rootpass"}' | jq -r .token)
```

Get the root user id:

```bash
ROOT_ID=$(awk -F'\t' '$2=="root"{print $1}' ./data/users.tbl)
```

## Permissions

Supported permission names in the HTTP API:

```text
read_table
write_table
create_table
drop_table
create_database
drop_database
manage_users
grant_permissions
```

Admin users bypass RBAC checks. Non-admin users need explicit permissions.

The `/query` endpoint maps SQL statements to permissions:

| SQL statement | Required permission |
|---------------|---------------------|
| `CREATE DATABASE` | `create_database` |
| `DROP DATABASE` | `drop_database` |
| `CREATE TABLE` | `create_table` |
| `DROP TABLE` | `drop_table` |
| `SELECT` | `read_table` |
| `INSERT` | `write_table` |
| `UPDATE` | `write_table` |
| `DELETE` | `write_table` |
| `REVERT` | `write_table` |

Grant permission:

```bash
curl -s -X POST http://127.0.0.1:8080/admin/permissions/grant \
  -H "Authorization: Bearer $ROOT_TOKEN" \
  -H 'Content-Type: application/json' \
  -d "{\"subject_type\":\"user\",\"subject_id\":\"$ROOT_ID\",\"database_name\":\"app\",\"table_name\":\"users\",\"permission\":\"read_table\"}"
```

Grant permission to a group:

```bash
curl -s -X POST http://127.0.0.1:8080/admin/permissions/grant \
  -H "Authorization: Bearer $ROOT_TOKEN" \
  -H 'Content-Type: application/json' \
  -d "{\"subject_type\":\"group\",\"subject_id\":\"$GROUP_ID\",\"database_name\":\"app\",\"table_name\":\"users\",\"permission\":\"read_table\"}"
```

Grant default permission:

```bash
curl -s -X POST http://127.0.0.1:8080/admin/permissions/grant \
  -H "Authorization: Bearer $ROOT_TOKEN" \
  -H 'Content-Type: application/json' \
  -d "{\"subject_type\":\"default\",\"subject_id\":\"\",\"database_name\":\"app\",\"table_name\":\"users\",\"permission\":\"read_table\"}"
```

For database-level permissions, use an empty `table_name`:

```bash
curl -s -X POST http://127.0.0.1:8080/admin/permissions/grant \
  -H "Authorization: Bearer $ROOT_TOKEN" \
  -H 'Content-Type: application/json' \
  -d "{\"subject_type\":\"user\",\"subject_id\":\"$ROOT_ID\",\"database_name\":\"app\",\"table_name\":\"\",\"permission\":\"create_database\"}"
```

## User Registration And Permission Grant

Users are created through `/register`, not through the admin API.

Find Alice's user id:

```bash
ALICE_ID=$(awk -F'\t' '$2=="alice"{print $1}' ./data/users.tbl)
```

Grant Alice query permissions:

```bash
curl -s -X POST http://127.0.0.1:8080/admin/permissions/grant \
  -H "Authorization: Bearer $ROOT_TOKEN" \
  -H 'Content-Type: application/json' \
  -d "{\"subject_type\":\"user\",\"subject_id\":\"$ALICE_ID\",\"database_name\":\"app\",\"table_name\":\"users\",\"permission\":\"read_table\"}"
```

## Raw HTTP: Execute SQL

The `/query` endpoint accepts raw SQL text.

```bash
curl -s -X POST http://127.0.0.1:8080/query \
  -H "Authorization: Bearer $ROOT_TOKEN" \
  -H 'Content-Type: text/plain' \
  --data-binary $'CREATE DATABASE app;\nCREATE TABLE app.users (id INT INDEXED, name STRING NOT NULL);\nINSERT INTO app.users (id, name) VALUE (1, "Ann"), (2, "Bob");\nSELECT * FROM app.users;'
```

Example output contains one JSON response per SQL statement:

```json
{"message":"Database 'app' created","status":"ok"}
{"message":"Table 'users' created","status":"ok"}
{"count":2,"operation":"insert","status":"ok"}
[{"id":1,"name":"Ann"},{"id":2,"name":"Bob"}]
```

Run a read query as Alice:

```bash
ALICE_TOKEN=$(curl -s -X POST http://127.0.0.1:8080/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"alicepass"}' | jq -r .token)

curl -s -X POST http://127.0.0.1:8080/query \
  -H "Authorization: Bearer $ALICE_TOKEN" \
  -H 'Content-Type: text/plain' \
  --data-binary 'SELECT * FROM app.users;'
```

## Smoke Tests

Missing token returns `401 Unauthorized`:

```bash
curl -i -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: text/plain' \
  --data-binary 'SELECT * FROM app.users;'
```

Authenticated user without required RBAC permission returns `403 Forbidden`:

```bash
BOB_TOKEN=$(curl -s -X POST http://127.0.0.1:8080/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"bob","password":"bobpass"}' | jq -r .token)

curl -i -X POST http://127.0.0.1:8080/query \
  -H "Authorization: Bearer $BOB_TOKEN" \
  -H 'Content-Type: text/plain' \
  --data-binary 'SELECT * FROM app.users;'
```

Metrics are public:

```bash
curl -s http://127.0.0.1:8080/metrics
```

## Notes

The first-admin bootstrap flow is automatic only when `users.tbl` is empty. Existing data directories keep their current admin flags.
