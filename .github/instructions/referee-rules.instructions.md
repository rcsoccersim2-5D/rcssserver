---
applyTo: 'src/referee.*'
---

# Referee & Game Rules

## TL;DR
`referee.h`/`referee.cpp` (~4150 lines, largest file in the repo) is **not one state machine class** but a **Composite of ten independent `Referee` subclasses**, each owning one rule concern and driven once per cycle by `Stadium`.
- Abstract base `Referee` ([referee.h:39](../../src/referee.h:39)) defines the observer-style interface (`kickTaken`, `ballTouched`, `analyse`, `playModeChange`, ...) plus shared protected helpers (`awardFreeKick`, `awardGoalKick`, `crossGoalLine`, `placePlayersInTheirField`).
- `Stadium` owns `std::list<Referee*> M_referees` populated with `TimeRef, BallStuckRef, OffsideRef, FreeKickRef, TouchRef, CatchRef, FoulRef, IllegalDefenseRef, KeepawayRef, PenaltyRef` at [stadium.cpp:85-94](../../src/stadium.cpp:85).
- Rule state is driven by the `PlayMode` enum ([types.h:132](../../src/types.h:132): `PM_PlayOn, PM_FreeKick_Left/Right, PM_OffSide_*, PM_PK_*, PM_Foul_*`, etc.) — every subclass reacts to a subset of these via `playModeChange()` / `analyse()`.
- Each subclass is self-contained: e.g. `OffsideRef` tracks offside candidates & marks, `PenaltyRef` runs the penalty-shootout sub-state-machine, `TimeRef` handles half/period/overtime transitions.
- **Open the full file when:** you need to trace one specific rule end-to-end (e.g. how a free kick foul escalates to `PM_Foul_Charge_Left`) or add a brand-new rule subclass.

## Overview
`referee.cpp` implements refereeing as a **Composite/Chain-of-Responsibility of Referee objects**, each a `Referee` subclass responsible for one rule domain. There is no single giant switch statement over game state — instead, `Stadium::update()` calls `Referee::analyse()` on every registered referee once per simulation cycle ([stadium.cpp:820](../../src/stadium.cpp:820): `for_each(M_referees.begin(), M_referees.end(), []( Referee* ref ){ ref->analyse(); })`), and separately notifies referees of ball/player events (`kickTaken`, `tackleTaken`, `ballCaught`, `ballTouched`) as they happen, and of `playModeChange` whenever `Stadium::changePlayMode()` fires.

## Architecture
- **Base class** `Referee` ([referee.h:39-282](../../src/referee.h:39)): pure-virtual event hooks + protected utility methods shared by all subclasses (`awardFreeKick` [referee.cpp:202](../../src/referee.cpp:202), `awardGoalKick`, `awardDropBall`, `awardKickIn`, `awardCornerKick`, `crossGoalLine` [referee.cpp:402](../../src/referee.cpp:402), `placePlayersInTheirField`, `clearPlayersFromBall`, `checkFoul`, static helpers `truncateToPitch`/`moveOutOfPenalty`/`inPenaltyArea`).
- **Ten concrete subclasses** ([referee.h:286-929](../../src/referee.h:286)), registered as a list in `Stadium::initObjects()` ([stadium.cpp:85-94](../../src/stadium.cpp:85)):
  | Class | Concern | analyse() line |
  |---|---|---|
  | `TimeRef` | half/period/overtime/golden-goal/time-up transitions | [referee.cpp:477](../../src/referee.cpp:477) |
  | `BallStuckRef` | detects ball stuck → drop ball | [referee.cpp:919](../../src/referee.cpp:919) |
  | `OffsideRef` | offside line tracking & calls | [referee.cpp:1019](../../src/referee.cpp:1019) |
  | `IllegalDefenseRef` | illegal defense (crowding own goal area) | [referee.cpp:1425](../../src/referee.cpp:1425) |
  | `FreeKickRef` | free-kick / kick-in / corner / goal-kick placement & timers | [referee.cpp:1728](../../src/referee.cpp:1728) |
  | `TouchRef` | out-of-bounds / goal detection via ball touching gline etc. | [referee.cpp:2087](../../src/referee.cpp:2087), impl [referee.cpp:2095](../../src/referee.cpp:2095) |
  | `CatchRef` | goalie catch legality/timeout | [referee.cpp:2583](../../src/referee.cpp:2583) |
  | `FoulRef` | charge/push/multiple-attacker fouls, cards | [referee.cpp:2789](../../src/referee.cpp:2789) |
  | `KeepawayRef` | keepaway-mode specific rules | [referee.cpp:2921](../../src/referee.cpp:2921) |
  | `PenaltyRef` | penalty-kick / shoot-out sub-state-machine | [referee.cpp:3108](../../src/referee.cpp:3108), impl [referee.cpp:3175](../../src/referee.cpp:3175) |
