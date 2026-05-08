# DBMS

A relational DataBase Management System, built using a B*+-tree

## Build

```bash
cmake -B build
cmake --build build
```

This produces three binaries in `build/`:

| Binary        | Description     |
|---------------|-----------------|
| `dbms`        | Interactive CLI |
| `dbms_server` | HTTP server     |
| `dbms_client` | HTTP client     |

### Build options

See [Documentation/Build.md](Documentation/Build.md).

## License

See [LICENSE](LICENSE).
