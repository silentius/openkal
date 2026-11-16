# Project release procedure

OpenKal 0.1.0 is a source-only release. Its release files are the source archive
and the SHA-256 checksum. Pull-request review checks the origin and
redistribution permissions of contributed code, data and assets. The source
manifest selects the reviewed project files.

## Source archive

Update `SourceFiles.txt` when adding or removing a project file. It is a sorted,
one-path-per-line list relative to the source root. Review additions as part of
the contribution. Then run:

```sh
python3 tools/package_source.py --check-tracked
python3 tools/check_image_inventory.py
python3 tools/package_source.py --version 0.1.0 --output build-release/openkal-0.1.0.tar.gz
```

Only `--check-tracked` uses Git. It compares both directions between the manifest
and the current tracked paths without reading history. Exported source trees
can validate and repackage without Git:

```sh
python3 tools/package_source.py
```

CMake also exposes `openkal_source_archive`. CPack's whole-working-directory
source export is disabled. Build trees, saves and unlisted local files do not
enter the source archive. Archive metadata is deterministic; current listed
file bytes are used, including local changes awaiting publication.

Create and verify the release checksum next to the source archive:

```sh
(
  cd build-release
  sha256sum openkal-0.1.0.tar.gz > SHA256SUMS
  sha256sum -c SHA256SUMS
)
```

## Optional local Debian build

After making the source archive above, extract it into a new directory and
build the Debian package from that tree. This gives the resource packer the
same reviewed inputs as the source archive, even when the working checkout
contains local data.

```sh
release_source=$(mktemp -d)
tar -xzf build-release/openkal-0.1.0.tar.gz -C "$release_source"
cmake -S "$release_source/openkal-0.1.0" -B build-project-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DOPENKAL_RELEASE_BUILD=ON \
  -DOPENKAL_ENGINE=gl -DOPENKAL_DATA_ROOT= -DOPENKAL_INCLUDE_USER_AUDIO=OFF \
  -DOPENKAL_BUILD_TESTS=OFF -DOPENKAL_BUILD_TOOLS=OFF \
  -DOPENKAL_BUILD_AREA_EDITOR=OFF -DOPENKAL_BUILD_PROFILE_CLIENT=OFF
cmake --build build-project-release --target package --parallel 2
python3 tools/check_release_package.py build-project-release/openkal-client_0.1.0_amd64.deb
```

Use the generated architecture-specific filename on non-amd64 systems. The
recipe builds a client package from project inputs. The [README](../README.md)
describes how to select user-supplied resources for a local combined build.

## Source-publication content checks

Complete the [repository rights and content review](OpenSourceReleaseChecklist.md)
for the actual public tree and archive. Include the intended license texts,
notices and content decisions, and exclude unintended private/copied inputs.
Separately supplied game files remain users' runtime inputs. File hashes and
inventory checks establish which bytes are selected, not permission to distribute
them. A source release does not require a finished or defect-free emulator.

## Technical checks for the source release

Run the owned null suite, the Release/OpenGL regressions with a working display,
and PostgreSQL integration for the supported source build configurations.
The [CI workflow](../.github/workflows/ci.yml) defines the combinations. A skip
does not count as a database or graphics check. The execution queue is in
[TODO.md](../TODO.md), tasks R13, R16 and R15.

Extract the source archive into a separate directory and build/test there without
`.git`, a neighboring data repository or saved profiles. Recheck image hashes,
license notices and actual archive contents. The CI client job also exercises
the local Debian recipe and launcher. Record compiler/dependency versions and
the source archive checksum. Test runs and release notes must describe the
same candidate.

The `debian/rules` compatibility recipe declares the core dependencies and
disables the Qt Area Editor. CPack is the verified project binary-package recipe;
the separate Debian source-package workflow is outside this initial release's
support claim. Optional Qt tools are also outside the core support claim.

## Publication

Review [release notes](ReleaseNotes-0.1.0.md), [security policy](../SECURITY.md)
and the [release checklist](OpenSourceReleaseChecklist.md). Publish the reviewed
source archive and its checksum together. Tagging, committing and publishing
require an explicit publication instruction.
