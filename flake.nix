{
    description = "Touhou Toolkit";

    inputs = {
        nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";
    };

    outputs = inputs: {
        packages = builtins.mapAttrs (system: pkgs: let
            inherit (pkgs) lib stdenv fetchFromGitHub;
            thtypes = fetchFromGitHub {
                owner = "thpatch";
                repo = "thtypes";
                rev = "29ed7d6e1db555ad15fd29edcc0763754ad5b764";
                hash = "sha256-j3lrToLuS7SbzzdL3/8uB0nL1z04GtmfmKIC/v37jwI=";
            };
        in {
            thtk = stdenv.mkDerivation {
                pname = "thtk";
                version = "12";

                src = inputs.self;

                env.NIX_CFLAGS_COMPILE = "-I${thtypes}";

                nativeBuildInputs = with pkgs; [
                    cmake
                    pkg-config
                    validatePkgConfig
                    flex
                    bison
                ];

                buildInputs = with pkgs; [
                    zlib
                    libpng
                ];

                cmakeFlags = [
                    (lib.cmakeBool "WITH_LIBPNG_SOURCE" false)
                ];

                meta = {
                    description = "Touhou Toolkit";
                    homepage = "https://github.com/thpatch/thtk";
                    changelog = "https://github.com/thpatch/thtk/releases";
                    license = lib.licenses.bsd2;
                    pkgConfigModules = [ "thtk" ];
                    platforms = with lib.platforms; linux ++ windows;
                };
            };

            default = inputs.self.packages.${system}.thtk;
        }) inputs.nixpkgs.legacyPackages;
    };
}
