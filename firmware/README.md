# CTS UHF RX/TX Switch Firmware

This firmware runs on the RP2040-Zero microcontroller board on the PCB.

## Building (Linux and macOS)

To build the firmware, follow these steps:

```bash
# Optionally, specify the Pico SDK path to your cloned Pico SDK (not recommended):
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
export PICO_SDK_PATH=~/pico-sdk

# If you skip the steps above (recommended to skip), cmake will automatically clone the Pico SDK.
cd firmware  # Navigate to the firmware directory of the repo.
mkdir build && cd build
cmake ..
make -j
```

## Flashing

1. Hold **BOOTSEL** on the RP2040-Zero while plugging it in. Or, hold **BOOTSEL** and press the reset button.
2. See that it mounts as a USB drive.
3. Drag `build/cts_uhf_rxtx_switch.uf2` onto it.
4. It'll reboot and run.

## Monitor via USB Serial

Open the USB serial port at 115200 baud:

```bash
# Linux
screen /dev/ttyACM0 115200

# macOS
screen /dev/tty.usbmodem* 115200
```

Press keys 0, 1, 2, 3 to set log level (0 = default and least verbose, 3 = most verbose).

## IDE Support

After building, enable IDE support by creating a symbolic link to `compile_commands.json` in the build directory:

```bash
cd firmware
ln -s build/compile_commands.json compile_commands.json
```
