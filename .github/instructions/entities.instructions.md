---
applyTo: 'src/player.*,src/object.*,src/heteroplayer.*,src/team.*,src/coach.*,src/field.*,src/weather.*'
---

# Game Entities — Player, Team, Coach, Field

## TL;DR
Everything the simulator moves or tracks derives from `PObject`/`MPObject` (src/object.h), and `Player` glues that physics base to a UDP client connection, per-cycle command parsing, and heterogeneous physical parameters.
- `PObject` (object.h:382) = static/positioned entity (goal posts, lines, landmarks); `MPObject` (object.h:489) adds velocity/accel/collision and pure-virtual physics hooks.
- `Player` (player.h:47) multiply-inherits `MPObject` + `RemoteClient` + `rcss::Listener` + `rcss::pcom::Builder` — it IS both a physics body and a network client.
- `Team` (team.h:36) is a plain array-backed roster (`Player* M_players[MAX_PLAYER]`), not a container class with logic; `Coach`/`OnlineCoach` (coach.h:41,167) mirror `Player`'s RemoteClient pattern for the trainer/coach roles.
- **Open the full file when:** you need exact per-command math (dash/kick/tackle formulas in player.cpp) or the full stamina/collision state machine — this doc only maps structure and entry points.

## Overview
- **object.h/.cpp** — foundational geometry (`PVector`, `RArea`, `CArea`) plus the entity hierarchy: `PObject` (id, name variants for far/close vision, size, position) → `MPObject` (adds `Stadium&` back-reference, velocity/accel, decay/weight/max_speed, collision bookkeeping) → concrete `Ball` (object.h:633) which trivially implements the pure-virtual hooks. `Player` also derives from `MPObject` but implements those hooks with real physics (player.cpp:2527-2568).
- **player.h/.cpp** (~2848 lines) — the agent-controlled entity. Holds sensors (visual/audio), stamina model, body/neck/focus angles, per-command counters (`M_kick_count`, `M_dash_count`, …), two `Leg` objects + one `Arm`, and a `rcss::pcom::Parser` for the wire protocol. Command entry point is `parseMsg()` (player.cpp:528) → `parseCommand()` (player.cpp:558, ~370 lines dispatching `dash`/`kick`/`turn`/`move`/`catch`/`tackle`/`say`/…).
- **heteroplayer.h/.cpp** — `HeteroPlayer` holds ONE sampled instance of a heterogeneous player type (speed, decay, kick power rate, observation lengths, etc.). `Player::M_player_type` (player.h:104) points to one; `Player::playerType()` exposes it. See config-params instruction for how ranges are sampled.
- **team.h/.cpp** (~411 lines) — `Team` is a fixed-size roster (`MAX_PLAYER` slots), side (`LEFT`/`RIGHT`), score/penalty bookkeeping, player-type usage counters (for heterogeneous player limits), and an `OnlineCoach*` back-reference. No physics or protocol logic lives here.
- **coach.h/.cpp** (~1894 lines) — `Coach` is the **offline trainer** base (arbitrary ball/player placement, `change_mode`, `recover`, `change_player_type`); `OnlineCoach : public Coach` adds team-scoped CLang messaging (`M_message_queue` of `rcss::clang::Msg`, freeform/advice/define message quotas). Both are `RemoteClient` + `rcss::Listener`, same pattern as `Player`.
- **field.h/.cpp** (~91/108 lines) — `Field` owns the four boundary `PObject` lines (`line_l/r/t/b`), a `goals` vector, and a `landmarks` vector (flags) used for vision/quantization; almost no logic, just geometry constants assembled once at `Stadium` construction.
- **weather.h/.cpp** (~85 lines) — `Weather` holds a wind `PVector` + `wind_rand`; `MPObject::wind()`/`noise()` (object.h:555-556, private) consult it when integrating motion. Minimal environmental effect, reset via `halfTime()`.

## Architecture
```
PObject (static, has pos/size/name variants)
  └── MPObject (+vel/accel/decay/weight, Stadium&, collision state, pure-virtual physics hooks)
        ├── Ball            (object.h:633 — no-op hooks)
        └── Player          (player.h:47 — real dash/kick/turn physics)
                also inherits: RemoteClient (UDP transport), rcss::Listener, rcss::pcom::Builder (protocol)

Coach (RemoteClient + rcss::Listener)      -- NOT an MPObject, coaches don't move
  └── OnlineCoach            -- adds Team& binding + CLang message queue

Team  -- plain roster holding Player* M_players[MAX_PLAYER] + OnlineCoach*
Field -- static geometry: goal/line/landmark PObjects
Weather -- wind vector consulted by MPObject::noise()/wind()
```
- `MPObject`'s pure-virtual contract (`turnImpl`, `updateAngle`, `collidedWithPost`, `maxAccel`, `maxSpeed`) is the seam every movable body must fill in — `Player::turnImpl` (player.cpp:2527), `updateAngle` (2536), `collidedWithPost` (2543), `maxAccel`/`maxSpeed` (2550/2562) pull from `M_player_type` (HeteroPlayer) rather than fixed constants, which is how heterogeneous players get different physics.
- `Stadium` (see game-loop-and-timers instruction) drives everything: it owns the `Field`, `Weather`, `Ball`, `Team[2]`, and calls `Player::_inc()`/`_turn()` (inherited from `MPObject`) each simulation cycle after commands are applied.

