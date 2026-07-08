# rcssserver — Copilot Instructions

## TL;DR
rcssserver is the C++ game server for the RoboCup Soccer Simulation 2D league: it runs the per-cycle physics/rules simulation and speaks a plain-text UDP protocol to 22 player agents, 2 coaches, and any number of monitor/visualization clients.
- **3D ball flight is now an opt-in extension** (protocol version 20): a new `2d_mode` `ServerParam` (default `true`) gates it off entirely so existing deployments/clients see byte-identical legacy behavior; setting `2d_mode=false` enables ball-only vertical physics (loft kicks, gravity, bouncing, height-aware goalie catch) for clients that negotiate the new version. See [3d-protocol-migration.instructions.md](instructions/3d-protocol-migration.instructions.md) for the external-consumer migration guide.
- Entry point: [src/main.cpp](../src/main.cpp) → `ServerParam::init()` → `Stadium::init()` → `timer->run()` (see [game-loop-and-timers.instructions.md](instructions/game-loop-and-timers.instructions.md)).
- `src/` is a **flat** directory (~140 files, no subfolders) — components are grouped by filename convention only (e.g. all `serializer*.cpp`, all `initsender*.cpp`); use the Instruction Index below to navigate by topic instead of by folder.
- Dual build system: Autotools (`./bootstrap && ./configure && make`) and CMake (`cmake . && make`) — see Build & Test below.
- **Open a component instruction when:** you are about to touch a specific subsystem — each one is scoped to a small file set and documents the exact classes/functions/gotchas for that area.

## Project Overview
`rcssserver` simulates a soccer match: each cycle it reads UDP commands from connected agents (dash/kick/turn/say/etc.), advances physics and referee rule state, then sends back sensory messages (visual/see, sense_body, fullstate) plus display updates to monitors, and appends the cycle to a game log. It is one repository inside the broader RoboCup 2D ecosystem at `D:\workspace\robo\ss2d` — see the Related Repositories section for how it fits with the monitor, sample agent team, and shared client library.

## Architecture
High-level per-cycle flow (detailed in [game-loop-and-timers.instructions.md](instructions/game-loop-and-timers.instructions.md)):

1. **`main.cpp`** parses config ([config-params.instructions.md](instructions/config-params.instructions.md)) and starts either `StandardTimer` (real-time) or `SyncTimer` (lock-step, blocks on agent `(done)` replies) driving `Stadium`.
2. **`Stadium`** (`src/stadium.cpp`, ~3200 lines) is the central orchestrator: owns all `Player`/`Team`/`Coach`/`Monitor` entities ([entities.instructions.md](instructions/entities.instructions.md)), receives raw UDP bytes via `RemoteClient`/sockets ([networking-io.instructions.md](instructions/networking-io.instructions.md)), and each cycle runs `Stadium::step()`.
3. Raw incoming text is parsed into structured actions by the player command grammar ([player-command-protocol.instructions.md](instructions/player-command-protocol.instructions.md)) or, for the online coach, the Coach Language grammar ([coach-language-clang.instructions.md](instructions/coach-language-clang.instructions.md)).
4. `Stadium` dispatches to a **composite list of `Referee` subclasses** (`M_referees`) to evaluate rules and advance play mode ([referee-rules.instructions.md](instructions/referee-rules.instructions.md)).
5. Each cycle, `Stadium` triggers per-client **Senders** ([senders-dispatch.instructions.md](instructions/senders-dispatch.instructions.md)), which format data through a versioned **Serializer** factory ([serialization-protocol-versions.instructions.md](instructions/serialization-protocol-versions.instructions.md)) before writing bytes back out over the transport layer.
6. Monitor/visualization clients get their own dedicated entity, serializer and sender family ([monitor-protocol.instructions.md](instructions/monitor-protocol.instructions.md)).
7. Every cycle is also appended to `.rcg`/`.rcl` game logs via a pluggable "saver" pattern ([logging-and-savers.instructions.md](instructions/logging-and-savers.instructions.md)).

## Build, Test, Run
Two supported build systems (see [README.md](../README.md) for full dependency list — g++ C++17, autoconf, automake, libtool, flex, bison, boost ≥1.44):

