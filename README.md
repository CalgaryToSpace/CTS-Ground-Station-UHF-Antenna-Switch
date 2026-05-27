# CTS-Ground-Station-UHF-Antenna-Switch

PCB and firmware for a ground station UHF antenna switch (between high-power uplink and low-noise downlink, via a single antenna).

This is a high-power UHF antenna switch PCB (for separate RX/TX paths), controlled with automatic TX sensing.

## Status

Rev 1 PCB works and is deployed at the CalgaryToSpace ground station as of 2026-05-26.

Rev 2 PCB is under development, though it is only a small incremental improvement over Rev 1. Main goal with Rev 2 is to improve the isolation performance.

## Render

![PCB Top Render](./docs/PCB_Render_Top.png)

See the Releases for schematics, gerbers, and BOM.

## Features

* High Power: Supports TX power of >45 dBm (>30 W)
* Low insertion loss
* Automatic carrier sensing on the TX line (easy to deploy)
* Shunt-to-GND: Avoids damage to the transmitter by avoiding reflecting any power back on the TX port, in case of switching latency or failure. Dissipates as heat through a dummy load.
* LED indicators.
* Uses RF relays for switching. Simple design and minimal power supply requirements, compared to PIN Diode alternative approach.
* Logs via USB serial interface.
* Optional slide switch to override control modes.
* Optional control via USB serial interface overrides.

## Key Components

* RF Relay: `1462051-1` (`HF3-1` with 3V control, aka `HF3-51`)
* Dummy Load Resistor: `I100N50X4B`
* RF Attenuator (10 dB): `PAT1220-C-10DB-T5`
* RF Detector: `LTC5507ES6`
* Microcontroller: RP2040-Zero PCB

## License

* **Hardware Parts:** CERN Open Hardware Licence Version 2 - Permissive
* **Firmware:** Apache 2.0 License
