# Studying Memora: A Beginner's Guide

You said you can read the C syntax but don't yet see the *logic and flow* —
what the program is actually doing and why. This guide starts from zero and
builds up to exactly the code sitting in this repo.

Two files are doing two very different jobs right now:

- **`memora.c` / `memora.h`** — a TCP server. It listens for network
  connections and accepts them. This is the "database server" shell.
- **`tree.c` / `tree.h`** — a data structure (an LCRS tree). This is where
  data would actually be *stored*. It's a standalone program right now (it
  has its own `main()`) — it is **not yet wired up to the server**. That's
  the next piece of work once you're comfortable with both halves.

They aren't connected yet. Keep that in mind — it explains why `memora.c`
never mentions `Node` or `Leaf` anywhere.

---

## How to run this

### `tree.c` (standalone demo, has its own `main()`)

The `Makefile` builds this one already:

```sh
make        # compiles tree.c -> ./tree
./tree      # runs it, prints the values it stored/read via printf
```

`make` alone reruns `clean` first (see the `all: clean tree` target), so
it's always a fresh build. `make clean` removes `*.o` and `tree` if you
just want to tidy up.

### `memora.c` (the TCP server)

Not wired into the `Makefile` yet, so compile it by hand with the same
flags the Makefile uses for `tree`:

```sh
cc -Wall -O2 -std=c2x memora.c -o memora
./memora          # listens on the default port, 8008
./memora 9000     # or pass a port explicitly (argv[1])
```

You'll see:

```
Server listening on port 8008
```

From a second terminal, connect to it like a raw TCP client with `nc`
(netcat) — this is how you "talk" to the server before there's a real
protocol wired up:

```sh
nc 127.0.0.1 8008
```

The server prints `Connection from 127.0.0.1:<port>` and
`Status:100 - Connected to Memora`, then just sits there — `child_loop()`
is a stub that does nothing but `sleep(1)` in a loop, so nothing else
happens yet. Ctrl+C the `nc` session (or the server) to end it.

Note: `cc` will emit a warning about `zero((Client *)client, ...)` — a
pointer-type mismatch in the `zero()` call in `mainloop()`. It's harmless
(compiles and runs fine) but is one of the "things worth noticing" flagged
later in Part 3.

---

## Part 1 — C fundamentals you need before any of this makes sense

### Pointers, in one paragraph

A variable holds a value. A pointer holds an **address** — the location in
memory where a value lives, not the value itself. `int *p` means "p holds
the address of an int." `*p` ("dereference p") means "go to that address and
read/write what's there." `&x` means "give me the address of x." That's the
entire idea; everything else is bookkeeping around it.

```c
int x = 5;
int *p = &x;   // p now holds x's address
*p = 10;       // go to that address, write 10 there — x is now 10
```

### `struct` — grouping related fields

```c
struct s_client {
    int fd;
    char ip[16];
    int16 port;
};
```

A `struct` bundles fields together under one name, like a row in a table.
`client.fd`, `client.ip`, `client.port` — one variable, three pieces of data.

### `typedef` — giving a type a shorter nickname

```c
typedef struct s_client Client;
```

This doesn't create anything new. It just means you can write `Client`
instead of `struct s_client` everywhere else in the code. Purely a
readability convenience.

```c
typedef unsigned int int32;
typedef unsigned short int int16;
typedef unsigned char int8;
```

Same idea: `int32`/`int16`/`int8` are just renamed versions of built-in
types, chosen so it's obvious how many bits/bytes each variable takes up
(4 / 2 / 1 bytes respectively). This matters a lot in systems code because
struct layouts and network protocols care about *exact* sizes — plain `int`
isn't guaranteed to be the same size on every machine, but `unsigned int`
renamed as `int32` documents the intent.

### `malloc` / `free` — manual memory management

In C, nothing is garbage-collected. If you want memory that outlives the
current function call (e.g. a struct that needs to stick around after
`mainloop()` returns), you ask the OS for it explicitly:

```c
Client *client = (Client *)malloc(sizeof(struct s_client));
```

`malloc(n)` reserves `n` bytes on the **heap** and returns a pointer to the
start of that block. You are now responsible for calling `free(client)`
later — if you don't, that memory is never reclaimed until the process exits
(a "memory leak"). `sizeof(struct s_client)` computes exactly how many bytes
one `Client` struct needs, so `malloc` knows how much to hand back.

`assert(client)` right after is a safety check: `malloc` returns `NULL` if
the system is out of memory. `assert` crashes immediately with a clear
message if that ever happens, instead of silently continuing with a null
pointer and crashing confusingly later.

