---
applyTo: 'src/logger.*,src/initsenderlogger.*,src/csvsaver.*,src/stdoutsaver.*,src/mysqlsaver.*,src/resultsaver.*'
---

# Game Log Recording & Result Savers

## TL;DR
`Logger` (singleton, [logger.h](../../src/logger.h)) writes the per-cycle binary/text game record (`.rcg`, `.rcl`, `.kwy`), while the separate `ResultSaver` factory pattern ([resultsaver.hpp](../../src/resultsaver.hpp)) persists final match results (score, coin toss, penalties) to pluggable backends.
- `Logger::writeGameLog()` is called once per simulation cycle from `Stadium::_writeGameLog` chain (`stadium.cpp:951`).
- `ResultSaver` implementations register themselves in a global `rcss::Factory` via static `autoReg` and are instantiated in bulk by `Stadium` at startup (`stadium.cpp:207-215`).
- Only `CSVSaver` and `STDOutSaver` are actually built (see `Makefile.am`/`CMakeLists.txt`); `mysqlsaver.cpp` is a stale, unmaintained file using an incompatible legacy `rcss::ResultSaver` API and is **not compiled**.
- **Open the full file when:** touching the versioned rcg binary layout (`writeGameLogImpl`/`InitSenderLogger` factory dispatch) or adding a new `ResultSaver` backend.

## Overview
Two independent logging concerns live in this directory:
1. **Game/text/keepaway logs** (`logger.cpp`, ~1518 lines) — the authoritative replay record of a match (`.rcg` binary/JSON game log, `.rcl` text log, `.kwy` keepaway log), written continuously while a match runs.
2. **Result savers** (`resultsaver.hpp/.cpp`, `csvsaver.*`, `stdoutsaver.*`, `mysqlsaver.*`) — a pluggable "save the final score/metadata somewhere" mechanism invoked once at match end, independent of the rcg/rcl streams.

## Architecture
- `Logger` (logger.h:43) is a Meyer's singleton (`Logger::instance()`, logger.h:56) with a `pImpl` (`struct Impl`, logger.cpp:59) holding raw `std::ostream*` for the game log, text log, and an `std::ofstream` for the keepaway log.
- `Logger::open(stadium)` (logger.cpp:218) conditionally opens each stream based on `ServerParam::instance().gameLogging()/textLogging()/kawayLogging()` and delegates to `openGameLog`/`openTextLog`/`openKawayLog`.
- `openGameLog()` (logger.cpp:259) builds the path from `ServerParam::gameLogDir()` + `DEF_GAME_NAME`/`gameLogFixedName()` + `DEF_GAME_SUFFIX` (`".rcg"`, logger.cpp:54), then picks between a plain `std::ofstream` (binary mode) and `rcss::gz::gzofstream` when `HAVE_LIBZ` and `gameLogCompression() > 0` (logger.cpp:303-311) — appending `".gz"` to the path (logger.cpp:306). Same pattern for text logs with `".rcl"` suffix (logger.cpp:52) at logger.cpp:391-398.
- After opening, `setSenders()` (logger.cpp:156) looks up a `SerializerMonitor` and an `InitSenderLogger`/`DispSenderLogger` **by version** from their respective `rcss::Factory` registries, keyed off `ServerParam::instance().gameLogVersion()` — this is how the server supports multiple rcg format versions (`REC_OLD_VERSION`, `REC_VERSION_JSON`, etc., see logger.cpp:164-167).
- `ResultSaver` (resultsaver.hpp:31) is an abstract base using the **NVI (non-virtual interface) pattern**: public methods (`saveStart`, `saveTime`, `saveTeamName`, `saveScore`, `savePenTaken`, `savePenScored`, `saveCoinTossWinner`, `saveComplete`, `enabled`) each forward to a protected `doXxx()` virtual with a no-op default (resultsaver.hpp:59-163). Concrete savers register via `ResultSaver::factory().autoReg(&Creator, NAME)`.

