# Optional Docker/Podman environments

Containers provide source and the complete build environment. The game always
runs as a native host executable. There are two routes, using identical mounts.

## AIO: decomp and native build together (default)

From this native repository:

```sh
docker build -t spf2t-native:aio .
mkdir -p output
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/roms:/roms:ro" -v "$PWD/output:/output" spf2t-native:aio
```

The default `aio` target includes this native project and obtains the latest
`spf2t-decomp` tooling project from its default branch at image construction time.
The container runs its decomp process, then the native build process. Both use
one shared `/output`; ROMs, generated code/data and game builds are not baked
into image layers. The decomp project must be available on GitHub before using
the remote AIO build. No tools/ROMs are downloaded during its normal run.

Override the tooling repository/ref at image construction when needed:

```sh
docker build --build-arg DECOMP_REPOSITORY=https://github.com/gemmy-turbo/spf2t-decomp.git --build-arg DECOMP_REF=HEAD -t spf2t-native:aio .
```

`DECOMP_REF` accepts a branch, tag or commit. The resolved commit is saved in
`/opt/decomp/image-source-revision.txt`. To refresh a moving branch in an
existing Docker build cache, use `--no-cache-filter aio` with `docker buildx build`,
or rebuild with `--no-cache`. A named tag/commit is preferable for repeatable
releases; base/package inputs also affect the final image identity.

## Native-only: consume existing decomp output

```sh
docker build --target native-only -t spf2t-native:local .
```

First run the decomp image in the directory holding your `roms/` and `output/`:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/roms:/roms:ro" -v "$PWD/output:/output" spf2t-decomp:local
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/roms:/roms:ro" -v "$PWD/output:/output" spf2t-native:local
```

Only the image changes between runs. Native-only reads the complete handoff at
`/output/decomp/`. It includes no decomp project and performs no automatic fetch
or ROM import. The primary native process reports missing/incomplete handoff
files and tells users to run decomp first or use AIO.

## Build with a locally modified decomp project

The native build context is always your current local native checkout. To also
include a local decomp checkout without uploading it, use a named build context:

```sh
docker build --target aio-local --build-context decomp=../spf2t-decomp -t spf2t-native:aio-local .
mkdir -p output
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/roms:/roms:ro" -v "$PWD/output:/output" spf2t-native:aio-local
```

This uses the local decomp code and the same two primary processes; no GitHub
fetch is performed. The local-source target copies only code, compatibility metadata and notices;
it excludes ROMs, generated output, Git metadata and Python caches. See [Docker named contexts](https://docs.docker.com/build/concepts/context/#named-contexts).

## Output and host execution

All routes produce:

```text
output/
  decomp/                  complete source/generated/data handoff
  spf2t-native/
    puzzle-fighter-native  run this executable directly
    data/
    lib/
    licenses/
    user/                  created when played
```

```sh
./output/spf2t-native/puzzle-fighter-native
```

The executable locates adjacent data and default player state independently of
its working directory. Keep the whole `spf2t-native/` directory together when
moving it; no source checkout is needed to play. It bundles SDL3 and the GCC
C++ runtime, with notices. Linux system loader/glibc and display/audio backends
are still needed; Debian12/glibc2.36 is the container baseline. NixOS needs its
normal ELF packaging or a native host build. Windows/macOS are not runtime
targets. Local output contains original game content and is not a public
source-release artifact.

The images supply Python and the build tools. The user flag keeps output owned
by your host account; create `output/` before running. With rootless Docker, omit
`--user` when container root already maps to your host account. For Podman,
replace `docker` with `podman` and add `--userns=keep-id` to the run command. Configure suitable SELinux bind labels where
required. Podman execution is not verified here.

Image builds need network access for dependencies; decomp/native runs work
offline. An image digest identifies an exact image; rebuilding against moving
Git/apt inputs can change its contents.
