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

## Build options

### `DBMS_INDEX_TREE`

Selects the index-tree implementation used to store databases inside the DBMS instance.

| Value      | Tree      | Default |
|------------|-----------|:-------:|
| `BSP_tree` | B\*+-tree |    ✓    |
| `B_tree`   | B-tree    |         |
| `BP_tree`  | B+-tree   |         |
| `BS_tree`  | B\*-tree  |         |

```bash
# Default (B*+-tree)
cmake -B build

# B-tree
cmake -B build -DDBMS_INDEX_TREE=B_tree

# B+-tree
cmake -B build -DDBMS_INDEX_TREE=BP_tree

# B*-tree
cmake -B build -DDBMS_INDEX_TREE=BS_tree
```

## License

See [LICENSE](LICENSE).