### Pointer to pointer / array decay (you'll see `int8 *buffer`)

```c
void zero(int8 *buffer, int16 size)
{
    int8 *p;
    int16 n;
    for (n = 0, p = buffer; n < size; n++, p++)
    {
        *p = 0;
    }
}
```

`buffer` is the address of the first byte of some block of memory. `p` walks
forward one byte at a time (`p++` advances the pointer by one `int8`, i.e.
one byte), writing `0` to each byte until `n` reaches `size`. This is a
hand-rolled version of what the standard library calls `memset(buffer, 0,
size)`. It exists here so that freshly `malloc`'d structs start from known,
predictable "all zero" memory instead of whatever garbage bytes happened to
be there before — otherwise `client->fd` might start out as some random
leftover number.

---

## Part 2 — What is a socket, actually?

A **socket** is the OS's handle for "one end of a network connection." Think
of it like a file descriptor (in fact, on Linux, it *is* a file descriptor —
just an `int`), except instead of reading/writing to a file on disk, you're
reading/writing to a network peer.

Server-side TCP setup always follows the same five steps, and you'll see all
five in `initserver()`:

1. **`socket()`** — ask the OS for a fresh socket. Returns a file descriptor
   (an `int`), or a negative number on failure.
2. **`bind()`** — say "I want *this* socket to own this specific IP address
   and port number" (here: `127.0.0.1:8008` by default).
3. **`listen()`** — say "start queuing up incoming connection attempts on
   this socket; don't reject them, I'll get to them." The `20` argument is
   the max number of connections allowed to queue up before you `accept()`
   them.
4. **`accept()`** — pull one pending connection off that queue. This is a
   *blocking* call — the program pauses here doing nothing until a client
   actually connects. It returns a **brand new** file descriptor representing
   that one specific client's connection (separate from the original
   listening socket).
5. Read/write data on the fd `accept()` gave you, then eventually `close()`
   it.

### Why two file descriptors (`server_fd` vs `client_fd`)?

This trips people up. `server_fd` is the *listening* socket — it never
carries actual data, it only exists to keep spawning new client connections.
Each time `accept()` succeeds, you get a **different** fd (`client_fd`) that
represents the actual pipe to that one client. You keep `server_fd` open and
call `accept()` on it again and again, in a loop, to handle client after
client — this is exactly what the `while (s_continuation) { mainloop(server_fd); }`
loop in `initserver()` is doing.

### Byte order: `htons()` / `ntohs()`

Different computer architectures store multi-byte numbers in different byte
orders ("endianness"). Network protocols standardize on one order ("network
byte order," big-endian) so that any two machines can agree regardless of
their own internal representation.

- `htons(port)` = "host to network short" — convert a 16-bit number from
  your machine's native order to network order, before sending it out
  (used when `bind()`-ing: `sock.sin_port = htons(port)`).
- `ntohs(x)` = "network to host short" — the reverse, converting an
  incoming value back to your machine's native order so the number reads
  correctly (used in `mainloop()`: `port = (int16)ntohs(cli.sin_port)`).

You always convert going out, and convert back coming in. That's the whole
rule.

### `fork()` — spawning a child process

```c
pid = fork();
```

`fork()` is unusual: it's a function that **returns twice**. It clones the
entire currently-running process into two identical copies. Then:

- In the **parent**, `fork()` returns the child's process ID (a positive
  number).
- In the **child**, `fork()` returns `0`.

That's the *only* way the two copies can tell themselves apart afterward —
same code, same variables, same point of execution, just a different return
value from that one call. That's why you always see:

```c
pid = fork();
if (pid) {
    // parent path — pid is the child's PID here (truthy)
} else {
    // child path — pid is 0 here
}
```

In `mainloop()`, the **parent** immediately frees its `client` struct and
returns — going back to `accept()` to wait for the *next* connection. The
**child** is the one that actually services this specific client
(`child_loop(client)`), while the parent stays free to keep accepting new
connections. This is the classic "one process per connection" server model
(as opposed to threads, or a single-process event loop).

---

## Part 3 — `memora.c`, walked through in the order it actually runs

Execution order is **not** top-to-bottom in the file — it starts at
`main()`. Here's the real order:

### 1. `main()` (bottom of the file)

```c
int main(int argc, char *argv[])
{
    char *s_port;
    int16 port;

    if (argc < 2)
        s_port = PORT;        // default "8008", from memora.h
    else
        s_port = argv[1];     // user passed a port on the command line

    port = (int16)atoi(s_port);   // parse the string "8008" into the number 8008

    s_continuation = true;
    initserver(port);

    return 0;
}
```

