# Build and update version checks

Engine 0.5.3 and editor 1.1.1 are independent source projects. Each owns its
`JSD_PROJECT.json`, build scripts, CMake guards and update tools. The editor
accepts engine API 1, versions >= 0.5.3 and < 0.6.0; subsequent compatible engine
patches do not require editor changes.

## Builds

`./build.sh release` and `./build.sh debug` validate the project identity,
CMake/public/runtime versions, cache source path and cache project before
configuring, installing dependencies or building. The editor also checks its
selected engine source or installed SDK. `run.sh` builds incrementally before
launching, so an executable left over from another version is not reused.

Use `JSD_BUILD_DIR` for the engine build directory, `JSD_EDITOR_BUILD_DIR` for
the editor build directory, and `JSD_INSTALL_PREFIX` for the engine SDK.
`JSD_EDITOR_INSTALL_PREFIX` controls the editor install destination if using
CMake's install command; editor build.sh only builds. Source-local builds must
use build, build-* or prebuild directories. External build directories must be
empty or contain a cache owned by the same source tree. Copying a build cache
between projects or moving it between source checkouts is rejected.

CMake 3.21+ and Python 3 are required. DESTDIR redirection is rejected; use an
explicit install prefix instead.

Only -DNAME=VALUE build options are accepted; source/build/install redirection
and conflicting managed options are rejected. Direct CMake configure and
install commands also run the project/version/output checks. Use conventional
relative install directories (bin, lib, include, share). Do not reuse an
unmarked SDK directory from an older release: install into a fresh empty prefix.

Installed engine SDKs contain `share/jsd-game-engine/INSTALL.json` with identity,
version, API and file hashes. Both find_package and the editor verify this
receipt before importing the SDK. In installed mode, choose a single SDK:

```
./build.sh release -DJSD_EDITOR_USE_INSTALLED_ENGINE=ON \
  -DJSDGameEngine_DIR=/absolute/sdk/lib/cmake/JSDGameEngine
```

An editor source build installs only editor runtime files and runtime libraries;
it does not install engine headers or an engine SDK into the editor project.

`./build.sh clean` validates *all* selected output paths before deleting any.
It removes owned build/prebuild outputs and generated runtime/SDK files, while
preserving source files and unrelated files. A wrong build/prefix override
aborts the whole clean. Engine and editor operations use a per-project lock.

## Update packages

Extract an update outside the source projects. There is no target autodetection
or implicit sibling update. Select exactly the components you intend to update:

```
./apply.sh --check --engine /absolute/3d-engine
./apply.sh --engine /absolute/3d-engine
./apply.sh --editor /absolute/jsd-editor
./apply.sh --engine /absolute/3d-engine --editor /absolute/jsd-editor
```

The manifest declares component IDs, exact supported starting versions, target
versions and before/after hashes for every changed file. All selected components
and all payloads must pass preflight before any project file is written. A
matching folder name alone is never evidence of project identity. This release
can bootstrap identity markers only from the specific fingerprinted legacy
engine/editor revisions listed in its manifest.

Wrong projects, unsupported versions, downgrades, edited files, partial updates,
symlink destinations and corrupt payloads stop the update. Reapplying an update
is a no-op only when *all* its files, modes and target versions match. There is
no force/skip-version option. For later releases use tools/make_update.py to
build manifests from separate clean baseline and target source trees.

Files are staged and backed up in `.jsd-update-backups/` before replacement;
ordinary write failures roll back all selected components. If the process is
terminated during commit, `.jsd-update-journal.json` blocks subsequent builds
and updates. Restore the journal's listed files from its backup (remove files
whose before_sha256 is null), then remove the journal after verifying the
restored base. Do not delete a journal to bypass a partial update.

These guards protect this and future guarded packages. Previously downloaded
update scripts cannot be retroactively changed; do not reuse older updaters.
