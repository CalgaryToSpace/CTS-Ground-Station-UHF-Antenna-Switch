# CTS UHF RX/TX Switch Firmware

This firmware runs on the RP2040-Zero microcontroller board on the PCB.

## Quick Reference: Building

To build the firmware, follow these steps:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j
```

## Pico SDK Setup (Linux / macOS)

Quick setup to build this firmware.

### 1. Install toolchain

**Linux (Debian/Ubuntu):**
```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 libstdc++-arm-none-eabi-newlib build-essential git
```

**macOS (Homebrew):**
```bash
brew install cmake
brew install --cask gcc-arm-embedded
```

### 2. Clone the Pico SDK

```bash
git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
export PICO_SDK_PATH=~/pico-sdk
```

Add the `export` line to your `~/.bashrc` / `~/.zshrc` so it persists.

### 3. Build the firmware

From this project's directory:
```bash
mkdir build && cd build
cmake ..
make -j
```

### 4. Flash

Hold **BOOTSEL** on the RP2040-Zero while plugging it in. It mounts as a USB drive — drag `frontiersat_rxtx_switch.uf2` onto it. It'll reboot and run.

### 5. Connect

Open the USB serial port (any baud, CDC ignores it):
```bash
# Linux
screen /dev/ttyACM0

# macOS
screen /dev/tty.usbmodem*
```

Press `0`–`3` to set log level.
