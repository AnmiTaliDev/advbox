# AdvBox - Advanced Tool Box

> This project is archived and no longer maintained.

A collection of command-line utilities written in C, C++, and Rust.

## Utilities

- **calcx** - Calculator utility (C++)
- **colors** - Color manipulation tool (Rust)
- **countdown** - Countdown timer (C)
- **dateadd** - Date addition calculator (C++)
- **datediff** - Date difference calculator (Rust)
- **dirsize** - Directory size analyzer (C++)
- **estimate** - Estimation tool (Rust)
- **extract** - Universal archive extractor (Rust)
- **ftree** - File tree viewer (Rust)
- **killport** - Port killer utility (Rust)
- **lanlist** - LAN device lister (C++)
- **notes** - Note-taking tool (C++)
- **progress** - Progress bar utility (C)
- **randnum** - Random number generator (C)
- **selfkill** - Process self-termination utility (C)
- **sysinfo** - System information display (C)
- **tzconvert** - Timezone converter (C++)

## Building from Source

Prerequisites:

- GCC/G++ compiler
- Rust toolchain (rustc)
- Meson
- Ninja

Build:

```bash
meson setup build
ninja -C build
```

Install (optional):

```bash
sudo ninja -C build install
```

## License

MIT License. See the LICENSE file for details.

## Author

AnmiTaliDev
