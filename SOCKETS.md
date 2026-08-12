# Berkeley Sockets Walkthrough (for Memora)

This is a reference for the POSIX/BSD socket API — the pieces you're using
(and will soon need) in [memora.c](memora.c) to turn Memora into a TCP
server. It's organized in the order a server actually calls these things.

## Headers

| Header | What it gives you |
|---|---|
| `<sys/socket.h>` | Core socket API: `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`/`recv()`, `setsockopt()`, `struct sockaddr`. |
| `<netinet/in.h>` | Internet-domain addressing: `struct sockaddr_in`, `struct in_addr`, `htons()`/`htonl()`, `INADDR_ANY`, `IPPROTO_*`. |
| `<arpa/inet.h>` | Address text↔binary conversion: `inet_addr()`, `inet_pton()`, `inet_ntop()`, `inet_aton()`. |
| `<unistd.h>` | `close()`, `read()`, `write()`. |
| `<fcntl.h>` | `fcntl()` — used to set non-blocking mode (`O_NONBLOCK`). Not yet included in [memora.h](memora.h). |
| `<sys/epoll.h>` (Linux only) | Scalable I/O readiness notification for many concurrent client sockets: `epoll_create1()`, `epoll_ctl()`, `epoll_wait()`. |
| `<poll.h>` | Portable alternative to `select()`/`epoll`: `poll()`. |

All three of `sys/socket.h`, `netinet/in.h`, and `arpa/inet.h` are already
included in [memora.h](memora.h:15-17).

---

## 1. `socket()` — create an endpoint

```c
int socket(int domain, int type, int protocol);
```

- `domain` — address family. `AF_INET` = IPv4, `AF_INET6` = IPv6, `AF_UNIX` = local/Unix-domain socket.
- `type` — `SOCK_STREAM` = TCP (reliable, ordered byte stream), `SOCK_DGRAM` = UDP (connectionless datagrams).
- `protocol` — usually `0`, letting the kernel pick the default for the domain/type pair (TCP for `AF_INET`+`SOCK_STREAM`).
- Returns a file descriptor (an `int`) on success, `-1` on error (check `errno`).

In your code:
```c
s = socket(AF_INET, SOCK_STREAM, 0);   // memora.c:13
```
This asks for a IPv4 TCP socket — correct for a database server that speaks a request/response protocol over persistent connections.

A socket fd behaves like any other fd: it can be passed to `read()`/`write()`/`close()`, and to `select()`/`poll()`/`epoll`.

---

## 2. Address structures

```c
struct sockaddr_in {
    sa_family_t    sin_family;  // AF_INET
    in_port_t      sin_port;    // port, network byte order
    struct in_addr sin_addr;    // IPv4 address, network byte order
};

struct in_addr {
    uint32_t s_addr;
};
```

`bind()`, `connect()`, and `accept()` all technically take a generic
`struct sockaddr *`, which is why you see the cast:
```c
bind(s, (struct sockaddr *)&sock, sizeof(sock));   // memora.c:19
```
`struct sockaddr_in` and `struct sockaddr` are the same size; the cast is
just satisfying the older, generic C API that predates a proper union/tagged
type.

### Byte order: `htons()` / `htonl()` / `ntohs()` / `ntohl()`

Network protocols use big-endian ("network byte order"); x86/ARM are
little-endian. These convert between host and network order:

- `htons(x)` — host to network, 16-bit (use for **port**).
- `htonl(x)` — host to network, 32-bit (use for **IPv4 address**, if built manually).
- `ntohs()` / `ntohl()` — the reverse, used when reading a value *out* of a received packet/struct.

```c
sock.sin_port = htons(PORT);   // memora.c:10 — correct
```

### Address conversion: `inet_addr()` vs `inet_pton()`

```c
sock.sin_addr.s_addr = inet_addr(HOST);   // memora.c:11
```
`inet_addr()` converts a dotted-decimal string ("127.0.0.1") to a
network-byte-order `in_addr_t`. **It's deprecated** because its error value
(`INADDR_NONE`, i.e. `-1`/`0xFFFFFFFF`) is indistinguishable from the valid
address `255.255.255.255`. Prefer:

