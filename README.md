# mouse-sync

> Capture and apply mouse pointer settings between Windows and Linux — so your
> dual-boot setup feels consistent without manual guessing.

## Status

| Feature | Status |
|---|---|
| Windows capture (speed, EPP, thresholds, registry) | ✅ Implemented |
| Windows apply (SPI + registry + broadcast) | ✅ Implemented |
| `print` command (cross-platform, reads JSON) | ✅ Implemented |
| `schema` / `version` commands | ✅ Implemented |
| Linux KDE/Wayland backend | ✅ Basic capture/apply via KWin DBus |
| Linux X11/Cinnamon backend | 🔜 Planned (v0.2) |
| Calibration / auto-match | 🔜 Planned (v0.3) |
| GUI | ❌ Non-goal for now |

---

## What it does

`mouse-sync` is a CLI tool that:

1. **Captures** all relevant mouse pointer settings from the current OS and
   saves them to a JSON file.
2. **Applies** settings from a JSON file to the current OS.
3. Provides a shared JSON format with top-level metadata and a single active
  platform payload, ready for future backends.

### Captured Windows settings

- **Mouse speed** – `SPI_GETMOUSESPEED` (1 = slowest … 20 = fastest; default 10).
- **Enhance Pointer Precision (EPP)** – on/off flag from `SPI_GETMOUSE`.
- **Acceleration thresholds** – `MouseThreshold1` and `MouseThreshold2` from
  `SPI_GETMOUSE`.
- **Registry snapshot** from `HKCU\Control Panel\Mouse`:
  - `MouseSensitivity`, `MouseSpeed`, `MouseThreshold1`, `MouseThreshold2`
  - `SmoothMouseXCurve` / `SmoothMouseYCurve` (EPP curve binary data, stored
    as a hex string).

### Apply behaviour

When applying to Windows the tool:

1. Calls `SPI_SETMOUSESPEED` and `SPI_SETMOUSE`.
2. Writes the registry keys (if present in the JSON).
3. Broadcasts `WM_SETTINGCHANGE` so running applications pick up the changes
   immediately (no reboot needed).

### Captured KDE Wayland settings

For the current `kde-wayland` backend the tool captures per-device pointer
settings exposed by KWin DBus:

- device identity: `name`, `vendor`, `product`
- pointer acceleration speed and profile
- left-handed mode
- middle-button emulation
- natural scrolling
- scroll factor
- scroll-on-button-down

---

## JSON format

See [`examples/windows_example.json`](examples/windows_example.json) for a
Windows sample and [`examples/kde_wayland_example.json`](examples/kde_wayland_example.json)
for a KDE Plasma / Wayland sample.

Run `mouse-sync schema` for a full field-by-field description.

---

## Building

### Requirements

| Tool | Version |
|---|---|
| CMake | ≥ 3.20 |
| C++ compiler | MSVC 2019+ (Windows), GCC 10+ or Clang 12+ (Linux) |
| Internet access | Required for first build (FetchContent pulls nlohmann/json & CLI11) |

### Windows (MSVC / Visual Studio)

```powershell
# From the repository root
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\bin\Release\mouse-sync.exe version
```

Or open the folder in Visual Studio 2022 and use the built-in CMake support.

### Linux (GCC / Ninja)

On KDE Plasma / Wayland, the build includes a basic `kde-wayland` backend that
captures and applies pointer settings through KWin's input-device DBus API.
On other Linux desktops the build still succeeds, but backend auto-detection may
return no supported backend for the current session.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/mouse-sync version
```

---

## Usage

```
mouse-sync <command> [options]

Commands:
  capture   [--os auto] [--backend auto] --out <file>
                                          Capture settings from the current backend
  apply     [--os auto] [--backend auto] --in <file>
                                          Apply settings to the current backend
  print     --in <file>                 Pretty-print a JSON profile (any OS)
  schema                                Show the JSON schema description
  version                               Show version and available backends
```

### Examples

```powershell
# On Windows: save current settings
mouse-sync capture --out my-mouse.json

# On Windows: restore settings from a file
mouse-sync apply --in my-mouse.json

# Explicit form still works
mouse-sync capture --os windows --backend windows --out my-mouse.json

# On KDE Plasma / Wayland: capture the current KWin mouse profile
mouse-sync capture --backend kde-wayland --out plasma-mouse.json

# On any OS: inspect a saved profile
mouse-sync print --in my-mouse.json
```

If `--os` or `--backend` are omitted, the CLI treats them as `auto`. If the
current environment cannot be determined unambiguously, the command exits with
an explicit error and asks for a concrete value.

---

## Project layout

```
mouse-sync/
├── cmake/            CMake helper scripts (FetchContent deps)
├── core/             Header-only profile model + JSON serialization
│   └── include/mouse_sync/profile.hpp
├── backends/
│   ├── windows/      Windows SPI + registry backend (WIN32 only)
│   └── linux/kde_wayland/
│                      KDE Plasma / Wayland backend via KWin DBus
├── cli/              CLI entry point (main.cpp)
├── examples/         Sample JSON files
└── .github/workflows CI (Windows MSVC + Ubuntu GCC)
```

---

## Roadmap

| Version | Goal |
|---|---|
| **0.1** (this PR) | Windows capture/apply, cross-platform CLI skeleton |
| **0.2** | Expand KDE Wayland backend coverage and tests |
| **0.2** | Linux X11 / Cinnamon backend (`xinput` properties) |
| **0.3** | Calibration: measure px/cm and auto-match across OSes |

---

## Contributing

Pull requests are welcome. To add a new backend, implement a backend-specific
adapter and register it via the backend dispatch layer rather than wiring it
directly into `cli/main.cpp`.

## License

MIT — see [LICENSE](LICENSE).
