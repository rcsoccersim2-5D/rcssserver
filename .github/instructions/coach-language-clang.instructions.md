---
applyTo: 'rcss/clang/**'
---

# Coach Language (CLang)

## TL;DR
`rcss/clang/` is a standalone library (built as `RCSSCLangParser` / `RCSS::CLangParser`) that parses, represents, validates, and serializes the online-coach "Coach Language" text protocol used in `(say ...)` messages.
- Grammar is defined in Bison/Flex (`coach_lang_parser.ypp`, `coach_lang_tok.lpp`), consumed by a `Parser`/`Builder` pattern — the parser never builds objects itself, it calls abstract `Builder` methods.
- Message payload classes (`clangmetamsg`, `clangfreeformmsg`, `claninfomsg`, `clangadvicemsg`, `clangdefmsg`, `clangdelmsg`, `clangrulemsg`, `clangunsuppmsg`) all derive from `rcss::clang::Msg` (`clangmsg.h:41`).
- `rule.h`/`region.h` define the internal DSL primitives (`Rule`/`CondRule`/`SimpleRule`/`NestedRule`/`IDListRule`, and `Region`/`Point` hierarchies) used inside DEFINE/RULE messages.
- Consumed by `OnlineCoach::parseCommand` in `src/coach.cpp` (the "say" command handler) — this is the ONLY call site in the server.
- **Open the full file when:** modifying grammar productions (`coach_lang_parser.ypp`), adding a new message/action/condition type (touch `clangbuilder.h`, `clangmsgbuilder.h/.cpp`, and the matching `clang*msg.h/.cpp`), or debugging a parse failure (`(error could_not_parse_say)`).

## Overview
CLang ("Coach Language") is the s-expression-like textual mini-language coaches send via `(say "...")` to issue advice/rules to their team (protocol version >= 7.0). This directory implements the server-side reader: lexer → grammar → builder → typed `Msg` object tree, plus `printPretty`/`print` serializers to echo messages back out (e.g. to the other coach/log). It does NOT implement the network transport (see networking-io instruction) — it only turns a `const char *` command string into an in-memory `std::shared_ptr<rcss::clang::Msg>`.

## Architecture
Pipeline: `Parser::parse(msg)` ([clangparser.h:88](../../rcss/clang/clangparser.h)) → `doParse()` switches an istream into the Flex lexer (`Lexer` = `rcss::clang::coach_lang_tok` generated lexer) → calls generated `RCSS_CLANG_parse(Parser::Param&)` (bison, `coach_lang_parser.ypp`) → grammar actions call `Builder` methods (`buildXxx`) → `MsgBuilder` (`clangmsgbuilder.h/.cpp`) accumulates a stack of typed items (`ItemType` tagged union via `boost::any`, enum `Types` in [clangmsgbuilder.h:72](../../rcss/clang/clangmsgbuilder.h)) and finally produces a concrete `Msg` retrievable via `builder.getMsg()`.
- Grammar source files are `.ypp`/`.lpp`; CMake (`bison_target`/`flex_target` in [CMakeLists.txt:1-21](../../rcss/clang/CMakeLists.txt)) generates `coach_lang_parser.cpp/.hpp` and `coach_lang_tok.cpp` into the build dir at configure time — **do not look for these `.cpp` files in the source tree**, they don't exist until built.
- All bison/flex symbol names are prefixed `RCSS_CLANG_*` (via `#define yyparse RCSS_CLANG_parse` etc., `coach_lang_parser.ypp:31-71`) to avoid clashing with other parsers linked into the same binary.
- `Msg` base class ([clangmsg.h:41](../../rcss/clang/clangmsg.h)) exposes `Types` enum `{META, FREEFORM, INFO, ADVICE, DEFINE, DEL, RULE, UNSUP}`, version range (`M_min_ver`/`M_max_ver`, `setVer`/`isSupported`), send/recv timestamps, and `getSide()`.
- `Builder` is a pure-virtual interface ([clangbuilder.h:52](../../rcss/clang/clangbuilder.h), ~90 `buildXxx` methods) — separates grammar concerns from message-construction concerns per its own doc comment (lines 31-40): "the clang::Builder is called from within the parser... it is not the job of the builder to make sure the grammar meets the Clang grammar rules, that's the job of the parser."
- `MsgBuilder` is the sole concrete `Builder` ([clangmsgbuilder.h:60](../../rcss/clang/clangmsgbuilder.h)), using a `std::stack<ItemType>` as an operand stack that mirrors how the grammar reduces nested expressions (regions built from points, rules built from conds+dirs, etc.).