```c
inet_pton(AF_INET, HOST, &sock.sin_addr);   // returns 1 on success, 0 on bad string, -1 on error
```

For the reverse (binary → text), use `inet_ntop()` — useful for logging a
connected client's IP:
```c
char buf[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &client_addr.sin_addr, buf, sizeof(buf));
```

`INADDR_ANY` (i.e. `0.0.0.0`) is the usual choice for a server's bind
address instead of a hardcoded loopback IP — it listens on all local
interfaces. Since Memora binds only to `127.0.0.1`, it'll only be reachable
from the same machine, which is fine for local dev.

---

## 3. `bind()` — attach a socket to an address/port

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

Associates the socket with a local IP + port so the kernel knows to route
incoming packets on that port to this socket. Only needed for servers
(clients get an ephemeral port auto-assigned). Returns `0` on success, `-1`
on error — common failure is `EADDRINUSE` ("Address already in use"), which
happens constantly during dev when you restart the server quickly after a
previous run, because the OS holds the port in `TIME_WAIT` for a bit.

**Fix**: set `SO_REUSEADDR` before binding (see `setsockopt` below) — you'll
want this almost immediately, or every rerun of Memora during development
will intermittently fail to bind.

```c
your code (memora.c:19-21): on bind failure you perror() but don't return —
execution falls through to close(s) and the loop retries forever with a
socket that never bound. Worth an explicit `return;` there, mirroring the
socket() failure path above it.
```

---

## 4. `listen()` — mark socket as passive/accepting (not yet in your code)

```c
int listen(int sockfd, int backlog);
```

Turns an active (client-style) socket into a passive one that can accept
incoming connections. `backlog` is the max length of the queue of pending
(not-yet-accepted) connections — a common starting value is `128`. Must be
called after `bind()` and before `accept()`. This is the next call you're
missing to make Memora actually serve anything.

---

## 5. `accept()` — pull a connection off the queue (not yet in your code)

```c
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

Blocks until a client connects, then returns a **new** file descriptor
representing that specific client connection — the original `sockfd` stays
open and listening for more connections. `addr`/`addrlen` are optional
out-parameters that get filled with the client's address, useful for
logging/access control:

```c
struct sockaddr_in client;
socklen_t clen = sizeof(client);
int client_fd = accept(s, (struct sockaddr *)&client, &clen);
```

This is the fd you `read()`/`recv()`/`write()`/`send()` on for that client,
and `close()` when the client disconnects. `s` itself is never used for I/O.

---

## 6. `connect()` — client-side (for when Memora needs to act as a client, e.g. replication)

```c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

Used by a client (or a server connecting *out*, e.g. to a replica) to
initiate a TCP handshake to a remote `addr`. Not needed for Memora's basic
listen loop, but relevant if you ever add replication or a CLI client.

---

## 7. Sending/receiving data

Once you have a connected fd (from `accept()` server-side, or `connect()`
client-side):

