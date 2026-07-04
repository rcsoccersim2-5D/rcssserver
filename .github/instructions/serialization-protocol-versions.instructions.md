---
applyTo: 'src/serializer*.cpp,src/serializer*.h'
---

# Sensory Message Serialization & Protocol Versions

## TL;DR
The `serializer*` family turns `Player`/`Coach`/`Ball` entity state into version-specific text (or JSON) sensory messages, using a base-interface + linear-subclass-per-version pattern registered in per-file, per-version factories.
- 4 factories (`SerializerPlayer`, `SerializerCoach`, `SerializerOnlineCoach`, `SerializerCommon`), each an `rcss::Factory<Creator, int>` keyed by protocol version number.
- Each concrete `SerializerXStdvN` inherits the *previous* version's class (not the abstract base) and overrides only the methods that changed — diff-style versioning.
- Version selection happens once per connection, driven by the client's `(init ... (version N))` command, cached as `M_version` on `Player`/`Coach`.
- **Open the full file when:** you need the exact wire-format text for a specific message (e.g. `(see ...)`, `(sense_body ...)`) or are adding/porting a protocol version.

## Overview
`src/serializer.h`/`.cpp` define the abstract interfaces `SerializerCommon`, `Serializer` (common base), `SerializerPlayer`, `SerializerCoach`, `SerializerOnlineCoach` (all in `namespace rcss`). Concrete classes live one-per-version in `serializer{player,coach,onlinecoach,common}stdv{N}.cpp/.h`, plus a non-versioned `serializercommonjson.cpp/.h` (registered under the sentinel version `-1`). Each serializer formats sensory data (`(see ...)`, `(sense_body ...)`, `(hear ...)`, `(fullstate ...)`, `(player_type ...)`, `(server_param ...)`, etc.) into an `std::ostream` that Sender classes (`senders-dispatch` instruction) write to the socket.

## Architecture
- **`SerializerCommon`** (`src/serializer.h:46-141`) — pure-virtual-ish interface (all methods have empty default bodies) for server/player-param and generic `serializeParam` overloads shared by all client types. Own factory `SerializerCommon::factory()` (`serializer.cpp:32-37`), keyed by version.
- **`Serializer`** (`serializer.h:144-260`) — thin base holding a `const SerializerCommon::Ptr M_common` (composition, not inheritance) and forwarding `serializeParam`/`serializeServerParamBegin` etc. to it via `commonSerializer()`.
- **`SerializerPlayer : public Serializer`** (`serializer.h:263-678`) — the big one: pure-virtual audio methods (`serializeRefereeAudio`, `serializeCoachAudio`, `serializeSelfAudio`, `serializePlayerAudio`, `serializeCoachStdAudio` — must be implemented by v1) plus ~40 default-no-op virtuals for `(see)` visual objects, `(sense_body)` fields, fullstate, init/reconnect, tackle/collision/foul, etc.
- **`SerializerCoach`** (`serializer.h:682+`) and **`SerializerOnlineCoach : public Serializer`** — online-coach serializer additionally composes a `SerializerCoach::Ptr M_coach` (`serializer.cpp:101-107`) since online coaches reuse most coach formatting.
- **Concrete per-version classes** form a **linear inheritance chain per family** (confirmed via grep of all `serializer*.h`):
  - Player: `Stdv1 → Stdv5 → Stdv7 → Stdv8 → Stdv13 → Stdv14 → Stdv18`
  - Coach: `Stdv1 → Stdv7 → Stdv8 → Stdv13 → Stdv14`
  - OnlineCoach: `Stdv1 → Stdv6 → Stdv7 → Stdv8 → Stdv13 → Stdv14`
  - Common: `Stdv1 → Stdv7 → Stdv8`
  - `SerializerCommonJSON : public SerializerCommon` stands alone (no stdv ancestor), used by the JSON/pretty-print protocol variant.
- Each `StdvN::create()` static method looks up its matching `SerializerCommon` version via `SerializerCommon::factory().getCreator(cre, N)` and constructs itself with the resulting common serializer — e.g. [serializerplayerstdv1.cpp:432-444](../../src/serializerplayerstdv1.cpp:432).

## Patterns & Conventions
- **Registration idiom** (anonymous namespace, static init, at bottom of every `.cpp`): `RegHolder vN = SerializerX::factory().autoReg(&SerializerXStdvN::create, N);`. One class often registers under **multiple numeric versions** when nothing changed between protocol releases, e.g. `SerializerPlayerStdv1` registers for versions 1–4 ([serializerplayerstdv1.cpp:447-450](../../src/serializerplayerstdv1.cpp:447)); `SerializerPlayerStdv8` covers 8–12; `SerializerPlayerStdv14` covers 14–17; `SerializerCommonJSON` registers once at `-1` ([serializercommonjson.cpp:143](../../src/serializercommonjson.cpp:143)).
- **Diff-only overrides**: a new version class only overrides methods whose wire format actually changed (e.g. `SerializerPlayerStdv8` overrides `serializeVisualPlayer` overloads to add `point_dir`/`head_dir` fields — [serializerplayerstdv8.h:44-68](../../src/serializerplayerstdv8.h:44)); everything else falls through to the parent version via normal virtual dispatch.
- **Full-version headers `#include` their immediate predecessor** (e.g. `serializerplayerstdv8.h` includes `"serializerplayerstdv7.h"`), not the abstract base — this is what makes the "diff subclass" pattern discoverable just from headers.
- Every concrete class exposes a `static const SerializerX::Ptr create()` factory method (never public constructors — protected `explicit` ctor) and is instantiated only via `Ptr` (`std::shared_ptr`).

