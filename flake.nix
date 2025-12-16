{
  description = "Tetris from SSH";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    {
      overlays = {
        default = self.overlays.tetris;
        tetris =
          (final: prev: { tetris = self.packages.${prev.system}.default; });
      };
    } // flake-utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages.default = pkgs.clangStdenv.mkDerivation {
          pname = "tetris";
          version = "0.0.1";
          src = ./src/tetris.c;
          dontUnpack = true;
          buildInputs = with pkgs; [ libssh ];
          buildPhase = ''
            clang $src -O3 -std=c99 -Wall -Wextra $(pkg-config --cflags libssh) -o tetris $(pkg-config --libs libssh)
          '';
          installPhase = ''
            mkdir -p $out/bin
            cp tetris $out/bin/tetris
          '';
        };
        devShells = {
          default = pkgs.mkShell {
            buildInputs = with pkgs; [ libssh ];
            packages = with pkgs; [ pkg-config clang-tools clang bear ];
          };
        };
      });
}
