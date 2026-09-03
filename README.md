# Memora

![language](https://img.shields.io/badge/language-C%20(c2x)-blue)
![build](https://img.shields.io/badge/build-make-brightgreen)
![status](https://img.shields.io/badge/status-work--in--progress-yellow)

A small TCP database server written in C. Clients log in over a raw socket, create or select a
**database**, and shape a **path tree** (think directories) whose nodes can
eventually hold key/value **leaves**.

- **One process per client** — `fork()`'d off the accept loop, so a crash in
  one connection's handler can't take down another.
- **Left-child/right-sibling tree** — an arbitrary-arity tree encoded with
  just two pointers per node (`left`, `sibling`).
- **Hand-rolled hash table** — separate chaining, `path → Node*`, so lookups
  don't require walking the tree.
- **Ownership-scoped persistence** — every database is a flat file under
  `database/`, replayed line-by-line on `use-database`. Nodes are stored as
  `<path>` lines and leaves as `<path>|<key>|<value>` lines.

## Table of contents

- [Getting started](#getting-started)
- [Command reference](#command-reference)
- [Users & roles](#users--roles)
- [Data structures](#data-structures)
- [Project layout](#project-layout)
- [Known limitations](#known-limitations)

## Getting started

Needs libcrypt (`crypt.h` + `-lcrypt`) for password hashing — part of
glibc/libxcrypt on most distros; install `libcrypt-dev` if yours splits it
out separately.

```sh
make            # builds build/memora
./build/memora  # starts the server on 127.0.0.1:8008 (or ./build/memora <port>)
```

On first run (no `database/users.db` yet), the server prompts on its own
stdin/stdout to create the initial admin user before it starts accepting
connections — this happens exactly once, before `initserver()` ever calls
`accept()`, never over the socket.

Talk to it with any TCP client, e.g. `nc`:

```sh
$ nc 127.0.0.1 8008
OK: Connected to Memora.
login admin hunter2
OK: Logged in successfully.
create-database mydb
OK: Ownership added successfully.
OK: Database created successfully.
use-database mydb
OK: Database loaded successfully.
OK: Using database 'mydb'.
create-node /users
OK: Added child node.
create-leaf /users name Yash
OK: Leaf added successfully.
tree
OK: Printing tree.
/ (0)
  child: /users (1)
print-leaves /users
/users --> name:Yash
```

Every response follows the same convention: `OK: <message>` on success,
`ERR: <message>` on failure. Commands that return data (`tree`, `hash-table`,
`list-databases`, `ping`) print the payload as plain lines with no prefix.

## Command reference

Registered in `handlers[]` ([memora.c](src/memora.c)):

| Command | Args | Login required | Description |
|---|---|---|---|
| `ping` | 0 | no | Health check → `pong` |
| `login` | `<username> <password>` | no | Authenticate the connection |
| `logout` | 0 | yes | End the session |
| `create-user` | `<username> <password>` | yes, admin only | Create a new user account |
| `delete-user` | `<username>` | yes, admin only | Delete a user account (not the caller's own) |
| `create-database` | `<name>` | yes | Create a new database, owned by the caller |
| `use-database` | `<name>` | yes | Load a database's tree + hash table |
| `delete-database` | `<name>` | yes | Delete a database owned by the caller |
| `rename-database` | `<old_name> <new_name>` | yes | Rename a database owned by the caller |
| `list-databases` | 0 | yes | List databases owned by the caller |
| `create-node` | `<path>` | yes | Add a path node under the active database |
| `remove-node` | `<path>` | yes | Remove a path node (and its subtree) |
| `search-node` | `<path>` | yes | Check whether a path exists |
| `tree` | 0 | yes | Print the active database's tree |
| `hash-table` | 0 | yes | Dump the active database's hash table buckets |
| `create-leaf` | `<path> <key> <value>` | yes | Add a key/value leaf to the node at `path` |
| `update-leaf` | `<path> <key> <value>` | yes | Replace the value of an existing leaf |
| `delete-leaf` | `<path> <key>` | yes | Remove a leaf from the node at `path` |
| `print-leaves` | `<path>` | yes | Print all leaves on the node at `path` |

## Users & roles

Every user has a `UserRole` (`ROLE_ADMIN` or `ROLE_USER`, defined in
[memora.h](include/memora.h)). `login` looks it up in `users.db` and caches
it on the `Client` struct, so admin-gated handlers just check
`client->role` instead of re-reading the users file on every command.

- The first admin is bootstrapped once, interactively, on the server's own
  stdin/stdout (`init()`/`create_admin()` in [users.c](src/users.c)) — never
  over the wire.
- Every account after that is managed by an already-authenticated admin, via
  `create-user` / `delete-user`. `delete-user` also refuses to delete the
  caller's own account, to avoid an accidental admin lockout.
- `users.db` stores `username|password_hash|role` lines; passwords are
  hashed with libc's `crypt()`, never stored or compared in plaintext.

## Data structures

### The path tree (left-child / right-sibling)

Each path segment is a `Node`. A `Node` points at its **first child**
(`left`) and its **next sibling** (`sibling`) — a classic LCRS encoding of
an arbitrary-arity tree using only two pointers per node. A `Leaf` hangs
off a `Node`'s `leaves` list (a singly-linked list via `right`) to store an
actual key/value pair, managed with `create-leaf`/`update-leaf`/
`delete-leaf`/`print-leaves`.

```mermaid
graph TD
    root["/ (TagRoot|TagNode)"]
    a["/a (TagNode)"]
    b["/b (TagSibling)"]
    c["/c (TagSibling)"]
    x["/x (TagNode)"]
    y["/y (TagSibling)"]

    root -- left --> a
    a -- sibling --> b
    b -- sibling --> c
    a -- left --> x
    x -- sibling --> y
```

Reading this diagram: `/a`, `/b`, `/c` are siblings (children of root);
`/a` additionally has its own children `/x`, `/y`. The `tag` byte on each
node (`TagNode` vs `TagSibling`) records whether that node reached its
parent via the `left` link or the `sibling` link — `remove_node` needs
this to know which parent pointer to relink.

### The hash table

Separate chaining, `strdup`'d path string → `Node*` (stored as `int8*`).
Used so `add_node`/`remove_node` don't have to walk the tree to find a
parent by path.

```mermaid
graph LR
    subgraph buckets["buckets[capacity]"]
        b0["[0]"]
        b1["[1]"]
        b2["[2]"]
    end
    b0 --> e1["/a → Node*"] --> e2["/foo → Node*"]
    b1 --> null1["NULL"]
    b2 --> e3["/b → Node*"]
```

When `count / capacity` reaches `LOAD_FACTOR_THRESHOLD` (0.7), `insert`
doubles the capacity via `resize_table`, which rehashes every existing
entry into a fresh bucket array before continuing.

## Project layout

```
c-database-design/
├── include/           # Public headers, one per module
│   ├── database.h
│   ├── hash_table.h
│   ├── memora.h        # Client struct, CmdHandler, server entry points
│   ├── tree.h
│   ├── users.h
│   └── utils.h
├── src/                # One .c per header, same names
├── database/           # Runtime data: users.db, owners.db, <name>.db per db
├── build/              # Makefile output (objects + the memora binary)
└── Makefile
```

## Known limitations

- No self-service signup — accounts only come from an admin running
  `create-user`, or the one-time interactive admin bootstrap on first boot.
- One process per connection, no locking — concurrent clients touching
  the same database file (or `users.db`/`owners.db`) can race, including
  two writers colliding on the same `temp-<pid>.db` rewrite target.
