# Roadmap

Where this project is going, and what has to happen to get there.

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
- External programs via PATH lookup + `execv` — `grep`, `ls`, `wc`, `curl`, all of them
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

### 2.2 The Rotom Dex shape

```js
new BrowserWindow({ transparent: true, frame: false, hasShadow: false })
```

The window stays a rectangle — the parts outside the Dex shape are just
transparent. The shape itself is a PNG or CSS/SVG in the page. Needs a custom
drag region since `frame: false` removes the title bar.

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
