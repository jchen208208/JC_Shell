# JC_Shell

I built my own Unix shell in C, from scratch, with no shell libraries.

It handles the core of what bash does day to day: it runs any program on your
PATH, chains them with pipes, redirects input and output, runs jobs in the
background, expands variables, and has its own tab completion and command
history written directly against `termios` — no readline.

It is not a bash clone. Bash has around 50 builtins and a full scripting
language on top; this implements a working subset, and I am still adding to it.
See [ROADMAP.md](ROADMAP.md) for what is done and what is next.

Long term, this becomes the backend of a custom terminal app for my own use: a
Mac app with its own Dock icon, whose window is shaped like a Rotom Dex instead
of a rectangle, with a custom font and theme inside.

## What works

- **Builtins:** `echo`, `exit`, `type`, `pwd`, `cd`, `history`, `jobs`,
  `declare`, `complete`
- **External programs:** full PATH lookup and `execv`, so `grep`, `ls`, `curl`
  and everything else work as normal
- **Pipelines:** `ls | grep foo | wc -l`, any number of stages
- **Redirection:** `>`, `>>`, `2>`, `2>>` (and `1>` / `1>>`)
- **Background jobs:** `&`, with reaping and `jobs`
- **Variables:** assignment and `$VAR` expansion
- **Line editing:** tab completion and persistent history, hand-written on
  raw-mode `termios`

## Not yet

`<` input redirection, `&&` / `||` / `;`, `$?`, globbing, and `~` outside `cd`.
These are the current work queue in [ROADMAP.md](ROADMAP.md).

## Build

```sh
cmake -B build -S .
cmake --build build
```

## Run

```sh
./build/shell
```

## Note on terminology

A **shell** parses what you type and runs it — bash, zsh, this project. A
**terminal emulator** is the GUI that draws the text and owns the window —
Terminal.app, iTerm2. They are separate programs. Right now this repo is the
shell; the Rotom Dex terminal emulator is Phase 2.
