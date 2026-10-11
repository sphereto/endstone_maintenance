# Contributing

## Layout

```
include/maintenance/   public headers (config.h, form_response.h, plugin.h)
src/                   implementation; config.cpp and form_response.cpp have no Endstone dependency
tests/                 dependency-free unit tests for the Endstone-independent code
.github/workflows/     ci.yml (format, tests, builds), release.yml (tagged releases), weekly Endstone update check
```

Keep logic that does not need the server API in `config` / `form_response` (or a new Endstone-free file listed in
`MAINTENANCE_CORE_SOURCES`) so it can be unit tested.

## Development loop

```sh
# fast: unit tests only, no Endstone download
cmake --preset tests && cmake --build --preset tests && ctest --preset tests

# full plugin build (Linux needs clang + libc++; see README)
cmake --preset linux && cmake --build --preset linux
```

Format before committing (CI uses clang-format 18 with the repo's `.clang-format`):

```sh
find include src tests -name '*.h' -o -name '*.cpp' | xargs clang-format-18 -i
```

## Pull requests

- Add or update tests for behaviour you change.
- Add a line under **Unreleased** in `CHANGELOG.md`.
- Update the README when config keys, commands or permissions change.

## Updating Endstone

1. Bump `ENDSTONE_VERSION` in `CMakeLists.txt` (a weekly workflow opens an issue when a new stable tag exists).
2. Build, fix any API changes, and run the tests.
3. Update the compatibility table in the README and the changelog.

## Releasing

1. Move the **Unreleased** changelog entries under a new `## [X.Y.Z] - YYYY-MM-DD` heading and update the link
   references at the bottom of `CHANGELOG.md`.
2. Set `VERSION` in `project(...)` in `CMakeLists.txt`.
3. Merge to `main`, then tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`.

The **Release** workflow (`.github/workflows/release.yml`) then:

1. fails fast if the tag does not match the `CMakeLists.txt` version or `CHANGELOG.md` has no entry for it,
2. runs the full CI (format, tests, Linux and Windows builds),
3. publishes a GitHub release containing `endstone_Maintenance.so`, `endstone_Maintenance.dll` and `SHA256SUMS.txt`,
   using the changelog entry as the release notes.

Tags with a hyphen, such as `v3.1.0-rc.1`, are published as pre-releases. They need their own changelog heading
(`## [3.1.0-rc.1]`) while the `CMakeLists.txt` version stays at `3.1.0`.

If a release run fails before publishing, fix the problem, delete the tag
(`git push origin :refs/tags/vX.Y.Z`) and tag again.
