---
applyTo: 'src/serverparam.*,src/pcombuilder.*,src/player_command_parser.ypp,src/player_command_tok.lpp,src/visualsenderplayer.*,src/serializer*stdv20.*,src/serializermonitorstdv6.*,src/fullstatesender.*,src/py_test_client.py'
---

# 3D Protocol Migration Guide (External Consumers)

## TL;DR
Protocol version **20** (player/coach serializers) and monitor version **6** add an OPT-IN 3D ball-flight extension (loft kicks, gravity, bouncing, height-aware goalie catch) to rcssserver. **Nothing changes for existing clients** unless they explicitly negotiate the new version number — this doc is for people updating an **agent team** (e.g. `helios-base`/`librcsc`) or **`rcssmonitor`** to consume the new fields.
- Server-side gate: `2d_mode` `ServerParam` (default `true`) keeps physics legacy-equivalent and keeps protocols 1-19 wire-identical. A v20 player still receives the new vertical fields, with zero values; an operator must set `2d_mode=false` to produce real vertical motion.
- New wire additions (only sent to v20 players / v6+ monitors): raw `z` in every v20 `(see)` ball entry, raw `vz` when planar change fields are also present, ball `z`/`vz` in `(fullstate)`, and ball z in the monitor `(show ...)` stream.
- New commands: `kick`/`long_kick` gain an optional 3rd `loft` argument; a brand-new `chest_trap` command is added (renamed from its original working name `stop_ball`).
- **Reference implementations that need updating first**: `librcsc` (client protocol library) and `helios-base` (sample agent team) — see Related Repositories below. `rcssmonitor` needs a v6 monitor-serializer consumer to render the new ball-z field.

## Overview
This is a documentation-only cross-reference for people who do NOT work in this repository day-to-day but need to consume the new protocol. It intentionally duplicates a condensed subset of `config-params.instructions.md`, `player-command-protocol.instructions.md`, `serialization-protocol-versions.instructions.md`, and `monitor-protocol.instructions.md` — those files are the source of truth for implementation details; this file is the "what do I, as an external client author, need to change" summary.

## How to Opt In
- **Protocol version negotiation is unchanged in mechanism**: a player/coach client still sends `(init TeamName (version N))`; requesting `N >= 20` gets the new `SerializerPlayerStdv20`/`SerializerCoachStdv20` (see `serialization-protocol-versions.instructions.md`). Requesting `N < 20` (even against a server running `2d_mode=false`) gets the OLD serializer chain — version negotiation is completely independent of the server's `2d_mode` setting.
- **Monitors** negotiate via `(dispinit version <N>)`; `N >= 6` gets `SerializerMonitorStdv6` (ball z in `(show ...)`), see `monitor-protocol.instructions.md`.
- **Game logs**: an operator sets `gameLogVersion=7` (`ServerParam`) to record ball z into `.rcg` files via the new `InitSenderLoggerV7`; the default (`REC_VERSION_6`) is unchanged, so existing log-processing tools keep working unless the operator opts in.
- **`2d_mode` is a server-side master gate**, not a protocol version: even a v20 client connected to a `2d_mode=true` server (the default) sees zero ball-z movement — the ball simply never leaves `z=0`, `Ball::incZ()` is never called (`Stadium::incMovableObjects()` only calls it when `!ServerParam::instance().is2dMode()`), and the new wire fields are always present but always zero. To see real 3D behavior, BOTH the server must run `2d_mode=false` AND the client must negotiate `version >= 20`.

## Wire Format Changes

### `kick` / `long_kick` — new 3-arg form
- Old: `(kick power dir)` / `(long_kick power dir)` — unchanged, still valid (loft defaults to `0.0`, i.e. a flat/grounded kick, byte-for-byte the old behavior).
- New: `(kick power dir loft)` / `(long_kick power dir loft)` — `loft` is a degrees-style angle (0 = flat/grounded, higher = more airborne arc), consumed by `Player::kickImpl()`/`longKickImpl()` server-side. No new fault/error response was added for out-of-range loft values — same clamping-not-rejecting philosophy as `power`/`dir`.
- These are backward compatible: `rcss::pcom::Builder::kick`/`long_kick` widened to a 3rd parameter with a `= 0.0` default, so a client that never sends the 3rd argument is completely unaffected.

