{
  description = "Textor - a lightweight X11 code editor";

  inputs = {
    nixpkgs.url     = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # Build-time and runtime dependencies
        nativeDeps = with pkgs; [
          clang
          pkg-config
        ];

        runtimeDeps = with pkgs; [
          cairo
          pango
          libx11              # was xorg.libX11  (renamed in nixpkgs-unstable)
          libxext             # was xorg.libXext
          libxrender          # was xorg.libXrender
          libGL               # GL/gl.h + GL/glx.h headers + libGL.so (libglvnd)
          mesa                # libGLX.so
          glib
          fontconfig
          freetype
        ];

        # -- textor package -------------------------------------------------
        textor = pkgs.stdenv.mkDerivation {
          pname   = "textor";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = nativeDeps;
          buildInputs       = runtimeDeps;

          buildPhase = ''
            mkdir -p build
            clang++ -O3 -std=c++17 -Wall -flto \
              $(pkg-config --cflags cairo x11 pango pangocairo) \
              base.cxx ui/morphing.cxx -o build/textor \
              $(pkg-config --libs cairo x11 pango pangocairo) \
              -lGL -lGLX -lpthread
          '';

          installPhase = ''
            install -Dm755 build/textor $out/bin/textor

            # Bundled addons (if any)
            if [ -d addons ] && [ "$(ls -A addons 2>/dev/null)" ]; then
              install -dm755 $out/share/textor/addons
              cp -r addons/* $out/share/textor/addons/ 2>/dev/null || true   # one folder per addon, with svg/
            fi

            # Desktop entry
            install -Dm644 /dev/stdin $out/share/applications/textor.desktop << EOF
[Desktop Entry]
Name=Textor
Comment=Lightweight X11 code editor with addon support
Exec=textor
Icon=accessories-text-editor
Terminal=false
Type=Application
Categories=Development;TextEditor;
MimeType=text/plain;text/x-csrc;text/x-chdr;text/x-c++src;
EOF
          '';

          meta = with pkgs.lib; {
            description  = "Lightweight X11 Cairo code editor with per-token syntax coloring and addon system";
            homepage     = "https://github.com/user/textor";
            license      = licenses.mit;
            platforms    = [ "x86_64-linux" "aarch64-linux" ];
            mainProgram  = "textor";
          };
        };

      in {
        # -- Outputs --------------------------------------------------------
        packages = {
          default = textor;
          inherit textor;
        };

        # nix run .
        apps.default = flake-utils.lib.mkApp {
          drv  = textor;
          name = "textor";
        };

        # nix develop  - drop into a shell ready to build
        devShells.default = pkgs.mkShell {
          name = "textor-dev";

          nativeBuildInputs = nativeDeps;
          buildInputs       = runtimeDeps;

          shellHook = ''
            echo ""
            echo "  +==================================+"
            echo "  |   Textor development shell       |"
            echo "  +==================================+"
            echo ""
            echo "  Build:   mekanism base   (or the one-liner below)"
            echo "  Manual:  clang++ -O2 -std=c++17 -Wall \\"
            echo "             \$(pkg-config --cflags cairo x11 pango pangocairo) \\"
            echo "             base.cxx ui/morphing.cxx -o build/textor \\"
            echo "             \$(pkg-config --libs cairo x11 pango pangocairo) -lGL -lGLX"
            echo ""
          '';
        };

        # Formatter for nix files
        formatter = pkgs.nixpkgs-fmt;
      }
    )
    //
    # -- NixOS module ------------------------------------------------------
    {
      nixosModules.default = { config, lib, pkgs, ... }:
        let cfg = config.programs.textor;
        in {
          options.programs.textor = {
            enable = lib.mkEnableOption "Textor code editor";
            package = lib.mkOption {
              type    = lib.types.package;
              default = self.packages.${pkgs.system}.textor;
              description = "The textor package to use.";
            };
          };

          config = lib.mkIf cfg.enable {
            environment.systemPackages = [ cfg.package ];
          };
        };
    };
}
