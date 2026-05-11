# CTS-Ground-Station-UHF-Antenna-Switch

PCB and firmware for a ground station UHF antenna switch (between high-power uplink and low-noise downlink, via a single antenna).

This is a high-power UHF antenna switch PCB (for separate RX/TX paths), controlled with automatic TX sensing.

## Features

* High Power: Supports TX power of >45 dBm (>30 W)
* Low insertion loss
* Easy-to-deploy (automatic carrier sensing on the TX line)
* Shunt-to-GND: Avoids damage to the transmitter by avoiding reflecting any power back on the TX port, in case of switching latency or failure. Dissipates as heat through a dummy load.
* LED indicators.
* Uses RF relays for switching. Simple design and power supply requirements (compared to PIN Diode approach).
* Optional slide switch to override control modes.

## Key Components

* RF Switch
    * Relay: `1462051-1` (3V control)

* Dummy Load Resistor


## License

**Hardware Parts:** CERN Open Hardware Licence Version 2 - Permissive