- Each subclass overrides only the hooks it needs (most leave `kickTaken`/`ballCaught`/etc. empty `{}` — see `TimeRef` at [referee.h:296-324](../../src/referee.h:296)).

## Patterns & Conventions
- **Observer/Composite pattern**: `Stadium` is the subject; `Referee` subclasses are observers notified via virtual dispatch, never polling `Stadium` state directly except through `M_stadium` accessors (`M_stadium.playmode()`, `M_stadium.time()`, `M_stadium.ball()`).
- **`analyse()` vs `analyseImpl()` split**: some referees (`TouchRef`, `PenaltyRef`) wrap the real logic in a private `analyseImpl()` called from `analyse()` — useful indirection point if you need to add guards/logging around a rule's tick.
- Every mutating rule action funnels through `Stadium::changePlayMode(PlayMode)` and `Stadium::sendRefereeAudio(...)` — referees never move players/ball directly except via the protected `award*`/`place*`/`clear*` helpers on the base class.
- `PlayMode` values follow a `..._Left` / `..._Right` pairing convention keyed by `Side` (LEFT/RIGHT/NEUTRAL from `types.h`); most rule logic branches on `Side` rather than duplicating per-team code.
- `ServerParam::instance()` supplies all rule timing/geometry constants (`halfTime()`, `nrNormalHalfs()`, `extraHalfTime()`, `goldenGoal()`, `penaltyShootOuts()`, `PITCH_LENGTH`, `goalWidth()`, etc.) — see `TimeRef::analyse` ([referee.cpp:504-528](../../src/referee.cpp:504)) for the canonical usage pattern.

## Key Abstractions
- `Referee` (abstract base) — `Stadium & M_stadium` protected reference, no public state; construction takes `Stadium&` only ([referee.h:48-51](../../src/referee.h:48)).
- `PlayMode` enum ([types.h:132-186](../../src/types.h:132)) — the shared vocabulary all ten referees read/write.
- `OffsideRef::Candidate` ([referee.h:380-393](../../src/referee.h:380)) — private nested struct tracking offside-line candidates (`player_`, `pos_`) used only within `OffsideRef`.
- `Side` (LEFT/RIGHT/NEUTRAL, from `types.h`) — used as a multiplier trick in geometry checks, e.g. `( side * M_stadium.ball().pos().x ) >= 0` in `crossGoalLine` ([referee.cpp:428](../../src/referee.cpp:428)).

## Integration Points
- **Stadium** ([stadium.cpp](../../src/stadium.cpp)): owns/drives the referee list — construction at [stadium.cpp:85-94](../../src/stadium.cpp:85), per-cycle `analyse()` dispatch at [stadium.cpp:820](../../src/stadium.cpp:820) (also [843](../../src/stadium.cpp:843), [858](../../src/stadium.cpp:858)), event notification via `for_each` at [1435](../../src/stadium.cpp:1435), [1675](../../src/stadium.cpp:1675), [1871/1882/1900](../../src/stadium.cpp:1871). See `game-loop-and-timers.instructions.md`.
- **Player/Ball entities**: referees read `Player`/`Ball` position & velocity (via `PObject`-derived accessors) to evaluate offside, out-of-bounds, and catch/foul geometry. See `entities.instructions.md`.
- **ServerParam**: all rule constants (half length, extra time, penalty shoot-out thresholds, pitch/goal dimensions) come from `ServerParam::instance()`. See `config-params.instructions.md`.
- **Audio/commentary**: rules call `M_stadium.sendRefereeAudio("time_up")`, `"time_up_without_a_team"`, etc. to trigger referee speech/commentary broadcast.

