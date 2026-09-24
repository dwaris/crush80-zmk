# Crush 80 ZMK Firmware

Custom ZMK firmware for the Wobkey Crush 80 (Telink TLSR9518 / B91 RISC-V).
Ported from [scholzri/rainy75-zmk](https://github.com/scholzri/rainy75-zmk).

Keybindings mirror the stock Wobkey factory configuration and:
- `Fn + Esc`: Enters MCUboot bootloader mode (instead of factory reset).
- `Fn + Ins`: Clears active Bluetooth profile pairing.

## Quick Start

### 1. Build

```bash
nix run .#build
```

### 2. Flash

- **First-time install (from factory stock firmware):**
  ```bash
  nix run .#install
  ```
  This stages the OTA bridge via vendor HID protocol, then installs ZMK.

- **Updating (already running ZMK):**
  ```bash
  nix run .#update
  ```
  Flashes via mcumgr over `/dev/ttyACM0`. Unplug and replug the USB cable for 2 seconds after flashing to complete the image swap.

Note: For serial access on NixOS, add your user to the `dialout` group (`users.users.<name>.extraGroups = [ "dialout" ];`) or run `sudo chmod 666 /dev/ttyACM0`.

## Non-Nix Environments

```bash
bash scripts/setup.sh
bash scripts/build.sh
bash scripts/install_zmk.sh   # first-time install from stock
# bash scripts/update.sh  # updates
```

## License

[Apache-2.0](LICENSE).
