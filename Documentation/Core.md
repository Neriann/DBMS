# Core Module Documentation

## Overview

The `core` module implements the in-memory relational DBMS engine. It is organized in three layers:

```
DBMS  ->  Database  ─>  Table (Schema / Row / Index)
```

---

## Related files

| Header                        | Source                  |
|-------------------------------|-------------------------|
| `include/core/dbms.hpp`       | `src/core/dbms.cpp`     |
| `include/core/database.hpp`   | `src/core/database.cpp` |
| `include/core/table.hpp`      | `src/core/table.cpp`    |
| `include/core/schema.hpp`     | `src/core/schema.cpp`   |
| `include/core/row.hpp`        | *(header-only)*         |
| `include/core/value.hpp`      | `src/core/value.cpp`    |
| `include/core/index_tree.hpp` | *(header-only)*         |
| `include/core/allocator.hpp`  | *(header-only)*         |

---

## Types

### `Value`

```cpp
using Value = std::variant<int, std::string, std::nullptr_t>;
```

Represents a single cell value. The three alternatives correspond to `INT`, `STRING`, and `NULL`.

**`ValueComparator`** provides an ordering over `Value`: `int` < `string` < `nullptr_t`;
within the same type, values are ordered naturally.

---

### `Row` / `RowID`

`Row` is a vector of cell values aligned to a table's `Schema`. `RowID` is the index into `Table::data_`

---

### `Schema` / `Column`

A schema is a list of column descriptors. Column position is the stable identifier, names are only for lookup.

**`Column::is_not_null()`** — returns `true` when the `NOT_NULL` flag is set.
**`Column::is_indexed()`** — returns `true` when the `INDEXED` flag is set.

**`find(schema, name) → int`** — linear find, returns -1 if not found.

---

### `IndexTree<tkey, tvalue, cmp>`

A compile-time alias for the local `BSP_tree` (B*+ tree).

---

### `Allocator`

Alias for `std::pmr::unsynchronized_pool_resource`, used by the DBMS-level tree allocator.

---

## Classes

### `DBMS`

Holds all databases in an `IndexTree<string, unique_ptr<Database>>` and tracks the
current database for use.

| Method                  | Throws                            | Notes                                                                       |
|-------------------------|-----------------------------------|-----------------------------------------------------------------------------|
| `create_database(name)` | `runtime_error` if already exists | Inserts a new `Database` into the tree                                      |
| `drop_database(name)`   | `out_of_range` if absent          | Destroys the database, clears `current_db_` if it pointed to the dropped DB |
| `get_database(name)`    | `out_of_range` if absent          | Returns a mutable/const reference on DB                                     |
| `has_database(name)`    | —                                 | Returns `true` if exists else `false`                                       |
| `use(name)`             | `out_of_range` if absent          | Sets current DB                                                             |
| `current_database()`    | —                                 | Returns current DB and`nullptr` when no DB is selected                      |

---

### `Database`

Owns a named collection of tables in an `IndexTree<string, unique_ptr<Table>>`.

| Method                       | Throws                            | Notes                                   |
|------------------------------|-----------------------------------|-----------------------------------------|
| `create_table(name, schema)` | `runtime_error` if already exists | Schema is moved into the new `Table`    |
| `drop_table(name)`           | `out_of_range` if absent          | Destroys the table and all its indexes  |
| `get_table(name)`            | `out_of_range` if absent          | Returns a mutable/const reference on DB |
| `has_table(name)`            | —                                 | Returns `true` if exists else `false`   |
| `name()`                     | —                                 | Returns the current database name       |

---

### `Table`

Stores rows and maintains optional secondary indexes for `INDEXED` columns.

The tree maps a column value to the `RowID` of the row that holds it, enforcing uniqueness.

| Method            | Throws                                                                               | Notes                                                                    |
|-------------------|--------------------------------------------------------------------------------------|--------------------------------------------------------------------------|
| `insert(row)`     | `invalid_argument` on type/null/duplicate violations                                 | Inserts row                                                              |
| `update(id, row)` | `out_of_range` if id is invalid/deleted; `invalid_argument` on constraint violations | Re-indexes only columns whose value changed                              |
| `erase(id)`       | `out_of_range` if id is invalid/deleted                                              | Sets tombstone `true`, removes from all indexes; slot in `data_` is kept |
| `schema()`        | —                                                                                    | Returns const reference to the schema                                    |
| `data()`          | —                                                                                    | Returns const reference to the raw row vector (includes tombstoned rows) |
| `is_deleted(id)`  | —                                                                                    | Returns true if id is tombstone already                                  |