## Build & Test
- Part of the standard `rcssserver` autotools/CMake build (`src/` compiled into `librcssserver`/`rcssserver` binary) — no standalone unit tests for referee logic found in-repo; verification is typically done by running real matches and observing play-mode transitions in `.pb.log` / monitor output.
- No dedicated `referee_test.cpp` was found — treat behavior changes as high-risk and validate via a full game run.

## Logging
- Rule transitions are surfaced to clients via `Stadium::changePlayMode()` (broadcast to referee/monitor) and `sendRefereeAudio()` (synthetic referee speech events), not via a dedicated log file — check `.pb.log` game logs (see `pb_log_export` tool) for `LOG_EVENT` entries around play-mode changes.
- Some diagnostic `std::cerr` calls exist for invariant violations, e.g. `"(PenaltyRef::analyse) timer cannot be negative?"` at [referee.cpp:3226](../../src/referee.cpp:3226).

## Important Notes
- **3D ball extension — referee.cpp itself is untouched (comment-only)**: `OffsideRef` was audited and confirmed to have zero `Ball::posZ()`/`velZ()` reads anywhere ([referee.cpp:1123-1125](../../src/referee.cpp:1123)), so no referee subclass needed a code change. The actual z-reset logic lives one layer down, in `Stadium` (see `game-loop-and-timers.instructions.md`): `Stadium::placeBall()`/`Stadium::moveBall()` ([stadium.cpp:1370,1406](../../src/stadium.cpp:1370)) reset the ball's `pos_z`/`vel_z` to `0` after every `moveTo()` call — this single choke point covers all ~30+ restart call sites that referees trigger indirectly (`awardFreeKick`, `awardGoalKick`, `awardDropBall`, `awardCornerKick`, kickoff, etc.), so no per-referee-subclass change was required even though referees are what *decide* a restart is happening. The held-ball "catch" glue (also in `Stadium`, around [stadium.cpp:946-952](../../src/stadium.cpp:946)) additionally pins `pos_z=playerHeight()`/zeroes `vel_z` every cycle while a goalie holds the ball, belt-and-suspenders alongside `Ball::incZ()`'s own gravity-skip-while-held check. The catch legality gate itself (`Player::goalieCatch()`'s height check) lives in `player.cpp`, not `referee.cpp` — see `entities.instructions.md`.
- **Not a single state machine** — resist the urge to look for one big `switch(playmode)`; logic is distributed across ten classes, each independently subscribed to the same event stream.
- Adding a new rule generally means adding a new `Referee` subclass + registering it in `Stadium::initObjects()` ([stadium.cpp:85-94](../../src/stadium.cpp:85)), not editing an existing class's switch.
- Several `PlayMode` values are ignored by `TimeRef::analyse()` via an early-return guard list ([referee.cpp:482-502](../../src/referee.cpp:482)) — when a play mode is "owned" by another referee (offside, foul, illegal-defense, catch-fault), `TimeRef` intentionally does nothing that cycle.
- Penalty shoot-out logic is entangled between `TimeRef` (decides *whether* to enter shoot-out, [referee.cpp:517-521](../../src/referee.cpp:517)) and `PenaltyRef` (runs the shoot-out itself) — check both when debugging shoot-out behavior.
- `Referee::crossGoalLine()` ([referee.cpp:402-440](../../src/referee.cpp:402)) is a good template for any new geometry-based boundary check (guards against vertical-only movement, wrong-half crossing, already-crossed state).

## See Also
- [game-loop-and-timers.instructions.md](game-loop-and-timers.instructions.md)
- [entities.instructions.md](entities.instructions.md)
- [config-params.instructions.md](config-params.instructions.md)

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
