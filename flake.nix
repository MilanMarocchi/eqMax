{
  description = "eqMax - Advanced macOS Audio Equalizer and Pro Audio Enhancer in C++20 and Qt 6";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
        
        appleSdk = if pkgs.stdenv.hostPlatform.isDarwin then [ pkgs.apple-sdk ] else [];
      in
      {
        devShells.default = pkgs.mkShell {
          name = "eqMax-dev-shell";

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qttools
          ] ++ appleSdk;

          shellHook = ''
            echo "=========================================================="
            echo " Welcome to eqMax Dev Shell (C++20, Qt 6, macOS CoreAudio)"
            echo "=========================================================="
            export CMAKE_BUILD_TYPE=Debug
          '';
        };

        packages.default = pkgs.stdenv.mkDerivation {
          pname = "eqMax";
          version = "1.0.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qttools
          ] ++ appleSdk;

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          postInstall = ''
            mkdir -p $out/Applications
            if [ -d "$out/bin/eqMax.app" ]; then
              mv $out/bin/eqMax.app $out/Applications/eqMax.app
            fi
          '';
        };

        apps.install = {
          type = "app";
          program = toString (pkgs.writeShellScript "install-eqmax" ''
            echo "Building eqMax.app..."
            ${pkgs.nix}/bin/nix build .#default
            TARGET_DIR="$HOME/Applications"
            mkdir -p "$TARGET_DIR"
            echo "Installing eqMax.app into $TARGET_DIR/..."
            rm -rf "$TARGET_DIR/eqMax.app"
            cp -R result/Applications/eqMax.app "$TARGET_DIR/"
            echo "=========================================================="
            echo " Successfully installed eqMax.app in $TARGET_DIR/eqMax.app!"
            echo " You can open it from Launchpad or Finder -> Applications."
            echo "=========================================================="
          '');
        };
      }
    );
}
