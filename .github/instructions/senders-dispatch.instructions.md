---
applyTo: 'src/sender.h,src/initsender*.cpp,src/initsender*.h,src/visualsender*.cpp,src/visualsender*.h,src/dispsender.*,src/fullstatesender.*,src/bodysender.*'
---

# Senders — Per-Cycle Message Dispatch

## TL;DR
Every outbound protocol message (see/visual, sense_body, fullstate, init handshake, monitor display) is built by a versioned `*Sender` subclass that pulls formatted fields from a `Serializer` and writes them straight into an `std::ostream & transport()`.
- All senders derive (directly or via an intermediate base) from `rcss::Sender` (`sender.h:41`), which just wraps `std::ostream & M_transport`.
- Each concrete client-type sender (`VisualSenderPlayer`, `BodySenderPlayer`, `InitSenderPlayer`, `FullStateSenderPlayer`, `DispSenderMonitor`, `DispSenderLogger`) has its own `Factory<Creator,int>` keyed by protocol version, populated at static-init time via `autoReg`.
- `Player`/`Monitor`/`Logger`/`Coach` own the sender through a `BaseObserver<Sender>` wrapper (`observer.h:42`), not the sender directly — e.g. `Player::M_observer` is an `rcss::ObserverPlayer` holding a `shared_ptr<VisualSenderPlayer>`.
- `Player::setSenders()` (player.cpp:2439) is the one-time wiring point: looks up a `SerializerPlayer` for the client's declared version, then looks up and constructs each sender type from its own factory, passing the shared serializer in via a `Params` struct.
- **Open the full file when:** you need per-version wire-format details (quantization, noise, field lists) inside `send*()`/`serialize*()` bodies — this doc only covers dispatch plumbing, not payload bit-layout.

## Overview
This subsystem covers the classes that assemble and transmit the periodic (per-simulation-cycle) and handshake messages sent to every connected client type:
- **Players**: `sense_body` (`bodysender.*`), `see`/visual (`visualsenderplayer.*`), `init`/handshake (`initsenderplayer.*`, shares `initsender.*` base), `fullstate` (`fullstatesender.*`).
- **Coach / OnlineCoach**: `visualsendercoach.*` (`see_global`), `initsendercoach.*` / `initsenderonlinecoach.*`.
- **Monitor**: `dispsender.*` (`DispSenderMonitor`) for the binary/JSON `show` stream, `initsendermonitor.*` for its handshake.
- **Logger** (game/text log files): `dispsender.*` (`DispSenderLogger`) and `initsenderlogger.*` — same class hierarchy as monitor senders since a game log is structurally a recorded monitor stream.

## Architecture
```
rcss::Sender (sender.h)                     <- holds std::ostream& transport
  └─ BodySender / FullStateSender / DispSender / InitSender (protected Sender)
       └─ *Player / *Monitor / *Coach / *Logger  (adds M_serializer, M_self, M_stadium)
            └─ *V1 .. *V18 concrete version classes (override send*/serialize* only)
```
- `InitSenderCommon` (initsender.h:50) is a **parallel** hierarchy shared by ALL client types for the version-dependent parts of the init handshake (`sendServerParams`, `sendPlayerParams`, `sendPlayerTypes`) — `InitSenderPlayer`/`InitSenderCoach`/etc. hold a `shared_ptr<InitSenderCommon>` (`m_common_sender`, initsender.h:123) and delegate those three calls to it, while implementing client-specific `sendInit()`/`sendChangedPlayers()`/`sendScore()` themselves. Concrete common versions: `InitSenderCommonV1/V7/V8/JSON` (initsender.h:178-346).
- Ownership: the **client entity** (`Player`, `Monitor`, `Logger`, `Coach`) is-a `BaseObserver<XxxSender>` mixin (e.g. `class ObserverPlayer : protected BaseObserver<VisualSenderPlayer>`, visualsenderplayer.h:166) and stores the live `shared_ptr<Sender>` inside that template. The entity class itself typically multiply-inherits several observer mixins — e.g. `Player : public MPObject, public RemoteClient, public rcss::Listener, ...` plus separate `M_init_observer`, `M_observer`, `M_body_observer`, `M_fullstate_observer` members (player.h, player.cpp:156-158) — rather than inheriting the observers directly.
- `BaseObserver<S>::sender()` (observer.h:66) throws `std::logic_error("Sender is null")` if called before `setXxxSender()` — a common early-connection bug if visuals are requested before `setSenders()` completes.

