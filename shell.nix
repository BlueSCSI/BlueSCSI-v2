let
  # Pin nixpkgs to a specific revision for reproducibility.
  # See ./utils/update-nix-pin.sh for how to update pin.
  nixpkgs_rev = "6a08e6bb4e46";
  nixpkgs_hash = "0ixzzfdyrkm8mhfrgpdmq0bpfk5ypz63qnbxskj5xvfxvdca3ys3";

  pkgs =
    if builtins.getEnv "PINNED_NIXPKGS" == "1"
    then import (builtins.fetchTarball {
      url = "https://github.com/NixOS/nixpkgs/archive/${nixpkgs_rev}.tar.gz";
      sha256 = nixpkgs_hash;
    }) {}
    else import <nixpkgs> { };

  pico_sdk_rev = "v2.2.0-smaller-cyw43-spi-pio-rel";
  pico_extras_rev = "sdk-2.2.0";

  # Fetch pico-sdk from GitHub, including its submodules.
  # Using a specific revision for reproducibility.
  pico-sdk = pkgs.fetchFromGitHub {
    owner = "bluescsi";
    repo = "pico-sdk-internal";
    rev = pico_sdk_rev;
    fetchSubmodules = true;
    # nix-prefetch-git --url https://github.com/raspberrypi/pico-sdk --rev 2.2.0 --fetch-submodules
    hash = "sha256-RrJ1IIipggajM6MX+VXMILwfGdHt4o6Rj+utRxWE+mY=";
  };

  pico-extras = pkgs.fetchFromGitHub {
    owner = "raspberrypi";
    repo = "pico-extras";
    rev = pico_extras_rev;
    fetchSubmodules = true;
    # nix-prefetch-git --url https://github.com/raspberrypi/pico-extras --rev sdk-2.2.0 --fetch-submodules
    hash = "sha256-AfMycI+CMl76OERyRN8xQer7erh0wxpJnD4fu/Sl18c=";
  };

  fhs = pkgs.buildFHSEnv {
    name = "bluescsi-fhs";
    targetPkgs = pkgs: with pkgs; [
      platformio
      gcc-arm-embedded
      # dev pkgs
      cmake
      nodejs
      git
      git-filter-repo
      gh
      picotool
    ];

    profile = ''
      if [ -n "$BLUESCSI_FHS_ACTIVATED" ]; then
        return
      fi
      export BLUESCSI_FHS_ACTIVATED=1

      BLUE='[1;34m'
      GREEN='[1;32m'
      YELLOW='[1;33m'
      NC='[0m'
      export PICO_SDK_PATH="${pico-sdk}"
      export PICO_EXTRAS_PATH="${pico-extras}"
      echo -e "$BLUE""BlueSCSI development environment activated.""$NC"
      echo -e "$GREEN""PICO_SDK_PATH"\(${pico_sdk_rev}\)"$NC"" set to: ""$YELLOW""$PICO_SDK_PATH""$NC"
      echo -e "$GREEN""PICO_EXTRAS_PATH"\(${pico_extras_rev}\)"$NC"" set to: ""$YELLOW""$PICO_EXTRAS_PATH""$NC"
      echo -e "$(pio --version)"
      echo -e -n "$(arm-none-eabi-g++ --version | head -n1)\n"
      echo -e "$GREEN""nixpkgs version: ""$YELLOW""${pkgs.lib.version}""$NC"
      echo -e "$GREEN""nixpkgs hash: ""$YELLOW""${if builtins.getEnv "PINNED_NIXPKGS" == "1" then nixpkgs_hash else "unpinned"}""$NC"
    '';

    runScript = "/usr/bin/env zsh --login";
  };
in
pkgs.mkShell {
  buildInputs = [ fhs ];

  shellHook = ''
    # Only run if in an interactive shell
    if [ -t 1 ]; then
      echo "Entering FHS environment..."
      exec bluescsi-fhs
    fi
  '';
}
