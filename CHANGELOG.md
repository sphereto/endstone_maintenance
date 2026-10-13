# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [3.0.0]

### Added
- `/maintenance` command (alias `/mt`) with `on`, `off`, `status`, `reload`, `list`, `add`, `remove` and `password`.
- `maintenance.command` permission.
- Maintenance whitelist (`maintenance.whitelist`).
- Full lockdown for players on the login screen: movement, chat, commands, block and entity interaction, block
  break/place, item drop, pickup and use, and emotes are cancelled. Looking around is allowed: only real position
  changes are cancelled, so the player is not teleported back for every camera turn.
- Blindness/slowness/resistance applied on the login screen are removed when the player logs in or is kicked, or on
  their next join if they disconnected on the login screen. Effects are only removed if the plugin applied them.
- Server-list MOTD while maintenance is on (`messages.motd`).
- Configurable messages and form text (`messages.*`) and `maintenance.apply_effects`.
- Enabling maintenance now locks players who are already online; disabling it releases them.
- Unit tests for the config parser, form-response parser, settings (including a check that the shipped config matches
  the built-in defaults) and the movement threshold.
- `test-build.yml` workflow: downloadable Linux and Windows builds for every non-main push, or on demand with a
  chosen build type and Endstone version, so changes can be tried on a real server without cutting a release.
- `build.yml` reusable workflow shared by CI, releases and test builds.
- CMake presets, `.editorconfig`, CONTRIBUTING, issue and pull-request templates, Dependabot and a weekly Endstone
  update check.

### Changed
- Updated to Endstone 0.11.13.
- LICENSE: added a 2026 copyright line for this project alongside the original notice (MIT text unchanged).
- Reorganised sources into `include/maintenance/` and `src/`; the Endstone-independent code can be built and tested
  without downloading Endstone (`-DMAINTENANCE_BUILD_PLUGIN=OFF -DMAINTENANCE_BUILD_TESTS=ON`).
- The config file is located through the plugin data folder, supports comments, and is preserved when the plugin saves it.
  Saving is atomic (temporary file, then rename), so a crash or full disk cannot leave a truncated config.
- Config is reloaded with `/maintenance reload` instead of on every player join. Settings are parsed once into a typed
  snapshot, the whitelist is a cached case-insensitive set, and a failed reload keeps the previous settings.
- Event handlers that cancel a locked player's actions share one implementation and skip events another plugin already
  cancelled.
- CI is now `ci.yml` (format check, unit tests, Linux and Windows builds). Releases are a separate `release.yml`
  triggered by `v*` tags: it checks the tag against `CMakeLists.txt` and this changelog, reuses the full CI, then
  publishes the binaries with SHA256 checksums and the changelog entry as release notes. Actions bumped to current
  major versions and the third-party release and CMake setup actions removed.
- Plugin version is defined once, in `CMakeLists.txt`.

### Fixed
- Windows plugin build failed to configure: Endstone requires clang-cl (and CMake 3.29+), not MSVC's `cl.exe`. The
  `windows` preset and CI now use clang-cl.
- Unit tests failed to compile with MSVC: the test macros stringized raw string literals, which MSVC's default
  preprocessor cannot do. The macros no longer stringize their arguments.
- Password check accepted any form response that merely *contained* the password; it is now an exact comparison of the
  text field value.
- Sound effects used an invalid `effect ... playsound` command; they now use the player sound API.
- Pending kick timers were tracked through a raw `Task*` that could dangle; they are now tracked by task id and cancelled
  when the player leaves.
- Clearing effects after login now uses `effect <player> clear`.
- Release job downloaded an artifact name that the build jobs never uploaded, so releases could not be created.
- README advertised version 2.0 while the plugin reported 2.1.

[Unreleased]: https://github.com/sphereto/endstone_maintenance/compare/v3.0.0...HEAD
[3.0.0]: https://github.com/sphereto/endstone_maintenance/releases/tag/v3.0.0
