{
  description = "Crush 80 ZMK Firmware Build Environment & Tooling";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux" "aarch64-linux" "aarch64-darwin"];
    forEachSupportedSystem = f:
      nixpkgs.lib.genAttrs supportedSystems (system:
        f {
          pkgs = nixpkgs.legacyPackages.${system};
          inherit system;
        });
  in {
    packages = forEachSupportedSystem ({pkgs, ...}: let
      zephyrSdk = pkgs.stdenv.mkDerivation rec {
        pname = "zephyr-sdk-riscv";
        version = "1.0.1";

        srcMinimal = pkgs.fetchurl {
          url = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${version}/zephyr-sdk-${version}_linux-x86_64_minimal.tar.xz";
          hash = "sha256-ypvA/2b6/KHaydWSo22VPPFtCWqdCbHANX8CHPn2p+s=";
        };

        srcToolchain = pkgs.fetchurl {
          url = "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${version}/toolchain_gnu_linux-x86_64_riscv64-zephyr-elf.tar.xz";
          hash = "sha256-AXUINMRx+9szXBuLiu4XAQoZaJOJV9uFZAw2YjV3Gjg=";
        };

        dontBuild = true;
        dontConfigure = true;

        unpackPhase = "true";

        installPhase = ''
          mkdir -p $out
          tar -xf $srcMinimal -C $out --strip-components=1
          tar -xf $srcToolchain -C $out
        '';
      };

      pythonEnv = pkgs.python3.withPackages (ps:
        with ps; [
          west
          pyelftools
          pykwalify
          protobuf
          grpcio-tools
          hidapi
        ]);

      tools = [
        pkgs.cmake
        pkgs.ninja
        pkgs.dtc
        pkgs.git
        pkgs.gnumake
        pkgs.mcumgr-client
        pkgs.usbutils
        pythonEnv
      ];
    in rec {
      inherit zephyrSdk;

      build = pkgs.writeShellScriptBin "crush80-build" ''
        export ZEPHYR_SDK_INSTALL_DIR="''${ZEPHYR_SDK_INSTALL_DIR:-${zephyrSdk}}"
        export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
        export PATH="${pkgs.lib.makeBinPath tools}:$PATH"
        exec bash ./scripts/build.sh "$@"
      '';

      install = pkgs.writeShellScriptBin "crush80-install" ''
        export ZEPHYR_SDK_INSTALL_DIR="''${ZEPHYR_SDK_INSTALL_DIR:-${zephyrSdk}}"
        export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
        export PATH="${pkgs.lib.makeBinPath tools}:$PATH"
        exec bash ./scripts/install_zmk.sh "$@"
      '';

      update = pkgs.writeShellScriptBin "crush80-update" ''
        export ZEPHYR_SDK_INSTALL_DIR="''${ZEPHYR_SDK_INSTALL_DIR:-${zephyrSdk}}"
        export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
        export PATH="${pkgs.lib.makeBinPath tools}:$PATH"
        exec bash ./scripts/update.sh "$@"
      '';

      default = build;
    });

    devShells = forEachSupportedSystem ({pkgs, system, ...}: let
      sdk = self.packages.${system}.zephyrSdk;
    in {
      default = pkgs.mkShell {
        packages = [
          pkgs.cmake
          pkgs.ninja
          pkgs.dtc
          pkgs.git
          pkgs.gnumake
          pkgs.mcumgr-client
          pkgs.usbutils
          (pkgs.python3.withPackages (ps:
            with ps; [
              west
              pyelftools
              pykwalify
              protobuf
              grpcio-tools
              hidapi
            ]))
        ];

        shellHook = ''
          export ZEPHYR_SDK_INSTALL_DIR="''${ZEPHYR_SDK_INSTALL_DIR:-${sdk}}"
          export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
          export PATH="$HOME/.local/bin:$HOME/go/bin:$PATH"
        '';
      };
    });

    apps = forEachSupportedSystem ({system, ...}: rec {
      build = {
        type = "app";
        program = "${self.packages.${system}.build}/bin/crush80-build";
        meta.description = "Build Wobkey Crush 80 ZMK firmware targets";
      };
      install = {
        type = "app";
        program = "${self.packages.${system}.install}/bin/crush80-install";
        meta.description = "Install ZMK firmware onto factory stock keyboard";
      };
      update = {
        type = "app";
        program = "${self.packages.${system}.update}/bin/crush80-update";
        meta.description = "Flash ZMK firmware via mcumgr DFU";
      };
      default = build;
    });
  };
}
