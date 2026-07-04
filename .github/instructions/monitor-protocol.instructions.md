---
applyTo: 'src/monitor.*,src/serializermonitor.*,src/initsendermonitor.*'
---

# Monitor Protocol

## TL;DR
`Monitor` is the server-side entity for a connected visualization client (e.g. rcssmonitor); it plays back full ground-truth state each cycle and accepts a small set of god-mode admin commands.
- [monitor.h](../../src/monitor.h) / [monitor.cpp](../../src/monitor.cpp) (755 lines) — connection entity + command parser (`dispfoul`, `dispplayer`, `dispcard`, `compression`, coach_* commands).
- [serializermonitor.h](../../src/serializermonitor.h) — versioned text/JSON wire-format serializers (`SerializerMonitorStdv1..v5`, `SerializerMonitorJSON`), analogous to player serializers but monitors get the *entire* game state, not a limited view cone.
- [initsendermonitor.h](../../src/initsendermonitor.h) — versioned handshake senders (`InitSenderMonitorV1..V3`, `InitSenderMonitorJSON`) that push `server_param`/`player_param`/`player_type`/score/playmode on connect.
- **Open the full file when:** debugging a monitor command parse failure (`dispplayer`/`dispcard`/coach_*), adding a new monitor protocol version, or tracing why `sendShow()` output looks wrong for a given client version.

## Overview
A `Monitor` (`src/monitor.h:40`) is created when the server receives a UDP `(dispinit)` / `(dispinit version <N>)` packet on the shared player port. `Stadium::parseMonitorInit()` ([stadium.cpp:2774](../../src/stadium.cpp:2774)) parses the requested version (`ver`, default `1.0`; `-1` means JSON protocol), enforces `ServerParam::instance().maxMonitors()` ([stadium.cpp:2782](../../src/stadium.cpp:2782)), instantiates `new Monitor(*this, ver)`, calls `mon->connect(addr)` then `mon->setSenders()`, and on success appends to `Stadium::M_monitors` (`MonitorCont`, a `std::vector<Monitor*>`) and immediately calls `mon->sendInit()` ([stadium.cpp:2790-2819](../../src/stadium.cpp:2790)).

Monitors are NOT players or coaches: they get no view-cone-limited data, no cycle-based `(think)`/`(sense_body)` loop, and their commands are either admin/debug actions (`dispplayer`, `dispfoul`, `dispcard`, `dispdiscard`, `compression`) or — only when `ServerParam::coachMode()`/`coachWithRefereeMode()` is on — a coach-like subset (`change_mode`, `move`, `recover`, `change_player_type`, `check_ball`) reusing the *online coach* vocabulary (see `Monitor::coach_*` at [monitor.cpp:548+](../../src/monitor.cpp:548)).

## Architecture
- **Sender pattern**: same Observer/Sender split used elsewhere in rcssserver. `Monitor` owns `rcss::InitObserverMonitor * M_init_observer` and `rcss::ObserverMonitor * M_observer` ([monitor.h:44-45](../../src/monitor.h:44)); these hold `shared_ptr`s to version-specific `InitSenderMonitor`/`DispSenderMonitor` created via `rcss::Factory<Creator,int>` (`SerializerMonitor::factory()`, `InitSenderMonitor::factory()`, `DispSenderMonitor::factory()`).
- **`Monitor::setSenders()`** ([monitor.cpp:119-178](../../src/monitor.cpp:119)) is the composition root: looks up `SerializerMonitor::Creator` by `(int)version()`, builds the serializer, then looks up `DispSenderMonitor::Creator` and `InitSenderMonitor::Creator` by the same integer version and wires them into `M_observer`/`M_init_observer`. If any factory lookup fails (`Unsupported monitor protocol version`), the connection is rejected in `Stadium::parseMonitorInit`.
- **Version registration** (serializer side, [serializermonitor.cpp:850-855](../../src/serializermonitor.cpp:850)): `v1`,`v2`→`SerializerMonitorStdv1`; `v3`→`SerializerMonitorStdv3`; `v4`→`SerializerMonitorStdv4` (adds stamina capacity); `v5`→`SerializerMonitorStdv5` (adds focus point); `-1`→`SerializerMonitorJSON`. Each serializer internally binds to a specific `SerializerCommon` version too (v3→common 8, v4→common 15, v5→common 18, JSON→common -1) — see [serializermonitor.cpp:63,113,336,380,441](../../src/serializermonitor.cpp:63).
- **Init sender inheritance chain**: `InitSenderMonitorV1` → `V2` (server/player param/type format changes) → `V3` (also overrides `sendScore`/`sendPlayMode`); `InitSenderMonitorJSON` is a separate top-level branch off `InitSenderMonitor` for the JSON protocol (version `-1`).
- **`Monitor::sendInit()`** ([monitor.cpp:180-186](../../src/monitor.cpp:180)) calls `sendServerParams()`, `sendPlayerParams()`, `sendPlayerTypes()` — but NOT `sendPlayMode`/`sendScore` (those are sent lazily, only on change).

