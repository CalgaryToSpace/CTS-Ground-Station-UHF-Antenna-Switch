# Design Notes

## Checklist

* Unbroken ground plane under all RF.
* For FR4 (εr ≈ 4.5) with 1.6mm board: trace width ≈ 2.9mm

## Dummy Load

* Fan out microstrip to resistors.
* Use thin-film chip resistors. Standard thick-film resistors have too much parasitic inductance at UHF.
* Add a heatsink to the bottom of the board, potentially.
* Intended to operate only in small bursts, so thermal dissipation isn't as big of a deal.

### Test Notes

* SWR should be < 1.1:1 at 436 MHz for a good dummy load.
* Check for resonances. A peak in SWR somewhere means parasitic inductance or capacitance in your layout.

## Detector Chain

Input TX -> RF Directional Coupler -> Attenuator -> RF Detector -> Logic

* Input TX (rate up to 100W = 50 dBm for this assessment)
* RF Directional Coupler (50 dBm down - 33 dB = 17 dBm)
    * Part: X4C09F1-30S
* Attenuator (10 dB)
    * Part: PAT1220-C-10DB
* LTC5507 RF Detector
    * Supports -34 dBm to 14 dBm input.


## Relay Switching Logic

* 2 relays.
    * K1: Picks between RX and TX.
    * K2: Adds a dummy loads to the TX line when in RX mode.

* Actions:
    * Nominally: K1 is in RX mode, and K2 is in Dummy mode.
    * When sense is detected: K1 switch to TX mode. Delay. K2 to TX mode.
    * When sense is done: K2 to Dummy mode. K1 to RX mode. Delaying is unimportant.

