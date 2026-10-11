# Security policy

## Supported versions

Only the latest release receives fixes.

## Reporting a vulnerability

Please do **not** open a public issue for security problems. Use GitHub's private reporting instead:
**Security → Report a vulnerability** on this repository.

Include the plugin and Endstone versions and the steps to reproduce. You can expect an acknowledgement within a few days.

## Things to know

- The maintenance password is stored in plain text in `plugins/maintenance/config.yml`. Restrict read access to that
  file to the account that runs the server.
- `/maintenance password <password>` is a command, so the password may appear in the server console and logs.
- Released binaries come with `SHA256SUMS.txt`; verify your download against it.
