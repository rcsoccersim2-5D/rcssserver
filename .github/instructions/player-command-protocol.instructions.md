---
applyTo: 'src/player_command_parser.ypp,src/player_command_tok.lpp,src/pcomparser.*,src/pcombuilder.*'
---

# Player Command Protocol Parsing

## TL;DR
A bison/flex grammar turns raw text like `(dash 80 10)` sent by an agent over UDP into calls on the abstract `rcss::pcom::Builder` interface, which `Player` implements directly.
- Grammar: [player_command_parser.ypp](../../src/player_command_parser.ypp) (461 lines) — one rule per command, each action calls `BUILDER.<method>(...)`.
- Lexer: [player_command_tok.lpp](../../src/player_command_tok.lpp) (139 lines) — keyword/number/quoted-string tokenizer, flex C++ class `RCSSPComLexer`.
- Glue: [pcomparser.h/.cpp](../../src/pcomparser.h) wraps `RCSS_PCOM_parse()`; `Player` (player.h:51) directly `public rcss::pcom::Builder`s — no adapter class needed.
- **Open the full file when:** adding/removing a command (touch all 4: lexer keyword, parser rule+token, Builder pure-virtual, `Player::` override) or debugging a "illegal_command_form" parse failure.

## Overview
Every command an agent (player client) sends over UDP is a single S-expression string, e.g. `(dash 80)`, `(turn_neck 15)`, `(say "hello")`, `(move 0 0)`. This subsystem's job is purely syntactic: recognize the command form, extract typed arguments, and dispatch to one `Builder` virtual method call per command — no game-state logic lives here. `Player::parseCommand` (player.cpp:558+, ~370 lines) is a **separate, legacy hand-rolled string-matching parser** kept only as a Cygwin fallback (see Important Notes).

## Architecture
```
UDP datagram (RemoteClient)
  -> Player::parseMsg(char* msg, size_t len)         player.cpp:528
       -> M_parser.parse(command)                    player.cpp:545  (rcss::pcom::Parser, member M_parser, player.cpp:160)
            -> rcss::Parser::parse(istream)           (base class, pcomparser.cpp:41-46)
                 -> RCSS_PCOM_parse(Parser::Param&)    generated from player_command_parser.ypp
                      -> yylex() -> Param::getLexer().lex(holder)   player_command_tok.lpp (RCSSPComLexer)
                      -> grammar rule action -> BUILDER.<cmd>(args) -> Param::getBuilder() -> Player (the Builder impl)
```
- `rcss::pcom::Parser::Param` (pcomparser.h:44-72) bundles the `Lexer` instance and a `Builder&` reference per parse call; `getBuilder(param)` (player_command_parser.ypp:38-41) is the inline helper the grammar actions use via the `BUILDER` macro (ypp:54).
- `Player` inherits `MPObject`, `RemoteClient`, `rcss::Listener`, **`rcss::pcom::Builder`** (player.h:47-52) and implements every pure-virtual directly (e.g. `Player::dash` player.cpp:1046/1094, `Player::kick` player.cpp:1305, `Player::say` player.cpp:1704, `Player::done` player.cpp:1956) — so parsing writes straight into `Player`'s own state/queues, no intermediate command-object/AST is built.
- Build: CMake generates the real parser/lexer at build time — `bison_target(player_command_parser player_command_parser.ypp -> player_command_parser.cpp/.hpp)` and `flex_target(player_command_tokenizer player_command_tok.lpp -> raw_player_command_tok.cpp)`, then a `fix_lexer_file.cmake` post-process step renames the generated header include to `player_command_tok.h` (src/CMakeLists.txt:2-22). Same pattern as the CLang coach grammar (see coach-language-clang instructions).

