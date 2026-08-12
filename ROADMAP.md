# Roadmap

Where this project is going, and what has to happen to get there.

## Next session — start here

Cleared on 2026-08-10: 2.1a, 1.11, and 1.12 (`./hello`). Remaining, in the order
they are worth doing:

1. **[1.13](#113-getenvhome-can-return-null)** — `getenv("HOME")` unchecked in
   both `cd` paths. Tiny, and it is the same crash shape as 1.11.
2. **[1.10](#110-cursor-movement-with-left-and-right-arrows)** — left/right
   arrow navigation. **Not small.** Read that section before starting it.
3. **[1.14](#114-backspace-counts-bytes-not-columns)** — backspace eats the
   prompt on non-ASCII input. Low priority on its own, but it is the same `len`
   confusion 1.10 untangles, so do it as part of 1.10.

If you would rather build features than clean up, the Phase 1 sequence starts at
[1.1](#11-input-redirection-) (`<`) and [1.2](#12-exit-status-and-) (`$?`);
[1.3](#13-command-separators---) depends on 1.2.

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
- Double-quote handling
- Tab completion and history (`-r`, `-w`, `-a`), custom `termios` line editor
- Backspace in the line editor (`0x7F`, erases with `\b\x1b[K`)

**Not working yet** — each of these was tested and confirmed missing:

| Feature | Current behaviour |
|---|---|
| `<` input redirection | `wc -l < f` passes `<` to `wc` as an argument |
| `&&` and `\|\|` | Treated as literal text |
| `;` separator | Treated as literal text |
| `$?` exit status | Prints `$?` |
| Globbing (`*.md`) | Prints `*.md` |
| `~` outside `cd` | Prints `~` |
| Completion inside quotes | Word boundary ignores quotes, so `"My Doc<TAB>` fails |
| Completion of most builtins | Only `echo` and `exit` are offered, out of nine |
| Left/right arrows | Ignored; the cursor is always at the end of the line |
| Backspace on non-ASCII | Erases one column per *byte*, so it eats the `$ ` prompt |
| `cd` with `HOME` unset | `chdir(NULL)` — segfault |

---

## Phase 1 — Finish the shell

Work these in order; later ones lean on earlier ones. Each is written like a
CodeCrafters stage: the goal, what to do, and how to know it passed.

### 1.1 Input redirection `<`

**Goal:** `wc -l < README.md` reads the file as stdin.

**What to do:** Redirection is already parsed in the arg loop around
`src/main.c:938` for `>`, `2>`, `>>`. Add a `<` case that opens the file
`O_RDONLY` and `dup2`s it onto fd 0. Mirror how the existing cases save and
restore the original fd.

**Passes when:**
```sh
wc -l < README.md      # a number, no "No such file" error
cat < README.md        # file contents
```

### 1.2 Exit status and `$?`

**Goal:** `$?` expands to the previous command's exit code.

**What to do:** `waitpid` already receives `&status` at `src/main.c:1091`. Store
`WEXITSTATUS(status)` in a global after each command, and special-case `?` in
`expand_var()` (`src/main.c:229`) before the normal variable lookup, since `?`
isn't a valid variable name.

**Passes when:**
```sh
true;  echo $?    # 0
false; echo $?    # 1
ls /nope; echo $? # non-zero
```

### 1.3 Command separators `;`, `&&`, `||`

**Goal:** Chain commands on one line, with conditionals.

**What to do:** Depends on 1.2 — `&&` and `||` are defined by exit status.
Split the line into segments on these operators *before* the current parsing
runs, then run each segment through the existing path. `&&` runs the next
segment only on exit 0; `||` only on non-zero.

Watch out: `&&` must not be confused with the existing background `&` check at
`src/main.c:842`. Check for `&&` first.

**Passes when:**
```sh
echo a ; echo b              # a then b
true && echo yes             # yes
false && echo no             # nothing
false || echo fallback       # fallback
```

### 1.4 Tilde expansion

**Goal:** `~` works everywhere, not just in `cd`.

**What to do:** `cd` already special-cases `~` at `src/main.c:298`. Pull that
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

### 1.7 My own custom builtins

**Goal:** Commands that exist only in my shell.

**What to do:** Add to `is_builtin()` (`src/main.c:15`) and `run_builtin()`
(`src/main.c:275`). Ideas: `dex` for a themed prompt/banner, project shortcuts,
whatever is actually useful day to day.

A command only needs to be a builtin if it changes shell state — cwd, variables,
history. Anything else can just be a script on PATH.

### 1.8 Quote-aware tab completion

**Goal:** `echo "My Doc<TAB>` completes to `echo "My Document.txt`.

**What to do:** `src/main.c:493` finds the word boundary by scanning for the
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

### 1.9 Completion knows all the builtins

**Goal:** Tab-completing `cd`, `pwd`, `type` and the rest works.

**What to do:** `src/main.c:502` hardcodes a second, shorter list:

```c
const char builtins[][24] = {"echo", "exit"};
```

Nine builtins exist in `is_builtin()` but only two are offered. The quick fix is
to add the missing seven. The better fix is to have one list that both
`is_builtin()` and completion read from, so the two can never drift apart again.

**Passes when:** `de<TAB>` completes to `declare`, and adding a builtin in 1.7
makes it completable without touching a second list.

### 1.10 Cursor movement with left and right arrows

**Goal:** Left/right arrows move through the line I have typed, and typing
inserts at the cursor instead of at the end.

**This is the biggest change to `read_line()` so far — do not start it thinking
it is a two-line fix.** Detecting the arrows genuinely is easy: the escape block
at `src/main.c:670` already reads the 3-byte sequences, so `[C` (right) and
`[D` (left) are two more cases next to the existing `[A` and `[B`. Moving the
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
- The history redraws at `src/main.c:721` and `732` set `len` and assume the
  cursor lands at the end. They need to set `cursor` too.

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

### 1.13 `getenv("HOME")` can return NULL

**Goal:** `cd` with `HOME` unset prints an error instead of crashing.

**What to do:** Both `cd` paths — the bare-`cd` case added in 1.11 and the `~`
case beside it — pass `getenv("HOME")` to `chdir()` without checking it.
`getenv` returns NULL when the variable is not set, and `chdir(NULL)` segfaults.
Same crash shape as 1.11, one level down.

Bash prints `cd: HOME not set` and returns non-zero.

**Passes when:**
```sh
env -i ./build/shell    # then type: cd     -> error message, no crash
```

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
  live in `layout.js` in art pixels, like everything else. Currently two
  placeholder circles, to be replaced by something built into the Rotom art.

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

**The art pipeline.** `rotomdex_window_1.png` is the original as generated;
`rotom.png` is the cleaned version the app actually loads. The original was
1024x825 with **30,336 unique colours**, a fractional block size of
12.8 x 12.89 px, and 13,452 semi-transparent pixels — it looked like pixel art
but every block was full of compression noise, which `image-rendering:
pixelated` would have magnified rather than hidden.

Cleaning it is mechanical, so the generator's output quality does not matter
much:

1. Fit the pixel grid by building a per-column and per-row difference profile
   and scoring candidate cell counts. Columns peaked at 80, rows at 64.
2. Resample one modal colour per cell, sampling only the middle ~44% of each
   cell so antialiased edges are ignored.
3. Merge colours within a distance threshold into a small palette.

Result: 80x64, 10 colours, 1.1 KB, down from 388 KB. Crisp at any whole-number
scale.

**Still to do — the art needs a bigger screen.** The largest unobstructed
rectangle inside the green is only 28x14 cells: 35% of the art's width and 22%
of its height. Measured in the running app, a 960x768 window gives **38 columns
x 10 rows**, which is not usable. The eyes and mouth overlap the top of the
green, costing 4 further rows.

Target: a screen of about **52x30 cells on an 80x64 canvas** (65% of width, 47%
of height), with nothing overlapping it. At scale 13 — a 1040x832 window, about
the ceiling on a 1440x900 laptop — that gives 76x23 at font 14. A 55x31 screen
would give exactly 80x24.

Measured xterm.js cell size at font 14, Menlo: **8.84 x 16.80 px**. Use it to
convert any proposed screen rectangle into columns and rows before drawing.

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
  `buf[len]` overflow that normal runs sailed straight past:
  `gcc -std=gnu2x -fsanitize=address -g -o shell_asan src/main.c`
