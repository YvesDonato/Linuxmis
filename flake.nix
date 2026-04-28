{
  description = "linuxmis Qt Nix build and development shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    moonlightCommonC = {
      url = "github:ClassicOldSong/moonlight-common-c/ad329b240f18826f320ce6a99226b36354b86b59";
      flake = false;
    };

    enet = {
      url = "github:cgutman/enet/115a10baa1d7f291ff5b870765610fd3b4a6e43c";
      flake = false;
    };

    qmdnsengine = {
      url = "github:cgutman/qmdnsengine/b7a5a9f225d5e14b39f9fd1f905c4f505cf2ee99";
      flake = false;
    };

    h264bitstream = {
      url = "github:aizvorski/h264bitstream/34f3c58afa3c47b6cf0a49308a68cbf89c5e0bff";
      flake = false;
    };

    libsoundio = {
      url = "github:cgutman/libsoundio/34bbab80bd4034ba5080921b6ba6d61314126310";
      flake = false;
    };

    gameControllerDb = {
      url = "github:gabomdq/SDL_GameControllerDB/7979e7b29261c11ebce2deabc41ed081b6691398";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      enet,
      moonlightCommonC,
      qmdnsengine,
      h264bitstream,
      libsoundio,
      gameControllerDb,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          qt = pkgs.qt6;

          qtDeps = with qt; [
            qtbase
            qtdeclarative
            qtsvg
            qttools
            qtwayland
          ];

          multimediaDeps = with pkgs; [
            SDL2
            SDL2_ttf
            alsa-lib
            ffmpeg_6
            libpulseaudio
            openssl
            opus
          ];

          videoDeps = with pkgs; [
            libGL
            libdrm
            libva
            libvdpau
            libxkbcommon
            nv-codec-headers
            wayland
            wayland-protocols
            libx11
          ];

          sourceFilter =
            path: type:
            let
              name = builtins.baseNameOf path;
            in
            !(builtins.elem name [
              ".direnv"
              ".envrc"
              "flake.lock"
              "flake.nix"
              "result"
            ])
            && !(pkgs.lib.hasPrefix "result-" name);
        in
        rec {
          linuxmis = pkgs.stdenv.mkDerivation {
            pname = "linuxmis";
            version = pkgs.lib.removeSuffix "\n" (builtins.readFile ./app/version.txt);

            src = pkgs.lib.cleanSourceWith {
              src = ./.;
              filter = sourceFilter;
            };

            LC_ALL = "C.UTF-8";

            nativeBuildInputs = with pkgs; [
              pkg-config
              qt.qmake
              qt.wrapQtAppsHook
            ];

            buildInputs = qtDeps ++ multimediaDeps ++ videoDeps;

            postPatch = ''
              rm -rf \
                app/SDL_GameControllerDB \
                h264bitstream/h264bitstream \
                moonlight-common-c/moonlight-common-c \
                qmdnsengine/qmdnsengine \
                soundio/libsoundio

              mkdir -p \
                app/SDL_GameControllerDB \
                h264bitstream/h264bitstream \
                moonlight-common-c/moonlight-common-c \
                moonlight-common-c/moonlight-common-c/enet \
                qmdnsengine/qmdnsengine \
                soundio/libsoundio

              cp -R --no-preserve=mode,ownership ${gameControllerDb}/. app/SDL_GameControllerDB
              cp -R --no-preserve=mode,ownership ${h264bitstream}/. h264bitstream/h264bitstream
              cp -R --no-preserve=mode,ownership ${moonlightCommonC}/. moonlight-common-c/moonlight-common-c
              rm -rf moonlight-common-c/moonlight-common-c/enet
              mkdir -p moonlight-common-c/moonlight-common-c/enet
              cp -R --no-preserve=mode,ownership ${enet}/. moonlight-common-c/moonlight-common-c/enet
              cp -R --no-preserve=mode,ownership ${qmdnsengine}/. qmdnsengine/qmdnsengine
              cp -R --no-preserve=mode,ownership ${libsoundio}/. soundio/libsoundio
            '';

            configurePhase = ''
              runHook preConfigure
              qmake artemis.pro \
                CONFIG+=release \
                CONFIG+=disable-prebuilts \
                CONFIG+=disable-libplacebo \
                PREFIX=$out \
                BINDIR=bin
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              make -j$NIX_BUILD_CORES release
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              make install
              runHook postInstall
            '';

            meta = {
              description = "linuxmis Qt client for NVIDIA GameStream, Apollo, and Sunshine servers";
              homepage = "https://github.com/wjbeckett/artemis";
              license = pkgs.lib.licenses.gpl3Only;
              platforms = systems;
              mainProgram = "linuxmis";
            };
          };

          artemis = linuxmis;
          default = linuxmis;
        }
      );

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/linuxmis";
          meta.description = "Run linuxmis Qt";
        };
      });

      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          runtimeLibs = with pkgs; [
            SDL2
            SDL2_ttf
            alsa-lib
            ffmpeg_6
            libGL
            libdrm
            libpulseaudio
            libva
            libvdpau
            libxkbcommon
            openssl
            opus
            wayland
            libx11
          ];
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];

            packages = with pkgs; [
              gdb
              git
              gnumake
              pkg-config
            ];

            LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath runtimeLibs;

            shellHook = ''
              if [ ! -d qmdnsengine/qmdnsengine/src ]; then
                echo "Submodules are not initialized for local qmake builds."
                echo "Run: git submodule update --init --recursive"
                echo "Nix package builds fetch pinned submodule inputs automatically."
              fi

              echo "linuxmis Qt dev shell"
              echo "Package build: nix build"
              echo "Local debug build: qmake artemis.pro CONFIG+=debug CONFIG+=disable-prebuilts CONFIG+=disable-libplacebo && make -j\$(nproc) debug"
            '';
          };
        }
      );

      formatter = forAllSystems (system: (import nixpkgs { inherit system; }).nixfmt);
    };
}