`argc`/`argv` are how command-line arguments arrive in C: `argc` is how many
words were typed, `argv` is the array of those words as strings. `argv[0]` is
always the program's own name, so `argv[1]` is the first *real* argument —
here, an optional port number, e.g. running `./memora 9000`.

`atoi` = "ASCII to integer": converts the *text* `"9000"` into the *number*
`9000`.

`s_continuation` is a global `bool` used as an off switch for the server's
main loop (see below) — set once here, checked elsewhere.

### 2. `initserver(port)`

This does the five socket setup steps from Part 2, then loops:

```c
while (s_continuation)
{
    mainloop(server_fd);
}
```

Every iteration calls `mainloop()`, which — as you now know — blocks on
`accept()` until a client shows up.

### 3. `mainloop(server_fd)` — runs once per incoming connection

- Blocks on `accept()`.
- If nothing came through (`client_fd < 0`), sleeps 1 second and bails —
  avoids spinning the CPU at 100% retrying instantly.
- Otherwise: prints the client's IP/port, `malloc`s a `Client` struct,
  zeroes it, fills in its fields.
- `fork()`s. Parent frees its copy of `client` and returns (goes back to
  accepting). Child sets `c_continuation = true` and loops calling
  `child_loop(client)` until told to stop.

### 4. `child_loop(client)`

```c
void child_loop(Client *cli) {
    sleep(1);
    return;
}
```

Currently a placeholder — it just sleeps one second per iteration and
returns. This is where request handling (reading a command from the client,
looking it up in the tree, replying) will eventually go. Right now the
child process just loops doing nothing once a second, forever, per
connected client.

### Things worth noticing as you read (not bugs to necessarily "fix" — just things that'll make sense once you trace the flow yourself)

- `close(client_fd)` and the final `free(client)` at the bottom of
  `mainloop()` sit *after* the `if (pid) {...} else {...}` block, but both
  branches of that `if` already `return` — so that trailing code never
  executes. Once you're comfortable with control flow, try tracing this by
  hand: does execution ever reach line 78?
- The parent branch passes `server_fd` into `client->fd` (`client->fd =
  server_fd;`) rather than `client_fd` — worth asking yourself which fd
  should actually be used to talk to *this specific client*.
- `while (c_continuation) { child_loop(client); }` never sets
  `c_continuation` to false anywhere, so a connected child process loops
  forever (once a second) rather than exiting when the client disconnects.

You don't need to fix these to understand the flow — but once the shape of
the program clicks, these are the kind of things you'll start spotting
yourself, which is a good sign you're reading it as *logic* now instead of
just syntax.

---

## Part 4 — `tree.c` / `tree.h`: the actual data structure

This part is unrelated to sockets — it's a standalone exercise in building a
tree data structure, run via its own `main()` in `tree.c` (build/run it with
`make`, producing the `tree` executable — note the `Makefile` currently
builds `tree.c`, not `memora.c`).

### What is an LCRS tree?

"LCRS" = **Left-Child, Right-Sibling**. It's a trick for representing a tree
where any node can have *any number* of children, using a struct that only
has two pointer fields (instead of needing a variable-length array of
children). The idea:

- A node's `left` pointer points to its **first child**.
- A node's `right` pointer points to its **next sibling** (another child of
  the *same* parent).

So instead of "node → list of children," you get "node → first child, and
then follow `right` pointers sideways to find the rest of the children."

In this codebase specifically, the two pointer roles are split across two
different struct types with a themed metaphor — **think of it like a
filesystem**:

- **`Node`** = a directory. Its `left` points to a child `Node` one level
  deeper (like a subdirectory). Its `right` points to the *head of a list of
  `Leaf`s* — the "files" living directly in this directory.
- **`Leaf`** = a file: a key/value pair. Leaves belonging to the same `Node`
  are chained together via their own `right` pointer (`Leaf.right` → next
  `Leaf` under the same parent).

```c
struct s_node {
    Tag tag;
    struct s_node *uplink;   // parent (root points to itself)
    struct s_node *left;     // first child Node
    struct s_leaf *right;    // head of this node's Leaf chain
    int8 path[256];          // e.g. "/users/login"
};

struct s_leaf {
    Tag tag;
    union u_tree *left;      // owning Node (if first leaf) or previous Leaf
    struct s_leaf *right;    // next Leaf under the same Node
    int8 key[128];
    int8 *value;             // heap-allocated bytes
    int16 size;
};
```

