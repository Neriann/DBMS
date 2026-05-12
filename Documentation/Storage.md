# Storage Manager

The **StorageManager** is responsible for persisting the database state to the filesystem. It efficiently saves both metadata and raw row data, providing durability for `DBMS`.

## Directory Structure
When a `StorageManager` is instantiated, it relies on a root `data_dir` (e.g., `./data` or `./test_data`). Inside this directory, each database gets its own sub-directory. 

```
./data/
└── database/
    ├── schema.json
    ├── my_table1.bin
    └── my_table2.bin
    └── ... (other tables) ...
```

## Formats

### 1. `schema.json` (Metadata)
Each database directory contains a `schema.json` file. This file tracks the structure of every table inside the database, including the columns, their types, and integer constraints (e.g., `NOT_NULL`, `INDEXED`).

Example schema payload:
```json
{
    "table1": [
        {
            "name": "id",
            "type": 0,          
            "constraints": 1
        },
        {
            "name": "username",
            "type": 1,
            "constraints": 0
        }
    ]
}
```

### 2. `.bin` Files (Physical Rows Representation)
For each table, a `.bin` file stores the exact row data.

**Tombstones:** Each row begins with a `bool` byte (0 or 1) indicating if the row was deleted.

**Values:** Following the tombstone, the row specifies its columns as a list of parsed `std::variant<int, std::string, std::nullptr_t>` data types. 
- A single `char` prefix represents the active variant type:
  - `0`: Integer. Read next 4 bytes (`int`).
  - `1`: String. Read `size_t` (length), then read the raw array of characters.
  - `2`: Null. No further data read.

This layout allows the engine to accurately load rows on startup, ignoring elements logically marked as erased via Tombsotnes.
