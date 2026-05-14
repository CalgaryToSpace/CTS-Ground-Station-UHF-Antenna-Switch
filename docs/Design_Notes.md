# Design Notes

## Checklist

* Unbroken ground plane under all RF.

## Dummy Load

* Use thin-film chip resistors. Standard thick-film resistors have too much parasitic inductance at UHF.
* Add a heatsink to the bottom of the board, potentially.
* Intended to operate only in small bursts, so thermal dissipation isn't as big of a deal.

### Test Notes

* SWR should be < 1.1:1 at 436 MHz for a good dummy load.
* Check for resonances. A peak in SWR somewhere means parasitic inductance or capacitance in the layout.

## Detector Chain

Input TX -> RF Coupler with PCB Traces -> Attenuator -> RF Detector -> Logic

* Input TX (rate up to 100W = 50 dBm for this assessment)
* RF Coupler with PCB Traces (50 dBm - 40 dB = 10 dBm)
    * Raw PCB traces running parallel to each other on the PCB act as an RF coupler.
    * Target -40 dB power transfer in coupler.
    * See the design doc for this coupler section specifically for more details.
* Attenuator (10 dB)
    * Part: PAT1220-C-10DB
* LTC5507 RF Detector
    * Supports -34 dBm to 14 dBm input.
    * At 50 dBm input, output power is approximately 10 dBm.
    * At 100 mW (20 dBm), output power is approximately -30 dBm. Should be possible to test with even just an SDR directly.

### Alternative Plan

<details summary="Click to expand">

Input TX -> RF Directional Coupler -> Attenuator -> RF Detector -> Logic

* Input TX (rate up to 100W = 50 dBm for this assessment)
* RF Directional Coupler (50 dBm down - 33 dB = 17 dBm)
    * Part: X4C09F1-30S
* Attenuator (10 dB)
    * Part: PAT1220-C-10DB
* LTC5507 RF Detector
    * Supports -34 dBm to 14 dBm input.

</details>

## RF Connectors

* SMA Connectors are [rated to >100W](https://electronics.stackexchange.com/a/74929), under ideal conditions and construction.
* N-Type connectors are better, where feasible. A [PCB-mount N-Type connector](https://www.mouser.ca/ProductDetail/Amphenol-RF/172234) is a good option.

* Alternatives considered:
    * https://www.digikey.ca/en/products/detail/huber-suhner-inc/92-N-50-0-6-111-NY/16806991
    * https://www.digikey.ca/en/products/detail/amphenol-rf/83-1R/80248

## Footprints

* RP2040-Zero footprint copied from https://github.com/dj505/RP2040-Zero-KiCAD (CERN Open Hardware License v2 - Permissive).

## Relay Switching Logic

* 2 relays.
    * K1: Picks between RX and TX.
    * K2: Adds a dummy loads to the TX line when in RX mode.

* Actions:
    * Nominally: K1 is in RX mode, and K2 is in Dummy mode.
    * When sense is detected: K1 switch to TX mode. Delay. K2 to TX mode.
    * When sense is done: K2 to Dummy mode. K1 to RX mode. Delaying is unimportant.

## Stackup, RF PCB Impedance

By using JLCPCB's default 1.6mm FR4 2-layer board, we can get a 50 ohm impedance with a large trace width, which is helpful for managing the high current.

For nearly the same price, we could have used a 4-layer board with a smaller trace width, but we intenionally take advantage of the thick trace width to carry more current.

Using [Digikey's PCB Trace Impedance Calculator](https://www.digikey.com/en/resources/conversion-calculators/conversion-calculator-pcb-trace-impedance):
* Input Target Impedance: 50 ohm
* Input Trace Thickness: 1 oz/ft^2
* Input Height (dielectric thickness): 1.58 mm
* Input Dielectric Constant: 4.5 ([Source](https://jlcpcb.com/capabilities/pcb-capabilities))
* Output Trace Width: 2.88mm

JLCPCB stackup: FR4 TG135 1.6mm 2-layer (default)
