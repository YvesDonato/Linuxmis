# Native-Pixel Mango Split

The optional Mango integration reserves a right-hand stream region on DP-2 at
scale 1. Each decoded video pixel occupies one physical display pixel. The
remaining left region uses the adaptive scroller, with at least 640 pixels for
local applications. Shorter streams are centered vertically. Panel reservations
are respected; Windows DPI, compression, codec, and bitrate are unchanged.
Native mode uses EGL/SDL rendering; Vulkan is excluded because its acquired
swapchain frames cannot safely be withheld by the presentation gate. The default
Nix package already disables the Vulkan/libplacebo backend.

This requires both the updated Linuxmis client and the matching Mango patch in
the NixOS checkout. An unpatched compositor cannot confirm a reservation and the
client fails with an error instead of displaying scaled video.

## Configuration

The Mango session exports `LINUXMIS_MANGO_SPLIT=1`, including into the user
service and D-Bus activation environment. Both GUI and CLI launches use the mode
when `XDG_CURRENT_DESKTOP` includes `mango`. Other desktops are unaffected.

Only the SDL window titled `Linuxmis Native Stream`, with app ID
`com.linuxmis.linuxmis`, receives the `reserve_right:1` Mango rule. The regular
GUI retains ordinary tiling; CLI streaming uses a compact, fixed-size loading
window that Mango floats automatically. The reserved stream cannot be resized,
floated, grouped, or made fullscreen. Existing mouse and keyboard preferences
still apply. Use `LINUXMIS_MANGO_SPLIT=0 linuxmis` for ordinary window behavior.

At the current 3398x1440 usable size, 1920x1440 leaves 1478 pixels on the left;
2758x1440 is the maximum stream size. Oversized streams, unavailable DP-2,
fractional scaling, and a second stream on the same visible workspace are
rejected. Rejection disconnects without quitting the Windows application.

## Verification

`nix build` runs the Qt regression tests, including native geometry validation,
IPC timeout handling, and presentation gating. The Mango patch includes geometry
tests. Live verification should cover 1920x1080, 1920x1440, boundary sizes,
workspace changes, output removal, mouse coordinates, and restoration on close.
Use a one-pixel test pattern to inspect scaling; normal video compression can
still affect sharpness even with a pixel-exact viewport.

The standalone smoke client needs no host connection or pairing data. Build it
inside `nix develop`, then run it against an isolated patched Mango instance:

```sh
c++ -std=c++17 -fPIC -Iapp app/tests/mango-native-smoke.cpp \
  app/streaming/mangonativesplit.cpp $(pkg-config --cflags --libs Qt6Core sdl2) \
  -o /tmp/linuxmis-mango-smoke
/tmp/linuxmis-mango-smoke native 1920 1080 10000
```

Its arguments are `native|local WIDTH HEIGHT DURATION_MS`. It exits nonzero if
the native reservation cannot be established, and renders alternating one-pixel
columns only after acceptance. Set `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` to the
isolated compositor, not the live desktop, when running automated scenarios.
Start the isolated compositor with `env -u DBUS_SESSION_BUS_ADDRESS` as well.
Mango otherwise imports its test display and socket into the live D-Bus and
systemd user environment, even when `XDG_RUNTIME_DIR` is private. If this has
already happened, run `dbus-update-activation-environment --systemd DISPLAY
WAYLAND_DISPLAY MANGO_INSTANCE_SIGNATURE` from a terminal opened by the live
compositor, then `systemctl --user restart quickshell.service`.

Startup uses bounded `mmsg get all-clients` queries. Its `reservation` object
reports `pending`, `accepted`, or `rejected`, requested dimensions, rejection
reason, usable bounds, and scale. No background polling runs during normal
playback. A neutral buffer maps the window before checking acceptance; video is
gated until both the reservation and physical drawable are correct.

## Startup

The CLI loading window shows the connection stage without shortcut tips or a
toolbar. Configuration warnings wrap below the stage and are logged, without
the former 3.5-second delay per warning. Fatal errors and quit confirmations
remain interactive. UI-only controller scans are deferred until needed; stream
controller initialization is unchanged. Successful decoder probes are reused
only within one validation pass, never across sessions or for the real renderer.

Nix builds precompile QML with the pinned Qt runtime using
[`qtquickcompiler`](https://doc.qt.io/qt-6/qmldiskcache.html). This also avoids
loading stale disk-cached UI code after a reproducible build with unchanged
resource timestamps. Non-Nix developer builds still use Qt's normal disk cache.

For matched timing comparisons, use the same Qt/SDL dependencies, host state,
stream settings, and compositor. Run `linuxmis stream yves desktop --no-quit-after`
with each binary, then disconnect normally without quitting the Windows app.
Compare `Startup: stream validation completed`, `Received first video packet`,
and `First video frame submitted to renderer` logs across several runs. The
last is a render-submission timestamp, not measured display scanout. Do not
count shader compilation during the decoder preflight as the first stream frame.

## NixOS Deployment

Keep the existing GitHub input. Validate local changes without changing its pin:

```sh
cd /home/yvesd/nixos
nix build --dry-run --no-link --no-write-lock-file \
  --override-input linuxmis path:/home/yvesd/Codebases/Linuxmis \
  .#nixosConfigurations.nixos.config.system.build.toplevel
```

Publish the Linuxmis commit on `develop`, then refresh only the `linuxmis` input:

```sh
git -C /home/yvesd/Codebases/Linuxmis push origin develop
cd /home/yvesd/nixos
nix flake update linuxmis --refresh
git diff -- flake.lock
nixos-rebuild build --flake .#nixos
sudo nixos-rebuild switch --flake .#nixos
```

Run each step only after the previous one succeeds. Check that the lockfile's
`linuxmis` revision matches the published commit. Review any other pending NixOS
edits before rebuilding: the build includes tracked working-tree changes, not
only committed changes. Home Manager is included in this system configuration;
a separate `home-manager switch` is unnecessary.

Save your work, log out of Mango, and log back in (or reboot). A config reload
alone cannot load a new compositor binary. Launch `linuxmis` in the new session;
an accepted stream logs `Mango native split accepted`. Use
`LINUXMIS_MANGO_SPLIT=0 linuxmis` to temporarily bypass the integration.