## Patterns & Conventions
- Every message/value class exposes `print(ostream&)`, `printPretty(ostream&, line_header)`, and `deepCopy()` — a consistent trio across `Msg`, `Rule`, `Region`, `Point`. Follow this trio when adding new node types.
- Heavy use of `std::shared_ptr` for tree nodes (regions, points, rules, conditions) — deep copies are explicit via `deepCopy()`, not copy constructors (many classes `= delete` their copy ctor, e.g. `PointRel`, `RegQuad`).
- `Region`/`Point` hierarchies ([region.h](../../rcss/clang/region.h)) are abstract base + concrete leaf/composite subclasses: `Point` → `PointSimple`, `PointRel` (relative to another `Point`), `PointBall`, `PointPlayer`, `PointArith` (arithmetic combination via `rcss::util::ArithOp`); `Region` → `RegNull`, `RegQuad`, `RegArc`, `RegUnion`, plus (per grammar) named/triangle/rectangle/point regions built through `buildReg*` builder calls.
- `Rule` hierarchy ([rule.h:85](../../rcss/clang/rule.h)): abstract `Rule` → `CondRule` (holds a `shared_ptr<const Cond>`) → `SimpleRule` (cond + list of `Dir` actions) / `NestedRule` (cond + list of sub-`Rule`s, for `if`-nesting); `IDListRule` is a separate `Rule` subtype representing bare rule-ID references (used by `(rule (ourside) (ruleid ...))`-style referencing). `RuleIDList` treats an **empty list as "ALL rules"** — see comment at [rule.h:35-37](../../rcss/clang/rule.h): "If the rule list is empty then it counts as containing all rules... there is no support for an empty rule ID list."
- Grammar/token macros: everything the lexer returns is a `RCSS_CLANG_XXX` token (`RCSS_CLANG_LP`, `RCSS_CLANG_SAY`, `RCSS_CLANG_HEAR`, etc., `coach_lang_tok.lpp:53+`); numeric literal patterns `UINT/INT/REAL/EXP` are defined once at top of the `.lpp`.

## Key Abstractions
- `rcss::clang::Parser` / `Parser::Param` — [clangparser.h:35](../../rcss/clang/clangparser.h): RAII-ish wrapper binding a `Lexer` + `Builder` for one parse call; `parse(const char* msg)` is the convenience entry point used by callers.
- `rcss::clang::Builder` (interface) and `rcss::clang::MsgBuilder` (impl) — construction-time state machine driven by grammar reductions.
- `rcss::clang::Msg` and subclasses (`MetaMsg`, `FreeformMsg`, `InfoMsg`, `AdviceMsg`, `DefineMsg`, `DelMsg`, `RuleMsg`, `UnsuppMsg`) — one class per top-level CLang message type, matching `Msg::Types`.
- `rcss::clang::Rule` / `CondRule` / `SimpleRule` / `NestedRule` / `IDListRule`, `RuleIDList`, `ActivateRules` — the rule-activation sublanguage (used by RULE/DEFINE messages to turn team behaviors on/off conditionally).
- `rcss::clang::Region` / `Point` hierarchies — spatial primitives referenced by conditions (`buildCondPlayerPos`, `buildCondBallPos`) and directives (`buildActBallToReg`, `buildActMarkLineReg`).
- `BuilderErr` — exception type thrown by `MsgBuilder` when builder methods are invoked out of grammar-consistent order ([clangbuilder.h:343](../../rcss/clang/clangbuilder.h)); caught as `std::exception` by the caller in `coach.cpp`.