Current limitation worth noticing: because `Node.left` is a *single*
pointer, `create_node()` only supports **one child node per parent** right
now — calling it twice on the same parent overwrites the link to the first
child instead of adding a sibling next to it. A full LCRS implementation
would need `create_node()` to walk the existing sibling chain (via each
child's own... currently-unused sibling pointer) and append, the same way
`create_leaf()` already does for leaves. That's a natural next exercise once
you're comfortable with the existing code.

### The tagged union trick (`union u_tree`)

```c
union u_tree {
    Node node;
    Leaf leaf;
};
```

A `union` is a block of memory that can be *interpreted* as different
types — unlike a `struct` (which lays fields out one after another, each
with its own space), a `union`'s fields **overlap the same memory**. So
`Tree` is a block of memory big enough for the larger of `Node` or `Leaf`,
and you decide which one you're looking at based on context.

That's exactly why every struct starts with the same first field, `Tag
tag;` — since both `Node` and `Leaf` put `tag` first, no matter which one a
block of memory actually holds, reading the first byte always tells you
which kind it is (`TagNode` vs `TagLeaf`) before you interpret the rest.
This pattern (a shared leading tag field to distinguish union members) is
sometimes called a "tagged union" or "variant record" — pointers typed
`union u_tree *left` can therefore point at *either* a `Node` or a `Leaf`,
and code checks `->tag` to know which.

### Walking through `tree.c`'s `main()`

```c
node  = create_node((Node *)&root, "/users");
node2 = create_node(node, "/users/login");

l1 = create_leaf(node2, "jonas", "abc7123456aa", 12);

printf("%s", l1->value);   // prints: abc7123456aa
```

This builds:

```
root ("/")
 └── /users            (Node, root's child)
      └── /users/login (Node, /users' child)
           └── jonas -> "abc7123456aa"   (Leaf)
```

`create_node(parent, path)`:
1. `malloc`s a new `Node`, zeroes it.
2. Links it in: `parent->left = node` (parent's *first/only* child, per the
   limitation above).
3. Sets `node->uplink = parent` so you can walk back up later.
4. Copies the path string in with `strncpy` (bounded copy — always leave
   room for the string, hence copying at most 255 of the 256 available
   bytes, keeping the last byte `0` as the terminator).

`create_leaf(parent, key, value, count)`:
1. Calls `find_last_linear(parent)` to walk `parent`'s existing leaf chain
   and find the current last leaf (or discover there isn't one yet).
2. `malloc`s a new `Leaf`.
3. If `parent` had no leaves yet, attaches directly: `parent->right =
   new_leaf`. Otherwise, appends after the previous last leaf:
   `leaf->right = new_leaf`.
4. Copies `key` in (bounded), `malloc`s its own private copy of `value`
   (the leaf *owns* this memory — no one else should free it), and records
   `size`.

### `reterr()` — a small but idiomatic C pattern worth understanding

```c
#define reterr(x) \
    do { errno = (x); return null_ptr; } while (0)
```

`errno` is a global variable the C standard library uses to report *why*
something failed. `reterr(x)` sets `errno` to `x` and returns `NULL`
("nothing found") in one step — so callers can distinguish "empty, nothing
wrong" from "an actual error occurred," by checking `errno` after getting a
null pointer back.

The `do { ... } while (0)` wrapper is a common macro idiom: it lets a
multi-statement macro be used safely anywhere a single statement is
expected (e.g. inside `if (cond) reterr(x); else ...` — without the
wrapper, the trailing semicolon after a bare `{...}` block can break the
`else` clause).

---

## Part 5 — How the two halves are meant to fit together

Right now:

- `memora.c` accepts connections and does nothing with them yet
  (`child_loop` is a stub).
- `tree.c` builds and reads a tree, but only inside its own throwaway
  `main()` — the `root` tree and the `create_node`/`create_leaf` functions
  aren't reachable from `memora.c` at all.

The natural next step (once you're comfortable with both halves
separately) is wiring them together: `child_loop()` would read a command
off the client's socket (e.g. `SET /users/login jonas abc123` or `GET
/users/login jonas`), and call into the tree functions from `tree.c`/`tree.h`
to store or retrieve data, then write a response back on the socket. That's
the point where this becomes a real client/server database instead of two
separate demos.

## Suggested order to actually study this

1. Get comfortable with pointers/structs/malloc in isolation — Part 1.
2. Build and run `tree.c` (`make`, then `./tree`), and step through it with
   a debugger or just `printf` statements, until the parent/child/leaf
   linking in Part 4 feels obvious rather than something you're told.
3. Read `memora.c` bottom-to-top in *execution* order as laid out in Part 3,
   ideally while running it and connecting to it (`nc 127.0.0.1 8008` from
   another terminal) so you can watch `printf` output appear as you trigger
   each step.
4. Only once both halves feel solid, think about how you'd connect them.
