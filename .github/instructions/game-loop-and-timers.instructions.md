---
applyTo: 'src/main.cpp,src/stadium.*,src/stdtimer.*,src/synctimer.*,src/timer.h,src/timeable.h'
---

# Game Loop, Stadium & Timers

## TL;DR
`main()` builds one static `Stadium` and drives it forever through a `Timer` (`StandardTimer` real-time, or `SyncTimer` lock-step) that calls the `Timeable` interface Stadium implements.
- Real per-cycle physics/state advance is `Stadium::step()` (src/stadium.cpp:790-899); it is invoked from `Stadium::doNewSimulatorStep()` (src/stadium.cpp:2229).
- Timer choice hinges on `ServerParam::instance().synchMode()` in `main.cpp:88-96`; both timers reuse the same `q_*`/`c_*` LCM-based scheduling math from `stdtimer.cpp`/`synctimer.cpp`.
- Only `SyncTimer` calls `doSendThink()` (src/stadium.cpp:2393) — the "wait for all `(done)`" barrier that makes synch_mode lock-step; `StandardTimer` never blocks on clients, it just sleeps to real wall-clock ticks.
- **Open the full file when:** you need to touch play-mode transitions, referee hookup, stamina/coach timing, or the client "done" wait/timeout logic — `stadium.cpp` is ~3239 lines and `step()`/`doSendThink()` interact with almost every other subsystem (Referee, Player, senders).