### `chest_trap` — new command (renamed from `stop_ball`)
- Syntax: `(chest_trap)` — no arguments.
- Semantics: immediately zeroes the ball's velocity (all 3 axes) at its current position, only when the ball is currently kickable by the sending player (same `ballKickable()` gate as `kick`) — server-side implemented as `Player::chest_trap()` → `Stadium::chestTrap()`.
- Silently rejected (no-op, no error message) when `2d_mode==true` or the ball is not kickable — mirrors the existing silent-reject pattern of `Player::move()`, so client authors should not expect an `(error ...)` response to a rejected `chest_trap`.
- Offside enforcement: `Player::chest_trap()` also calls `Stadium::kickTaken( *this, PVector(0,0), 0.0 )` right after `Stadium::chestTrap()`, reusing the SAME referee fan-out dispatch a real kick uses (`Stadium::kickTaken()` iterates `M_referees` calling `Referee::kickTaken()`), so an offside-positioned player performing a chest trap is marked exactly as a kick would mark them (via `OffsideRef::kickTaken()` → `setOffsideMark()`). The zero accel/accel_z means no additional impulse is applied to the ball — the call exists purely for referee notification.
- Primary use case: a 3D-aware agent that wants to deliberately "trap"/deaden a bouncing or airborne ball instead of just receiving whatever velocity physics left it with.

### Player `(see)` — raw ball `z` and conditional `vz`

The normal ball name `(b)` and close/peripheral name `(B)` use the same numeric layouts:

| Observation | Protocols 1-19 | Protocol v20 |
|---|---|---|
| Low quality | `((b) dir)` | `((b) dir z)` |
| High quality, no change fields | `((b) dist dir)` | `((b) dist dir z)` |
| High quality, with change fields | `((b) dist dir dist_chg dir_chg)` | `((b) dist dir dist_chg dir_chg z vz)` |

- `z` is raw `Ball::posZ()` in metres. It is present in every v20 ball observation, including low quality where distance is absent.
- `vz` is raw `Ball::velZ()` in metres per simulation cycle. It is present exactly when `dist_chg` and `dir_chg` are present.
- `z` and `vz` are not quantized, clamped, derived, or passed through visual observation noise. Existing `dist`, `dir`, `dist_chg`, and `dir_chg` keep their established 2D meaning and noise behavior.
- Flags, goals, lines, and players have no new suffix. Apply these rules only after recognizing `(b)` or `(B)`.
- Protocols 1-19 keep their exact old ball layouts through legacy serializer defaults. Protocol v20 replaces the unreleased elevation draft in place; clients do not need to support both v20 shapes.
- Coach `(look)` remains a separate, unnoised channel with its existing raw-z extension. This player change does not add coach `vz`.

#### Client parsing checklist

1. Negotiate player protocol v20 explicitly with `(init TeamName (version 20))`.
2. Identify the object as normal `(b)` or close/peripheral `(B)` before applying v20 rules. Never strip a trailing value from flags, goals, lines, or players.
3. Decode exact ball numeric counts: `2` means `dir,z`; `3` means `dist,dir,z`; `6` means `dist,dir,dist_chg,dir_chg,z,vz`. Reject other v20 ball counts instead of guessing.
4. Store `z` for every v20 ball observation. Treat `vz` as optional and unavailable in the 2- and 3-value forms; do not report an omitted `vz` as zero.
5. Do not reconstruct planar x/y from low-quality `dir,z` alone because distance is absent. Keep last-known planar state explicitly stale or unavailable.
6. Under `2d_mode=true`, zero `z/vz` values are real present fields, not missing data.
7. Keep the legacy `1 / 2 / 4` numeric-count parser for protocols 1-19.

### `(fullstate)` — new ball `z`/`vel_z` fields
- Only for `version >= 20` (fullstate is player-only; there is no coach-side fullstate in this protocol at all).
- Exact v20 ball block: `((b) x y z vx vy vz)`. This task does not change that existing layout.

### Monitor `(show ...)` — new ball z field
- Only for monitor protocol `version >= 6`. `SerializerMonitorJSON` gets the equivalent field appended for JSON-protocol monitors regardless of numeric version (JSON has no version-int slot).

## `2d_mode` From a Client's Perspective
- If the **server** runs `2d_mode=true` (the default): physics is legacy-equivalent. Protocols 1-19 receive byte-identical legacy messages; protocol v20 receives its stable new shapes with `z=0` and, when present, `vz=0`.
- If the **server** runs `2d_mode=false`: legacy clients (negotiating `version < 20`) remain unchanged on the wire but are blind to vertical motion. A v20 player receives raw normal-see `z/vz` and fullstate `z/vz` needed to model the airborne ball.