- **Autotools** (traditional): `./bootstrap` (only if cloned from git) → `./configure` → `make` → `make install`.
- **CMake** (v17.0.0+, alternative): `cmake .` (or an out-of-tree build dir) → `make`.
- Both produce `src/rcssserver` (the server binary) and `src/rcssclient` (a minimal sample client for smoke-testing the protocol).
- Bison/flex grammars (`src/player_command_parser.ypp`/`.lpp`, `rcss/clang/coach_lang_parser.ypp`/`.lpp`) are code-generated at build time — generated files are NOT checked into the source tree; editing a `.ypp`/`.lpp` requires a full rebuild.
- CI: `.circleci/config.yml` runs the build pipeline (see README badge).
- Ad-hoc smoke tests: `src/playerparamtest.cpp`, `src/serverparamtest.cpp` are standalone mains, not a real test suite — there is no automated unit-test framework in this repo.
- To actually run a match you also need `rcssmonitor` (visualization) and two agent teams (e.g. `helios-base`) — see Related Repositories.

## Conventions
- **No subfolder-per-component** in `src/` — group by filename prefix instead (`serializer*`, `initsender*`, `visualsender*`).
- **Factory/self-registration pattern** used pervasively for anything versioned: `rcss::Factory<Creator,int>` + a static `autoReg(&Class::create, version)` call in an anonymous namespace at the bottom of each `.cpp` (seen in Serializers and Senders, keyed by protocol version int). `ResultSaver` uses the *same* `rcss::Factory` template but keyed by `std::string` name instead (`FactoryHolder = rcss::Factory<Creator, std::string>`, `resultsaver.hpp:40`) since savers are selected by backend name, not protocol version — see logging-and-savers instructions. When adding a new protocol version (or saver backend), follow the matching pattern rather than adding `if`/`switch` branches.
- **Config framework**: all tunable parameters go through `rcss::conf::Builder` (`rcss/conf/builder.hpp`) — `ServerParam`, `PlayerParam`, and `CSVSaverParam` are all child builders registered from `ServerParam::init()`.
- **Composition over inheritance for client entities**: `Player`, `Coach`/`OnlineCoach`, and `Monitor` all multiply-inherit `RemoteClient` (transport) + an `rcss::Listener`/`Observer`-style mixin (for Senders) + a protocol-specific `Builder` interface (for parsing).

## Instruction Index
Read the scoped instruction for the area you are working in (they are NOT auto-loaded — open them on demand):

