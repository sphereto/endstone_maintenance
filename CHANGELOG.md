# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [3.0.0]

### Added
- `/maintenance` command (alias `/mt`) with `on`, `off`, `status`, `reload`, `list`, `add`, `remove` and `password`.
- `maintenance.command` permission.
- Maintenance whitelist (`maintenance.whitelist`).
- Full lockdown for players on the login screen: movement, chat, commands, interaction, block break/place and item drop
  are cancelled.
- Server-list MOTD while maintenance is on (`messages.motd`).
- Configurable messages and form text (`messages.*`) and `maintenance.apply_effects`.
- Enabling maintenance now locks players who are already online; disabling it releases them.
- Unit tests for the config parser and form-response parser.
- CMake presets, `.editorconfig`, CONTRIBUTING, issue and pull-request templates, Dependabot and a weekly Endstone
  update check.

### Changed
- Updated to Endstone 0.11.13.
- Reorganised sources into `include/maintenance/` and `src/`; the Endstone-independent code can be built and tested
  without downloading Endstone (`-DMAINTENANCE_BUILD_PLUGIN=OFF -DMAINTENANCE_BUILD_TESTS=ON`).
- The config file is located through the plugin data folder, supports comments, and is preserved when the plugin saves it.
- Config is reloaded with `/maintenance reload` instead of on every player join.
- CI is now `ci.yml` (format check, unit tests, Linux and Windows builds). Releases are a separate `release.yml`
  triggered by `v*` tags: it checks the tag against `CMakeLists.txt` and this changelog, reuses the full CI, then
  publishes the binaries with SHA256 checksums and the changelog entry as release notes. Actions bumped to current
  major versions and the third-party release and CMake setup actions removed.
- Plugin version is defined once, in `CMakeLists.txt`.

### Fixed
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
