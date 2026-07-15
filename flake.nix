{
  description = "BlueSCSI-v2 SCSI development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin" ];
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          pico-sdk = pkgs.fetchFromGitHub {
            owner = "bluescsi";
            repo = "pico-sdk-internal";
            rev = "4458968e999258eb527a883ffbc4f184a90a8261"; # v2.3.0-bluescsi
            fetchSubmodules = true;
            hash = "sha256-SexHT0NOhnMSaKPDgudOxl1UciGQPLni+dDJpgRVJcg=";
          };

          pico-extras = pkgs.fetchFromGitHub {
            owner = "raspberrypi";
            repo = "pico-extras";
            rev = "sdk-2.3.0";
            fetchSubmodules = true;
            hash = "sha256-CkxLqfe8uqzz8H8mlY5UQbXJczydGpEyuUyRN/UhoUU=";
          };

          # tmp till merge in nixpkgs
          picotool = pkgs.picotool.overrideAttrs (old: {
            version = "2.3.0";
            src = pkgs.fetchFromGitHub {
              owner = "raspberrypi";
              repo = "picotool";
              rev = "2.3.0";
              hash = "sha256-w9kVCdwevEjc12NNZWztehp6SSgsd9ehSaxqc9sg4O4=";
            };
            # Our SDK fetch includes the mbedtls submodule, so nixpkgs'
            # mbedtls source substitution is unnecessary.
            postPatch = "";
            cmakeFlags = [ "-DPICO_SDK_PATH=${pico-sdk}" ];
          });

          pioasm = pkgs.pioasm.overrideAttrs (old: {
            version = "2.3.0";
            src = pico-sdk;
          });
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              gcc-arm-embedded-15
              cmake
              python3
              picotool
              pioasm
              git
              gh
              zip
            ];

            env = {
              PICO_SDK_PATH = "${pico-sdk}";
              PICO_EXTRAS_PATH = "${pico-extras}";
            };

            shellHook = ''
              unset SOURCE_DATE_EPOCH
              echo -e "\e[1;34mBlueSCSI development environment activated.\e[0m"
              echo -e "\e[1;32mPICO_SDK_PATH\e[0m: \e[1;33m$PICO_SDK_PATH\e[0m"
              echo -e "\e[1;32mPICO_EXTRAS_PATH\e[0m: \e[1;33m$PICO_EXTRAS_PATH\e[0m"
              echo -e "$(arm-none-eabi-g++ --version | head -n1)"
            '';
          };
        }
      );
    };
}
