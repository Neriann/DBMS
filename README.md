# DBMS

A relational DataBase Management System, built using a B*+-tree

## Build

```bash
cmake -B build
cmake --build build
```

This produces three binaries in `build/`:

| Binary        | Description                                          |
|---------------|------------------------------------------------------|
| `dbms`        | Interactive CLI                                      |
| `dbms_server` | Server process that owns DBMS state and executes SQL |
| `dbms_client` | Terminal client that sends SQL to the server         |

## Client-server mode

Start the server:

```bash
./build/dbms_server 8080 ./data
```

Send a query from stdin:

```bash
printf 'SELECT * FROM table_name;\n' | ./build/dbms_client --host 127.0.0.1 --port 8080
```

Or send a SQL file:

```bash
./build/dbms_client --port 8080 script.sql
```

### Build options

See [Documentation/Build.md](Documentation/Build.md).

## License

See [LICENSE](LICENSE).
