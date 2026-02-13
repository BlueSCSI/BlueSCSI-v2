{
  description = "BlueSCSI-v2 SCSI development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          pico-sdk = pkgs.fetchFromGitHub {
            owner = "bluescsi";
            repo = "pico-sdk-internal";
            rev = "v2.2.0-smaller-cyw43-spi-pio-rel";
            fetchSubmodules = true;
            hash = "sha256-RrJ1IIipggajM6MX+VXMILwfGdHt4o6Rj+utRxWE+mY=";
          };

          pico-extras = pkgs.fetchFromGitHub {
            owner = "raspberrypi";
            repo = "pico-extras";
            rev = "sdk-2.2.0";
            fetchSubmodules = true;
            hash = "sha256-AfMycI+CMl76OERyRN8xQer7erh0wxpJnD4fu/Sl18c=";
          };
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              gcc-arm-embedded
              cmake
              python3
              picotool
              git
              gh
            ];

            env = {
              PICO_SDK_PATH = "${pico-sdk}";
              PICO_EXTRAS_PATH = "${pico-extras}";
            };

            shellHook = ''
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