## Patterns & Conventions
- `Monitor::sendPlayMode()` / `sendScore()` are **edge-triggered**: they cache last-sent state (`M_playmode`, `M_team_l_name`, `M_team_l_score`, etc. — [monitor.h:50-56](../../src/monitor.h:50)) and only call the init-sender when something actually changed ([monitor.cpp:189-222](../../src/monitor.cpp:189)), avoiding redundant `(playmode ...)`/`(team ...)` messages every cycle.
- `Monitor::sendTeamGraphics()` ([monitor.cpp:225-293](../../src/monitor.cpp:225)) throttles XPM team-graphic tile sends: max 32 tiles/cycle in `PM_BeforeKickOff`, else 8/cycle, tracked via `M_left_sent_graphics`/`M_right_sent_graphics` (`std::set<GraphKey>` where `GraphKey = pair<x,y>`) so already-sent tiles are never resent.
- `Monitor::sendShow()` ([monitor.cpp:296-301](../../src/monitor.cpp:296)) = `M_observer->sendShow()` (delegates to the `DispSenderMonitor` for the full show packet) + `sendTeamGraphics()`.
- Command parsing (`Monitor::parseCommand`, [monitor.cpp:338-413](../../src/monitor.cpp:338)) is a flat `strcmp`/`strncmp` chain, not a table — mirrors the pattern used elsewhere in rcssserver (e.g. Player command parsing) rather than a generic parser combinator.
- `Monitor::parseMsg()` ([monitor.cpp:100-116](../../src/monitor.cpp:100)) defensively null-terminates unterminated buffers and, for `version() < 0 || >= 2.0`, warns the client via `(warning message_not_null_terminated)`.

## Key Abstractions
- `Monitor` (`RemoteClient` subclass) — per-connection entity, [monitor.h](../../src/monitor.h).
- `rcss::SerializerMonitor` (base, in `serializer.h`) with concrete `SerializerMonitorStdv1/v3/v4/v5` and `SerializerMonitorJSON` — pure text formatting of team/show/playmode/score/player/ball/message blocks ([serializermonitor.h:39-304](../../src/serializermonitor.h:39)).
- `rcss::InitSenderMonitor` (base) with `InitSenderMonitorV1/V2/V3/JSON` and its observer wrapper `rcss::InitObserverMonitor` ([initsendermonitor.h:42-316](../../src/initsendermonitor.h:42)) — one-shot handshake data (server params, player params, player types, playmode/score updates).
- `rcss::DispSenderMonitor` / `rcss::ObserverMonitor` (declared in `dispsender.h`, not read in this pass) — per-cycle `(show ...)` packet sender; wired the same way as the init sender inside `setSenders()`.
- `Stadium::MonitorCont` = `std::vector<Monitor*>` member `M_monitors` — the live monitor registry.

