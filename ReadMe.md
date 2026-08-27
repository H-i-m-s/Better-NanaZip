![NanaZip hero](assets/readme/hero.svg)

NanaZip is an open-source file archiver for the modern Windows experience,
forked from the [7-Zip] source code and rebuilt around today's Windows design
language: native dark mode, Mica material, deep File Explorer integration, and
MSIX packaging. The full 7-Zip 26.02 feature set is inherited and extended with
additional formats, hash algorithms, and security hardening.

[![License](https://img.shields.io/badge/license-MIT-58A6FF)](License.md)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%202004%2B-58A6FF)](BUILDING.md)
[![Core](https://img.shields.io/badge/core-7--Zip%2026.02-3FB950)](https://www.7-zip.org/)

## Highlights

- **7-Zip 26.02 core** with the enhancements from [7-Zip ZS] and
  [7-Zip NSIS] (Brotli, LZ4, LZ5, Lizard, Zstandard, Fast-LZMA2, NSIS scripts).
- **Native dark mode** for every GUI component, with immersive Mica on the
  main window.
- **File Explorer integration**: full context menu and file associations on
  Windows 10/11.
- **MSIX packaging** for a clean, dependency-bundled deployment story.
- **7-Zip execution alias** (`7z.exe` → NanaZip) to ease migration.
- **Per-Monitor DPI awareness**, modern message boxes and folder pickers,
  smart extraction, and Mark-of-the-Web propagation by default.
- **Extra archive formats** (read-only): .NET Single File bundles, Electron
  asar, ROMFS, UFS/UFS2, ZealFS, WebAssembly, littlefs.
- **30+ hash algorithms**, including MD2–MD5, SHA family, SHA-3, BLAKE2b/3,
  ED2K, GOST, Snefru, Tiger, TTH, Whirlpool, XXH32/64/3, SM3.
- **Security hardening**: Control Flow Guard, CET Shadow Stack, Package
  Integrity Check, strict handle validation, no dynamic code generation in
  Release builds.

## Screenshots

![NanaZip main window, dark mode](Documents/MainWindowDarkMode.png)
![NanaZip main window, light mode](Documents/MainWindowLightMode.png)
![NanaZip context menu](Documents/ContextMenu.png)

## Install

Grab the latest **MSIX bundle** from the
[Releases](https://github.com/M2Team/NanaZip/releases) page and double-click
it, or install for the current user from PowerShell:

```powershell
Add-AppxPackage -Path "path\to\NanaZip_x64.msixbundle"
```

The context menu appears after installation; if it is missing, restart File
Explorer via Task Manager.

## Build from source

See [BUILDING.md](BUILDING.md) for the full guide. The short version:

```bat
git clone --recursive https://github.com/your-fork/NanaZip.git
cd NanaZip
BuildAllTargets.cmd
```

Artifacts land in `Output\Binaries`. The
[Build Binaries](.github/workflows/BuildBinaries.yml) workflow builds the same
targets on GitHub Actions without any signing setup, so a fresh fork builds
out of the box.

## Documentation

- [Building from source](BUILDING.md)
- [Release Notes](Documents/ReleaseNotes.md)
- [Contributing](CONTRIBUTING.md)
- [License](License.md)
- [Security Policy](Security.md)
- [Privacy Policy](Documents/Privacy.md)
- [Group Policy Administrative Templates (ADMX/ADML)](Documents/PolicyDefinitions)
- [Section 508 Accessibility Conformance Report](Documents/Section508)

## License

NanaZip is licensed under the [MIT License](License.md).

NanaZip is a fork of [7-Zip] by Igor Pavlov and builds on the work of the
[7-Zip ZS] and [7-Zip NSIS] projects. Their licenses apply to the respective
upstream code.

[7-Zip]: https://www.7-zip.org/
[7-Zip ZS]: https://github.com/mcmilk/7-Zip-zstd
[7-Zip NSIS]: https://github.com/myfreeer/7z-build-nsis