| Instruction | Covers (applyTo) | Summary | Path |
|-------------|------------------|---------|------|
| Config & Parameter Handling | `src/serverparam.*, src/playerparam.*, src/heteroplayer.*, rcss/conf/**` | Generic `rcss::conf::Builder` framework backing `ServerParam`/`PlayerParam`; heterogeneous player-type sampling; now includes 12 version-20 3D-ball-extension params (`2d_mode`, `gravity`, `player_height`, `goal_height`, etc.) | [config-params.instructions.md](instructions/config-params.instructions.md) |
| Networking & Compressed I/O | `rcss/net/**, rcss/gzip/**, src/remoteclient.*, src/client.*` | IPv4-only UDP/TCP socket wrappers, `RemoteClient` agent transport, gzip log streams | [networking-io.instructions.md](instructions/networking-io.instructions.md) |
| Game Entities — Player, Team, Coach, Field | `src/player.*, src/object.*, src/heteroplayer.*, src/team.*, src/coach.*, src/field.*, src/weather.*` | `PObject`→`MPObject`→`Player`/`Ball` hierarchy, command dispatch, HeteroPlayer coupling | [entities.instructions.md](instructions/entities.instructions.md) |
| Game Loop, Stadium & Timers | `src/main.cpp, src/stadium.*, src/stdtimer.*, src/synctimer.*, src/timer.h, src/timeable.h` | The central per-cycle orchestrator (`Stadium::step()`), Sync vs Standard timer semantics | [game-loop-and-timers.instructions.md](instructions/game-loop-and-timers.instructions.md) |
| Referee & Game Rules | `src/referee.*` | Composite of 10 `Referee` subclasses (Time/Offside/Penalty/Foul/...) reacting to shared `PlayMode` | [referee-rules.instructions.md](instructions/referee-rules.instructions.md) |
| Player Command Protocol Parsing | `src/player_command_parser.ypp, src/player_command_tok.lpp, src/pcomparser.*, src/pcombuilder.*` | Bison/flex grammar → `rcss::pcom::Builder` interface implemented directly by `Player` | [player-command-protocol.instructions.md](instructions/player-command-protocol.instructions.md) |
| Coach Language (CLang) | `rcss/clang/**` | Online-coach directive grammar/parser/builder, consumed solely by `OnlineCoach::parseCommand`'s `"say"` handler | [coach-language-clang.instructions.md](instructions/coach-language-clang.instructions.md) |
| Sensory Message Serialization & Protocol Versions | `src/serializer*.cpp, src/serializer*.h` | Versioned `Serializer*Stdv{N}` subclass chains selected via per-connection `Factory` lookup; new version-20 `SerializerPlayerStdv20`/`SerializerCoachStdv20` add ball z/vel_z (fullstate) and elevation (see) fields | [serialization-protocol-versions.instructions.md](instructions/serialization-protocol-versions.instructions.md) |
| Senders — Per-Cycle Message Dispatch | `src/sender.h, src/initsender*.*, src/visualsender*.*, src/dispsender.*, src/fullstatesender.*, src/bodysender.*` | Thin `Sender` classes that hold a resolved `Serializer` and write through `RemoteClient`'s transport | [senders-dispatch.instructions.md](instructions/senders-dispatch.instructions.md) |
| Monitor Protocol | `src/monitor.*, src/serializermonitor.*, src/initsendermonitor.*` | Server-side `Monitor` entity for visualization clients (rcssmonitor); full ground-truth state, no view-cone limiting; new `SerializerMonitorStdv6` appends ball z to the `(show ...)` stream | [monitor-protocol.instructions.md](instructions/monitor-protocol.instructions.md) |
| Game Log Recording & Result Savers | `src/logger.*, src/initsenderlogger.*, src/csvsaver.*, src/stdoutsaver.*, src/mysqlsaver.*, src/resultsaver.*` | `.rcg`/`.rcl`/`.kwy` log writers; pluggable `ResultSaver` factory (CSV/stdout live, MySQL dead code); new `REC_VERSION_7`/`InitSenderLoggerV7` records ball z (opt-in, `DEFAULT_REC_VERSION` stays at `REC_VERSION_6`) | [logging-and-savers.instructions.md](instructions/logging-and-savers.instructions.md) |
| 3D Protocol Migration Guide | External-consumer facing (agent teams, `rcssmonitor`) | How to opt into the version-20 3D ball extension: new `kick`/`long_kick` 3-arg form, `stop_ball` command, `2d_mode` client-side semantics, monitor rendering pointers | [3d-protocol-migration.instructions.md](instructions/3d-protocol-migration.instructions.md) |

## Related Repositories (sibling directories under `D:\workspace\robo\ss2d`, NOT part of this unit)
These are separate repos with their own history — this repo does not duplicate their content, only references it:

| Repo | Path | Role |
|------|------|------|
| rcssmonitor | `../../rcssmonitor` | Qt-based visualization/monitor client and offline `.rcg` log player; connects to this server's monitor protocol (see [monitor-protocol.instructions.md](instructions/monitor-protocol.instructions.md)). |
| librcsc | `../../librcsc` | Shared C++ client library (world model, geometry, CLang parser, RCG I/O) used by agent teams to talk to this server. Client-side mirror of this repo's `rcss/clang/` and protocol serialization. |
| helios-base | `../../helios-base` | Sample/reference agent team (11 players + coach + trainer) built on `librcsc`; a real consumer of this server's player/coach protocols. `player.conf`/`coach.conf` show default connection ports. |
| rcsoccersim.github.io | `../../rcsoccersim.github.io` | Community docs/website (Docusaurus). Only protocol-adjacent doc found: `rcssserver-manual-20030211.pdf` at its repo root — dated 2003, treat as historical background only, not authoritative over current code. |

**Default ports** (consistent across the ecosystem): `6000` UDP — players/trainer/monitor; `6002` — online coach; `6032` (`6000+32`) — debug/logging side-channel.

## Where to Look
- Component instructions: `.github/instructions/` (indexed above).
- No skills directory in this unit by design (instructions only, per project scope).
- Root docs: [README.md](../README.md) (build/deps), `ChangeLog`, `NEWS`, `AUTHORS`, `BUGS`, `PLATFORMS`.
