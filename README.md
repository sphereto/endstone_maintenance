# Maintenance

[![CI](https://github.com/sphereto/endstone_maintenance/actions/workflows/ci.yml/badge.svg)](https://github.com/sphereto/endstone_maintenance/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A lightweight C++ plugin for [Endstone](https://endstone.dev) servers that puts the server into a password-protected
maintenance mode.

While maintenance is on, every player who joins must enter the password in a login form before they can do anything.
Operators, players with the `maintenance.bypass` permission and whitelisted names skip the form entirely.

## Features

- **Password login form** with a configurable timeout; players who fail, close the form or run out of time are kicked.
- **Full lockdown** while on the login screen: movement, chat, commands, interaction, block breaking/placing and item
  dropping are all blocked, plus optional blindness/slowness/resistance effects.
- **In-game control** with `/maintenance`: toggle the mode, manage the whitelist and change the password without editing
  files or restarting. Turning maintenance on also locks players who are already online.
- **Maintenance whitelist** for people who should always get in.
- **Server-list MOTD** that shows the server is under maintenance.
- **Every message is configurable**, including colour codes.
- Exact, constant-time password comparison.

## Compatibility

| Plugin | Endstone |
|--------|----------|
| 3.0.x  | 0.11.x (built against 0.11.13) |

## Installation

1. Download `endstone_Maintenance.dll` (Windows) or `endstone_Maintenance.so` (Linux) from the
   [latest release](https://github.com/sphereto/endstone_maintenance/releases/latest).
2. Put it in your server's `plugins/` folder and start the server.
3. Edit `plugins/maintenance/config.yml`, **change the password**, and run `/maintenance reload`.

## Commands

All commands need the `maintenance.command` permission (operators by default). `/mt` is an alias.

| Command | Description |
|---------|-------------|
| `/maintenance on` / `off` | Enable or disable maintenance mode. The choice is saved to the config. |
| `/maintenance status` | Show whether maintenance is on, how many players are on the login screen, and the settings in effect. |
| `/maintenance reload` | Re-read `config.yml` after editing it by hand. |
| `/maintenance list` | List whitelisted players. |
| `/maintenance add <player>` | Add a player to the whitelist. |
| `/maintenance remove <player>` | Remove a player from the whitelist. |
| `/maintenance password <password>` | Change the login password. |

## Permissions

| Permission | Default | Description |
|------------|---------|-------------|
| `maintenance.command` | Operator | Use `/maintenance`. |
| `maintenance.bypass` | Operator | Skip the login form during maintenance. |

## Configuration

`plugins/maintenance/config.yml` is created on first start. It uses simple `key: value` lines; `#` starts a comment.

| Key | Default | Description |
|-----|---------|-------------|
| `maintenance.enabled` | `true` | Master switch. |
| `maintenance.password` | `passwordhere` | Password required to join. **Change this.** An empty password means nobody can log in. |
| `maintenance.kick_delay` | `15` | Seconds to enter the password. |
| `maintenance.apply_effects` | `true` | Apply blindness, slowness and resistance on the login screen. |
| `maintenance.whitelist` | *(empty)* | Comma-separated names that skip the form (case-insensitive). |
| `messages.motd` | `§cServer under maintenance` | Server-list MOTD while maintenance is on; empty keeps the normal one. |
| `messages.form_title`, `form_body`, `form_input_label`, `form_submit` | see file | Text of the login form. |
| `messages.timeout`, `wrong_password`, `closed` | see file | Kick messages. |
| `messages.accepted`, `bypass` | see file | Chat messages. |

> **Note:** the password is stored in plain text in `config.yml`. Keep that file readable by the server only.

## Building from source

You need CMake 3.21+, Ninja, and Clang with libc++ (Linux) or MSVC (Windows). Endstone is downloaded automatically.

```sh
# Linux
sudo apt install clang libc++-dev libc++abi-dev ninja-build
cmake --preset linux && cmake --build --preset linux

# Windows (from a "x64 Native Tools" prompt)
cmake --preset windows && cmake --build --preset windows
```

The plugin ends up in `build/<preset>/` as `endstone_Maintenance.so` / `.dll`.
To build against a different Endstone version, pass `-DENDSTONE_VERSION=vX.Y.Z`.

Run the unit tests (no Endstone download needed):

```sh
cmake --preset tests && cmake --build --preset tests && ctest --preset tests
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow and release process.

## License

MIT. See [LICENSE](LICENSE).