## Integration Points
- **Sole caller**: `OnlineCoach::parseCommand` handling the `"say"` command, `src/coach.cpp:1141-1300`. Only active when `version() >= 7.0`. Flow:
  1. Constructs `rcss::clang::MsgBuilder builder;` and sets `builder.setFreeformMsgSize(ServerParam::instance().freeformMsgSize())` ([coach.cpp:1147-1148](../../src/coach.cpp)).
  2. Constructs `rcss::clang::Parser parser(builder);` and calls `parser.parse(command)` inside a `try/catch(std::exception&)` — parse failures send `(error could_not_parse_say)` back to the coach ([coach.cpp:1150-1161](../../src/coach.cpp)).
  3. On success, retrieves `builder.getMsg()`, stamps `msg->setTimeRecv(M_stadium.time())`, then switches on `msg->getType()` to enforce per-type rate limits (`M_meta_messages_left`, `M_freeform_messages_allowed`, `M_info_messages_left`, `M_advice_messages_left`, `M_define_messages_left`, `M_del_messages_left`, `M_rule_messages_left`) and decides `should_queue` before pushing onto `M_message_queue` ([coach.cpp:1164-1300](../../src/coach.cpp)).
  4. Outgoing direction: `OnlineCoach::say(const rcss::clang::Msg&)` ([coach.cpp:1486](../../src/coach.cpp)) serializes a `Msg` back out (e.g. echoing to the coach or forwarding to online players via `(hear ...)`).
- **Transport**: messages arrive/leave as plain text over the coach's UDP connection — see `networking-io.instructions.md`; this module has zero socket/`RemoteClient` awareness.
- **Cross-repo counterpart**: the separate `librcsc` repo has its own client-side `rcsc/clang/` module (used by `helios-base`'s coach client) that must stay grammar-compatible with this server-side parser — if you change the grammar here, check version compatibility (`Msg::setVer`/`isSupported`, CLang version negotiation via `buildMetaTokenVer`) against that client implementation.

## Build & Test
- Built as its own CMake target `RCSSCLangParser` (shared lib, alias `RCSS::CLangParser`), SOVERSION 18 ([CMakeLists.txt:23-77](../../rcss/clang/CMakeLists.txt)). Depends only on `Boost::boost` (uses `boost::any` in `MsgBuilder::ItemType`).
- Grammar generation is a **build-time step**: `bison_target(coach_lang_parser ...)` and `flex_target(coach_lang_tokenizer ...)` generate sources into `${CMAKE_CURRENT_BINARY_DIR}`; a custom command (`fix_lexer_file.cmake`) post-processes the raw flex output to fix the generated header include name ([CMakeLists.txt:1-21](../../rcss/clang/CMakeLists.txt)).
- No dedicated unit tests found under this directory; validate grammar changes by round-tripping through `OnlineCoach`/an actual coach client (e.g. `helios-base`), or by writing a small driver that calls `rcss::clang::Parser::parse()` directly with sample CLang strings and inspecting `MsgBuilder::getMsg()`.
- After editing `.ypp`/`.lpp`, a full reconfigure/rebuild is required (bison/flex re-invoked) — incremental compiles of only `.cpp` files won't pick up grammar changes.

## Logging
- No structured logger in this module; parse errors surface via `std::cerr` (see `yyerror` declarations in `coach_lang_parser.ypp:74-75`) and are converted to `std::exception` at the `Parser::doParse` boundary, then reported to the coach as `(error could_not_parse_say)` by `coach.cpp:1159-1160`.

## Important Notes
- **Do not confuse the `Builder` interface (`clangbuilder.h`) with `MsgBuilder` (`clangmsgbuilder.h`)** — the former is grammar-facing/abstract, the latter is the concrete stack machine; if you add a new grammar production you must add a `buildXxx` pure-virtual to `Builder` AND implement it in `MsgBuilder`.
- `RuleIDList` empty-means-ALL semantics ([rule.h:35](../../rcss/clang/rule.h)) is easy to get backwards when writing new rule-activation logic — an empty list is never invalid input, it's "apply to all rules."
- Version gating (`Msg::M_min_ver`/`M_max_ver`, `isSupported()`) lets one message declare it only applies to certain CLang protocol versions — check this before assuming a parsed message is unconditionally valid.
- The generated bison/flex `.cpp` files are NOT checked into `rcss/clang/` — don't grep this directory expecting to find `coach_lang_parser.cpp`; they only exist post-build in the CMake binary dir.
- Per-message rate limiting (`M_*_messages_left` counters) lives in `src/coach.cpp`, not in this library — this module has no concept of message quotas, only message structure/validity.

## See Also
- [`entities.instructions.md`](./entities.instructions.md) — `OnlineCoach`/`Coach` entity that owns the parser/builder call site.
- [`networking-io.instructions.md`](./networking-io.instructions.md) — transport layer carrying CLang text payloads.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
