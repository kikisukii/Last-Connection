# Environment inspection and installation — 2026-11-09

Historical inspection. Machine-specific paths were redacted before the first source commit; current portable setup is in [BUILD.md](BUILD.md).

Project location: `<project>`. It existed and was empty at first inspection. No parent or project AGENTS.md was found. Source was not relocated or duplicated elsewhere.

| Tool | Initial inspection | Current verified installation | Newly installed? |
|---|---|---|---|
| Git | Available in PATH | 2.55.0.windows.3, `<local tool path>` | No |
| PowerShell | Available in Codex runtime; Windows PowerShell also present | 7.6.9, Codex bundled runtime on C: | No |
| Python | `<local tool path>` reported no interpreters; old MSI records and an unusable venv reference did not establish availability | Existing Codex Python 3.12.14 used for bootstrap; SDK-managed Python 3.13.3 now at `<local tool path>` | Only Emscripten's bundled component |
| CMake | Not found in PATH, D: search or inspected common locations | 4.4.5, `<local tool path>` | Yes, portable |
| Ninja | Not found | 1.13.2, `<local tool path>` | Yes, portable |
| Native C++ | GCC, Clang, MSVC/Build Tools not found in inspected locations | GCC 16.2.0 from w64devkit 2.11.0, `<local tool path>` | Yes, portable |
| Emscripten | Not found; SDK variables absent | 4.0.23, `<local tool path>` | Yes |
| Node | Existing Codex Node available | Emscripten separately manages its matching Node 24.19.0 under `<local tool path>` | Only Emscripten's bundled component |
| raylib | Not found | 5.5, CMake build dependency under `build/*/_deps/raylib-src` | Project dependency |

Before installing, inspected command resolution, process/user/machine PATH, SDK environment variables, D: files, Program Files directories, common user program locations and relevant uninstall registry entries. This is an inspection of likely locations, not a claim that every C: file was searched.

Portable installation paths were announced before installation. No system installer, permanent PATH edit or administrator-dependent Visual Studio installation was used. Sandbox permission was obtained for network access and writes to tool directories outside the workspace. Download staging and temporary build files are under the project on D:. Existing C: system/runtime files were reused.

## Sources and reproducibility

- CMake: official Kitware release `v4.4.5`, Windows x86_64 ZIP. Its SHA-256 matched the official release checksum file before extraction.
- Ninja: official ninja-build release `v1.13.2`, Windows ZIP.
- GCC toolkit: official skeeto/w64devkit release `v2.11.0`, x64 archive, extracted with the already-installed `<local tool path>`.
- Emscripten: official emsdk Git repository; installed/activated SDK `4.0.23`, release revision `aaa43392544d695232b70eda706d751f18980c2a`.
- raylib: release tag `5.5`, observed revision `c1ab645ca298a2801097931d1079b10ff7eb9df8`.

The public GitHub API was rate-limited; official GitHub release HTML supplied asset links instead. No mirror or unverified third-party binary source was used.

Run `scripts/check_environment.ps1` to recheck paths. Tool paths are centralized in `scripts/toolchain.ps1`. Initial build logs and archives are retained in ignored `.downloads/`; dependency trees/build products are ignored under `build/`.

## Verified outcomes and limits

- Native Debug and Web Release builds passed at the exact project path with spaces.
- Native render smoke initialized OpenGL 3.3 on NVIDIA GeForce RTX 4070 and exited successfully.
- 34 deterministic movement checks passed through CTest.
- Edge displayed the compiled WebAssembly canvas and accepted browser key presses through all three lanes; both boundary directions were blocked.
- Server binds only to `127.0.0.1:8000` and serves `build/web`.
- Native physical-key handling has not had a separate human playtest. Browser automation and core tests do not establish final movement feel.
- Git initialized on `main`; no commit, remote, public upload or GitHub Pages deployment has been performed.