## Patterns & Conventions
- **One bison rule = one Builder call.** E.g. `dash_com` (ypp:158-167) supports 3 arities (`(dash p)`, `(dash p dir)`, `(dash (l ...)(r ...))` per-leg) each mapping to a distinct overload: `Builder::dash(double)`, `dash(double,double)`, `dashLeftLeg`/`dashRightLeg` (ypp:160,164,177,183).
- **Typed semantic values via `$<type>N`** — no `%union`; instead each token/nonterm declares its C++ field name directly, e.g. `%token < m_view_w > RCSS_PCOM_VIEW_WIDTH_NARROW` (ypp:96), consumed as `$< m_view_w >3` in actions (ypp:253).
- **Enums defined in the Builder header, not the parser** — `VIEW_WIDTH`, `VIEW_QUALITY`, `TEAM`, `EAR_MODE` (pcombuilder.h:44-67) are shared vocabulary between lexer tokens, grammar, and `Player`'s implementation.
- **`say` has two lexical forms**: quoted `(say "msg")` → `RCSS_PCOM_STR` stripped via `rcss::stripQuotes()` (ypp:229), and an unquoted-say fast path `RCSS_PCOM_UNQ_SAY` matched by a single flex regex `"\(say ...\)"` (player_command_tok.lpp:125-135) that skips full tokenization of the whole message for performance.
- **`floating_point_number` nonterminal** (ypp:438-446) normalizes both `RCSS_PCOM_INT` and `RCSS_PCOM_REAL` tokens to `double` — always route numeric args through it rather than raw tokens.
- **Multi-valued optional args use dedicated boolean/enum nonterminals**: `on_off` (ypp:382-390, only on/off), `boolean_value` (ypp:392-408, also accepts true/false — used only by `tackle`'s foul flag), `team_side` (ypp:410-426), `partial_complete` (ypp:428-436, for `ear`).

## Key Abstractions
- `rcss::pcom::Builder` (pcombuilder.h:70-106) — pure abstract interface, ~24 methods, one (or two overloads) per command: `dash`, `turn`, `turn_neck`, `change_focus`, `kick`, `long_kick`, `goalieCatch`, `say`, `sense_body`, `score`, `move`, `change_view` (2 overloads), `compression`, `bye`, `done`, `pointto`, `attentionto`, `tackle` (2 overloads), `clang`, `ear`, `synch_see`, `gaussian_see`.
- `rcss::pcom::BuilderErr` (pcombuilder.h:109-125) — exception type for builder-side misuse detection (out-of-context calls), decoupled from grammar validity.
- `rcss::pcom::Parser` (pcomparser.h:36-91) — subclass of `rcss::Parser`; `doParse()` (pcomparser.h:78-83) switches flex streams and invokes the generated `RCSS_PCOM_parse`. Convenience `Parser::parse(const char* msg)` (pcomparser.cpp:40-46) wraps the string in an `istringstream`.
- `rcss::pcom::Parser::Param` (pcomparser.h:44-72) — per-parse context: owns the `Lexer`, refs the `Parser` and the `Builder`.
- `RCSSPComLexer` (flex class, `%option yyclass="RCSSPComLexer"`, prefix `RCSSPCom`, player_command_tok.lpp:37-42) — holds `M_lexed_val` (a `Holder` with `m_int`/`m_double`/`m_str`, `STR_MAX`-bounded).

## Integration Points
- Constructed once per player: `M_parser( *this )` in `Player`'s member-init list (player.cpp:160) — `Player` itself is the `Builder&` passed in, confirming the direct-inheritance link.
- Invoked per datagram: `Player::parseMsg` (player.cpp:528-555) is called from the UDP receive path (see networking-io instructions) after `RemoteClient` delivers bytes; it null-terminates, logs via `Logger::instance().writePlayerLog(..., RECV)` (player.cpp:540), then calls `M_parser.parse(command)`.
- On parse failure (`!= 0` from `RCSS_PCOM_parse`, or `yyerror` reporting a syntax error to stderr, ypp:451-461), `Player` replies `(error illegal_command_form)` (player.cpp:552) and logs to stderr.

## Build & Test
- Generated files live under the CMake build dir, never committed: `player_command_parser.cpp/.hpp` (bison_target, src/CMakeLists.txt:2-6), `raw_player_command_tok.cpp` → renamed to `player_command_tok.cpp` via `fix_lexer_file.cmake` (src/CMakeLists.txt:13-22, replaces the include with `player_command_tok.h`). `add_flex_bison_dependency` (line 11) orders the two generators correctly since the lexer includes the parser's token header.
- `pcombuilder.cpp` and `pcomparser.cpp` are hand-written and compiled directly (src/CMakeLists.txt:44-45), alongside the two generated `.cpp` files (lines 88-89) into the `RCSSServer` target.
- No dedicated unit tests found for this grammar in `src/`; validate changes by building `RCSSServer` and manually sending a raw UDP command (e.g. via `rcssserver`'s player port) or exercising `Player::parseMsg` through an integration/client test.

## Logging
- Every raw incoming command is logged via `Logger::instance().writePlayerLog(M_stadium, *this, command, RECV)` (player.cpp:540) **before** parsing — so malformed input is still visible in player logs even on parse failure.
- Bison's default `yyerror` (ypp:451-455) prints the raw error string to `std::cerr` and is a no-op otherwise (grammar errors are not otherwise surfaced structurally); `Player::parseMsg` additionally prints `"Error parsing >" << command << "<"` to stderr (player.cpp:553).

## Important Notes
- **Adding a new command requires touching 4 files in lockstep**: (1) add a keyword rule in `player_command_tok.lpp` returning a new `RCSS_PCOM_*` token, (2) declare the token and add a grammar rule + `command` alternative in `player_command_parser.ypp`, (3) add the pure-virtual method to `rcss::pcom::Builder` (pcombuilder.h), (4) implement it in `Player` (player.cpp) — missing any step is a compile error (pure virtual) or silent lex failure (unmatched keyword falls through to the generic `[\-\_a-zA-Z0-9]+` STR rule, player_command_tok.lpp:105-113).
- **Two parallel parsers coexist.** `Player::parseCommand` (player.cpp:558, hand-written `strncmp`-based dispatcher, ~370 lines) is a **legacy fallback used only under `__CYGWIN__`** (player.cpp:544-549: `#ifndef __CYGWIN__ M_parser.parse(...) #else !parseCommand(...)`), because Cygwin's flex/bison historically had issues. Any grammar change must be mirrored in `parseCommand` if Cygwin support matters, or the two will diverge silently.
- Strings are fixed-size and bounds-checked: `RCSSPComLexer::Holder::STR_MAX` — both the plain and quoted string lex rules (player_command_tok.lpp:105,115) return `RCSS_PCOM_ERROR` if `yyleng > STR_MAX`, and manually null-pad the rest of the buffer.
- `left`/`right` keywords double as single-char aliases `l`/`r` (player_command_tok.lpp:87-89) and `partial`/`complete` alias to `p`/`c` (lines 90-92) — short forms save bandwidth in high-frequency traffic (e.g. `ear` team_side arg).
- The `clang_com` rule (ypp:338-342) only recognizes the negotiation form `(clang (ver min max))`; full CLang message bodies are handled by the separate coach-language grammar (see coach-language-clang instructions), not here.

## See Also
- [entities.instructions.md](./entities.instructions.md) — `Player` class, `parseMsg`/`parseCommand` consumer, `MPObject`/`RemoteClient`/`Listener` mixins.
- [networking-io.instructions.md](./networking-io.instructions.md) — how raw UDP bytes reach `Player::parseMsg`.
- [coach-language-clang.instructions.md](./coach-language-clang.instructions.md) — analogous bison/flex CLang grammar for the coach protocol, same CMake generation pattern.

---
Part of: [`rcssserver copilot-instructions.md`](../copilot-instructions.md)
