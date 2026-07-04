---
applyTo: 'rcss/net/**,rcss/gzip/**,src/remoteclient.*,src/client.*'
---

# Networking & Compressed I/O

## TL;DR
`rcss/net` wraps raw BSD sockets in `iostream`-compatible stream buffers, `rcss/gzip` layers optional zlib compression on top, and `src/remoteclient.*` glues both into the per-agent UDP transport used by players/coaches/monitors.
- `rcss::net::Socket` (`rcss/net/socket.hpp:32`) is the IPv4-only, ref-counted base (`std::shared_ptr<SocketDesc>`) for `UDPSocket`/`TCPSocket`.
- `RemoteClient` (`src/remoteclient.h:39`) owns one `UDPSocket` + `SocketStreamBuf` + optional `gzstreambuf`, forming the send/recv path for every `Player`/`Coach`/`Monitor`.
- `rcss::gz::gzstreambuf` (`rcss/gzip/gzstream.hpp:34`) compresses/decompresses in-memory (used by `RemoteClient` for `(compression)` protocol messages); `gzofstream`/`gzifstream` (`rcss/gzip/gzfstream.hpp`) write/read `.rcg.gz`/`.rcl.gz` files directly to disk.
- **Open the full file when:** modifying send/recv framing, non-blocking/timeout behavior, or the gzip flush semantics — these files are small (50-600 lines) and behavior is easy to get subtly wrong.

## Overview
This subsystem has three layers:
1. **`rcss/net/`** — thin C++ wrapper around POSIX/Winsock sockets exposing `std::streambuf`-based I/O (`SocketStreamBuf`) so the rest of the server can `operator<<`/`operator>>` onto a socket like any stream.
2. **`rcss/gzip/`** — zlib-based `std::streambuf` implementations: `gzstreambuf` wraps another streambuf (used to compress the live UDP message stream) and `gzfilebuf` wraps a `.gz` file directly (used by the game/text logger).
3. **`src/remoteclient.*` / `src/client.cpp`** — the server-side per-connection object model: `RemoteClient` (abstract base with `parseMsg()`) is subclassed by `Player`, `Coach`, `Monitor`; `src/client.cpp`'s `Client` class is the standalone `rcssserver::client`/monitor-launch helper (uses raw `select()` over `UDPSocket`, not `RemoteClient`).

## Architecture
```
Socket (rcss/net/socket.hpp)         Addr (sockaddr_in wrapper, addr.hpp)
  ├─ UDPSocket  (udpsocket.cpp)  ── used by RemoteClient (agent/monitor traffic)
  └─ TCPSocket  (tcpsocket.cpp)  ── available but NOT instantiated anywhere in src/
        (accept/listen only referenced from rcss/net itself; no TCP consumer in src/)

SocketStreamBuf (socketstreambuf.cpp) : std::streambuf
  wraps a Socket -> overflow()/sync() call Socket::send(); underflow() calls Socket::recv()

gzstreambuf (gzstream.cpp) : std::streambuf
  wraps ANY std::streambuf (e.g. a SocketStreamBuf) -> zlib deflate/inflate in memory

gzfilebuf (gzfstream.cpp) : std::streambuf
  wraps a gzFile handle (zlib direct-to-disk) -> used ONLY for .rcg.gz / .rcl.gz logs

RemoteClient (src/remoteclient.h:39)
  M_socket (UDPSocket) -> M_socket_buf (SocketStreamBuf) -> M_transport (std::ostream)
  M_gz_buf (gzstreambuf), swapped into M_transport->rdbuf() when compression is enabled
```
Subclasses of `RemoteClient` (`Player`, `Coach`, `Monitor` — see `src/player.h`, `src/coach.h`, `src/monitor.h`) implement `parseMsg()` to interpret decompressed agent commands.

## Patterns & Conventions
- **IPv4 only, hardcoded**: `UDPSocket::doOpen()` (`rcss/net/udpsocket.cpp:74`) and presumably `TCPSocket::doOpen()` call `::socket(AF_INET, SOCK_DGRAM/SOCK_STREAM, ...)` directly — no IPv6 path exists anywhere in this subtree.
- **Ref-counted FD ownership**: `Socket` stores its descriptor in a `std::shared_ptr<SocketDesc>` with a custom deleter `Socket::closeFD` (`socket.cpp:71`) that calls `::close()`/`::closesocket()`. Copying a `Socket` shares the FD; the socket only truly closes when the last reference drops or `close()` is called explicitly.
- **Non-blocking by construction**: `RemoteClient::open()` (`remoteclient.cpp:109`) always calls `M_socket.setNonBlocking()` right after `open()`; `recv()` (`remoteclient.cpp:163`) treats `errno == EWOULDBLOCK` as "no data" rather than an error, and `ECONNREFUSED` is silently swallowed (common for UDP "destination unreachable" ICMP bounces from disconnected clients).
- **Streambuf overflow/sync/underflow triad**: Both `SocketStreamBuf` and `gzstreambuf` follow the same `std::streambuf` override pattern — `overflow()` flushes the put area then re-arms it, `sync()` is just a flush, `underflow()` reads into an internal buffer and calls `setg()`. When editing either, preserve this triad or streaming I/O silently corrupts.
- **Connectionless vs connected framing**: `SocketStreamBuf::underflow()` (`socketstreambuf.cpp:141`) auto-connects the socket to the sender's address on first read when `ConnType == CONN_ON_READ` — this is how a `UDPSocket` bound to the well-known port (6000) "learns" a specific client's ephemeral port on first packet.
- **Compression is a runtime toggle, not a build option**: `RemoteClient::setCompressionLevel()` (`remoteclient.cpp:231`) swaps `M_transport`'s `rdbuf()` between the raw `M_socket_buf` and a `gzstreambuf` wrapping it — this implements the `(compression <lvl>)` server command clients can send mid-session.

