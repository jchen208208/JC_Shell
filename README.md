# JC_Shell

I built my own Unix shell in C, from scratch, with no shell libraries.

It follows a standard REPL cycle, and it runs any program on your
PATH, chains them with pipes, redirects input and output, runs jobs in the
background, expands variables, and has its own tab completion and command
history written directly against `termios,` etc. NO readline.

It is not a bash clone since bash has around 50 builtins and a full scripting
language on top. I've created a working subset that I'm still adding on to.

I will also create a custom terminal app for my own use with its own Dock icon and a window shaped like a Rotom Dex instead
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

## Build

```sh
cmake -B build -S .
cmake --build build
```

## Run

```sh
./build/shell
```
