# −40 dB Short-Section Microstrip Coupler at 436 MHz

## Chosen Configuration

| Parameter | Value |
|---|---|
| Trace width W | 2.9 mm |
| Gap S | ~2.5 mm |
| Coupled length L | 12 mm |

## Substrate

- FR-4, 1.6 mm thick, 2-layer
- εr = 4.5
- 1 oz copper (35 µm)
- Target Z0 = 50 Ω

## Design Goal

Sense 30–100 W transmit power at 436 MHz for an automatic PTT trigger, feeding an LTC5507ES6 RF power detector (input range −34 to +15 dBm).

## Coupling Math

Target coupling: **C = −40 dB**, so k = 10^(−40/20) = 0.01

For a short coupled section (L ≪ λ/4), coupling scales approximately as:

```
C(short) ≈ C(λ/4) × sin(βL)
```

where β = 2π / λg is the propagation constant in the medium.

**Effective wavelength on FR-4 (εr ≈ 4.5, εeff ≈ 3.35):**

```
λg = c / (f × √εeff) = (3×10⁸) / (436×10⁶ × √3.35) ≈ 376 mm
```

**Electrical length of the 12 mm section:**

```
βL = (2π / 376) × 12 ≈ 0.200 rad  (≈11.5°)
sin(βL) ≈ 0.199
```

To get final coupling of −40 dB (k = 0.01), the equivalent quarter-wave coupling would need to be:

```
k(λ/4) = 0.01 / 0.199 ≈ 0.0503  →  ≈ −26 dB
```

So the cross-section is sized for "moderate" coupling (~−26 dB at full λ/4), then truncated to 12 mm to attenuate the coupling down to −40 dB. The 2.5 mm gap achieves this moderate per-unit-length coupling on the given stackup.

## Power Budget

| TX Power | Main line | Coupled port (−40 dB) |
|---|---|---|
| 30 W | +44.8 dBm | +4.8 dBm |
| 100 W | +50.0 dBm | +10.0 dBm |

Add a **−10 dB chip attenuator** (0603/0805, ≥100 mW rated) between coupler and detector:

| TX Power | LTC5507 input |
|---|---|
| 30 W | −5.2 dBm |
| 100 W | 0.0 dBm |

Both well inside the LTC5507's log-linear range (−34 to +15 dBm).

## Signal Chain

```
TX in ──┬───────────────── TX out (to antenna)
        │
     [−40 dB coupler: 12 mm × 2.5 mm gap, W = 2.9 mm]
        │
     [−10 dB chip pad]
        │
     [LTC5507 RFIN]
        │
     [VOUT → comparator / MCU ADC → PTT logic]
```

The isolated port of the coupler is terminated in a 50 Ω SMD resistor (1206 thin-film, 0.5 W).

## Expected Performance

- Coupling: −40 dB ± ~2 dB (FR-4 εr variation)
- Insertion loss on through-line: < 0.1 dB
- Directivity: 6–10 dB (irrelevant for PTT sensing)
- Frequency response: essentially flat across the 70 cm band

## Tradeoffs Accepted

- No frequency selectivity (broadband coupling)
- Low directivity (forward and reverse couple nearly equally)
- Coupling rises with frequency below λ/4, but variation across a few MHz is negligible

## Layout Notes

- Solid ground plane on layer 2, no splits under the coupler
- Via fence along outer edges of the coupled section, ~5 mm pitch
- Feed lines: standard 50 Ω microstrip (~2.9 mm wide), continuous with coupler width
- Keep coupled-port output trace short; isolate detector area with ground pour and stitching vias
- Small RC lowpass (1 kΩ + 100 pF) on LTC5507 VOUT before the comparator to reject residual RF