## What `rcssmonitor` Needs to Change
- Negotiate `(dispinit version 6)` (or the JSON protocol equivalent) instead of the current max version to start receiving ball z in the `(show ...)` stream — see `monitor-protocol.instructions.md` for exactly which `SerializerMonitorStdv6`/`SerializerMonitorJSON` fields carry it.
- Rendering the extra dimension is entirely `rcssmonitor`'s own concern (out of scope for this repo) — as a **purely illustrative, non-dependency reference**, the sibling sandbox [`3d-kick-lab`](../../../3d-kick-lab) prototypes one plausible rendering convention worth looking at: its Three.js `physToThree(p)` helper (`3d-kick-lab/main.js:247-249`) maps physics-space `(x, y, z)` — where `x,y` is the ground plane (matching rcssserver's own field coordinate convention) and `z` is height — onto Three.js's Y-up world via `new THREE.Vector3(p.x, p.z, p.y)`. `rcssmonitor` is Qt-based, not Three.js, so this is inspiration for the *coordinate-mapping convention* only, not a library or code dependency.
- No protocol-level work is required beyond the version bump — the new field is a simple trailing numeric value in an already-parsed s-expression/JSON structure.

## Reference Client Implementations to Update First
Per this repo's root `copilot-instructions.md` Related Repositories table, these sibling repos are real protocol consumers that should be updated before most agent teams can use the new features:
- **`librcsc`** (`../../librcsc`) — shared C++ client library used by `helios-base` and other agent teams; its world-model/geometry classes have no concept of ball height today. This is the natural place to add a `posZ()`/`velZ()` accessor to the client-side ball model and to teach the RCG-log reader about `REC_VERSION_7`.
- **`helios-base`** (`../../helios-base`) — sample/reference agent team built on `librcsc`; would need `player.conf`/`coach.conf` version bumps plus any strategy code that currently assumes the ball is always at `z=0` (e.g. interception/pass-prediction logic) to at least tolerate (if not exploit) a nonzero ball height once `librcsc` exposes it.
- **`rcssmonitor`** (`../../rcssmonitor`) — see dedicated section above.

## Important Notes
- **Nothing on legacy protocol versions changes if you don't negotiate the new version.** A client requesting v20 opts into its stable extra fields even when the server remains in 2D mode.
- **`2d_mode` and protocol version are orthogonal knobs** — don't conflate "server enables 3D physics" with "client understands 3D fields"; both must be true simultaneously to get real 3D gameplay end-to-end.
- The formulas underlying `gravity`/`ball_bounce_restitution`/`height_power_cost`/etc. were hand-ported (not shared/merged code) from the `3d-kick-lab` sandbox's `physics.js` — see that repo's own copilot-instructions.md, which now documents its role as historical/reference now that porting is complete. **(2026-07-10 rework)**: `air_decay`, `loft_power_cost`, and `ball_bounce_friction` have all been REMOVED from both the sandbox and this server's `ServerParam` — a kick's horizontal/vertical split is now pure geometry (`cos(loft)`/`sin(loft)` of the same `eff_power`, no extra cost for aiming upward), the ball has ZERO horizontal friction while airborne (ground-only `ball_decay` friction, applied only once `pos_z<=0`), and `ball_bounce_restitution` alone now scales the ball's ENTIRE velocity vector (vx, vy, and the just-reflected vz) on every ground/post/crossbar bounce, replacing the old vz-only-restitution-plus-separate-friction-coupling model. `gravity`'s default also changed `0.15` → `0.1`, and the former separate `player_reach_height` concept was merged into `player_height` (one field is now both the visual/collision height and the max ball-z still considered kickable/headable).

## See Also
- [config-params.instructions.md](config-params.instructions.md) — the 9 `ServerParam` fields (`2d_mode`, `gravity`, etc.) and their defaults.
- [entities.instructions.md](entities.instructions.md) — `Ball`-only z physics (`incZ()`), height-gated goalie catch.
- [player-command-protocol.instructions.md](player-command-protocol.instructions.md) — `kick`/`long_kick` 3-arg grammar, `chest_trap` grammar chain.
- [serialization-protocol-versions.instructions.md](serialization-protocol-versions.instructions.md) — `SerializerPlayerStdv20`/`SerializerCoachStdv20` implementation detail.
- [monitor-protocol.instructions.md](monitor-protocol.instructions.md) — `SerializerMonitorStdv6` implementation detail.
- [logging-and-savers.instructions.md](logging-and-savers.instructions.md) — `REC_VERSION_7` game-log format.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
