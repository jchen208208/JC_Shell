I built my own shell in C. It has almost all the standard commands and functions of a normal terminal shell, such as bash, but I have customized it for my own use with extra unique features tailored to myself. I've also customized the UI to look like a Rotom Dex Pokémon.

## build

```sh
cmake -B build -S .
cmake --build build
```

## run

```sh
./build/shell
```