## Patterns & Conventions
- **Factory-per-sender-family, keyed by protocol version** (`int`), using the shared `rcss::Factory<Creator,int>` template (`rcss/factory.hpp`) — identical pattern to the Serializer factories. Registration is static, e.g. `visualsenderplayer.cpp:1329-1346`:
  ```cpp
  RegHolder vp1 = VisualSenderPlayer::factory().autoReg( &create<VisualSenderPlayerV1>, 1 );
  ...
  RegHolder vp18 = VisualSenderPlayer::factory().autoReg( &create<VisualSenderPlayerV18>, 18 );
  ```
  Many versions map to the same concrete class (v9-v12 all use `VisualSenderPlayerV8`) — only bump the class when the wire format actually changes.
- **`Params` inner struct** — every leaf sender family defines a `Params` value type (transport ref, `self` entity ref, `shared_ptr<Serializer>`, optional `Stadium&`) passed to both the `Creator` function pointer and the constructor chain, e.g. `VisualSenderPlayer::Params` (visualsenderplayer.h:56-72), `BodySenderPlayer::Params` (bodysender.h:74-87).
- **Serializer hand-off**: the sender never picks its own serializer — `Player::setSenders()` resolves ONE `SerializerPlayer` for the connection's version (`rcss::SerializerPlayer::factory().getCreator(ser_cre, (int)version())`, player.cpp:2442) and threads that same `shared_ptr<SerializerPlayer>` into every `Params` struct (body, visual, init, fullstate) — so one client always uses one consistent serializer instance across all its message types. Senders call it as `serializer().serializeVisualBegin(transport(), stadium().time())` etc. (visualsenderplayer.cpp:384-390).
- **Write pattern**: `serializeXxx(transport(), ...)` calls stream fields directly into `transport()`; the sender terminates each message with `transport() << std::ends << std::flush;` (visualsenderplayer.cpp:390) — `std::ends` writes a NUL terminator expected by the UDP client-side parser, `std::flush` pushes bytes through `RemoteClient`'s `M_transport` streambuf (gzip layer) immediately.
- **Version-delta overriding**: subclasses only override the methods that changed for that protocol bump (e.g. `BodySenderPlayerV8` only overrides `sendBodyData()` for arm/focus/tackle fields, bodysender.h:285-298) and inherit everything else — read the diff of overridden methods across the `V*` chain to see exactly what changed per version.

## Key Abstractions
- `rcss::Sender` (sender.h) — root class, `transport()` accessor only.
- `rcss::BaseObserver<S>` (observer.h) — generic "holds and forwards to a sender" mixin; used for every family (`ObserverPlayer`, `BodyObserverPlayer`, `InitObserverPlayer`, `FullStateObserverPlayer`, `ObserverMonitor`, `ObserverLogger`, `ObserverCoach`...).
- `InitSenderCommon` / `InitSenderCommonV1/V7/V8/JSON` (initsender.h) — version-gated `sendServerParams/sendPlayerParams/sendPlayerTypes`, shared across player/coach/onlinecoach/monitor/logger init senders.
- `DispSender` (dispsender.h:45) — base for monitor/logger display protocol: `sendShow()`, `sendMsg(BoardType, const char*)`, `sendTeamGraphic(Side, x, y)`. `DispSenderMonitor` and `DispSenderLogger` are siblings sharing this interface and a `SerializerMonitor`.
- `FullStateSenderPlayer` (fullstatesender.h:74) — near-identical shape to `VisualSenderPlayer`/`BodySenderPlayer` but sends unquantized/omniscient state, gated per-cycle by `ServerParam::fullstateLeft()/fullstateRight()`.

