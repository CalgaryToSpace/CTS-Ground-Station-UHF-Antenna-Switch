# PCB Test Plan

## Basic Bring-Up

1. Check GND-to-3V3 and GND-to-5V rail resistance. Should be >30k.
2. Check for RF shorts to GND, especially at the high-power nodes through the relays.

## Initial Flash and Naive Functionality Checks

1. Flash microcontroller with firmware.
2. Open a serial terminal on the virtual serial port of the MCU.
3. Set the log level to the max (press 3 on the keyboard).
4. Ensure LEDs are the expected states. That is, all indicate RX mode by default.
5. Try switching the switch control state. Ensure the lights update as expected.
  * For each state, use an ohm meter to check that the path through the center pins of the RF connectors are as expected.
    * In RX and Auto (thus RX), there should be <1 ohm from Antenna to RX and high impedance to TX.
    * In TX mode, there should be <1 ohm from Antenna to TX and high impedance from Antenna to RX.

## RF Validation

1. **Details coming:** Use a VNA to confirm impedance, SWR, and insertion loss.

## Functional Validation

1. Flash board.
2. Set log level to max.
3. Send >1 watt (30 dBm) RF power into the input RF port.
4. Observe that the logs show detection.
5. Observe that the LEDs switch to the TX state, with the K1 switching first to TX, and then the dummy load being removed second.
