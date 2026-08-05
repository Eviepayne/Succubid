{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs = inputs: inputs.flake-parts.lib.mkFlake { inherit inputs; } {

    systems = [ "x86_64-linux" "aarch64-linux" "armv6l-linux" "armv7l-linux" "x86_64-darwin" "aarch64-darwin" ];
    perSystem = { pkgs, self', ... }: {
      packages.default = self'.packages.succubid;
      packages.succubid = import ./succubid.nix { inherit pkgs; };
    };
  };
}