## Integration Points
- **Connection/handshake**: `Stadium::parseMonitorInit()` ([stadium.cpp:2774-2824](../../src/stadium.cpp:2774)), triggered from the shared player-port UDP receive loop `Stadium::udp_recv_message()` ([stadium.cpp:2578](../../src/stadium.cpp:2578)) — monitors and players share one UDP socket (`M_player_socket`); the server disambiguates by matching `cli_addr` against `M_remote_players` first, then `M_monitors` ([stadium.cpp:2599-2620](../../src/stadium.cpp:2599)), falling through to `parseMonitorInit`/`parsePlayerInit` for brand-new senders.
- **Per-cycle push**: `Stadium::sendDisp()` ([stadium.cpp:940](../../src/stadium.cpp:940)) iterates `M_monitors` and calls `m->sendShow()` for each — this is called from `Stadium::step()`'s per-cycle path (confirmed in `game-loop-and-timers` instruction, [stadium.cpp:890](../../src/stadium.cpp:890)).
- **Admin/debug commands** dispatch into `Stadium` mutators: `dropBall`, `callFoul`, `movePlayer`, `discardPlayer`, `yellowCard`/`redCard`, `kickOff` — see `Monitor::dispfoul/dispplayer/dispdiscard/dispcard/parseCommand` ([monitor.cpp:338-517](../../src/monitor.cpp:338)).
- **`ServerParam::instance().maxMonitors()`** caps concurrent monitor connections; `coachMode()`/`coachWithRefereeMode()` gates the coach-style subset of monitor commands.
- **`mon->setEnforceDedicatedPort(ver < 0 || ver >= 2.0)`** ([stadium.cpp:2815](../../src/stadium.cpp:2815)) — v2+/JSON monitors are expected to move to a dedicated port after handshake (like players), v1 monitors stay on the shared port.

## Build & Test
No dedicated unit tests for `Monitor`/`SerializerMonitor` found under `src/`; validated indirectly via integration with `rcssmonitor` (sibling repo `D:\workspace\robo\ss2d\rcssmonitor\src\monitor_client.cpp`) or manual `rcssserver`+`rcssmonitor` runs. Build is part of the standard rcssserver `src/` object list (autotools/CMake — see root build files, not covered in this pass).

## Logging
Uses plain `std::cout`/`std::cerr` (no structured logger) for connection lifecycle: `"A new (v<ver>) monitor connected."` / `"A new (json) monitor connected."` ([stadium.cpp:2808-2812](../../src/stadium.cpp:2808)), and `setSenders()` failures (`"Unsupported monitor protocol version..."`, `"Could not create monitor serializer."`, `"failed to create DispSenderMonitor v..."` / `InitSenderMonitor`). Commented-out `std::cerr` debug traces remain in `sendTeamGraphics()`/`setSenders()` (dead code, safe to ignore or clean up).

## Important Notes
- **Version is a plain `double`** (`M_version`, cast to `int` for factory lookups) — fractional versions (e.g. `2.5`) truncate to the same serializer/sender as their integer floor; only whole-number factory slots exist (1,2,3,4,5,-1 for JSON). `version() < 0` is the sentinel for the JSON protocol, not a real negative version.
- `monitor_protocol-v3.txt` at repo root of `src/` ([monitor_protocol-v3.txt](../../src/monitor_protocol-v3.txt)) is a legacy Shift-JIS (Japanese) plaintext spec of the v3 `(show ...)` s-expression grammar (`<Player>`, `<Pos>`, `<Counts>`, etc.) and binary struct sizes (`short_showinfo_t2 = 1428`, `dispinfo_t2 = 2056`); useful as a wire-format reference but encoding may render as mojibake in some editors — treat it as historical documentation, cross-check against `SerializerMonitorStdv3` for the current authoritative format.
- The commented-out binary-struct code in `Monitor::sendMsg()` ([monitor.cpp:308-335](../../src/monitor.cpp:308)) shows the old binary `dispinfo_t`/`dispinfo_t2` message-board protocol (v1/v2) that has been superseded by the text-based `(msg ...)` format for v3+ — dead code kept for reference.
- Monitor and Player share the exact same UDP socket/port for the initial handshake; only after `setEnforceDedicatedPort` do v2+/JSON monitors migrate off it, so a stray malformed packet on the player port must be checked against both `M_remote_players` and `M_monitors` before being treated as a new connection attempt.
- Coach-style monitor commands (`change_mode`, `move`, etc.) silently share code paths with genuine online-coach commands via `Stadium` — a monitor in `coachMode()` has nearly the same god-mode power as an online coach client.

## See Also
- [senders-dispatch.instructions.md](senders-dispatch.instructions.md) — generic Sender/Observer/Factory framework `DispSenderMonitor` and `SerializerMonitor` build on.
- [serialization-protocol-versions.instructions.md](serialization-protocol-versions.instructions.md) — cross-cutting protocol version story (player/coach/monitor serializers together).
- [networking-io.instructions.md](networking-io.instructions.md) — `RemoteClient`/socket transport `Monitor` inherits from.
- [game-loop-and-timers.instructions.md](game-loop-and-timers.instructions.md) — `Stadium::step()`/`sendDisp()` per-cycle scheduling that drives `Monitor::sendShow()`.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