## Overview
`src/main.cpp` (105 lines) is the entire executable entry point:
1. Sets classic locale, prints version/copyright.
2. `ServerParam::init(argc, argv)` (see config-params instructions) — parses CLI/conf, exits on failure.
3. Installs `SIGINT`/`SIGTERM`/`SIGHUP` → `sigHandle()` (main.cpp:47) which calls `Std.finalize("Server Killed. Exiting...")` on the file-scope static `Stadium Std` (main.cpp:45).
4. `Std.init()` (src/stadium.cpp:178) — seeds RNG, builds `ResultSaver`s, loads `LandmarkReader`, builds `HeteroPlayer` types, binds the player UDP socket, etc. Returns `false` on any setup failure.
5. Picks the timer: `synchMode()` → `SyncTimer(Std)` (src/synctimer.h/.cpp) else `StandardTimer(Std)` (src/stdtimer.h/.cpp) — both constructed with a `Timeable&` (main.cpp:88-96).
6. `timer->run()` — **never returns until the process is told to quit** (it's the whole server lifetime).
7. `ServerParam::instance().clear()` then `return 0`.

## Architecture
`Timer` (src/timer.h) is a pure-virtual base holding a `Timeable&` (`M_timeable`), exposing only `run()` and a protected `getTimeableRef()`. `Timeable` (src/timeable.h) is a second pure-virtual interface with public non-virtual wrapper methods (`recvFromClients()`, `newSimulatorStep()`, `sendSenseBody()`, `sendVisuals()`, `sendSynchVisuals()`, `sendCoachMessages()`, `sendThink()`, `quit()`, `alive()`) that forward to protected `do*`/`is*` virtuals — a classic template-method / NVI pattern so timers never call the `do*` methods directly.

`Stadium` (src/stadium.h:66-67) is `class Stadium : public virtual Timeable`, implementing every `do*` hook:
| Timeable call | Stadium impl (stadium.cpp) |
|---|---|
| `recvFromClients()` | `doRecvFromClients()` — line 2191 |
| `newSimulatorStep()` | `doNewSimulatorStep()` — line 2229 |
| `sendSenseBody()` | `doSendSenseBody()` — line 2253 |
| `sendVisuals()` | `doSendVisuals()` — line 2301 |
| `sendSynchVisuals()` | `doSendSynchVisuals()` — line 2322 |
| `sendCoachMessages()` | `doSendCoachMessages()` — line 2343 |
| `sendThink()` | `doSendThink()` — line 2393 (bool, returns shutdown flag) |
| `quit()` | `doQuit()` — line 3133 → `finalize()` (line 3139) |
| `alive()` | `isAlive()` (returns `M_alive`, set false by `disable()` line 3154) |

## Patterns & Conventions
- **LCM scheduling math**: both timers precompute `q_simt/q_sent/q_rect/q_sbt/q_svt = lcmStep() / <interval>Step()` — the number of ticks-of-that-kind inside one "least common multiple" period (`ServerParam::instance().lcmStep()`), then track `c_*` counters and a running `lcmt` (ms accumulator) each loop iteration. When `lcmt >= lcmStep()`, everything resets to 1. This lets a single default `TIMEDELTA`-granularity loop (see `param.h`) drive several independently-timed activities (sim step every `simStep()` ms, sense-body every `senseBodyStep()` ms, visuals every `sendStep()*0.25` ms — quarter-step for narrow/wide view scheduling, coach visuals every `coachVisualStep()` ms) without multiple threads.
- **Synch-see offset**: both timers gate `sendSynchVisuals()` on `synchSeeOffset()`, tracked with a `sent_synch_see` bool reset every new sim step, so it fires at most once per `simStep()`.
- Every `do*` in Stadium wraps its body with `std::chrono::system_clock::now()` before/after and reports to `Logger::instance().writeProfile(*this, start, end, "<TAG>")` — tags: `"RECV"`, `"SIM"`, `"SB"`, `"VIS"`, `"VIS_S"`, `"COACH"`. Follow this convention if you add a new per-cycle phase.
- `Stadium::step()` always: (1) apply/reset player command effects + arm age, (2) coach message-queue bookkeeping, (3) playmode-dependent branch — `PM_BeforeKickOff` freezes movement & just runs `Referee::analyse()`, dead-ball/after-goal/offside/foul modes clear ball catcher + advance objects once then re-check referees, live play increments `M_time` and moves all movable objects, `PM_TimeOver` just accumulates stoppage time — (4) update stamina/capacity for enabled players, (5) award online-coach freeform message quota every `nrNormalHalfs()*halfTime()` cycles, (6) `sendDisp()` to monitors + game log, (7) `resetState()` on all players.

## Key Abstractions
- `Stadium::step()` (stadium.cpp:790) — the single authoritative "advance one simulation cycle" function; called only from `doNewSimulatorStep()`.
- `Stadium::doNewSimulatorStep()` (stadium.cpp:2229) — wraps `step()` with `startTeams()` (kick off waiting teams) and `checkAutoMode()`, plus `Logger::instance().writeTimes()` for interval diagnostics.
- `Stadium::doSendThink()` (stadium.cpp:2393) — **only called by `SyncTimer`**. Sends `"(think)"` to every enabled player/coach/trainer, then busy-waits (sleeping `synchMicroSleep()` µs per poll, calling `doRecvFromClients()` each poll) until all `doneReceived()` flags are set or `max_msec_waited` (25*50 ms) elapses; tracks `cycles_missed`, force-shuts-down the server if `> max_cycles_missed` (20) consecutive cycles are missed.
- `M_players`, `M_olcoaches[2]`, `M_coach`, `M_referees`, `M_remote_players`, `M_shuffle_players` — Stadium-owned entity containers iterated every cycle (see entities instructions for `Player`/`Team`/`MPObject` details).

## Integration Points
- **ServerParam** (config-params instructions): every scheduling constant (`simStep`, `sendStep`, `recvStep`, `senseBodyStep`, `coachVisualStep`, `lcmStep`, `synchOffset`, `synchSeeOffset`, `synchMicroSleep`, `nrNormalHalfs`, `halfTime`) is read fresh from `ServerParam::instance()` — no local caching beyond the `q_*` precompute at timer-`run()` start, so changing params requires a server restart.
- **Referee** (referee-rules instructions): `step()` calls `ref->analyse()` on every `M_referees` entry each cycle, in a playmode-dependent branch — the referee subsystem drives playmode/foul/offside transitions that `step()` reacts to.
- **Senders / networking-io instructions**: `doSendSenseBody/doSendVisuals/doSendSynchVisuals/doSendCoachMessages` all delegate to per-`Player`/`OnlineCoach` sender objects (`p->sendBody()`, `p->sendFullstate()`, `p->sendVisual()`, `p->sendSynchVisual()`, `M_coach->send_visual_info()`) which write through `RemoteClient`/UDP sockets. `doRecvFromClients()` calls `udp_recv_message()`, `udp_recv_from_online_coach()`, `udp_recv_from_coach()` then `removeDisconnectedClients()`.
- **Signal handling**: `SIGINT/SIGTERM/SIGHUP` → `sigHandle()` in main.cpp → `Stadium::finalize()` sets `M_alive=false`, which both timer `while (getTimeableRef().alive())` loops check every iteration — this is the only way the infinite loop in `run()` exits (besides the sync-mode `cycles_missed` shutdown path also calling `finalize` indirectly via disable-then-quit-on-next-check).

## Build & Test
No dedicated unit test for the loop; validate by running the built `rcssserver` binary with `-server::synch_mode 1` vs default and watching cycle logs / `Logger::instance().writeProfile` output. Standard project build is autotools/CMake from repo root (see top-level `copilot-instructions.md` if present) — compile just `stadium.cpp`, `stdtimer.cpp`, `synctimer.cpp`, `main.cpp` objects when iterating (`make src/stadium.o` etc. depending on generated Makefile).

## Logging
- `Logger::instance().writeProfile(*this, start, end, tag)` — timing/perf log per phase (tags above).
- `Logger::instance().writeTimes(*this, prev_time, start_time)` — inter-cycle timing, called once in `doNewSimulatorStep()`.
- `Logger::instance().writeTextLog(*this, buf, LOG_TEXT)` — used in `doSendThink()` to log `"Num sleeps called: N"` when `logTimes()` is enabled.
- Missed-cycle diagnostics go straight to `std::cerr` (`"Someone missed a cycle at <time>"`, `"Waiting too long for clients! Exiting"`) — not through `Logger`.

## Important Notes
- **StandardTimer never calls `sendThink()`.** In real-time (non-synch) mode, clients act asynchronously whenever they like within the real-time budget; `doRecvFromClients()` (called every `recvStep()`) is the only ingestion point. In synch mode, `SyncTimer::run()` explicitly calls `getTimeableRef().sendThink()` (synctimer.cpp:132) once per `simStep()` (offset by `synchOffset()`), and that call **blocks the whole loop** until all clients respond or timeout — this is the fundamental behavioral difference, not just a different sleep strategy.
- `StandardTimer::run()` computes `sleep_count = default_delta - elapsed` every iteration and sleeps that duration (stdtimer.cpp:79-81); if the previous iteration ran long, `sleep_count` goes negative and `elapsed` (already-consumed time) is added directly into `lcmt` to avoid "catch-up" cycles (comment at stdtimer.cpp:92-94).
- `Stadium::step()`'s playmode branch has near-duplicated dead-ball-mode logic across ~10 `PM_*` enum values (AfterGoal/OffSide/Illegal_Defense/Foul_Charge/Foul_Push/Back_Pass/Free_Kick_Fault/CatchFault, both sides) — when adding a new stoppage playmode, this `if/else if` chain (stadium.cpp:822-837) is where it must be added or it will silently fall into the "live play" branch and increment `M_time` incorrectly.
- `finalize()` (stadium.cpp:3139) is guarded by `static bool s_first` so it only runs once even if called from both a signal and `doQuit()`.
- `doSendThink()`'s player wait array is fixed-size `MAX_PLAYER*2` (stadium.cpp:2423) — indexes directly into `M_players`, not filtered by team.

## See Also
- entities.instructions.md
- config-params.instructions.md
- referee-rules.instructions.md
- senders-dispatch.instructions.md
- networking-io.instructions.md

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