## Patterns & Conventions
- **Multiple inheritance for role composition**: `Player`/`Coach`/`OnlineCoach` all combine a domain base (`MPObject` or none) with `RemoteClient` (transport) + `rcss::Listener` (event sink) + a parser/builder mixin. This is the standard way any "thing with a socket" is modeled in this codebase — don't add ad-hoc socket members, inherit `RemoteClient` instead.
- **`= delete` for non-copyable identity types**: `PObject`, `MPObject`, `Player`, `Team`, `Coach`, `OnlineCoach` all delete their default ctor and `operator=` — entities are constructed once with required references (`Stadium&`, `Team*`) and never reassigned or copied. Follow this when adding new entity types.
- **Far/close/short name variants precomputed**: `PObject`/`Player` precompute `M_name`, `M_short_name`, `M_close_name`, `M_name_far`, `M_name_toofar` etc. at init time (not per-observation) — vision noise/quantization consults the precomputed string rather than formatting on the fly (perf pattern for the hot per-cycle sense loop).
- **Command state flags reset per cycle**: `Player::resetState()` (player.cpp:436) and per-command counters (`M_kick_count`, `M_dash_count`, `M_command_done`, `M_done_received`) are cleared each cycle by `Stadium`; `PlayerState` (types.h:47) is a bitmask (`KICK|TACKLE|GOALIE|...`) combined with `addState()`/`removeState()`.
- **Physics params delegate to `HeteroPlayer`, never hardcode**: any new per-cycle physics calc on `Player` should read `M_player_type->xxx()` (e.g. `playerDecay()`, `dashPowerRate()`, `kickPowerRate()`) instead of `ServerParam` defaults, so heterogeneous variation keeps working.

## Key Abstractions
- `PVector` (object.h:63) — 2D vector math used everywhere (dash/kick geometry); has both legacy methods and interop with `rcss::geom::Vector2D`.
- `RArea`/`CArea` (object.h:252,313) — rectangular/circular region helpers for collision & field-boundary checks (`nearestEdge`, `inArea`).
- `MPObject::collide()/updateCollisionVel()/moveToCollisionPos()` (object.h:586-600) — shared collision-accumulation protocol used by both `Ball` and `Player` (multiple simultaneous collisions accumulate into `M_post_collision_pos`, resolved once per cycle).
- `Player::kickableArea()`/`ballKickable()` (player.cpp:2726,2734) — `pos().distance2(ball.pos()) <= kickableArea()^2`; central gate used by `kick`, `long_kick`, `tackle` command handlers.
- `HeteroPlayer::delta()` (heteroplayer.cpp:~) — private sampler; see config-params instruction for retry-loop details.
- `Team::GraphCont` (team.h:39) — map of team-graphic (XPM) tiles keyed by `(x,y)`, used for the coach's visual team banner feature, not gameplay.
- `Leg`/`Arm` (leg.h/arm.h, included by player.h) — sub-objects modeling per-limb dash effects and the point/attention "arm" gesture; `Player::applyLegsEffect()`/`applyDashEffect()` (player.cpp:1198,1206) apply them each cycle.

## Integration Points
- **Networking**: `Player`/`Coach`/`OnlineCoach` all inherit `RemoteClient` (src/remoteclient.h) for their UDP socket — see networking-io instruction for transport details (non-blocking recv, `parseMsg()` callback wiring via `rcss::Listener`).
- **Config params**: `Player::M_player_type` is a `const HeteroPlayer*` sampled from `PlayerParam`'s delta ranges at team/type assignment time (`Team::assignPlayerTypes()`, team.cpp) — see config-params instruction for `PlayerParam::init()` and `HeteroPlayer::delta()` retry logic.
- **Game loop**: `Stadium` (see game-loop-and-timers instruction) owns `Field`, `Weather`, `Team[2]`, and the `Ball`; it calls `MPObject::_inc()`/`_turn()` on every movable object per cycle and invokes `Player::sense_body()`/`sendVisual()` for sensor output.
- **Client counterpart**: `librcsc`'s player/coach world-model classes (in the sibling `helios-base`/`librcsc` repos) mirror this state on the agent side over the same protocol — not part of this repo, referenced only for context. `helios-base` player.conf connects on port 6000, coach.conf on 6002.

## Build & Test
- Part of the standard rcssserver CMake/autotools build (`src/` is compiled into `librcssserver`/the `rcssserver` binary); no entity-specific build flags. No dedicated unit tests for these classes were found under `src/`; validate changes by running a full match (`rcssserver` + sample agents) and inspecting `.pb.log`/rcg output for anomalous player state or collisions.

## Logging
- These classes don't log directly; state is exposed to observers (`rcss::ObserverPlayer`, `rcss::BodyObserverPlayer`, `rcss::FullStateObserver` — player.h:59-62) which serialize to the `.rcg`/`.pb.log` game log consumed by monitors. Use `pb_log_export` on a recorded game to inspect actual per-player state transitions rather than adding ad-hoc logging.

## Important Notes
- `Team::player(i)` indexes by roster slot, NOT uniform number — check `Player::unum()` if you need a specific jersey number.
- `MPObject::noise()`/`wind()` are **private** (object.h:555-556) — `Weather` effects are applied internally during `_inc()`, not something callers invoke directly.
- `Coach` (offline trainer) and `OnlineCoach` share almost all wire-protocol scaffolding but have very different command sets (`change_mode`/`recover`/`change_player_type` for the trainer vs. CLang messaging quotas for the online coach) — don't assume symmetry beyond the base class.
- `Ball` is also an `MPObject` but has no player-type/hetero variation — its physics hooks are no-ops/fixed constants (object.h:643-665), unlike `Player`'s hetero-driven versions.

## See Also
- [`config-params.instructions.md`](config-params.instructions.md) — `PlayerParam`/`HeteroPlayer` delta sampling.
- [`networking-io.instructions.md`](networking-io.instructions.md) — `RemoteClient`/`Client` transport used by `Player`/`Coach`.
- [`game-loop-and-timers.instructions.md`](game-loop-and-timers.instructions.md) — `Stadium` orchestration calling into these entities each cycle.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