## Integration Points
- **Construction / wiring**: `Player::setSenders()` (player.cpp:2439-~2505) is called once from `Player`'s constructor path (`player.cpp:363`, guarded — connection aborts if any factory lookup fails) and resolves Serializer + all four sender families for that player's declared version. `Monitor`'s analogous wiring for `DispSenderMonitor` is at monitor.cpp:148-155.
- **Per-cycle invocation from Stadium** (see `game-loop-and-timers.instructions.md`):
  - `Stadium::doSendSenseBody()` (stadium.cpp:2253) → `p->sendBody()` (stadium.cpp:2268) then conditionally `p->sendFullstate()` (stadium.cpp:2275) if that side has fullstate enabled — both `Player` methods just forward to `M_body_observer`/`M_fullstate_observer`.
  - `Stadium::doSendVisuals()` (stadium.cpp:2301) → `p->sendVisual()` (stadium.cpp:2313) → forwards to `M_observer` (`ObserverPlayer`) → `VisualSenderPlayer::sendVisual()`.
  - `Stadium::sendDisp()` (stadium.cpp:940, called from `step()` around stadium.cpp:890) drives every registered `Monitor`/`Logger`'s `ObserverMonitor::sendShow()` → `DispSenderMonitor::sendShow()`.
- **Transport**: `transport()` is an `std::ostream&` bound to the owning `RemoteClient`'s stream (see `networking-io.instructions.md`) — swapped to a gzip streambuf transparently; senders are unaware of compression.
- **Serializer**: see `serialization-protocol-versions.instructions.md` for the `Factory<Creator,int>` pattern used to pick `SerializerPlayer`/`SerializerCoach`/`SerializerOnlineCoach`/`SerializerMonitor` instances that senders hold via `serializer()`.

## Build & Test
- Part of the core `librcssserver`/`rcssserver` target build (CMake/autotools under `rcssserver/`); no standalone unit tests target senders specifically — verification is via `rcssserver/src/tests` (protocol regression) if present, or manual client connection with `rcssmonitor`/`soccerwindow2`/sample players at a given `-server::version`.
- Any new `V*` class must be registered in the corresponding `.cpp`'s bottom `RegHolder ... = Factory::autoReg(&create<Class>, N);` block or it silently falls back to "No XxxSender::Creator vN" (`std::cerr` message, e.g. player.cpp:2463) and the connection setup fails (`setSenders()` returns `false`).

## Logging
- No dedicated logger; failures during sender wiring go to `std::cerr` with a distinct "No <Family>::Creator vN" / "No <Family> vN" message per family (player.cpp:2445,2452,2463,2476,2490 and the fullstate equivalent) — grep `std::cerr <<` in `player.cpp` near `setSenders()` to see all failure paths.
- Per-message wire content is not logged in production; the recorded game log (`.rcg`) IS effectively the `DispSenderLogger` output stream itself.

## Important Notes
- **One Serializer per connection, shared across all its senders** — do not construct a second serializer instance per message family; `setSenders()` deliberately resolves it once and threads the same `shared_ptr` through every `Params`.
- **Sender may be null before `setSenders()` succeeds** — `BaseObserver<S>::sender()` throws `std::logic_error` if `setXxxSender()` was never called; any code path that can send messages before full connection setup must guard against this.
- **Version-class-reuse is intentional, not a gap** — many consecutive protocol versions map to the same `V*` concrete class (e.g. visual v9-v12 → `VisualSenderPlayerV8`); don't assume a missing `VisualSenderPlayerV9` class is a bug.
- **`InitSenderCommon` vs `InitSender` are two separate hierarchies** that intersect only through composition (`InitSender::m_common_sender`) — do not confuse per-client `InitSenderPlayer/Coach/...` version classes with the shared `InitSenderCommonV1/V7/V8/JSON` classes; server params/player params/player types formatting lives only in the latter.
- **Message termination**: forgetting `transport() << std::ends << std::flush;` after a hand-rolled send routine will silently corrupt the client-side stream framing — always mirror the existing `sendVisual()`/`sendBody()` tail pattern.

## See Also
- `serialization-protocol-versions.instructions.md`
- `game-loop-and-timers.instructions.md`
- `networking-io.instructions.md`
- `monitor-protocol.instructions.md`

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