## Key Abstractions
- `rcss::net::Addr` (`addr.hpp:33`) — pimpl wrapper around `sockaddr_in`; `PortType=uint16_t`, `HostType=uint32_t`; `Addr::ANY`/`Addr::BROADCAST` constants; `getHostStr()`/`getPortStr()` for DNS/service-name resolution (uses `getaddrinfo`-style lookups internally in `addr.cpp`).
- `rcss::net::Handler` (`handler.hpp:33`) — Meyer's singleton (`Handler::instance()`) that performs `WSAStartup`/`WSACleanup` on Windows only (`#ifdef _WIN32`); no-op on POSIX. **Not referenced anywhere under `src/`** in this codebase — it exists for library completeness / other consumers (e.g. rcssmonitor, librcsc) rather than being wired into rcssserver's own startup path.
- `rcss::gz::gzstreambuf` vs `gzfilebuf` — do not confuse: `gzstreambuf` compresses an in-memory/socket byte stream (level `-1` = passthrough, matches `RemoteClient::M_comp_level`); `gzfilebuf` (`gzfstream.hpp:45`) drives a real `gzFile` for `.rcg.gz`/`.rcl.gz` disk logs, with `DEFAULT_COMPRESSION`/`BEST_SPEED`/`BEST_COMPRESSION` enums (`gzfstream.hpp:57`) and its own `flushBuf()`/`makeModeString()` helpers.
- `Decompressor` (from `src/compress.h`, used in `remoteclient.h:50`) — zlib inflate helper invoked in `RemoteClient::processMsg()` (`remoteclient.cpp:208`) with `Z_SYNC_FLUSH`, feeding the decompressed bytes to the virtual `parseMsg()`.

## Integration Points
- **`src/logger.cpp:307,395`** — game log (`.rcg.gz`) and text log (`.rcl.gz`) writers construct `rcss::gz::gzofstream` directly, bypassing `RemoteClient`/sockets entirely — pure file I/O compression path.
- **`src/player.cpp`, `src/coach.cpp`, `src/monitor.cpp`** — all subclass `RemoteClient` to get UDP send/recv + optional compression "for free"; they only implement `parseMsg()`.
- **`src/client.cpp`** (the standalone `Client` class, not `RemoteClient`) — uses a raw `UDPSocket` plus POSIX `select()` (`client.cpp:257`) directly; this is a separate, older/alternate connection helper, not part of the `RemoteClient` hierarchy — check call sites before assuming they share code paths.
- Default UDP ports (6000 players/monitor, 6002 online coach) are bound via `Addr`/`UDPSocket( const Addr & addr )` constructors in the `Stadium`/`ServerParam` setup code, not in this subtree itself.

## Build & Test
- Both `rcss/net` and `rcss/gzip` build as part of the dual Autotools (`Makefile.am`) / CMake (`CMakeLists.txt`) system alongside the rest of `rcssserver`; no standalone unit tests exist for these files — validate changes by building the full server (`make` or `cmake --build`) and running a short game with `rcssserver` + a couple of agents/monitor to confirm connect/compress/log round-trips still work.
- `HAVE_LIBZ` (from `config.h`, autoconf `AC_CHECK_LIB(z, ...)`) gates all gzip code paths in `RemoteClient`; building without zlib disables compression (`setCompressionLevel()` always returns `-1`).

## Logging
- Socket-level errors are written directly to `std::cerr` with `__FILE__`/`__LINE__` (e.g. `remoteclient.cpp:100`, `:115`, `:124`) rather than going through a central logger — grep for `strerror( errno )` in this subtree to find all such sites.
- `ECONNREFUSED` is treated as expected/benign noise (a departed UDP peer) and is explicitly excluded from the stderr spam in both `RemoteClient::send()` and `RemoteClient::recv()`.

## Important Notes
- Changing `SocketStreamBuf`'s buffer size (default 8192, `socketstreambuf.hpp:64`) affects max single-datagram UDP write size — oversized writes will fail/truncate since UDP has no fragmentation-reassembly here.
- `TCPSocket` is fully implemented (`connect`/`accept`/`listen`) but has **no active caller inside `src/`** as of this investigation — treat any change to it as speculative/library-surface work, not a live server code path, unless you find a new caller.
- The `M_remained`/`M_remained_char` byte-parity bookkeeping in both `SocketStreamBuf::underflow()` and `gzstreambuf`'s equivalent exists to handle `sizeof(char_type)` != 1 platforms; on normal `char`-based builds this is effectively a no-op but do not remove it without checking `char_type` usage.

## See Also
- `player-command-protocol.instructions.md` — parses the decompressed bytes `RemoteClient::parseMsg()` hands off (built on top of this transport).
- `monitor-protocol.instructions.md` — `Monitor` subclass of `RemoteClient`; consumes the same UDP+gzip path for board-state broadcasts.
- `logging-and-savers.instructions.md` — uses `rcss::gz::gzofstream`/`gzifstream` directly for `.rcg.gz`/`.rcl.gz` files (no socket involvement).
- `senders-dispatch.instructions.md` — higher-level per-agent "Sender" classes that ultimately call `RemoteClient::send()`.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