## Key Abstractions
- `rcss::Factory<Creator, int>` (from `<rcss/factory.hpp>`) — generic registry mapping an `int` key to a creator function pointer; `.autoReg(fn, key)` registers at static-init time, `.getCreator(out, key)` looks up.
- `SerializerPlayer::Creator` / `SerializerCoach::Creator` / `SerializerOnlineCoach::Creator` / `SerializerCommon::Creator` — all `typedef const Ptr (*)()` function-pointer types.
- Virtual interface names to know: `serializeVisualObject`/`serializeVisualPlayer` (see-message ball/player entries), `serializeBody*` (sense_body), `serializeFS*` (fullstate), `serialize*Audio` (hear), `serializeInit`/`serializeReconnect`/`serializeChangePlayer`, `serializeServerParamBegin/End`, `serializePlayerParamBegin/End`, `serializePlayerTypeBegin/End`.

## Integration Points
- **Instantiation call sites** (confirmed by grep): [player.cpp:2438-2454](../../src/player.cpp:2438) `Player::setSenders()` does `rcss::SerializerPlayer::factory().getCreator(ser_cre, (int)version())` then `ser_cre()`, and passes the resulting `Ptr` into `BodySenderPlayer`/`VisualSenderPlayer`/etc. constructors. Analogous lookups exist in `coach.cpp:129` (`SerializerCoach`) and `coach.cpp:1007` (`SerializerOnlineCoach`).
- `Player::M_version` is set from the client's `(init TeamName (version N))` command (`player.cpp:306`, default `3.0` at `player.cpp:162`); `setSenders()` is invoked right after parsing init (`player.cpp:363`).
- Downstream from Stadium: `Stadium::doSendSenseBody()`/`doSendVisuals()` (stadium.cpp) call the already-configured Sender objects (see `senders-dispatch` instruction) — Stadium itself never touches `Serializer::factory()` directly; the serializer is fixed once at connection time, not re-selected per cycle.
- `SerializerMonitor` (`serializermonitor.cpp/.h`) is a sibling factory family for the **monitor** protocol — structurally identical (`autoReg`/`getCreator` on version ints, `SerializerCommon::factory().getCreator` for shared param formatting) but documented separately; see `monitor-protocol.instructions.md`.

## Build & Test
- No standalone unit tests for serializers found under `src/`; validated indirectly via `rcssserver` integration/manual play with real clients requesting different `(version N)`.
- All `serializer*.cpp` compile into the main `librcssserver`/`rcssserver` target (see root `Makefile.am`/`CMakeLists.txt`); adding a new file requires registering it in the build sources list.

## Logging
- No dedicated logging in serializers themselves; malformed/unsupported version lookups log to `std::cerr` at the call site, e.g. `"No SerializerPlayer::Creator vN"` and `"No SerializerPlayer vN"` in [player.cpp:2445,2452](../../src/player.cpp:2445).

## Important Notes
- **Adding a new protocol version**: (1) if the wire format is unchanged from the latest existing class, just add another `autoReg(&LatestStdvN::create, NEW_N)` line in that class's `.cpp`; (2) if formats changed, create `serializerXstdv{NEW_N}.{h,cpp}` subclassing the **current latest** version class (not the abstract base), override only the changed virtuals, add its own `create()` (mirroring the `SerializerCommon::factory().getCreator` lookup pattern), and register with `autoReg`. Remember to also update the matching `SerializerCommon` version chain if shared param formatting changed, and the `BodySenderPlayer`/`VisualSenderPlayer`/etc. factories which are versioned the same way and looked up alongside the serializer in `setSenders()`.
- **Gotcha**: `getCreator` failures are silent-ish (`std::cerr`, not exception) and the caller (`setSenders()`) returns `false` — verify calling code aborts the connection instead of proceeding with a null serializer.
- **Gotcha**: version numbers are **not 1:1 with class names** — e.g. `SerializerPlayerStdv8` serves versions 8–12, so "stdv8" in a filename means "introduced at v8", not "only v8". Always check the `autoReg` lines in that file to know the true version range.
- **Gotcha**: `SerializerOnlineCoach` composes a `SerializerCoach::Ptr` internally (`serializer.cpp:101`) — online-coach serializer files (`serializeronlinecoachstdv*.cpp`) do their own extra `SerializerCoach::factory().getCreator(cre_coach, N)` lookup in `create()`, independent from the player-side chain.
- JSON output (`serializercommonjson.cpp`) is an alternate `SerializerCommon` implementation selected via version key `-1`; it is NOT part of the numeric stdv inheritance chain and has no player/coach/online-coach JSON counterpart in this file set — only common param formatting is JSON-ified.

## See Also
- [entities.instructions.md](./entities.instructions.md) — `Player`/`Coach`/`Ball` state that serializers format
- [game-loop-and-timers.instructions.md](./game-loop-and-timers.instructions.md) — `Stadium::doSendSenseBody()`/`doSendVisuals()` per-cycle callers
- [senders-dispatch.instructions.md](./senders-dispatch.instructions.md) — Sender classes that own a `Serializer::Ptr` and write to sockets
- [monitor-protocol.instructions.md](./monitor-protocol.instructions.md) — sibling `SerializerMonitor` family (`serializermonitor.cpp/.h`)
- [config-params.instructions.md](./config-params.instructions.md) — `ServerParam`/version flags controlling which protocol version clients negotiate

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
