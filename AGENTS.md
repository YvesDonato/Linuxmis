# Repository Guidelines

## Project Structure & Module Organization

linuxmis is a Qt/C++ desktop streaming client. The root `artemis.pro` qmake project ties together the app and bundled libraries. Main code lives in `app/`: `backend/` handles GameStream/Apollo/Sunshine communication, `streaming/` handles audio/video/input sessions, `settings/` stores QSettings-backed preferences, `gui/` contains QML views, and `res/`, `shaders/`, and `languages/` hold assets and translations. Supporting qmake projects live in `moonlight-common-c/`, `qmdnsengine/`, `h264bitstream/`, `soundio/`, and `AntiHooking/`. Packaging and CI assets are in `scripts/`, `.github/`, `wix/`, and `docs/`.

## Build, Test, and Development Commands

- `nix build`: builds the default release package and updates `./result/bin/linuxmis`.
- `nix run`: builds if needed, then runs linuxmis.
- `nix develop`: enters the Qt/FFmpeg/SDL development shell.
- `direnv allow .`: enables automatic flake shell loading in this directory.
- `qmake artemis.pro CONFIG+=debug CONFIG+=disable-prebuilts CONFIG+=disable-libplacebo && make -j$(nproc) debug`: local debug build inside the dev shell.
- `nix flake check --no-build`: validates flake outputs without compiling the full app.

For local qmake builds, initialize submodules with `git submodule update --init --recursive`. Nix package builds fetch pinned inputs automatically.

## Coding Style & Naming Conventions

Follow the existing Qt/C++ style: 4-space indentation, nearby brace style, `PascalCase` for Qt classes, and `camelCase` for methods, variables, and QML properties. Prefer Qt APIs where surrounding code uses them (`QString`, `QSettings`, signals/slots). Update translations for user-visible strings. Do not reformat unrelated code.

## Testing Guidelines

There is no single mandatory test runner. Use targeted checks for the area changed: `nix build`, `nix flake check --no-build`, and focused helpers such as `test_hash.py`, `test_otp_hash.cpp`, or OTP pairing test sources when relevant. For streaming, renderer, or UI changes, launch `./result/bin/linuxmis` and start a real stream against Apollo or Sunshine.

## Commit & Pull Request Guidelines

Git history uses short conventional-style messages such as `fix: added libavformat to flatpak build`, `ci: Fix Flatpak HW decode and AppImage launcher`, and `chore: version bump for next dev build`. Prefer `type(scope): summary`, for example `fix(streaming): avoid Vulkan renderer crash`.

Open PRs from `feature/*`, `fix/*`, or `hotfix/*` branches. Include a clear description, testing performed, linked issues, and screenshots or video for UI changes. Target `develop` for normal work and reserve `main` for stable releases or urgent hotfixes.

## Security & Configuration Tips

Do not commit personal pairing keys, host certificates, generated logs, `result`, or `.direnv/`. Treat `~/.config/linuxmis/linuxmis.conf` as local user state, not fixture data.
