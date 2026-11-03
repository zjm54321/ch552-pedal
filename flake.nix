{
  description = "Reproducible native CH552G development tools";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/1c89083e451e891152365b799595cf7790a43325";

  outputs = { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          sdcc # Native mcs51 compiler and packihx.
          gnumake
          wchisp
          python3
          stdenv.cc # Host compiler for portable logic tests.
          just
        ];
      };
    };
}
