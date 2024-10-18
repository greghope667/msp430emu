let
	crossPkgs = import <nixpkgs> { crossSystem = { config = "msp430-elf"; }; };
	pkgs = import <nixpkgs> {};
in
pkgs.mkShell {
	packages = [
		crossPkgs.buildPackages.binutilsNoLibc
	];
}