## Patterns & Conventions
- **pImpl idiom** for `Logger` — all mutable state lives in `Logger::Impl` (logger.cpp:59-136); the public header only exposes behavior, keeping ABI stable across format-version churn.
- **Factory + NVI double pattern**: `Logger` uses `rcss::Factory` to select serializers/init-senders *by version int* (logger.cpp:169-197); `ResultSaver` uses the same `rcss::Factory` template to select savers *by name string* (resultsaver.hpp:40, `FactoryHolder = rcss::Factory<Creator, std::string>`).
- **Static self-registration**: each saver `.cpp` ends with a file-scope `rcss::RegHolder s = ResultSaver::factory().autoReg(&CSVSaver::create, CSVSaver::NAME);` (csvsaver.cpp:420) — this is how `STDOutSaver`/`CSVSaver` become discoverable without any central registry edit; adding a new saver means writing this one line plus a `create()` factory method.
- **Conditional compression**: both game log and text log follow the identical `#ifndef HAVE_LIBZ` / `#ifdef HAVE_LIBZ` branch structure (logger.cpp:296-321, 384-405) — copy this pattern exactly if adding a third compressible stream.
- **Config-owned sub-builder**: `CSVSaverParam` (csvsaver.h:39) is a config-param class following the same `Builder`-child pattern as `PlayerParam`/`ServerParam` (see `.github/instructions/config-params.instructions.md`), not a `ResultSaver` itself — `CSVSaver` (the actual saver) reads `CSVSaverParam::instance().save()`/`.filename()` to decide whether/where to write.

## Key Abstractions
- `Logger` (logger.h:43) — singleton; public write API: `writeGameLog`, `writeTextLog`, `writePlayerLog`, `writeCoachLog`, `writeOnlineCoachLog`, `writeKeepawayLog`, `writeRefereeAudio`/`writePlayerAudio`/`writeCoachAudio`, `writeTimes`/`writeProfile` (perf logging), `writeTeamGraphic`.
- `Logger::Impl` (logger.cpp:59) — private state: `game_log_filepath_`, `text_log_filepath_`, `kaway_log_filepath_`, raw stream pointers, `init_observer_`/`observer_` (`rcss::InitObserverLogger`/`ObserverLogger`).
- `ResultSaver` (resultsaver.hpp:31) — base interface; `team_id` enum (`TEAM_LEFT`/`TEAM_RIGHT`); `Ptr = std::shared_ptr<ResultSaver>`; `Creator = Ptr(*)()`; `FactoryHolder = rcss::Factory<Creator,std::string>`.
- `CSVSaver` (csvsaver.h:93) / `CSVSaverParam` (csvsaver.h:39) — writes one CSV row per finished match; `CSVSaverParam::init(parent)` registers config options as a child `Builder` of `ServerParam` (serverparam.cpp:485).
- `STDOutSaver` (stdoutsaver.h:29) — always `doEnabled() == true` (stdoutsaver.cpp:52-55); prints a human-readable results block to stdout, no config needed.
- `MySQLSaver` (mysqlsaver.cpp:45) — **legacy/dead code**: extends a different base signature (`rcss::ResultSaver(argc, argv, module_name)`) that doesn't match the current `ResultSaver` ctor; not referenced by any build file. Treat as reference-only, not functional.
- `InitSenderLogger` (initsenderlogger.h:38) — writes the rcg header/`ServerParam`/`PlayerParam`/player-type block once at file open (`logger.cpp:338-341`); versioned via its own `FactoryHolder` keyed by `int` (initsenderlogger.h:61).

