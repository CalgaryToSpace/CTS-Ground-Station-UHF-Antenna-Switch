# Compare Rev 1 vs. Rev 2 Isolation (2026-06-22)

After purchasing the Rev 2 PCB, but before purchasing components to assemble Rev 2, I wanted to use bare PCBs to confirm that Rev 2 has better isolation than Rev 1. Rev 2 improves on the Rev 1 design by using fencing vias around RF traces.

**Summary:** It's at least a bit better, but may require more parts to fully quantify how much better.

## Preparation

1. With both Rev 1 and Rev 2 boards, take a fresh board.
2. For each of R1, R2, and U2, short together all their terminals (within each component).
3. Run a jumper within K1 (RX/TX relay) to short together the Uplink Input port and the Antenna port.
    * Note: Pre-check done with RX Out port and Antenna port shorted.
    * Varied between Configuration 1 (K1=RX) and Configuration 2/3 (K1=TX).
4. Solder on SMA Female stubs for RX OUT port and Uplink Input port.
5. Set the NanoVNA range to 400 MHz to 500 MHz. Calibrate it.

## Results

Isolation values are the XXX values of "S21 LOGMAG 10dB/ XXX dB".

### Configuration 1: K1=RX, K1 bodge wire tight against PCB

This configuration is not of particular interest, but was tested nonetheless.

* K1=RX, Rev 1 isolation (RX Out to Uplink Input): -52 dB
* K1=RX, Rev 2 isolation (RX Out to Uplink Input): -59 dB

### Configuration 2: K1=TX, K1 bodge wire tight against PCB

* K1=TX, Rev 1 isolation (RX Out to Uplink Input): -26 dB
* K1=TX, Rev 2 isolation (RX Out to Uplink Input): -30 dB

### Configuration 3: K1=TX, K1 bodge wire pulled 2mm above PCB

* K1=TX, Rev 1 isolation (RX Out to Uplink Input): -39.0 dB
* K1=TX, Rev 2 isolation (RX Out to Uplink Input): -39.6 dB

## Conclusion

Isolation appears to be improved with Rev 2 vs. Rev 1, especially looking at Configuration 1 (-52 dB -> -59 dB) and Configuration 2 (-26 dB -> -30 dB). However, there's significant error in this approach: the bodge wire used in place of the RF relay leaks to the trace underneath it.

Configuration 3 shows little improvement in isolation (-39.0 dB -> -39.6 dB), though matching bodge wire geometries was challenging.

Testing with a real relay would help offer more confidence, as the measured isolation improvement differ significant between Configuration 2 and Configuration 3.

### Threats to Validity

* Antenna port is open instead of terminated. Would be terminated in real deployment.
* In Configuration 3, matching the bodge wire geometry (height above PCB) between Rev 1 and Rev 2 was difficult and more art than science. Small geometry changes yielded large isolation changes.
