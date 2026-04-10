# Star Idle

Early bootstrap for an idle game inspired by Unnamed Space Idle.

Current stack:

- C++23
- GCC (prefers Homebrew `g++-15` on Apple Silicon when available)
- SDL3 for windowing and rendering
- Dear ImGui docking branch for the UI
- `stb_image` for image loading
- `nlohmann_json` for persistence

## What Is Included

- SDL3 window + renderer startup
- Dear ImGui in docking mode
- A small application layer instead of a single giant `main.cpp`
- Starter idle-game simulation systems
- JSON save/load using `SDL_GetPrefPath()`
- `stb_image`-based texture loading for future UI art

## Build

The first configure step downloads Dear ImGui, `stb`, and `nlohmann_json`.

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/star_idle
```

If you drop an image at `assets/images/sector_preview.png`, the UI will load and preview it through `stb_image`.

