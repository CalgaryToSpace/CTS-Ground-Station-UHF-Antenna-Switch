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


## Relay Switching Logic

* 2 relays.
    * SW1: Picks between RX and TX.
    * SW2: Adds a dummy loads to the TX line.

* Actions:
    * Nominally: SW1 is in RX mode, and SW2 is in Dummy mode.
    * When sense is detected: SW1 switch to TX mode. Delay. SW2 to TX mode.
    * When sense is done: SW2 to Dummy mode. SW1 to RX mode. Delaying is unimportant.
