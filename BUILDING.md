# Building NanaZip from source

NanaZip builds with standard Visual Studio tooling. No custom scripts or
signing setup are required — a fresh fork builds out of the box, locally and
on GitHub Actions.

## Prerequisites

- **Windows 10 2004 (Build 19041)** or later (development machine; the
  produced packages run on Windows 10 2004+ and Windows Server 2022+).
- **Visual Studio 2022** with the **Desktop development with C++** workload
  (MSVC toolset, Windows SDK, C++ CMake tools).
  - Any recent **Windows 10/11 SDK** (10.0.26100.0 or newer recommended).
- **Git** with Git LFS if your clone needs it (the repository itself has no
  LFS assets).
- **.NET SDK 8 or later** only if you rebuild the maintainer tools in
  `NanaZip.MaintainerTools.slnx`; the normal build does not require it.

The MSBuild project SDK `Mile.Project.Configurations` is pinned in
`global.json` (1.0.2075) and is restored from NuGet automatically during the
build.

## Local build

```bat
git clone --recursive https://github.com/<your-account>/NanaZip.git
cd NanaZip

rem Optional: pre-restore NuGet packages (recommended if you use
rem Visual Studio 2026, which may not restore packages automatically).
RestoreNuGetPackages.cmd

rem Full build: x64 + ARM64, Debug + Release, MSIX bundle, extension
rem package installer, portable binaries and debug symbols.
BuildAllTargets.cmd
```

`BuildAllTargets.cmd` wraps `MSBuild -m BuildAllTargets.proj`. You can also
invoke MSBuild directly from a **Developer Command Prompt**:

```bat
MSBuild -m BuildAllTargets.proj
```

The default target chain is `RefreshVersion → Restore → Build → Packaging`:

- `RefreshVersion` derives the package version from the git commit date.
- `Restore` restores NuGet packages for both configurations.
- `Build` builds the MSIX package project (x64 + ARM64, Debug + Release) and
  the extension-package installer.
- `Packaging` assembles the portable binaries and debug-symbol archives.

## Outputs

| Path | Contents |
| --- | --- |
| `Output\Binaries\AppPackages` | MSIX bundle(s) for installation |
| `Output\Binaries\Root\Binaries` | Portable binaries (x64 + arm64) |
| `Output\Binaries\Root\Symbols` | PDB symbol files |
| `Output\NanaZip.ExtensionPackage_*.exe` | Extension package installer (Inno Setup) |
| `Output\NanaZip_*_Binaries.zip` | Archived portable release |
| `Output\NanaZip_*_DebugSymbols.zip` | Archived debug symbols |

## Building on GitHub Actions

The [Build Binaries](../.github/workflows/BuildBinaries.yml) workflow is the
canonical CI build. It runs on `windows-2022`, executes
`MSBuild BuildAllTargets.proj`, and uploads the MSIX bundle, extension
installer, portable binaries, debug symbols and the MSBuild binary log as
artifacts.

To build your fork:

1. Fork the repository.
2. Open **Actions** → **Build Binaries**.
3. Click **Run workflow**.

No repository secrets are required. The workflow is triggered manually
(`workflow_dispatch`) so forks do not burn minutes on every push.

## Troubleshooting

- **NuGet restore failures on fresh runners**: run
  `dotnet nuget locals all --clear` once, then rebuild. The workflows already
  do this.
- **Visual Studio does not restore packages automatically**: run
  `RestoreNuGetPackages.cmd` before building.
- **`Mile.Project.Configurations` resolution errors**: make sure `global.json`
  is intact and you have an internet connection on the first build (the SDK
  is fetched from nuget.org once, then cached).
- **Missing MSIX tools**: install the Windows SDK with the *App packaging tools*
  component via the Visual Studio Installer.

## Modifying and debugging

Open `NanaZip.slnx` in Visual Studio and build the projects you need
(`NanaZip.Modern` for the XAML dialogs, `NanaZip.UI.Modern` for the File
Manager and shell extension, `NanaZipPackage` for the MSIX package). See
[CONTRIBUTING.md](CONTRIBUTING.md) for code style and workflow notes.
