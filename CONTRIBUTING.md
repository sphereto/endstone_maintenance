# Contributing

## Layout

```
include/maintenance/   public headers (config.h, form_response.h, plugin.h)
src/                   implementation; config.cpp and form_response.cpp have no Endstone dependency
tests/                 dependency-free unit tests for the Endstone-independent code
.github/workflows/     ci.yml (format, tests, builds, releases) and the weekly Endstone update check
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

1. Move the **Unreleased** changelog entries under a new version heading.
2. Set `VERSION` in `project(...)` in `CMakeLists.txt`.
3. Commit, then tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`.

CI refuses to build a tag that does not match the `CMakeLists.txt` version, and attaches the Windows and Linux binaries
to the GitHub release with generated release notes.
