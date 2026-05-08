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
| `B_tree`   | B-tree    |         |
| `BP_tree`  | B+-tree   |         |
| `BS_tree`  | B\*-tree  |         |
| `BSP_tree` | B\*+-tree |    ✓    |

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

### `DBMS_ALLOCATOR`

Selects the memory allocator used by the index tree.

| Value            | Allocator        | Default |
|------------------|------------------|:-------:|
| `global_heap`    | Global heap      |         |
| `boundary_tags`  | Boundary tags    |         |
| `sorted_list`    | Sorted free list |         |
| `buddies_system` | Buddy system     |         |
| `red_black_tree` | Red-black tree   |    ✓    |

```bash
# Default (red-black tree)
cmake -B build

# Global heap allocator
cmake -B build -DDBMS_ALLOCATOR=global_heap

# Boundary tags allocator
cmake -B build -DDBMS_ALLOCATOR=boundary_tags

# Buddy system allocator
cmake -B build -DDBMS_ALLOCATOR=buddies_system

# Sorted list allocator
cmake -B build -DDBMS_ALLOCATOR=sorted_list
```

Options can be combined:

```bash
cmake -B build -DDBMS_INDEX_TREE=B_tree -DDBMS_ALLOCATOR=red_black_tree
```

## License

See [LICENSE](LICENSE).