## Integration Points
- **Stadium → Logger (per-cycle)**: `Stadium::_writeGameLog`-equivalent path calls `Logger::instance().writeGameLog(*this)` then `Logger::instance().flush()` every cycle (`stadium.cpp:951-952`); `writeGameLog` (logger.cpp:756) skips writing during `PM_BeforeKickOff`/after the final `PM_TimeOver` cycle already written (guarded by static `wrote_final_cycle`), delegating actual serialization to `writeGameLogImpl` (logger.cpp:813).
- **Stadium → Logger (lifecycle)**: `Logger::instance().open(*this)` at `stadium.cpp:298` (match start); `Logger::instance().close(*this)` at `stadium.cpp:3147` (match end) — `close()` calls `renameLogs()` (logger.cpp:495) which renames the `"incomplete"` placeholder file to the final timestamped name and re-appends `.gz` if compressed (logger.cpp:598,631).
- **Stadium → ResultSaver (result persistence)**: `Stadium` builds `M_savers` at construction by iterating `ResultSaver::factory().list()` (stadium.cpp:207-215); at match end it calls `rcss::save_results(team_id, Team&, ResultSaver&)` (stadium.cpp:3163) for each saver in `M_savers` (stadium.cpp:3202-3220), which invokes `saveTeamName`/`saveScore`/`savePenTaken`/`savePenScored`/`saveCoinTossWinner`/`saveComplete`.
- **Compression backend**: `#include <rcss/gzip/gzfstream.hpp>` (logger.cpp:45) — see `.github/instructions/networking-io.instructions.md` for `gzstream`/`gzfstream` internals; this is the ONLY consumer of `gzofstream` for on-disk logs (network streams use a different gzip path).
- **Config bootstrap**: `ServerParam::init()` calls `CSVSaverParam::init(instance().M_builder.get())` (serverparam.cpp:485) right after `PlayerParam::init()`, wiring `CSVSaver`'s config into the same CLI/conf-file parser as everything else — see `.github/instructions/config-params.instructions.md`.

## Build & Test
- Build system membership: `csvsaver.cpp/.h` and `stdoutsaver.cpp/.h` are listed in `src/Makefile.am` (lines 11, 61, 82, 136) and `src/CMakeLists.txt`; `mysqlsaver.*` appears in NEITHER — it is dead source kept in the tree but excluded from all build systems (no `mysql` token found in `CMakeLists.txt`).
- No dedicated unit tests found for `Logger`/`ResultSaver` under `src/`; verification is typically done by running a full match and inspecting the produced `.rcg`/`.rcl` files or piping through `rcg2csv`-style external tools (not in this repo).
- Compression support is compile-time gated (`HAVE_LIBZ` from `config.h`), so a build without zlib silently falls back to uncompressed `.rcg`/`.rcl` with a stderr warning (logger.cpp:299,387).

## Logging
- All failures in this subsystem go to `std::cerr` with `__FILE__ ": " __LINE__` prefixes (e.g. logger.cpp:269-271, 325-327) — no structured logger; grep `std::cerr` in `logger.cpp`/`csvsaver.cpp` to find all failure paths.
- `STDOutSaver` treats stdout itself as its "log sink" (stdoutsaver.cpp:58 `"\nGame Results:\n"`), distinct from the rcg/rcl game logs.

## Important Notes
- **rcg vs rcl vs kwy**: `.rcg` = binary/JSON game state replay (versioned, `DEF_GAME_SUFFIX`, logger.cpp:54); `.rcl` = human-readable text log of messages/events (`DEF_TEXT_SUFFIX`, logger.cpp:52); `.kwy` = keepaway-mode episode log (`DEF_KAWAY_SUFFIX`, logger.cpp:56) — three separate files, independently enabled/compressed.
- Legacy `writeGameLogV1`-`V4` methods (logger.cpp:853,911,974,1073) are `#if 0`-disabled dead code kept for historical reference — all current writes go through `writeGameLogImpl` + the versioned `InitSenderLogger`/`DispSenderLogger`/`SerializerMonitor` factories instead of per-version methods.
- `CSVSaverParam` is NOT a `ResultSaver` — don't confuse the config-holder class with the saver; `CSVSaver` (a `ResultSaver`) reads from `CSVSaverParam::instance()` at save time.
- `mysqlsaver.cpp` requires `<mysql/mysql.h>` and uses a divergent `ResultSaver` constructor signature — if reviving it, expect a non-trivial port to the current `ResultSaver` NVI interface, not just re-adding it to the build.
- Rename-on-close (`renameLogs`, logger.cpp:250) means log files are named `"incomplete.rcg"`/`"incomplete.rcl"` WHILE the match is running and only get their final name at `Logger::close()` — a crash mid-match leaves `incomplete.*` files behind.

## See Also
- [`networking-io.instructions.md`](networking-io.instructions.md) — `gzstream`/`gzfstream` compression backend shared with logger.cpp.
- [`config-params.instructions.md`](config-params.instructions.md) — `CSVSaverParam::init()` bootstrapping via `ServerParam::init()`.
- [`game-loop-and-timers.instructions.md`](game-loop-and-timers.instructions.md) — per-cycle `Stadium` loop that drives `Logger::writeGameLog()`.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
