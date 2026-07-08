---
applyTo: 'src/serverparam.*,src/playerparam.*,src/heteroplayer.*,rcss/conf/**'
---

# Config & Parameter Handling

## TL;DR
Server-wide and per-player-type tunables are declared once via a generic templated `rcss::conf::Builder` framework, then parsed from CLI args + a `~/.rcssserver/server.conf`-style file into typed C++ member variables.
- `rcss/conf/builder.hpp` (~900 lines) is the reusable, type-erased param registry (`int`/`bool`/`double`/`string`), shared by `ServerParam` and `PlayerParam`.
- `ServerParam::addParams()` in `src/serverparam.cpp` (~lines 625-1180) registers ~250+ options with a `version` tag used for protocol-version gating.
- **3D ball extension (version 20)**: 12 new fields added to `serverparam.h` (members [serverparam.h:636-647](../../src/serverparam.h:636), accessors [serverparam.h:1014-1025](../../src/serverparam.h:1014)) and registered via `addParam(...)` at version `20` ([serverparam.cpp:976-987](../../src/serverparam.cpp:976), defaults set [serverparam.cpp:1512-1523](../../src/serverparam.cpp:1512)): `2d_mode` (master gate, default `true`), `player_height`, `goal_height`, `gravity`, `ball_bounce_restitution`, `ball_bounce_friction`, `loft_power_cost`, `air_decay`, `bounce_stop_speed`, `roll_stop_speed`, `height_power_cost`, `precise_bounce_timing`. Formulas are hand-ported from the sibling `3d-kick-lab/physics.js` sandbox (see that repo's copilot-instructions.md) — not a merge/shared-code relationship.
- `PlayerParam` defines *ranges* (`*_delta_min/max`), and `HeteroPlayer` (`src/heteroplayer.cpp`) randomly samples within those ranges to build each heterogeneous player type.
- **Open the full file when:** you need to add/modify a specific option's default, description, or version gate — always add both the member declaration (`.h`) and the `addParam(...)` registration call (`.cpp`) together.

## Overview
This subsystem is the single source of truth for every tunable of the simulation: pitch/ball geometry, player physics (speed, stamina, kick/dash/catch rates), rule timings (half time, drop time, extra time), network/protocol flags, synch-mode, fullstate/coach visibility, and heterogeneous-player randomization ranges. Values arrive from three places, in this precedence order: compiled-in defaults → `~/.rcssserver/server.conf` (auto-created/rewritten each run) → command-line args (highest priority, parsed last in `ServerParam::init`).

## Architecture
- `rcss::conf::Builder` (`rcss/conf/builder.hpp`/`.cpp`) — a registry keyed by param name (`std::map<std::string, ParamInfo<V>>` for each of `int/bool/double/string`). `ParamInfo<V>` bundles a type-erased `Setter<V>`/`Getter<V>` (see `paramsetter.hpp`/`paramgetter.hpp`, using `GetterMFun`/`GetterPFun`/`GetterRVal` wrapper templates, lines ~30-90) plus a help description string. Supports nested builders via parent/child (`Builder(Builder* parent, ...)`) so `PlayerParam` registers as a child module of the top-level `ServerParam` builder.
- `rcss::conf::Parser` (`rcss/conf/parser.hpp`/`.cpp`) — does the actual CLI (`parse(argc, argv)`) and conf-file (`parse(path)`, `parseCreateConf(path, module)`) parsing/writing, reporting errors through a `StatusHandler` (here `StreamStatusHandler`, prints to stderr).
- `ServerParam` (`src/serverparam.h`/`.cpp`, singleton via `ServerParam::instance()`) owns the root `Builder`, the `Parser`, and the error handler as `std::shared_ptr`s (`M_builder`, `M_conf_parser`, `M_err_handler`).
- `PlayerParam` (`src/playerparam.h`/`.cpp`, singleton via `PlayerParam::instance()`) is constructed as a **child** builder: `PlayerParam::init(instance().M_builder.get())` is called from inside `ServerParam::init` (serverparam.cpp:478) *before* the CLI/conf parse, so player-type ranges appear under the same `server.conf`/CLI namespace.
- `HeteroPlayer` (`src/heteroplayer.h`/`.cpp`) is NOT a param container itself — it's a consumer: its constructor (heteroplayer.cpp:52) reads `ServerParam::instance()` (base values) and `PlayerParam::instance()` (delta ranges) and uses `HeteroPlayer::delta(min,max)` (uniform random) to derive each generated player type's stats, with up to `MAX_TRIAL = 1000` retries (line 54) to avoid degenerate (≤0) values.

## Patterns & Conventions
- Every option is added via one `addParam(name, member_ref, description, version)` call (simple case) or the `addParam(name, setter, getter, description, version)` overload for computed/derived values (`ServerParam::addParam`, serverparam.cpp:603-621; declared as private templates in the header).
- `version` is an integer protocol version number (e.g., 7, 9, 14, 18, 19) used both for conf-file compatibility checks and for gating what fields get serialized to clients — see `HeteroPlayer::toJSON`-style methods (heteroplayer.cpp:445-489: `if (version >= 14)`, `>= 18`, `>= 19` blocks progressively add fields like `kick_power_rate`, `unum_far_length`, `dist_noise_rate`).
- Each `ServerParam`/`PlayerParam` keeps a `VerMap M_ver_map` (`std::map<std::string,uint>`) populated as a side effect of every `addParam` call (serverparam.cpp:609,621) — used elsewhere to answer "which protocol version introduced param X".
- Naming: C++ member fields use `M_` prefix + snake/camel case (e.g., `M_player_speed_max`); the registered CLI/conf key is the plain snake_case string (`"player_speed_max"`) — the two are decoupled, so grep for the **string literal**, not the member name, when tracing a CLI flag end-to-end.
- Legacy/back-compat: `ServerParam::convertOldConf` (serverparam.cpp:512-552) shells out to `awk` (POSIX only, skipped on `RCSS_WIN`) to migrate a pre-`=`-syntax old config file into the current `key = value` format.

## Key Abstractions
- `rcss::conf::Builder::ParamInfo<V>` — private nested template class (builder.hpp:229-259) pairing a `Setter<V>`/`Getter<V>` with a description; the atomic unit of one registered option.
- `rcss::conf::Builder::doBuildParam(...)` overloads (builder.hpp:278-331) — type-coercion dispatch (e.g., a `bool` CLI token can be built into an `int` param) called by `Parser` while walking tokens.
- `ServerParam::SERVER_CONF` / `OLD_SERVER_CONF` — well-known conf file names (see near serverparam.cpp:440, `conf_path /= ServerParam::SERVER_CONF`, default dir from `tildeExpand(conf_dir)`, typically `~/.rcssserver/`).
- `PlayerParam::convertToStruct()` (playerparam.h:113) — flattens the live singleton into a POD `player_params_t` (see `types.h`) for passing across simpler C-style interfaces.
- `HeteroPlayer::delta(min,max)` — private static helper implementing the uniform-random-in-range sampling used for every trade-off pair (speed/stamina, decay/inertia, dash_power_rate/player_size, etc.), called repeatedly in the constructor (heteroplayer.cpp:63-90+).

## Integration Points
- `main.cpp` → `ServerParam::init(argc, argv)` is the very first call in `main()`; it internally calls `PlayerParam::init(...)` and `CSVSaverParam::init(...)` (serverparam.cpp:478,485) before parsing CLI/conf, then `Stadium` and the rest of the server read values via `ServerParam::instance()` accessors everywhere.
- `Stadium::init()` (see entities/game-loop instructions) consumes `ServerParam` heavily for field geometry, timers, and rule constants.
- Player-type assignment during game setup (team registration) pulls generated `HeteroPlayer` instances, which in turn depend on both param singletons — so changing `PlayerParam` ranges changes the *distribution* of generated types, not fixed values.
- `librcsc`'s `rcsc/param/` module (separate repo) implements an analogous generic param-registration pattern for the **client** side (agent config) — same general idea, not code-shared with this server-side framework.

## Build & Test
- `rcss/conf/CMakeLists.txt` / `Makefile.am` build the conf framework as its own static lib linked into `rcssserver`.
- Standalone smoke tests: `src/serverparamtest.cpp` (8 lines — calls `ServerParam::init(argc, argv)` then `ServerParam::instance()`, prints "success") and `src/playerparamtest.cpp` — these are minimal executables (not a unit-test framework) intended to catch parse/registration crashes; check `Makefile.am`/`CMakeLists.txt` in `src/` for how they're wired as test targets.
- No assertions beyond "did it construct/parse without throwing/erroring" — validate real behavior changes by running `rcssserver --help` (via `Builder::displayHelp()`) to confirm a new/changed option shows up correctly.

## Logging
- Parse/build errors and warnings go through `StatusHandler::buildError/buildWarning/parseError` (builder.hpp:161-176), routed to `StreamStatusHandler` → stderr by default; conf-file creation/failure messages (`creatingConfFile`, `confCreationFailed`) also flow through the same handler interface.
- `ServerParam::init` prints direct `std::cerr` diagnostics for conf-file version mismatches (serverparam.cpp:463-476) and directory create failures (serverparam.cpp:432-451).

## Important Notes
- **Version mismatch is fatal at startup**: if `server.conf`'s recorded version differs from the compiled `Builder::version()`, init aborts (serverparam.cpp:463) — regenerate the conf file or don't ship stale ones across server upgrades.
- **Registration order matters**: `PlayerParam::init` and `CSVSaverParam::init` must run *before* `M_conf_parser->parse(argc, argv)` (serverparam.cpp:478-497) so their options exist in the registry when CLI/conf tokens for them are encountered — adding a new child-param module elsewhere requires the same ordering.
- Adding a new option requires touching **3 places**: member declaration in `.h`, default value assignment in `setDefaults()`, and the `addParam(...)` call in `addParams()` — missing any one causes either a compile error or a silently-ignored CLI flag.
- `HeteroPlayer` retries up to 1000 times per generated type to avoid non-physical (≤0) derived values; extremely wide `PlayerParam` delta ranges can make this loop degrade (rare but possible starvation), worth checking if player-type generation ever hangs/slows.
- **`player_height`/`goal_height` are deliberately NOT part of `HeteroPlayer`** — confirmed via grep (no `HeteroPlayer` references to either field). This was an explicit design decision for the 3D ball extension: player/goal height are fixed global `ServerParam` constants, not per-player-type randomized values, so all heterogeneous player types share the same reach-height gate for goalie catch / header physics.

## See Also
- entities.instructions.md — heteroplayer.cpp / player type consumption during team/game setup
- game-loop-and-timers.instructions.md — Stadium::init() and rule timing constants sourced from ServerParam
- serialization-protocol-versions.instructions.md — version-gated serialization fields (JSON/ASCII protocol) driven by the same `version` tags

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