```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

Or the plain fd equivalents (equivalent to `send`/`recv` with `flags = 0`):
```c
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
```

Key things to know for a DB server's wire protocol:
- Both can return **fewer bytes than requested** — TCP is a stream, not a
  message protocol. You must loop until you've sent/read everything you
  expect, or implement length-prefixed framing.
- Return `0` from `recv()`/`read()` means the peer closed the connection
  (EOF) — treat as a normal disconnect, not an error.
- Return `-1` means error; check `errno` (e.g. `EINTR` means "retry",
  `ECONNRESET` means the peer reset the connection).
- Common `flags`: `MSG_NOSIGNAL` (Linux — suppress `SIGPIPE` when writing to
  a closed socket, so you get an `EPIPE` errno instead of the process dying).

---

## 8. `close()` and `shutdown()`

```c
close(s);   // memora.c:23
```
`close()` releases the fd. For a connected TCP socket it also initiates
connection teardown (FIN) once there are no other references to the fd.

`shutdown(fd, SHUT_RDWR)` is a finer-grained tool — it can close just the
read side, just the write side, or both, *without* releasing the fd — useful
when you need to signal "no more writes" to a peer but still want to drain
remaining reads, or when multiple threads/processes share the fd and you
don't want to invalidate it for all of them yet.

**Note on your current code**: `mainloop()` creates a socket, binds it, and
immediately `close()`s it, then the `while (server_continuation)` loop in
`main()` calls `mainloop()` again — creating and destroying a fresh socket
every iteration, forever, without ever accepting a connection. This is
presumably scaffolding; the real structure you're heading toward is closer
to: bind + listen **once**, then loop on `accept()`.

---

## 9. `setsockopt()` / `getsockopt()`

```c
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
```

Configures socket behavior. The one you'll want almost immediately:

```c
int opt = 1;
setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```
Call this **after `socket()`, before `bind()`**. Lets you rebind to a port
still in `TIME_WAIT` from a previous run — without it, restarting Memora
during development will often fail with `EADDRINUSE`.

Other options worth knowing:
- `SO_REUSEPORT` — allows multiple processes to bind the *same* port (used for multi-process load balancing across cores).
- `TCP_NODELAY` (level `IPPROTO_TCP`) — disables Nagle's algorithm, reducing latency for small, latency-sensitive request/response messages (very relevant for a DB protocol).
- `SO_RCVTIMEO` / `SO_SNDTIMEO` — timeouts on blocking recv/send calls.

---

## 10. Handling multiple clients: blocking vs. multiplexing

A naive server that calls `accept()` then `recv()` in a single-threaded loop
can only serve one client at a time. Options, roughly in order of
sophistication:

1. **Fork/thread per connection** — simple, but doesn't scale to many
   thousands of connections.
2. **`select(2)`** — portable, but limited to `FD_SETSIZE` (usually 1024)
   fds and O(n) scanning each call.
3. **`poll(2)`** — like `select`, no fd-count limit, still O(n) scanning.
4. **`epoll` (Linux-only)** — O(1) readiness notification, the standard
   choice for high-performance Linux servers (Redis, Nginx use this model):
   ```c
   int epfd = epoll_create1(0);
   struct epoll_event ev = { .events = EPOLLIN, .data.fd = client_fd };
   epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
   struct epoll_event events[MAX_EVENTS];
   int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
   ```
   For sockets used this way, set them non-blocking first via `fcntl(fd, F_SETFL, O_NONBLOCK)`, since `epoll` only tells you a socket is *ready*, not how much data is available.

Given Memora is a from-scratch database server, `epoll` is the natural
long-term target once you have `listen()`/`accept()` working — it's the
same model Redis's event loop uses.

---

## 11. Error handling

- `perror("label")` — prints `label: <strerror(errno)>` to stderr. You're
  already using this correctly (`memora.c:15,20`).
- Always check return values: `socket()`, `bind()`, `listen()`, `accept()`
  all return `-1` on failure and set `errno`.
- `strerror(errno)` / `errno` directly, if you want to branch on specific
  failure reasons (e.g. retry on `EINTR`, treat `EADDRINUSE` specially).

---

## Minimal correct skeleton (where memora.c is heading)

```c
int s = socket(AF_INET, SOCK_STREAM, 0);
if (s < 0) { perror("socket"); return; }

int opt = 1;
setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

struct sockaddr_in addr = {0};
addr.sin_family = AF_INET;
addr.sin_port = htons(PORT);
inet_pton(AF_INET, HOST, &addr.sin_addr);

if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(s); return; }
if (listen(s, 128) < 0) { perror("listen"); close(s); return; }

while (server_continuation) {
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);
    int cfd = accept(s, (struct sockaddr *)&client, &clen);
    if (cfd < 0) { perror("accept"); continue; }

    // handle_client(cfd) — read request, process, send response
    close(cfd);
}

close(s);
```

The key structural difference from your current `mainloop()`: `socket()` +
`bind()` + `listen()` happen **once**, and the loop is around `accept()`,
not around the whole setup.
