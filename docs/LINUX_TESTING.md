# Linux Startup and Streaming Checks

## Build and Run

```sh
nix build
env -u LD_LIBRARY_PATH ./result/bin/linuxmis
```

`nix build` runs the Linux regression checks before installing the package.
Use the `result` executable to test local changes: a separately installed
system package still refers to its pinned revision until it is updated.

The development shell no longer exports `LD_LIBRARY_PATH`. Existing shells
may retain the old value; `env -u LD_LIBRARY_PATH` prevents it from overriding
the packaged SDL, FFmpeg, and graphics libraries.

## Automated Checks

`app/tests/regressions.cpp` uses Qt Test with test-only linker wrappers for
network requests and platform failures. It does not contact a streaming host
or use personal settings. It checks:

- Repeated display enumeration, missing displays, and balanced SDL ownership.
- Deferred, idempotent decoder capability probing.
- Cached-host discovery, explicit app-list fetching, polling cleanup, and
  rejection of unrelated host updates while quitting an app.
- Renderer preparation before rendering threads start, thread-creation
  failures, idle teardown, and one reset event per failed EGL renderer.

To rerun after a local qmake release build, from `app/` in `nix develop`:

```sh
qmake -o Makefile.tests regression-tests.pro CONFIG+=release CONFIG+=disable-prebuilts CONFIG+=disable-libplacebo
make -f Makefile.tests -j4 release
./linuxmis-regression-tests
```

## Live Verification

```sh
env -u LD_LIBRARY_PATH ./result/bin/linuxmis list HOST
env -u LD_LIBRARY_PATH ./result/bin/linuxmis stream --no-quit-after HOST Desktop
```

Check first launch, reconnect, opening Settings, and window resizing on the
target compositor. Compare the final decoding/rendering statistics using
the same resolution, FPS, bitrate, codec, and hardware-decoder settings.
The hardware capability scan now occurs on first opening Settings, not on
initial window creation; stream startup still validates its chosen decoder.

Logs include host discovery and HTTP endpoint/request timings without query
arguments. A delay before the first video packet is distinct from decoding
time. A successful automated check cannot establish real GPU presentation
or Windows-host reliability; verify those with live streams and screenshots.
