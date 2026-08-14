# Roadmap

Where this project is going, and what has to happen to get there.

## Next session — start here

Cleared on 2026-08-13: 1.13, 1.2 (`$?`), 1.9, `rotom expand`/`shrink` from 1.7,
and three bugs found along the way — 1.15, 1.16, 1.17. Remaining, in the order
they are worth doing:

1. **[1.3](#13-command-separators---)** — `;`, `&&`, `||`. Unblocked now that
   1.2 is done: `&&` is just "run the next segment if the last status was 0".
2. **[1.10](#110-cursor-movement-with-left-and-right-arrows)** — left/right
   arrow navigation. **Not small.** Read that section before starting it.
3. **[1.14](#114-backspace-counts-bytes-not-columns)** — backspace eats the
   prompt on non-ASCII input. Low priority on its own, but it is the same `len`
   confusion 1.10 untangles, so do it as part of 1.10.
4. **[1.1](#11-input-redirection-)** (`<`) and
   **[1.18](#118-redirection-is-ignored-inside-a-pipeline)** — both live in the
   redirection parser, so do them together.

More `rotom` subcommands can slot in any time —
[1.7](#17-my-own-custom-builtins--rotom) explains the pattern, and adding one
needs no new plumbing on either side.

## The end goal

A double-clickable Mac app, with its own icon in the Dock, that opens a terminal
window shaped like a Rotom Dex. Inside the shaped border is a real, working
terminal running my own shell.

That is **two programs**, not one:

| Component | What it is | Where it lives |
|---|---|---|
| The shell | Parses input, runs commands, handles pipes and redirection | `src/main.c` (done, being extended) |
| The terminal emulator | Draws the text, owns the window, handles the shape | New, not started |

They talk to each other over a PTY. `src/main.c` stays a plain stdin/stdout
program and never learns anything about the GUI.

---

## Current state

Verified by running the binary, not by reading the code.

**Working:**

- Builtins: `echo`, `exit`, `type`, `pwd`, `cd`, `complete`, `jobs`, `history`, `declare`
- Every builtin survives being called with no arguments (1.11)
- External programs via PATH lookup + `execv` — `grep`, `ls`, `wc`, `curl`, all of them
- Running a program by path — `./hello`, `/bin/echo`, `../x/y` (1.12)
- Command lines up to 1023 chars, matching history buffers
- Pipelines: `a | b | c`, multi-stage
- Output redirection: `>`, `1>`, `2>`, `>>`, `1>>`, `2>>`
- Background jobs with `&`, plus reaping
- Shell variables and `$VAR` expansion
- `$?` exit status — real codes, 127 for not found, 128+signal when killed (1.2)
- All nine builtins tab-complete, from one shared list (1.9)
- Errors go to stderr, so `2>` and pipes behave (1.15)
- `rotom expand` / `rotom shrink` — fullscreen the Dex window from the shell,
  over an OSC escape, with no GUI code in `main.c` (1.7)
- Double-quote handling
- Tab completion and history (`-r`, `-w`, `-a`), custom `termios` line editor
- Backspace in the line editor (`0x7F`, erases with `\b\x1b[K`)

**Not working yet** — each of these was tested and confirmed missing:

| Feature | Current behaviour |
|---|---|
| `<` input redirection | `wc -l < f` passes `<` to `wc` as an argument |
| `&&` and `\|\|` | Treated as literal text |
| `;` separator | Treated as literal text |
| Globbing (`*.md`) | Prints `*.md` |
| `~` outside `cd` | Prints `~` |
| Completion inside quotes | Word boundary ignores quotes, so `"My Doc<TAB>` fails |
| Left/right arrows | Ignored; the cursor is always at the end of the line |
| Backspace on non-ASCII | Erases one column per *byte*, so it eats the `$ ` prompt |
| Redirection inside a pipeline | `a \| b > f` passes `>` to `b` as an argument |
| `>` with no surrounding spaces | `ls >f` passes `>f` to `ls` as an argument |

---

## Phase 1 — Finish the shell

Work these in order; later ones lean on earlier ones. Each is written like a
CodeCrafters stage: the goal, what to do, and how to know it passed.

### 1.1 Input redirection `<`

**Goal:** `wc -l < README.md` reads the file as stdin.

**What to do:** Redirection is already parsed in the arg loop around
`src/main.c:1042` for `>`, `2>`, `>>`. Add a `<` case that opens the file
`O_RDONLY` and `dup2`s it onto fd 0. Mirror how the existing cases save and
restore the original fd.

**Passes when:**
```sh
wc -l < README.md      # a number, no "No such file" error
cat < README.md        # file contents
```

### 1.2 Exit status and `$?` — DONE (2026-08-13)

Built in five blocks, each verified before the next: `$?` expands at all →
external commands set it → not-found sets 127 → builtins set it → pipelines set
it. Splitting it that way meant every failure had exactly one possible cause.

**`waitpid`'s status is not an exit code.** It packs two facts into one int:
the exit code in the high byte, the terminating signal in the low byte. Storing
it directly gave `false` → 256 and `exit 42` → 10752 (42 × 256). The `W*` macros
are the accessors, and `decode_status()` now wraps them.

**`WIFEXITED` means "the program chose its own code", not "it succeeded".**
`false` exits normally with 1. The other case is death by signal, where nothing
was chosen and `WEXITSTATUS` is meaningless — reading it without checking
`WIFEXITED` first is the classic bug here.

**Signals are reported as `128 + signal`** because `$?` is one number covering
two kinds of ending. Without the offset, "killed by SIGINT" (2) and "exited 2"
would be indistinguishable. That puts signal deaths in 129–159: Ctrl-C is 130,
segfault 139. Not-found is 127 for the same reason — it means "no program ever
ran", so it cannot collide with a real code.

**Builtins report by return value, not by writing the global.** `run_stage()`
calls `run_builtin()` *inside a forked child*, which has its own copy of every
global — writing `last_status` there dies with the child. Only the exit status
crosses a fork. So `run_builtin` returns `int`, the parent stores it, and the
child passes it to `exit()`.

**Pipelines report the last stage only.** `false | true` is a success. Every
stage still has to be waited on, to reap it; only the final status is kept.
Background jobs deliberately set nothing — `cmd &` leaves `$?` at 0, reporting
that the fork worked.

**In `expand_var()`, the return value counts input characters consumed, not
output produced.** `$?` always returns 1 — one `?` — whether it expands to `0`
or `137`. The caller uses it to advance its scan position.

Two missing-return warnings turned up during this and both were real
(`decode_status`, then `is_builtin` in 1.9). Worth keeping warnings visible.

**Verified:** `true`→0, `false`→1, `exit 42`→42, `nosuchcmd`→127, SIGKILL→137,
Ctrl-C→130, `cd /nope`→1, `false | true`→0, `ls /nope | wc -l`→0.

### 1.3 Command separators `;`, `&&`, `||`

**Goal:** Chain commands on one line, with conditionals.

**What to do:** Depends on 1.2 — `&&` and `||` are defined by exit status.
Split the line into segments on these operators *before* the current parsing
runs, then run each segment through the existing path. `&&` runs the next
segment only on exit 0; `||` only on non-zero.

Watch out: `&&` must not be confused with the existing background `&` check at
`src/main.c:942`. Check for `&&` first.

**Passes when:**
```sh
echo a ; echo b              # a then b
true && echo yes             # yes
false && echo no             # nothing
false || echo fallback       # fallback
```

### 1.4 Tilde expansion

**Goal:** `~` works everywhere, not just in `cd`.

**What to do:** `cd` already special-cases `~` at `src/main.c:363`. Pull that
out into a general expansion applied to every token during parsing. Handle bare
`~` and the `~/path` prefix. Leave a mid-token `~` alone.

**Passes when:**
```sh
echo ~           # /Users/botboy
ls ~/Documents   # listing
echo a~b         # a~b, unchanged
```

### 1.5 Globbing

**Goal:** `*.md` expands to matching filenames.

**What to do:** The biggest of the five. Any token containing `*` or `?` gets
replaced by the sorted list of matches, which means one token can become many —
so the arg array has to grow. POSIX `glob(3)` from `<glob.h>` does the matching;
writing the matcher by hand is optional extra credit. No matches means the token
stays literal, which is what bash does.

**Passes when:**
```sh
echo *.md        # README.md ROADMAP.md
ls src/*.c       # src/main.c
echo *.nothing   # *.nothing, unchanged
```

### 1.6 Signal handling

**Goal:** Ctrl-C kills the running command, not the shell.

**What to do:** Currently Ctrl-C likely kills everything. The shell should
ignore `SIGINT` itself and let the foreground child receive it. Needs process
groups — `setpgid` in the child, `tcsetpgrp` to hand it the terminal.

**Passes when:** `sleep 10`, then Ctrl-C — the sleep dies, the prompt returns,
the shell is still alive.

### 1.7 My own custom builtins — `rotom`

**Goal:** A `rotom` command with subcommands. First two — `rotom expand` goes
fullscreen, `rotom shrink` comes back — are built and working end to end in the
app. Further subcommands are open-ended; the plumbing below is reusable.

**Builtin, not an external program.** The general rule is that a command only
*needs* to be a builtin if it changes shell state — cwd, variables, history —
and anything else can be a script on PATH. But the deciding factor here is
**2.4 packaging**: an external `rotom` would have to be found on `PATH`, so the
`.app` bundle would need to carry a second binary and arrange a `PATH` pointing
at it on a machine we have never seen. A builtin ships inside `build/shell`
already. Bundling one binary is hard enough.

Do **1.9 first** — done — so adding `rotom` is one string in one array.

**Structure:** one line in `run_builtin`'s chain dispatching to
`run_rotom(args, nargs)`, which does its own `args[1]` dispatch and returns an
exit status like every other builtin now does. A separate `src/rotom.c` is fine,
but `file(GLOB_RECURSE ...)` will not see it until `cmake -B build -S .` is
re-run (0.3s).

#### How `expand` / `shrink` reach the window

`main.c` must not know a window exists. So it does not call Electron — it
**prints an escape sequence to stdout** and the terminal emulator acts on it.
This is how real terminals do it (iTerm2's proprietary codes, xterm's
window-title sequence), and it keeps the architecture rule intact.

```
rotom expand          writes \x1b]7777;expand\x07 to stdout
   ↓ PTY
xterm.js              parser.registerOscHandler(7777, ...)
   ↓ IPC
main.js               win.setFullScreen(true)
```

An **OSC** (Operating System Command) is `\x1b]`, a number, `;`, a payload,
`\x07`. Pick a high private number — 0–2 are window title, 52 clipboard, 1337
is iTerm's.

**Explicit, not toggle.** `ui:toggle-fullscreen` already exists at
`gui/main.js:62` and flips the current state — so `rotom expand` while already
fullscreen would *shrink*. Needs a new channel calling `setFullScreen(true)` /
`(false)`. The dot buttons keep using the toggle.

**The shell stays stateless.** It never tracks whether the window is expanded;
it cannot and should not. The GUI owns that, and already broadcasts it via
`ui:mode`. That is why these are two commands and not one `rotom toggle`.

**Exit status is always 0.** The shell writes bytes into a pipe and has no way
to learn whether anything was listening. Under Terminal.app the sequence is
silently swallowed — correct behaviour, not a bug to work around.

**The `main.c` half is testable with no GUI:**

```sh
rotom expand | xxd     # 1b 5d 37 37 37 37 3b ... 07
```

If the bytes are right, that side is done. The xterm.js handler, the preload
function and the `main.js` IPC are GUI plumbing.

#### What was built

`run_rotom(char **args, int nargs)` sits above `run_builtin` in `main.c`, which
dispatches one line to it. `"rotom"` was added to the shared builtins array —
one string, and `type rotom` plus tab completion came free. That is 1.9 paying
for itself immediately.

Behaviour: `expand` / `shrink` print their sequence and return 0; bare `rotom`
prints usage and returns 1; anything else names the offending word and returns
1. Both messages go to stderr.

**The unknown-subcommand branch is not politeness, it is the missing `return`.**
Without it `rotom expnad` falls off the end of a non-void function — the third
time that mistake appeared in one session. A typo would silently "succeed" with
a garbage `$?`.

**The escape takes no newline; the error messages do.** The sequence is bytes
for a parser, where a stray `\n` scrolls the screen and breaks the byte count.
The errors are lines for a person. Easy to get backwards.

No `fflush` is needed — `setbuf(stdout, NULL)` at `src/main.c:817` makes stdout
unbuffered, which is also why the prompt shows without a newline.

**Verified:** `rotom expand | xxd` → `1b5d 3737 3737 3b65 7870 616e 6407`,
exactly 14 bytes, same for `shrink`; `$?` of 0 / 1 / 1 across the three paths;
messages on stderr; and the real thing — typed in the app, the window expands
and shrinks.

#### The GUI side, as built

Eleven lines across three files:

- `renderer.js` — `term.parser.registerOscHandler(7777, ...)`, dispatching on
  the payload and returning `true` to consume the sequence.
- `preload.js` — `setFullscreen(on)` added to the `ui` bridge.
- `main.js` — `ipcMain.on('ui:fullscreen', ...)` → `win.setFullScreen(!!on)`,
  plus the matching `removeAllListeners` on close, like every other channel.

**Half the sequence is a standard, half is invented.** `ESC ]` … `BEL` is OSC,
defined in ECMA-48 and understood by every terminal: "this is a message for the
terminal program, not text to draw." The number `7777` and the words
`expand`/`shrink` are ours. Only a few numbers are claimed by convention — 0–2
window title, 52 clipboard, 1337 iTerm2 — so an unused high one is free to take,
much like a port. A terminal that does not know it discards the whole sequence,
which is why `rotom expand` does nothing visible under Terminal.app instead of
printing junk.

**Nothing in C calls anything.** `term.write()` feeds a state machine: it sees
`\x1b`, then `]`, collects the number until `;`, collects the payload until
BEL, then looks 7777 up in a table of registered handlers and calls ours. The
shell writes to stdout blind, exactly like `printf("hi")`.

**Why not scan for the sequence in `main.js`**, where the PTY data already
arrives, and skip the renderer round trip? Because PTY reads land in arbitrary
chunks — `\x1b]7777;exp` can arrive in one `onData` and `and\x07` in the next.
A hand-rolled `indexOf` passes every test you would think to write and then
fails under load. The xterm.js parser is a state machine across `write()` calls
and already handles it, and it strips what it consumes so nothing renders.

**Adding subcommands needs no new plumbing.** The payload is a plain string, so
`rotom theme dark` later is one more `printf` in C and one more branch in the
handler — no new OSC number, no new IPC channel.

Later ideas: `dex` banner, project shortcuts. Anything that only prints text has
none of the above complexity.

### 1.8 Quote-aware tab completion

**Goal:** `echo "My Doc<TAB>` completes to `echo "My Document.txt`.

**What to do:** `src/main.c:576` finds the word boundary by scanning for the
last space anywhere in the line:

```c
for (int i = 0; i < len; i++) {
    if (buf[i] == ' ') word_start = i + 1;
}
```

It has no idea quotes exist, so for `echo "My Doc` it lands after `My ` and
tries to complete `Doc`, which matches nothing. Track quote state while
scanning: a space inside a quote does not start a new word, and the word begins
after the opening quote.

Completing inside quotes is correct behaviour — bash does it, and it is how you
complete paths containing spaces. The bug is only the boundary.

**Passes when:**
```sh
touch "My Document.txt"
echo "My Doc<TAB>     # completes to My Document.txt
echo Notes<TAB>       # still works unquoted
```

### 1.9 Completion knows all the builtins — DONE (2026-08-13)

One file-scope `static const char *builtins[]` now feeds both `is_builtin()` and
the completion scan. Adding a builtin is one string in one place, which is the
point — the copy-the-missing-seven fix would have left the same trap for `rotom`
to fall into.

The completion site needed **no edit at all**. Its local
`const char builtins[][24] = {"echo", "exit"};` was *shadowing* the new global;
deleting that one line was the whole fix, since `sizeof(builtins)/sizeof(...)`
then measures the global and gives nine.

Rewriting `is_builtin()` from a `||` chain to a loop introduced a missing
`return false` — reachable on **every external command**, since that is exactly
the case where nothing matches. It happened to work, which is worse than
failing; the compiler warning is what caught it.

**Verified:** `decl`→`declare`, `compl`→`complete`, `jobs`, `cd`, `his`→
`history`. Note `de`, `pw` and `com` correctly *beep* rather than complete —
`de` also matches `/usr/bin/defaults`, `pw` matches `pwpolicy`, so the common
prefix adds nothing. Bash behaves the same.

### 1.10 Cursor movement with left and right arrows

**Goal:** Left/right arrows move through the line I have typed, and typing
inserts at the cursor instead of at the end.

**This is the biggest change to `read_line()` so far — do not start it thinking
it is a two-line fix.** Detecting the arrows genuinely is easy: the escape block
at `src/main.c:775` already reads the 3-byte sequences, so `[C` (right) and
`[D` (left) are two more cases next to the existing `[A` (779) and `[B` (790). Moving the
cursor on screen is also easy — `\b` or `\x1b[D` left, `\x1b[C` right.

The hard part is that **the entire line editor currently assumes you are always
at the end of the line.** `len` doubles as both "how long the line is" and
"where the cursor is". Splitting those apart touches everything:

- A new `cursor` variable, separate from `len`.
- Typing mid-line must `memmove` the tail right, then redraw from the cursor on.
- Backspace mid-line must `memmove` the tail left, then redraw — `len--` is only
  enough because deletion currently happens at the end.
- After any redraw, the cursor has to be put back where it belongs, since
  printing the tail leaves it at the end of the line.
- Tab completion inserts at `len`; it will need to insert at `cursor`.
- The history redraws in the `[A` / `[B` blocks (from `src/main.c:779`) set
  `len` and assume the cursor lands at the end. They need to set `cursor` too.

**Decide up front whether `cursor` counts bytes or columns.** Every bug in
[1.14](#114-backspace-counts-bytes-not-columns) comes from `len` silently
meaning both. Getting this right here fixes the non-ASCII backspace for free;
getting it wrong bakes the same confusion in one level deeper.

Suggested order: get left/right moving with no editing first (movement only,
typing still appends at the end — wrong but harmless), then make insertion
cursor-aware, then deletion, then completion.

**Passes when:**
```
type "echo hello", press Left 5 times, type "X"  ->  "echo Xhello"
press Right 3 times, backspace                   ->  deletes mid-line correctly
press Left to the start, backspace               ->  does nothing, prompt intact
```

### 1.11 Bare `cd` segfaults — DONE (2026-08-10)

`cd`, `complete` and `type` all read `args[1]` before checking `nargs`. The
tokenizer NULL-terminates the array, so with no argument `args[1]` was `NULL`
and `strcmp` dereferenced it — SIGSEGV, exit 139, taking the whole shell down
(builtins run in the shell's own process, not a fork).

`complete` needed the guard on *every* branch of its `else if` chain, not just
the first: falling past a guarded `if` lands on an unguarded `else if`.

`history` and `declare` were already correct and are the reference pattern —
`declare`'s `nargs >= 3 && strcmp(...)` is the better of the two, because `&&`
short-circuits before the dereference.

All eight builtins now exit 0 with no arguments, and `cd` with no argument goes
to `$HOME`. Remaining hole: `getenv("HOME")` itself can be NULL — see 1.13.

### 1.12 Run a program by path — DONE (2026-08-10)

`./hello`, `/bin/echo` and `../x/y` all failed with "command not found".
`find_in_path()` only ever built `PATH_dir + "/" + command`, so a name that was
already a path had nothing to match.

The rule: **if the command contains a `/`, do no PATH search at all.** It is
already a pathname, and the kernel resolves relative paths against the cwd.
Adding `"."` to PATH would have been the wrong fix — it would not help
`/bin/echo`, and it would make bare `hello` run a binary from the cwd, which
bash deliberately refuses.

Two traps this hid:

- **The fall-through mattered.** Before the fix, `./ls` in a directory with no
  `ls` silently ran `/bin/ls`, because the loop built `/bin/./ls` and `.` is a
  real directory entry meaning "here". So a slash command must `return NULL`,
  never continue into the loop. `./hello` only *looked* correct — no PATH
  directory happens to contain a file named `hello`.
- **`access(X_OK)` is true for directories**, since execute on a directory means
  "searchable". Bare `.` resolved to `/bin/.` and reached `execv`. Both branches
  need `stat` + `S_ISREG` to confirm it is a regular file.

`strdup` on the returned path is required — callers `free()` it, and `command`
is `args[0]`, freed again by `free_args`. Returning it directly is a double free.

**Verified:** `ls`, `wc`, `./hello`, `./run.sh`, `/bin/echo` work; `./ls`, `.`,
`..` and a real directory found on PATH all report "command not found".

### 1.13 `getenv("HOME")` can return NULL — DONE (2026-08-13)

Both `cd` paths now check before `chdir()` and print `cd: HOME not set`.

**There was a third site, and it was the dangerous one.** Tab completion did
`strdup(getenv("PATH"))` with no check, so pressing Tab with `PATH` unset
segfaulted the whole shell. `strdup(NULL)` crashes on macOS — confirmed, exit
139. `find_in_path()` had guarded this correctly all along; the completion path
just never copied the guard.

The first attempt guarded the wrong thing:

```c
char *path_copy = strdup(getenv("PATH"));   // crashes here
if (path_copy != NULL) {                    // never reached
```

Check what `getenv` returned, then copy. Completion of builtins is left outside
the guard, so `ec<TAB>` still works with no `PATH` — the right degradation.

**Verified through a PTY:** `env -i ./build/shell`, then `cd`, `cd ~` and Tab —
error messages, no crash.

### 1.14 Backspace counts bytes, not columns

**Goal:** Backspace erases one character per keypress, whatever the character.

**What to do:** `read_line()` reads one *byte* at a time and increments `len`
per byte, but backspace emits one `\b\x1b[K` per `len` step — and `\b` moves one
*column*. For ASCII those are the same number, which is why the `len > 0` guard
looks correct. For anything else they diverge, and the extra `\b`s walk left
through the `$ ` prompt and erase it.

Confirmed through a PTY:

```
é     2 bytes, 1 column  -> 2 backspaces emitted, 1 column too far
emoji 4 bytes, 2 columns -> 4 backspaces emitted, 2 columns too far
ab    2 bytes, 2 columns -> 2 backspaces, correct
```

This is the same conflation 1.10 has to resolve, so decide there whether the
cursor is a byte offset or a column offset and this comes with it. Line wrapping
is the other half of the same problem — `\b` at column 0 does not climb to the
previous row.

**Passes when:** typing `é` or an emoji and holding backspace leaves `$ ` intact.

### 1.15 Errors went to stdout, not stderr — DONE (2026-08-13)

Every diagnostic in the shell used `printf`; only one site used `stderr`. That
is not cosmetic once `2>` and pipes exist: `cd /nope 2>/dev/null` still printed,
and `cd /nope | wc -l` counted the error as **data**.

Nine sites converted across `cd`, `complete`, `declare`, `type` and the
not-found path. Results — `echo`, `pwd`, `type`, `history` output — stay on
stdout and must remain pipeable.

Consequence worth remembering: stdout is block-buffered into a pipe while
stderr is unbuffered, so once they are on different streams the *order* can
change under redirection. Normal shell behaviour, not a defect.

### 1.16 `cd` always said "No such file or directory" — DONE (2026-08-13)

The message was hardcoded, so `cd src/main.c` claimed the file did not exist.
`chdir` sets `errno`; `strerror(errno)` is the whole fix, and it covers cases
neither of us enumerated (`ELOOP`, `ENAMETOOLONG`).

Measured on this Mac: missing path → `ENOENT` (2), a file → `ENOTDIR` (20),
also `ENOTDIR` when a *middle* component is a file, unreadable dir → `EACCES`
(13). Never switch on the numbers — they are not portable, and `strerror`
already holds the table.

`errno` is only meaningful **after a call has failed**, and any libc call can
overwrite it, so read it immediately inside the failure branch.

**Verified:** `cd src/main.c` → `Not a directory`, `cd /nope` →
`No such file or directory`, `cd /var/root` → `Permission denied`.

### 1.17 Tab on an empty line overflowed `match[]` — DONE (2026-08-13)

Pressing Tab on an empty line segfaulted the shell.

`strncmp(name, buf, 0)` compares *zero* characters, so it returns 0 — a match —
for everything. With an empty line that meant nine builtins plus **every
executable on PATH**, 1481 of them here, all copied into `char match[64][256]`
by an unchecked `strcpy(match[count++], ...)`. Writing slot 64 onward walks off
the end of a stack array.

Fixed by bounding `count < 64` before every write — there are four such sites,
and the PATH scan is the one that actually overflowed. Raising the limit would
not have been a fix; 1481 does not fit in any sane array.

Same shape as 1.11 and 1.13: a value used without first asking whether it is
valid.

**Verified under AddressSanitizer through a PTY** — empty Tab, `ec<TAB>` and
`echo <TAB>` all clean. ASAN is the tool that finds this class; normal runs sail
past it.

### 1.18 Redirection is ignored inside a pipeline

**Goal:** `ls /tmp | wc -l > out.txt` writes to the file.

**Current behaviour:** `wc: >: open: No such file or directory` — `>` and the
filename are passed to `wc` as arguments.

**Why:** the pipeline branch returns at `src/main.c:977`, and all the `>`
parsing lives at `src/main.c:1042` — *after* it. A pipeline never reaches the
redirection code. Plain `ls /tmp > /dev/null` works; only the combination fails.

**What to do:** redirection has to be parsed **per stage**, before the stages
are launched, rather than once for the whole line. Same code and same shape as
[1.1](#11-input-redirection-), so do the two together.

Related and separate: `>` is only recognised as its own token, so `ls >f` passes
`>f` to `ls`. Real shells accept both spellings. Fixing the tokenizer to split
`>` off a word covers `<`, `>>` and `2>` at the same time.

---

## Phase 2 — The terminal emulator

**2.1 is done.** It was deliberately built before Phase 1 finished: it depends
on none of those tasks, and it retires the one genuine unknown in the project —
whether the hand-written raw-mode `termios` line editor survives running under a
PTY. It does. Tab completion, arrow-key history and resizing were all verified
working through the GUI, so no `main.c` rewrite is needed.

```
Rotom Dex window  →  xterm.js  ←→  node-pty  ←→  ./build/shell
   (Electron)        (draws)        (PTY)        (main.c)
```

**Stack:**

- **Electron** — the app shell. Chosen over Tauri because `node-pty` is far more
  mature than Rust's `portable-pty`, and over Godot because Godot would mean
  writing a terminal emulator from scratch.
- **xterm.js** — the terminal widget. Renders text, handles ANSI escapes and the
  cursor. Same component VS Code uses.
- **node-pty** — the missing link. xterm.js only draws; the PTY is what actually
  connects it to the shell process. Without it, raw-mode `termios` line editing
  does not work at all.

### 2.1 Plain rectangular terminal — DONE

Built in `gui/`. Electron 43, xterm.js 6, node-pty 1.1.

```
gui/
  package.json   deps and the `start` / `rebuild` scripts
  main.js        Electron main process — owns the window, spawns the PTY
  preload.js     contextBridge — the page gets 3 functions, no Node access
  index.html     xterm.js container
  renderer.js    draws the terminal, forwards keystrokes
```

Run it:

```sh
cd gui
npm start
```

Verified working: typing, tab completion, up/down arrow history, and window
resize reflow.

**Gotcha worth remembering:** `node-pty` is a native module and must be compiled
against Electron's ABI, not Node's. If it ever throws `NODE_MODULE_VERSION`
errors after an `npm install` or an Electron upgrade, run `npm run rebuild`.

### 2.1a Fix: no prompt on startup — DONE (2026-08-10)

**Was:** `npm start` opened a window with nothing in it. The first `$ ` never
appeared; everything after the first Enter worked.

**Cause — a startup race.** `main.js` called `pty.spawn()` immediately after
`win.loadFile()`, but `loadFile` is asynchronous. The page had not loaded,
`renderer.js` had not run, and no `pty:data` listener existed yet. The shell
printed its first `$ `, `webContents.send()` delivered it to a page that was not
listening, and the bytes were dropped.

**Fix — a `ready` handshake.** `renderer.js` attaches its `onData` handler,
fits, then calls `window.pty.ready(cols, rows)`; `main.js` spawns the PTY inside
`ipcMain.once('pty:ready', ...)`.

Waiting on `did-finish-load` alone would have traded one race for another:
`renderer.js` sends its initial `pty:resize` while it runs, which lands today
only because the handlers register synchronously. Delaying the spawn without
carrying the size across would drop it and leave the shell at 80x24. Passing
cols/rows through the ready message fixes both.

`shell` is null until ready, so the input/resize/kill paths use `shell?.`.

**Verified** by instrumenting the main process: `ready received, spawned at
93 x 30` (the fitted size, not the hardcoded default), then `first chunk "$ "`
delivered with `loading=false`.

### 2.1b Can backspace eat the prompt? — ANSWERED (2026-08-10)

**Yes, but not for either reason guessed here.** Neither line wrapping nor
history recall was the cause. `read_line()` counts *bytes* while `\b` moves
*columns*, so any non-ASCII character emits more backspaces than it occupies and
the surplus walks through the prompt.

Tracked as [1.14](#114-backspace-counts-bytes-not-columns). Line wrapping is
still a real and separate defect for the same underlying reason, but it corrupts
the display rather than eating the prompt.

### 2.2 The Rotom Dex shape — SCAFFOLD DONE, ART PENDING

The window stays a rectangle — the parts outside the Dex shape are just
transparent.

**Note on the GUI source:** `gui/*.js` and `gui/index.html` are deliberately
written with no comments and no blank lines. Anything worth explaining goes
here instead.

**Decided:**

- **Two layouts, one window.** Rotom mode is a fixed-size window; fullscreen
  gets its own design later. `main.js` sends a `ui:mode` message on
  `enter-full-screen` / `leave-full-screen`; the page sets `data-mode` on
  `<body>` and CSS keys off it. Both modes currently render the same frame, so
  adding the fullscreen look is a new CSS block, not a restructure.
- **Fixed size, no resizing.** The art is only ever drawn at a whole-number
  scale — scaling pixel art by a fraction is what makes it look mushy.
  Fullscreen is the way to get more room.
- **`layout.js` is the single source of truth** for geometry, required by
  `main.js` and loaded as a plain `<script>` by the page. Art size, scale and
  the screen-hole rectangle live there in *art pixels*. All values are
  placeholders until the real PNG lands.
- **The terminal covers the screen hole rather than showing through it.** Keeps
  the z-order trivial. `.frame` is `-webkit-app-region: drag` (the only way to
  move a frameless window) and `#terminal` is `no-drag`, or every click-drag to
  select text would move the window.
- **Window controls are ours to draw.** `frame: false` means no red/green
  buttons, so the page provides them: `ui:close` and `ui:toggle-fullscreen` go
  over IPC to `win.close()` and `win.setFullScreen()`. Their hit rectangles
  live in `layout.js` in art pixels, like everything else. They are drawn into
  the art as the two round face dots: left = fullscreen, right = close, with a
  hover highlight from CSS.

**Measured, not assumed** (Electron 43, macOS):

- `resizable: false` does **not** disable fullscreen. `isFullScreenable()` is
  still `true` and the round trip works.
- **A custom button must be `-webkit-app-region: no-drag`.** Anything inside the
  drag region swallows clicks instead of firing them. Verified end to end by
  synthesising real mouse events with `webContents.sendInputEvent` — both
  buttons toggle fullscreen and close the window.
- **macOS restores the window size on its own after fullscreen**, provided the
  window fits the display. An earlier note here claimed the opposite and said
  `leave-full-screen` had to call `setSize()`; that was wrong. The 800x640 →
  800x522 shrink that prompted it happened only because the test display was
  800x600 — the window was taller than the screen, so it came back clamped, and
  `setSize()` could not have fixed that either. No `setSize()` call is needed.
- **The window must fit inside the display's work area.** This is a real
  constraint on `art.h * scale`: a window taller than the screen gets clamped
  on fullscreen exit, which breaks whole-number scaling. Check the chosen art
  size against the smallest display it has to run on.
- `show: false` in a `BrowserWindow` made `loadFile()` fail with `ERR_FAILED
  (-2)` and then SIGTRAP the process. Avoided; not investigated further.
- `loadURL('data:text/html,...')` is blocked for top-level navigation.

**The art is generated by `gui/rotom-gen.js`** (run: `cd gui &&
./node_modules/.bin/electron rotom-gen.js` → overwrites `gui/rotom.png`). It
does not draw anything: it rebuilds the hand-made `rotomdex_window_1.png` and
edits the screen area. A from-scratch generated Rotom was tried and rejected —
the hand-made one is the design.

The original PNG *looks* like pixel art but was 1024x825 with **30,336 unique
colours**, a fractional 12.8 x 12.89 px block size and 13,452 semi-transparent
pixels — compression noise that `image-rendering: pixelated` would have
magnified rather than hidden. The pipeline:

1. Fit the pixel grid with difference-profile scoring — columns peaked at 80,
   rows at 64.
2. Resample one modal colour per cell from the middle ~44% of each cell, so
   antialiased edges are ignored.
3. Merge into a 10-colour palette. 1.1 KB, down from 388 KB.
4. Paint the screen rectangle with the terminal's own background (`#1e1e2e`),
   preserving eyes and mouth where they overlap, with **rounded corners**.
5. Recolour any remaining green to the same background.

Because the art's screen and the terminal share one colour, the join between
them is invisible and a 1px misalignment cannot show.

**Geometry** (`gui/layout.js`, art pixels): art 80x64, painted screen
24,31 32x20, text area 25,33 30x17, buttons on the two arm circles. The text
area is deliberately inset 1 cell inside the painted screen so the terminal's
square corners sit within the art's rounded ones — that is why no CSS
`border-radius` is needed and why no character can be clipped by the curve.
The screen's top two rows carry the eye and mouth cutouts and hold no text.

**Scale is computed at launch, not hardcoded:** the biggest whole number where
`art * scale` fits the work area of the **largest connected display**, then the
window is centred on it. Using `getPrimaryDisplay()` was a real bug — this Mac
reports work areas of 800x575, 1920x1055 and 1680x927, and picking the primary
gave a 640x512 window. Choosing the largest gives 1280x1024 with a 55x17
terminal, exactly double the linear size.

Verified with synthesized mouse clicks: both dot buttons work, fullscreen
toggles, size is restored exactly on exit, close fires `closed`. In fullscreen
the frame stretches, so anything computing button positions must derive them
from the *current* window size, as `applyLayout()` does.

### 2.3 Theming

Custom font, background, xterm.js colour scheme, and the Dex screen styled as
the terminal viewport.

### 2.4 Packaging

`electron-builder` or Electron Forge → a `.app` with an `.icns` icon, installed
to `/Applications`, double-clickable, with a proper Dock icon.

---

## Open questions

- ~~Does `read_line()` behave correctly under a PTY?~~ **Answered by 2.1: yes.**
  Raw mode, tab completion and arrow escape sequences all work unchanged.
- Should the shell ship inside the `.app` bundle, or stay a separate binary the
  app launches? Bundling makes it portable to another Mac. `main.js` currently
  hardcodes `../build/shell`, which only works from the repo — 2.4 has to solve
  this.
- Backspace only accepts `0x7F`, not `0x08`. Correct for Terminal.app, iTerm2
  and xterm.js as configured today, but worth remembering if backspace ever
  misbehaves in the GUI or over SSH.
- `CMAKE_C_STANDARD 23` currently emits `-std=gnu2x` on AppleClang 14, so it is
  not true C23. Fine for now; worth knowing before relying on a C23-only feature.
- `find_in_path()` returns NULL for three different failures — does not exist,
  is not a regular file, is not executable — and the caller prints "command not
  found" for all three. `./run.sh` without `+x` should say "permission denied".
  Needs a wider return contract, e.g. reporting through `errno`.
- The line editor caps input at 1023 chars (`input[1024]`), with the history
  buffers sized to match. Real shells allocate dynamically; revisit only if the
  fixed cap actually gets in the way.
- ASAN is worth keeping in the loop for line-editor work — it caught the
  `buf[len]` overflow that normal runs sailed straight past, and confirmed the
  1.17 fix: `cc -std=gnu2x -fsanitize=address -g -o shell_asan src/main.c`
- Should `CMakeLists.txt` carry `-Wall -Werror`? Two real bugs in 1.2 and 1.9
  were caught only by the default missing-return warning, and 1.10 will generate
  more of that class. The cost is that pre-existing warnings have to be cleared
  first.
- Environment variables do not expand — `echo $USER` prints nothing, because
  `find_var()` only searches the shell's own `variables[]` array. Whether the
  shell should read the environment is a real question, not on the roadmap yet.
- Line numbers in this file go stale as `main.c` grows. They were re-checked on
  2026-08-13; treat anything older as approximate and grep for the code instead.
